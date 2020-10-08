/*
 * Copyright (c) 2016 - 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/* Disclaimer: This client implementation of the Apple Notification Center Service can and will be changed at any time by Nordic Semiconductor ASA.
 * Server implementations such as the ones found in iOS can be changed at any time by Apple and may cause this client implementation to stop working.
 */

#include <bluetooth/services/ancs_c.h>
#include <bluetooth/services/ancs_app_attr_get.h>
#include <logging/log.h>
#include "ancs_c_internal.h"

LOG_MODULE_DECLARE(ancs_c, CONFIG_BT_GATT_ANCS_C_LOG_LEVEL);

/** Maximum length of the data that can be sent in a write. */
#define ANCS_GATTC_WRITE_PAYLOAD_LEN_MAX CONFIG_BT_GATT_ANCS_C_CP_BUFF_SIZE

/**@brief Enumeration for keeping track of the state-based encoding while requesting app attributes. */
typedef enum {
	/** Currently encoding the command ID. */
	APP_ATTR_COMMAND_ID,
	/** Currently encoding the app ID. */
	APP_ATTR_APP_ID,
	/** Currently encoding the attribute ID. */
	APP_ATTR_ATTR_ID,
	/** Encoding done. */
	APP_ATTR_DONE,
	/** Encoding aborted. */
	APP_ATTR_ABORT
} encode_app_attr_t;

/**@brief Function for determining whether an attribute is requested.
 *
 * @param[in] ancs_c iOS notification structure. This structure must be supplied by
 *                   the application. It identifies the particular client instance to use.
 *
 * @return True  If it is requested
 * @return False If it is not requested.
*/
static bool app_attr_is_requested(struct bt_gatt_ancs_c *ancs_c,
				  uint32_t attr_id)
{
	if (ancs_c->ancs_app_attr_list[attr_id].get == true) {
		return true;
	}
	return false;
}

/**@brief Function for counting the number of attributes that will be requested upon a "get app attributes" command.
 *
 * @param[in] ancs_c iOS notification structure. This structure must be supplied by
 *                   the application. It identifies the particular client instance to use.
 *
 * @return           Number of attributes that will be requested upon a "get app attributes" command.
*/
static uint32_t app_attr_nb_to_get(struct bt_gatt_ancs_c *ancs_c)
{
	uint32_t attr_nb_to_get = 0;
	for (uint32_t i = 0; i < (sizeof(ancs_c->ancs_app_attr_list) /
				  sizeof(bt_gatt_ancs_c_attr_list_t));
	     i++) {
		if (app_attr_is_requested(ancs_c, i)) {
			attr_nb_to_get++;
		}
	}
	return attr_nb_to_get;
}

/**@brief Function for encoding the command ID as part of assembling a "get app attributes" command.
 *
 * @param[in]     p_index    Pointer to the length encoded so far for the current write.
 * @param[in,out] p_data     Pointer to the BLE GATT write data buffer.
 */
static encode_app_attr_t app_attr_encode_cmd_id(uint32_t *index,
						uint8_t *p_data)
{
	uint8_t *p_value = (uint8_t *)p_data;
	LOG_DBG("Encoding command ID.");

	/* Encode Command ID. */
	p_value[(*index)++] = BT_GATT_ANCS_COMMAND_ID_GET_APP_ATTRIBUTES;
	return APP_ATTR_APP_ID;
}

/**@brief Function for encoding the app ID as part of assembling a "get app attributes" command.
 *
 * @param[in] ancs_c                       iOS notification structure. This structure must be supplied by
 *                                         the application. It identifies the particular client instance to use.
 * @param[in] p_app_id                     The app ID of the app for which to request app attributes.
 * @param[in] app_id_len                   Length of the app ID.
 * @param[in] p_index                      Pointer to the length encoded so far for the current write.
 * @param[in] p_offset                     Pointer to the accumulated offset for the next write.
 * @param[in] p_data                       Pointer to the BLE GATT write data buffer.
 * @param[in] p_app_id_bytes_encoded_count Variable to keep count of the encoded app ID bytes.
 *                                         As long as it is lower than the length of the app ID,
 *                                         parsing continues.
 */
static encode_app_attr_t
app_attr_encode_app_id(struct bt_gatt_ancs_c *ancs_c, uint32_t *p_index,
		       uint16_t *p_offset, uint8_t *p_data,
		       const uint8_t *p_app_id, const uint32_t app_id_len,
		       uint32_t *p_app_id_bytes_encoded_count)
{
	uint8_t *p_value = (uint8_t *)p_data;

	LOG_DBG("Encoding app ID.");

	/* Encode app identifier. */
	if (*p_app_id_bytes_encoded_count == app_id_len) {
		if (*p_index >= ANCS_GATTC_WRITE_PAYLOAD_LEN_MAX) {
			return APP_ATTR_ABORT;
		}
		p_value[(*p_index)++] = '\0';
		(*p_app_id_bytes_encoded_count)++;
	}
	LOG_DBG("%c", p_app_id[(*p_app_id_bytes_encoded_count)]);
	if (*p_app_id_bytes_encoded_count < app_id_len) {
		if (*p_index >= ANCS_GATTC_WRITE_PAYLOAD_LEN_MAX) {
			return APP_ATTR_ABORT;
		}
		p_value[(*p_index)++] =
			p_app_id[(*p_app_id_bytes_encoded_count)++];
	}
	if (*p_app_id_bytes_encoded_count > app_id_len) {
		return APP_ATTR_ATTR_ID;
	}
	return APP_ATTR_APP_ID;
}

