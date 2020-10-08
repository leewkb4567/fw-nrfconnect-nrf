/*
 * Copyright (c) 2012 - 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <sys/byteorder.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#include <bluetooth/services/ancs_c.h>
#include <bluetooth/services/ancs_attr_parser.h>
#include <bluetooth/services/ancs_app_attr_get.h>

#include "ancs_c_internal.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(ancs_c, CONFIG_BT_GATT_ANCS_C_LOG_LEVEL);

enum {
	ANCS_C_NS_NOTIF_ENABLED,
	ANCS_C_DS_NOTIF_ENABLED
};

/**< Index of the Event ID field when parsing notifications. */
#define BT_GATT_ANCS_NOTIF_EVT_ID_INDEX 0
/**< Index of the Flags field when parsing notifications. */
#define BT_GATT_ANCS_NOTIF_FLAGS_INDEX 1
/**< Index of the Category ID field when parsing notifications. */
#define BT_GATT_ANCS_NOTIF_CATEGORY_ID_INDEX 2
/**< Index of the Category Count field when parsing notifications. */
#define BT_GATT_ANCS_NOTIF_CATEGORY_CNT_INDEX 3
/**< Index of the Notification UID field when parsing notifications. */
#define BT_GATT_ANCS_NOTIF_NOTIF_UID 4

/**@brief Function for checking whether the data in an iOS notification is out of bounds.
 *
 * @param[in] notif  An iOS notification.
 *
 * @retval 0       If the notification is within bounds.
 * @retval -EINVAL If the notification is out of bounds.
 */
static int
bt_gatt_ancs_verify_notification_format(bt_gatt_ancs_c_evt_notif_t const *notif)
{
	if ((notif->evt_id >= BT_GATT_ANCS_NB_OF_EVT_ID) ||
	    (notif->category_id >= BT_GATT_ANCS_NB_OF_CATEGORY_ID)) {
		return -EINVAL;
	}
	return 0;
}

/**@brief Function for receiving and validating notifications received from the Notification Provider.
 *
 * @param[in] ancs_c     Pointer to an ANCS instance to which the event belongs.
 * @param[in] p_data_src Pointer to the data that was received from the Notification Provider.
 * @param[in] hvx_len    Length of the data that was received from the Notification Provider.
 */
static void parse_notif(struct bt_gatt_ancs_c *ancs_c,
			uint8_t const *p_data_src, uint16_t const hvx_data_len)
{
	bt_gatt_ancs_c_evt_t ancs_evt;
	int err;
	if (hvx_data_len != BT_GATT_ANCS_NOTIFICATION_DATA_LENGTH) {
		ancs_evt.evt_type = BT_GATT_ANCS_C_EVT_INVALID_NOTIF;
		ancs_c->evt_handler(&ancs_evt);
	}

	/*lint --e{415} --e{416} -save suppress Warning 415: possible access out of bound*/
	ancs_evt.notif.evt_id = (bt_gatt_ancs_c_evt_id_values_t)
		p_data_src[BT_GATT_ANCS_NOTIF_EVT_ID_INDEX];

	ancs_evt.notif.evt_flags.silent =
		(p_data_src[BT_GATT_ANCS_NOTIF_FLAGS_INDEX] >>
		 BT_GATT_ANCS_EVENT_FLAG_SILENT) &
		0x01;

	ancs_evt.notif.evt_flags.important =
		(p_data_src[BT_GATT_ANCS_NOTIF_FLAGS_INDEX] >>
		 BT_GATT_ANCS_EVENT_FLAG_IMPORTANT) &
		0x01;

	ancs_evt.notif.evt_flags.pre_existing =
		(p_data_src[BT_GATT_ANCS_NOTIF_FLAGS_INDEX] >>
		 BT_GATT_ANCS_EVENT_FLAG_PREEXISTING) &
		0x01;

	ancs_evt.notif.evt_flags.positive_action =
		(p_data_src[BT_GATT_ANCS_NOTIF_FLAGS_INDEX] >>
		 BT_GATT_ANCS_EVENT_FLAG_POSITIVE_ACTION) &
		0x01;

	ancs_evt.notif.evt_flags.negative_action =
		(p_data_src[BT_GATT_ANCS_NOTIF_FLAGS_INDEX] >>
		 BT_GATT_ANCS_EVENT_FLAG_NEGATIVE_ACTION) &
		0x01;

	ancs_evt.notif.category_id = (bt_gatt_ancs_c_category_id_val_t)
		p_data_src[BT_GATT_ANCS_NOTIF_CATEGORY_ID_INDEX];

	ancs_evt.notif.category_count =
		p_data_src[BT_GATT_ANCS_NOTIF_CATEGORY_CNT_INDEX];
	ancs_evt.notif.notif_uid =
		sys_get_le32(&p_data_src[BT_GATT_ANCS_NOTIF_NOTIF_UID]);
	/*lint -restore*/

	err = bt_gatt_ancs_verify_notification_format(&ancs_evt.notif);
	if (!err) {
		ancs_evt.evt_type = BT_GATT_ANCS_C_EVT_NOTIF;
	} else {
		ancs_evt.evt_type = BT_GATT_ANCS_C_EVT_INVALID_NOTIF;
	}

	ancs_c->evt_handler(&ancs_evt);
}

