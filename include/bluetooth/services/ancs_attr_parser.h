/*
 * Copyright (c) 2012 - 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef ANCS_ATTR_PARSER_H__
#define ANCS_ATTR_PARSER_H__

#include "ancs_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @file
 *
 * @addtogroup bt_gatt_ancs_c
 * @{
 */

/**@brief Function for parsing notification or app attribute response data.
 *
 * @details The data that comes from the Notification Provider can be much longer than what
 *          would fit in a single GATTC notification. Therefore, this function relies on a
 *          state-oriented switch case.
 *          UID and command ID will be received only once at the beginning of the first
 *          GATTC notification of a new attribute request for a given iOS notification.
 *          After this, we can loop several ATTR_ID > LENGTH > DATA > ATTR_ID > LENGTH > DATA until
 *          we have received all attributes we wanted as a Notification Consumer.
 *          The Notification Provider can also simply stop sending attributes.
 *
 * 1 byte  |  4 bytes    |1 byte |2 bytes |... X bytes ... |1 bytes| 2 bytes| ... X bytes ...
 * --------|-------------|-------|--------|----------------|-------|--------|----------------
 * CMD_ID  |  NOTIF_UID  |ATTR_ID| LENGTH |    DATA        |ATTR_ID| LENGTH |    DATA
 *
 * @param[in] ancs_c       Pointer to an ANCS instance to which the event belongs.
 * @param[in] p_data_src   Pointer to data that was received from the Notification Provider.
 * @param[in] hvx_data_len Length of the data that was received from the Notification Provider.
 */
void ancs_parse_get_attrs_response(struct bt_gatt_ancs_c *ancs_c,
				   const uint8_t *p_data_src,
				   const uint16_t hvx_data_len);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* ANCS_ATTR_PARSER_H__ */
