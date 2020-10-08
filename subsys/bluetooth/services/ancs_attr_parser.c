/*
 * Copyright (c) 2016 - 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/* Disclaimer: This client implementation of the Apple Notification Center Service can and will be changed at any time by Nordic Semiconductor ASA.
 * Server implementations such as the ones found in iOS can be changed at any time by Apple and may cause this client implementation to stop working.
 */

#include <sys/byteorder.h>
#include <bluetooth/services/ancs_c.h>
#include <bluetooth/services/ancs_attr_parser.h>
#include <logging/log.h>

LOG_MODULE_DECLARE(ancs_c, CONFIG_BT_GATT_ANCS_C_LOG_LEVEL);

static bool all_req_attrs_parsed(struct bt_gatt_ancs_c *ancs_c)
{
	if (ancs_c->parse_info.expected_number_of_attrs == 0) {
		return true;
	}
	return false;
}

static bool attr_is_requested(struct bt_gatt_ancs_c *ancs_c,
			      bt_gatt_ancs_c_attr_t attr)
{
	if (ancs_c->parse_info.p_attr_list[attr.attr_id].get == true) {
		return true;
	}
	return false;
}

/**@brief Function for parsing command id and notification id.
 *        Used in the @ref parse_get_notif_attrs_response state machine.
 *
 * @details UID and command ID will be received only once at the beginning of the first
 *          GATTC notification of a new attribute request for a given iOS notification.
 *
 * @param[in] ancs_c     Pointer to an ANCS instance to which the event belongs.
 * @param[in] p_data_src Pointer to data that was received from the Notification Provider.
 * @param[in] index      Pointer to an index that helps us keep track of the current data to be parsed.
 *
 * @return The next parse state.
 */
static bt_gatt_ancs_c_parse_state_t
command_id_parse(struct bt_gatt_ancs_c *ancs_c, const uint8_t *p_data_src,
		 uint32_t *index)
{
	bt_gatt_ancs_c_parse_state_t parse_state;

	ancs_c->parse_info.command_id =
		(bt_gatt_ancs_c_cmd_id_val_t)p_data_src[(*index)++];

	switch (ancs_c->parse_info.command_id) {
	case BT_GATT_ANCS_COMMAND_ID_GET_NOTIF_ATTRIBUTES:
		ancs_c->evt.evt_type = BT_GATT_ANCS_C_EVT_NOTIF_ATTRIBUTE;
		ancs_c->parse_info.p_attr_list = ancs_c->ancs_notif_attr_list;
		ancs_c->parse_info.nb_of_attr = BT_GATT_ANCS_NB_OF_NOTIF_ATTR;
		parse_state = NOTIF_UID;
		break;

	case BT_GATT_ANCS_COMMAND_ID_GET_APP_ATTRIBUTES:
		ancs_c->evt.evt_type = BT_GATT_ANCS_C_EVT_APP_ATTRIBUTE;
		ancs_c->parse_info.p_attr_list = ancs_c->ancs_app_attr_list;
		ancs_c->parse_info.nb_of_attr = BT_GATT_ANCS_NB_OF_APP_ATTR;
		parse_state = APP_ID;
		break;

	default:
		/* no valid command_id, abort the rest of the parsing procedure. */
		LOG_DBG("Invalid Command ID");
		parse_state = DONE;
		break;
	}
	return parse_state;
}

static bt_gatt_ancs_c_parse_state_t
notif_uid_parse(struct bt_gatt_ancs_c *ancs_c, const uint8_t *p_data_src,
		uint32_t *index)
{
	ancs_c->evt.notif_uid = sys_get_le32(&p_data_src[*index]);
	*index += sizeof(uint32_t);
	return ATTR_ID;
}

static bt_gatt_ancs_c_parse_state_t app_id_parse(struct bt_gatt_ancs_c *ancs_c,
						 const uint8_t *p_data_src,
						 uint32_t *index)
{
	ancs_c->evt.app_id[ancs_c->parse_info.current_app_id_index] =
		p_data_src[(*index)++];

	if (ancs_c->evt.app_id[ancs_c->parse_info.current_app_id_index] !=
	    '\0') {
		ancs_c->parse_info.current_app_id_index++;
		return APP_ID;
	} else {
		return ATTR_ID;
	}
}

/**@brief Function for parsing the id of an iOS attribute.
 *        Used in the @ref parse_get_notif_attrs_response state machine.
 *
 * @details We only request attributes that are registered with @ref bt_gatt_ancs_c_attr_add
 *          once they have been reveiced we stop parsing.
 *
 * @param[in] ancs_c     Pointer to an ANCS instance to which the event belongs.
 * @param[in] p_data_src Pointer to data that was received from the Notification Provider.
 * @param[in] index      Pointer to an index that helps us keep track of the current data to be parsed.
 *
 * @return The next parse state.
 */