static uint8_t on_received_ns(struct bt_conn *conn,
			      struct bt_gatt_subscribe_params *params,
			      const void *data, uint16_t length)
{
	struct bt_gatt_ancs_c *ancs_c;

	/* Retrieve ANCS client module context. */
	ancs_c = CONTAINER_OF(params, struct bt_gatt_ancs_c, ns_notif_params);

	parse_notif(ancs_c, data, length);

	return BT_GATT_ITER_CONTINUE;
}

static uint8_t on_received_ds(struct bt_conn *conn,
			      struct bt_gatt_subscribe_params *params,
			      const void *data, uint16_t length)
{
	struct bt_gatt_ancs_c *ancs_c;

	/* Retrieve ANCS client module context. */
	ancs_c = CONTAINER_OF(params, struct bt_gatt_ancs_c, ds_notif_params);

	ancs_parse_get_attrs_response(ancs_c, data, length);

	return BT_GATT_ITER_CONTINUE;
}

int bt_gatt_ancs_c_init(struct bt_gatt_ancs_c *ancs_c,
			const struct bt_gatt_ancs_c_init_param *ancs_c_init)
{
	int err;

	if (ancs_c == NULL || ancs_c_init == NULL) {
		return -EINVAL;
	}

	memset(ancs_c, 0, sizeof(struct bt_gatt_ancs_c));

	ancs_c->evt_handler = ancs_c_init->evt_handler;

	err = k_sem_init(&ancs_c->cp_write_params_sem, 1, 1);

	return err;
}

void bt_gatt_ancs_c_on_disconnected(struct bt_gatt_ancs_c *ancs_c)
{
	atomic_clear_bit(&ancs_c->state, ANCS_C_NS_NOTIF_ENABLED);

	atomic_clear_bit(&ancs_c->state, ANCS_C_DS_NOTIF_ENABLED);

	k_sem_give(&ancs_c->cp_write_params_sem);
}

int bt_gatt_ancs_c_handles_assign(struct bt_gatt_dm *dm,
				  struct bt_gatt_ancs_c *ancs_c)
{
	const struct bt_gatt_dm_attr *gatt_service_attr =
		bt_gatt_dm_service_get(dm);
	const struct bt_gatt_service_val *gatt_service =
		bt_gatt_dm_attr_service_val(gatt_service_attr);
	const struct bt_gatt_dm_attr *gatt_chrc;
	const struct bt_gatt_dm_attr *gatt_desc;

	if (bt_uuid_cmp(gatt_service->uuid, BT_UUID_ANCS_SERVICE)) {
		return -ENOTSUP;
	}
	LOG_DBG("ANCS found");