/**@brief Function for encoding the attribute ID as part of assembling a "get app attributes" command.
 *
 * @param[in]     ancs_c       iOS notification structure. This structure must be supplied by
 *                             the application. It identifies the particular client instance to use.
 * @param[in]     p_index      Pointer to the length encoded so far for the current write.
 * @param[in]     p_offset     Pointer to the accumulated offset for the next write.
 * @param[in,out] p_data       Pointer to the BLE GATT write data buffer.
 * @param[in]     p_attr_count Pointer to a variable that iterates the possible app attributes.
 */
static encode_app_attr_t
app_attr_encode_attr_id(struct bt_gatt_ancs_c *ancs_c, uint32_t *p_index,
			uint16_t *p_offset, uint8_t *p_data,
			uint32_t *p_attr_count, uint32_t *attr_get_total_nb)
{
	uint8_t *p_value = (uint8_t *)p_data;

	LOG_DBG("Encoding attribute ID.");

	/* Encode Attribute ID. */
	if (*p_attr_count < BT_GATT_ANCS_NB_OF_APP_ATTR) {
		if (app_attr_is_requested(ancs_c, *p_attr_count)) {
			if ((*p_index) >= ANCS_GATTC_WRITE_PAYLOAD_LEN_MAX) {
				return APP_ATTR_ABORT;
			}
			p_value[(*p_index)] = *p_attr_count;
			ancs_c->number_of_requested_attr++;
			(*p_index)++;
			LOG_DBG("offset %i", *p_offset);
		}
		(*p_attr_count)++;
	}
	if (*p_attr_count == BT_GATT_ANCS_NB_OF_APP_ATTR) {
		return APP_ATTR_DONE;
	}
	return APP_ATTR_APP_ID;
}

/**@brief Function for sending a "get app attributes" request.
 *
 * @details Since the app ID may not fit in a single write, long write
 *          with a state machine is used to encode the "get app attributes" request.
 *
 * @param[in] ancs_c     iOS notification structure. This structure must be supplied by
 *                       the application. It identifies the particular client instance to use.
 * @param[in] p_app_id   The app ID of the app for which to request app attributes.
 * @param[in] app_id_len Length of the app ID.
 * @param[in] timeout    Time-out for taking Control Point semaphore.
 */
static int app_attr_get(struct bt_gatt_ancs_c *ancs_c, const uint8_t *p_app_id,
			uint32_t app_id_len, k_timeout_t timeout)
{
	uint32_t index = 0;
	uint32_t attr_bytes_encoded_count = 0;
	uint16_t offset = 0;
	uint32_t app_id_bytes_encoded_count = 0;
	encode_app_attr_t state = APP_ATTR_COMMAND_ID;
	int err;

	err = k_sem_take(&ancs_c->cp_write_params_sem, timeout);
	if (err) {
		return err;
	}

	ancs_c->number_of_requested_attr = 0;

	uint32_t attr_get_total_nb = app_attr_nb_to_get(ancs_c);
	uint8_t *p_data = ancs_c->cp_data;

	while (state != APP_ATTR_DONE && state != APP_ATTR_ABORT) {
		switch (state) {
		case APP_ATTR_COMMAND_ID:
			state = app_attr_encode_cmd_id(&index, p_data);
			break;
		case APP_ATTR_APP_ID:
			state = app_attr_encode_app_id(
				ancs_c, &index, &offset, p_data, p_app_id,
				app_id_len, &app_id_bytes_encoded_count);
			break;
		case APP_ATTR_ATTR_ID:
			state = app_attr_encode_attr_id(
				ancs_c, &index, &offset, p_data,
				&attr_bytes_encoded_count, &attr_get_total_nb);
			break;
		case APP_ATTR_DONE:
			break;
		case APP_ATTR_ABORT:
			break;
		default:
			break;
		}
	}

	if (state == APP_ATTR_DONE) {
		err = bt_gatt_ancs_c_cp_write(ancs_c, index);

		ancs_c->parse_info.expected_number_of_attrs =
			ancs_c->number_of_requested_attr;
	} else {
		err = -ENOMEM;
	}

	return err;
}

int ancs_c_app_attr_request(struct bt_gatt_ancs_c *ancs_c,
			    const uint8_t *p_app_id, uint32_t len,
			    k_timeout_t timeout)
{
	int err;

	/* App ID to be requested must be null-terminated. */
	if (len == 0) {
		return -EINVAL;
	}
	if (p_app_id[len] !=
	    '\0') {
		return -EINVAL;
	}

	ancs_c->parse_info.parse_state = COMMAND_ID;
	err = app_attr_get(ancs_c, p_app_id, len, timeout);

	return err;
}