static bt_gatt_ancs_c_parse_state_t attr_id_parse(struct bt_gatt_ancs_c *ancs_c,
						  const uint8_t *p_data_src,
						  uint32_t *index)
{
	ancs_c->evt.attr.attr_id = p_data_src[(*index)++];

	if (ancs_c->evt.attr.attr_id >= ancs_c->parse_info.nb_of_attr) {
		LOG_DBG("Attribute ID Invalid.");
		return DONE;
	}
	ancs_c->evt.attr.p_attr_data =
		ancs_c->parse_info.p_attr_list[ancs_c->evt.attr.attr_id]
			.p_attr_data;

	if (all_req_attrs_parsed(ancs_c)) {
		LOG_DBG("All requested attributes received. ");
		return DONE;
	} else {
		if (attr_is_requested(ancs_c, ancs_c->evt.attr)) {
			ancs_c->parse_info.expected_number_of_attrs--;
		}
		LOG_DBG("Attribute ID %i ", ancs_c->evt.attr.attr_id);
		return ATTR_LEN1;
	}
}

/**@brief Function for parsing the length of an iOS attribute.
 *        Used in the @ref parse_get_notif_attrs_response state machine.
 *
 * @details The Length is 2 bytes. Since there is a chance we reveice the bytes in two different
 *          GATTC notifications, we parse only the first byte here and then set the state machine
 *          ready to parse the next byte.
 *
 * @param[in] ancs_c     Pointer to an ANCS instance to which the event belongs.
 * @param[in] p_data_src Pointer to data that was received from the Notification Provider.
 * @param[in] index      Pointer to an index that helps us keep track of the current data to be parsed.
 *
 * @return The next parse state.
 */
static bt_gatt_ancs_c_parse_state_t
attr_len1_parse(struct bt_gatt_ancs_c *ancs_c, const uint8_t *p_data_src,
		uint32_t *index)
{
	ancs_c->evt.attr.attr_len = p_data_src[(*index)++];
	return ATTR_LEN2;
}

/**@brief Function for parsing the length of an iOS attribute.
 *        Used in the @ref parse_get_notif_attrs_response state machine.
 *
 * @details Second byte of the length field. If the length is zero, it means that the attribute is not
 *          present and the state machine is set to parse the next attribute.
 *
 * @param[in] ancs_c     Pointer to an ANCS instance to which the event belongs.
 * @param[in] p_data_src Pointer to data that was received from the Notification Provider.
 * @param[in] index      Pointer to an index that helps us keep track of the current data to be parsed.
 *
 * @return The next parse state.
 */
static bt_gatt_ancs_c_parse_state_t
attr_len2_parse(struct bt_gatt_ancs_c *ancs_c, const uint8_t *p_data_src,
		uint32_t *index)
{
	ancs_c->evt.attr.attr_len |= (p_data_src[(*index)++] << 8);
	ancs_c->parse_info.current_attr_index = 0;

	if (ancs_c->evt.attr.attr_len != 0) {
		/* If the attribute has a length but there is no allocated space for this attribute */
		if ((ancs_c->parse_info.p_attr_list[ancs_c->evt.attr.attr_id]
			     .attr_len == 0) ||
		    (ancs_c->parse_info.p_attr_list[ancs_c->evt.attr.attr_id]
			     .p_attr_data == NULL)) {
			return ATTR_SKIP;
		} else {
			return ATTR_DATA;
		}
	} else {
		LOG_DBG("Attribute LEN %i ", ancs_c->evt.attr.attr_len);
		if (attr_is_requested(ancs_c, ancs_c->evt.attr)) {
			ancs_c->evt_handler(&ancs_c->evt);
		}
		if (all_req_attrs_parsed(ancs_c)) {
			return DONE;
		} else {
			return ATTR_ID;
		}
	}
}

/**@brief Function for parsing the data of an iOS attribute.
 *        Used in the @ref parse_get_notif_attrs_response state machine.
 *
 * @details Read the data of the attribute into our local buffer.
 *
 * @param[in] ancs_c     Pointer to an ANCS instance to which the event belongs.
 * @param[in] p_data_src Pointer to data that was received from the Notification Provider.
 * @param[in] index      Pointer to an index that helps us keep track of the current data to be parsed.
 *
 * @return The next parse state.
 */