	gatt_chrc = bt_gatt_dm_char_by_uuid(dm, BT_UUID_ANCS_CONTROL_POINT);
	if (gatt_chrc == NULL) {
		return -EINVAL;
	}
	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc,
					    BT_UUID_ANCS_CONTROL_POINT);
	if (gatt_desc == NULL) {
		return -EINVAL;
	}
	ancs_c->handle_cp = gatt_desc->handle;
	LOG_DBG("Control Point characteristic found.");

	gatt_chrc =
		bt_gatt_dm_char_by_uuid(dm, BT_UUID_ANCS_NOTIFICATION_SOURCE);
	if (gatt_chrc == NULL) {
		return -EINVAL;
	}
	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc,
					    BT_UUID_ANCS_NOTIFICATION_SOURCE);
	if (gatt_desc == NULL) {
		return -EINVAL;
	}
	ancs_c->handle_ns = gatt_desc->handle;
	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc, BT_UUID_GATT_CCC);
	if (gatt_desc == NULL) {
		return -EINVAL;
	}
	ancs_c->handle_ns_ccc = gatt_desc->handle;
	LOG_DBG("Notification Source characteristic found.");

	gatt_chrc = bt_gatt_dm_char_by_uuid(dm, BT_UUID_ANCS_DATA_SOURCE);
	if (gatt_chrc == NULL) {
		return -EINVAL;
	}
	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc,
					    BT_UUID_ANCS_DATA_SOURCE);
	if (gatt_desc == NULL) {
		return -EINVAL;
	}
	ancs_c->handle_ds = gatt_desc->handle;
	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc, BT_UUID_GATT_CCC);
	if (gatt_desc == NULL) {
		return -EINVAL;
	}
	ancs_c->handle_ds_ccc = gatt_desc->handle;
	LOG_DBG("Data Source characteristic found.");

	/* Finally - save connection object */
	ancs_c->conn = bt_gatt_dm_conn_get(dm);

	return 0;
}

int bt_gatt_ancs_c_notif_source_notif_enable(struct bt_gatt_ancs_c *ancs_c)
{
	int err;

	if (ancs_c == NULL) {
		return -EINVAL;
	}

	if (atomic_test_and_set_bit(&ancs_c->state, ANCS_C_NS_NOTIF_ENABLED)) {
		return -EALREADY;
	}

	ancs_c->ns_notif_params.notify = on_received_ns;
	ancs_c->ns_notif_params.value = BT_GATT_CCC_NOTIFY;
	ancs_c->ns_notif_params.value_handle = ancs_c->handle_ns;
	ancs_c->ns_notif_params.ccc_handle = ancs_c->handle_ns_ccc;
	atomic_set_bit(ancs_c->ns_notif_params.flags,
		       BT_GATT_SUBSCRIBE_FLAG_VOLATILE);

	err = bt_gatt_subscribe(ancs_c->conn, &ancs_c->ns_notif_params);
	if (err) {
		atomic_clear_bit(&ancs_c->state, ANCS_C_NS_NOTIF_ENABLED);
		LOG_ERR("Subscribe Notification Source failed (err %d)", err);
	} else {
		LOG_DBG("Notification Source subscribed");
	}

	return err;
}

int bt_gatt_ancs_c_data_source_notif_enable(struct bt_gatt_ancs_c *ancs_c)
{
	int err;

	if (ancs_c == NULL) {
		return -EINVAL;
	}

	if (atomic_test_and_set_bit(&ancs_c->state, ANCS_C_DS_NOTIF_ENABLED)) {
		return -EALREADY;
	}

	ancs_c->ds_notif_params.notify = on_received_ds;
	ancs_c->ds_notif_params.value = BT_GATT_CCC_NOTIFY;
	ancs_c->ds_notif_params.value_handle = ancs_c->handle_ds;
	ancs_c->ds_notif_params.ccc_handle = ancs_c->handle_ds_ccc;
	atomic_set_bit(ancs_c->ds_notif_params.flags,
		       BT_GATT_SUBSCRIBE_FLAG_VOLATILE);

	err = bt_gatt_subscribe(ancs_c->conn, &ancs_c->ds_notif_params);
	if (err) {
		atomic_clear_bit(&ancs_c->state, ANCS_C_DS_NOTIF_ENABLED);
		LOG_ERR("Subscribe Data Source failed (err %d)", err);
	} else {
		LOG_DBG("Data Source subscribed");
	}

	return err;
}

