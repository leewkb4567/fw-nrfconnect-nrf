/*
 * Copyright (c) 2012 - 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef ANCS_APP_ATTR_GET_H__
#define ANCS_APP_ATTR_GET_H__

#include "ancs_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @file
 *
 * @addtogroup bt_gatt_ancs_c
 * @{
 */

/**@brief Function for requesting attributes for an app.
 *
 * @param[in] ancs_c  iOS notification structure. This structure must be supplied by
 *                    the application. It identifies the particular client instance to use.
 * @param[in] app_id  App identifier of the app for which to request app attributes.
 * @param[in] len     Length of the app identifier.
 * @param[in] timeout Time-out for taking Control Point semaphore.
 *
 * @retval 0 If all operations were successful.
 *           Otherwise, a (negative) error code is returned.
 */
int ancs_c_app_attr_request(struct bt_gatt_ancs_c *ancs_c,
			    const uint8_t *app_id, uint32_t len,
			    k_timeout_t timeout);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* ANCS_APP_ATTR_GET_H__ */