static bt_gatt_ancs_c_parse_state_t
attr_data_parse(struct bt_gatt_ancs_c *ancs_c, const uint8_t *p_data_src,
		uint32_t *index)
{
	/* We have not reached the end of the attribute, nor our max allocated internal size.
	Proceed with copying data over to our buffer. */
	if ((ancs_c->parse_info.current_attr_index <
	     ancs_c->parse_info.p_attr_list[ancs_c->evt.attr.attr_id].attr_len) &&
	    (ancs_c->parse_info.current_attr_index <
	     ancs_c->evt.attr.attr_len)) {
		/* LOG_DBG("Byte copied to buffer: %c", p_data_src[(*index)]); */ /* Un-comment this line to see every byte of an attribute as it is parsed. Commented out by default since it can overflow the uart buffer. */
		ancs_c->evt.attr
			.p_attr_data[ancs_c->parse_info.current_attr_index++] =
			p_data_src[(*index)++];
	}

	/* We have reached the end of the attribute, or our max allocated internal size.
	Stop copying data over to our buffer. NUL-terminate at the current index. */
	if ((ancs_c->parse_info.current_attr_index ==
	     ancs_c->evt.attr.attr_len) ||
	    (ancs_c->parse_info.current_attr_index ==
	     ancs_c->parse_info.p_attr_list[ancs_c->evt.attr.attr_id].attr_len -
		     1)) {
		if (attr_is_requested(ancs_c, ancs_c->evt.attr)) {
			ancs_c->evt.attr.p_attr_data
				[ancs_c->parse_info.current_attr_index] = '\0';
		}

		/* If our max buffer size is smaller than the remaining attribute data, we must
		increase index to skip the data until the start of the next attribute. */
		if (ancs_c->parse_info.current_attr_index <
		    ancs_c->evt.attr.attr_len) {
			return ATTR_SKIP;
		}
		LOG_DBG("Attribute finished!");
		if (attr_is_requested(ancs_c, ancs_c->evt.attr)) {
			ancs_c->evt_handler(&ancs_c->evt);
		}
		if (all_req_attrs_parsed(ancs_c)) {
			return DONE;
		} else {
			return ATTR_ID;
		}
	}
	return ATTR_DATA;
}

static bt_gatt_ancs_c_parse_state_t attr_skip(struct bt_gatt_ancs_c *ancs_c,
					      const uint8_t *p_data_src,
					      uint32_t *index)
{
	/* We have not reached the end of the attribute, nor our max allocated internal size.
	Proceed with copying data over to our buffer. */
	if (ancs_c->parse_info.current_attr_index < ancs_c->evt.attr.attr_len) {
		ancs_c->parse_info.current_attr_index++;
		(*index)++;
	}
	/* At the end of the attribute, determine if it should be passed to event handler and
	continue parsing the next attribute ID if we are not done with all the attributes. */
	if (ancs_c->parse_info.current_attr_index ==
	    ancs_c->evt.attr.attr_len) {
		if (attr_is_requested(ancs_c, ancs_c->evt.attr)) {
			ancs_c->evt_handler(&ancs_c->evt);
		}
		if (all_req_attrs_parsed(ancs_c)) {
			return DONE;
		} else {
			return ATTR_ID;
		}
	}
	return ATTR_SKIP;
}

void ancs_parse_get_attrs_response(struct bt_gatt_ancs_c *ancs_c,
				   const uint8_t *p_data_src,
				   const uint16_t hvx_data_len)
{
	uint32_t index;

	for (index = 0; index < hvx_data_len;) {
		switch (ancs_c->parse_info.parse_state) {
		case COMMAND_ID:
			ancs_c->parse_info.parse_state =
				command_id_parse(ancs_c, p_data_src, &index);
			break;

		case NOTIF_UID:
			ancs_c->parse_info.parse_state =
				notif_uid_parse(ancs_c, p_data_src, &index);
			break;

		case APP_ID:
			ancs_c->parse_info.parse_state =
				app_id_parse(ancs_c, p_data_src, &index);
			break;

		case ATTR_ID:
			ancs_c->parse_info.parse_state =
				attr_id_parse(ancs_c, p_data_src, &index);
			break;

		case ATTR_LEN1:
			ancs_c->parse_info.parse_state =
				attr_len1_parse(ancs_c, p_data_src, &index);
			break;

		case ATTR_LEN2:
			ancs_c->parse_info.parse_state =
				attr_len2_parse(ancs_c, p_data_src, &index);
			break;

		case ATTR_DATA:
			ancs_c->parse_info.parse_state =
				attr_data_parse(ancs_c, p_data_src, &index);
			break;

		case ATTR_SKIP:
			ancs_c->parse_info.parse_state =
				attr_skip(ancs_c, p_data_src, &index);
			break;

		case DONE:
			LOG_DBG("Parse state: Done ");
			index = hvx_data_len;
			break;

		default:
			/* Default case will never trigger intentionally. Go to the DONE state to minimize the consequences. */
			ancs_c->parse_info.parse_state = DONE;
			break;
		}
	}
}