int bt_gatt_ancs_c_notif_source_notif_disable(struct bt_gatt_ancs_c *ancs_c)
{
	int err;

	if (ancs_c == NULL) {
		return -EINVAL;
	}

	if (!atomic_test_bit(&ancs_c->state, ANCS_C_NS_NOTIF_ENABLED)) {
		return -EFAULT;
	}

	err = bt_gatt_unsubscribe(ancs_c->conn, &ancs_c->ns_notif_params);
	if (err) {
		LOG_ERR("Unsubscribe Notification Source failed (err %d)", err);
	} else {
		atomic_clear_bit(&ancs_c->state, ANCS_C_NS_NOTIF_ENABLED);
		LOG_DBG("Notification Source unsubscribed");
	}

	return err;
}

int bt_gatt_ancs_c_data_source_notif_disable(struct bt_gatt_ancs_c *ancs_c)
{
	int err;

	if (ancs_c == NULL) {
		return -EINVAL;
	}

	if (!atomic_test_bit(&ancs_c->state, ANCS_C_DS_NOTIF_ENABLED)) {
		return -EFAULT;
	}

	err = bt_gatt_unsubscribe(ancs_c->conn, &ancs_c->ds_notif_params);
	if (err) {
		LOG_ERR("Unsubscribe Data Source failed (err %d)", err);
	} else {
		atomic_clear_bit(&ancs_c->state, ANCS_C_DS_NOTIF_ENABLED);
		LOG_DBG("Data Source unsubscribed");
	}

	return err;
}

static uint16_t encode_notif_action(uint8_t *p_encoded_data, uint32_t uid,
				    bt_gatt_ancs_c_action_id_values_t action_id)
{
	uint8_t index = 0;

	p_encoded_data[index++] =
		BT_GATT_ANCS_COMMAND_ID_GET_PERFORM_NOTIF_ACTION;
	sys_put_le32(uid, &p_encoded_data[index]);
	index += sizeof(uint32_t);
	p_encoded_data[index++] = (uint8_t)action_id;

	return index;
}

static void
bt_gatt_ancs_c_cp_write_callback(struct bt_conn *conn, uint8_t err,
				 struct bt_gatt_write_params *params)
{
	struct bt_gatt_ancs_c *ancs_c;

	/* Retrieve ANCS client module context. */
	ancs_c = CONTAINER_OF(params, struct bt_gatt_ancs_c, cp_write_params);

	k_sem_give(&ancs_c->cp_write_params_sem);

	bt_gatt_ancs_c_evt_t ancs_evt;

	ancs_evt.evt_type = BT_GATT_ANCS_C_EVT_NP_ERROR;
	ancs_evt.err_code_np = err;

	ancs_c->evt_handler(&ancs_evt);
}

int bt_gatt_ancs_c_cp_write(struct bt_gatt_ancs_c *ancs_c, uint16_t len)
{
	int err;
	struct bt_gatt_write_params *p_write_params = &ancs_c->cp_write_params;

	p_write_params->func = bt_gatt_ancs_c_cp_write_callback;
	p_write_params->handle = ancs_c->handle_cp;
	p_write_params->offset = 0;
	p_write_params->data = ancs_c->cp_data;
	p_write_params->length = len;

	err = bt_gatt_write(ancs_c->conn, p_write_params);
	if (err) {
		k_sem_give(&ancs_c->cp_write_params_sem);
	}

	return err;
}

int bt_gatt_ancs_perform_notif_action(
	struct bt_gatt_ancs_c *ancs_c, uint32_t uuid,
	bt_gatt_ancs_c_action_id_values_t action_id, k_timeout_t timeout)
{
	int err;

	err = k_sem_take(&ancs_c->cp_write_params_sem, timeout);
	if (err) {
		return err;
	}

	uint8_t *p_data = ancs_c->cp_data;
	uint16_t len = encode_notif_action(p_data, uuid, action_id);

	err = bt_gatt_ancs_c_cp_write(ancs_c, len);

	return err;
}

static int bt_gatt_ancs_get_notif_attrs(struct bt_gatt_ancs_c *ancs_c,
					uint32_t const p_uid,
					k_timeout_t timeout)
{
	int err;

	err = k_sem_take(&ancs_c->cp_write_params_sem, timeout);
	if (err) {
		return err;
	}

	uint32_t index = 0;
	uint8_t *p_data = ancs_c->cp_data;

	ancs_c->number_of_requested_attr = 0;

	/* Encode Command ID. */
	*(p_data + index++) = BT_GATT_ANCS_COMMAND_ID_GET_NOTIF_ATTRIBUTES;

	/* Encode Notification UID. */
	sys_put_le32(p_uid, p_data + index);
	index += sizeof(uint32_t);

	/* Encode Attribute ID. */
	for (uint32_t attr = 0; attr < BT_GATT_ANCS_NB_OF_NOTIF_ATTR; attr++) {
		if (ancs_c->ancs_notif_attr_list[attr].get == true) {
			*(p_data + index++) = (uint8_t)attr;

			if ((attr == BT_GATT_ANCS_NOTIF_ATTR_ID_TITLE) ||
			    (attr == BT_GATT_ANCS_NOTIF_ATTR_ID_SUBTITLE) ||
			    (attr == BT_GATT_ANCS_NOTIF_ATTR_ID_MESSAGE)) {
				/* Encode Length field. Only applicable for Title, Subtitle, and Message. */
				sys_put_le16(ancs_c->ancs_notif_attr_list[attr]
						     .attr_len,
					     p_data + index);
				index += sizeof(uint16_t);
			}

			ancs_c->number_of_requested_attr++;
		}
	}

	ancs_c->parse_info.expected_number_of_attrs =
		ancs_c->number_of_requested_attr;

	err = bt_gatt_ancs_c_cp_write(ancs_c, index);

	return err;
}

int bt_gatt_ancs_c_request_attrs(struct bt_gatt_ancs_c *ancs_c,
				 bt_gatt_ancs_c_evt_notif_t const *p_notif,
				 k_timeout_t timeout)
{
	int err;

	err = bt_gatt_ancs_verify_notification_format(p_notif);
	if (err) {
		return err;
	}

	err = bt_gatt_ancs_get_notif_attrs(ancs_c, p_notif->notif_uid, timeout);
	ancs_c->parse_info.parse_state = COMMAND_ID;
	return err;
}

int bt_gatt_ancs_c_attr_add(struct bt_gatt_ancs_c *ancs_c,
			    bt_gatt_ancs_c_notif_attr_id_val_t const id,
			    uint8_t *p_data, uint16_t const len)
{
	if (ancs_c == NULL || p_data == NULL) {
		return -EINVAL;
	}

	if ((len == 0) || (len > BT_GATT_ANCS_ATTR_DATA_MAX)) {
		return -EINVAL;
	}

	ancs_c->ancs_notif_attr_list[id].get = true;
	ancs_c->ancs_notif_attr_list[id].attr_len = len;
	ancs_c->ancs_notif_attr_list[id].p_attr_data = p_data;

	return 0;
}

int bt_gatt_ancs_c_app_attr_add(struct bt_gatt_ancs_c *ancs_c,
				bt_gatt_ancs_c_app_attr_id_val_t const id,
				uint8_t *p_data, uint16_t const len)
{
	if (ancs_c == NULL || p_data == NULL) {
		return -EINVAL;
	}

	if ((len == 0) || (len > BT_GATT_ANCS_ATTR_DATA_MAX)) {
		return -EINVAL;
	}

	ancs_c->ancs_app_attr_list[id].get = true;
	ancs_c->ancs_app_attr_list[id].attr_len = len;
	ancs_c->ancs_app_attr_list[id].p_attr_data = p_data;

	return 0;
}

int bt_gatt_ancs_c_app_attr_request(struct bt_gatt_ancs_c *ancs_c,
				    uint8_t const *p_app_id, uint32_t len,
				    k_timeout_t timeout)
{
	return ancs_c_app_attr_request(ancs_c, p_app_id, len, timeout);
}
