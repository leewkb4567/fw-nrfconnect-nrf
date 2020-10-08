/*
 * Copyright (c) 2012 - 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <sys/printk.h>
#include <sys/byteorder.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/gatt_dm.h>
#include <bluetooth/services/ancs_c.h>

#include <settings/settings.h>

#include <dk_buttons_and_leds.h>

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define RUN_STATUS_LED DK_LED1
#define CON_STATUS_LED DK_LED2
#define RUN_LED_BLINK_INTERVAL 1000

#define KEY_REQ_NOTI_ATTR DK_BTN1_MSK
#define KEY_REQ_APP_ATTR DK_BTN2_MSK
#define KEY_POS_ACTION DK_BTN3_MSK
#define KEY_NEG_ACTION DK_BTN4_MSK

#define ATTR_DATA_SIZE                                                         \
	BT_GATT_ANCS_ATTR_DATA_MAX /**< Allocated size for attribute data. */

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_LIMITED | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_SOLICIT128, ANCS_UUID_SERVICE),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static struct bt_gatt_ancs_c ancs_c;

static bool has_ancs;
static bt_security_t security_level;

static bt_gatt_ancs_c_evt_notif_t
	m_notification_latest; /**< Local copy to keep track of the newest arriving notifications. */
static bt_gatt_ancs_c_attr_t
	m_notif_attr_latest; /**< Local copy of the newest notification attribute. */
static bt_gatt_ancs_c_attr_t
	m_notif_attr_app_id_latest; /**< Local copy of the newest app attribute. */

static uint8_t
	m_attr_appid[ATTR_DATA_SIZE]; /**< Buffer to store attribute data. */
static uint8_t
	m_attr_title[ATTR_DATA_SIZE]; /**< Buffer to store attribute data. */
static uint8_t
	m_attr_subtitle[ATTR_DATA_SIZE]; /**< Buffer to store attribute data. */
static uint8_t
	m_attr_message[ATTR_DATA_SIZE]; /**< Buffer to store attribute data. */
static uint8_t m_attr_message_size
	[ATTR_DATA_SIZE]; /**< Buffer to store attribute data. */
static uint8_t
	m_attr_date[ATTR_DATA_SIZE]; /**< Buffer to store attribute data. */
static uint8_t
	m_attr_posaction[ATTR_DATA_SIZE]; /**< Buffer to store attribute data. */
static uint8_t
	m_attr_negaction[ATTR_DATA_SIZE]; /**< Buffer to store attribute data. */
static uint8_t
	m_attr_disp_name[ATTR_DATA_SIZE]; /**< Buffer to store attribute data. */

/**@brief String literals for the iOS notification categories. used then printing to UART. */
static char const *lit_catid[BT_GATT_ANCS_NB_OF_CATEGORY_ID] = {
	"Other",
	"Incoming Call",
	"Missed Call",
	"Voice Mail",
	"Social",
	"Schedule",
	"Email",
	"News",
	"Health And Fitness",
	"Business And Finance",
	"Location",
	"Entertainment"
};

/**@brief String literals for the iOS notification event types. Used then printing to UART. */
static char const *lit_eventid[BT_GATT_ANCS_NB_OF_EVT_ID] = { "Added",
							      "Modified",
							      "Removed" };

/**@brief String literals for the iOS notification attribute types. Used when printing to UART. */
static char const *lit_attrid[BT_GATT_ANCS_NB_OF_NOTIF_ATTR] = {
	"App Identifier",
	"Title",
	"Subtitle",
	"Message",
	"Message Size",
	"Date",
	"Positive Action Label",
	"Negative Action Label"
};

/**@brief String literals for the iOS notification attribute types. Used When printing to UART. */
static char const *lit_appid[BT_GATT_ANCS_NB_OF_APP_ATTR] = { "Display Name" };

static void enable_notifications(void)
{
	int err;

	if (has_ancs && security_level >= BT_SECURITY_L2) {
		err = bt_gatt_ancs_c_notif_source_notif_enable(&ancs_c);
		if (err) {
			printk("Failed to enable Notification Source notification (err %d)\n",
			       err);
		}

		err = bt_gatt_ancs_c_data_source_notif_enable(&ancs_c);
		if (err) {
			printk("Failed to enable Data Source notification (err %d)\n",
			       err);
		}
	}
}

static void discover_completed_cb(struct bt_gatt_dm *dm, void *ctx)
{
	int err;

	printk("The discovery procedure succeeded\n");

	bt_gatt_dm_data_print(dm);

	err = bt_gatt_ancs_c_handles_assign(dm, &ancs_c);
	if (err) {
		printk("Could not init ANCS client object, error: %d\n", err);
	} else {
		has_ancs = true;

		if (security_level < BT_SECURITY_L2) {
			err = bt_conn_set_security(ancs_c.conn, BT_SECURITY_L2);
			if (err) {
				printk("Failed to set security (err %d)\n",
				       err);
			}
		} else {
			enable_notifications();
		}
	}

	err = bt_gatt_dm_data_release(dm);
	if (err) {
		printk("Could not release the discovery data, error "
		       "code: %d\n",
		       err);
	}
}

static void discover_service_not_found_cb(struct bt_conn *conn, void *ctx)
{
	printk("The service could not be found during the discovery\n");

	bt_conn_disconnect(conn, BT_HCI_ERR_LOCALHOST_TERM_CONN);
}

static void discover_error_found_cb(struct bt_conn *conn, int err, void *ctx)
{
	printk("The discovery procedure failed, err %d\n", err);

	bt_conn_disconnect(conn, BT_HCI_ERR_LOCALHOST_TERM_CONN);
}

static const struct bt_gatt_dm_cb discover_cb = {
	.completed = discover_completed_cb,
	.service_not_found = discover_service_not_found_cb,
	.error_found = discover_error_found_cb,
};

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (err) {
		printk("Connection failed (err 0x%02x)\n", err);
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Connected %s\n", addr);

	dk_set_led_on(CON_STATUS_LED);

	has_ancs = false;
	security_level = BT_SECURITY_L0;

	err = bt_gatt_dm_start(conn, BT_UUID_ANCS_SERVICE, &discover_cb, NULL);
	if (err) {
		printk("Failed to start discovery (err %d)\n", err);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Disconnected from %s (reason 0x%02x)\n", addr, reason);

	bt_gatt_ancs_c_on_disconnected(&ancs_c);

	dk_set_led_off(CON_STATUS_LED);
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		printk("Security changed: %s level %u\n", addr, level);

		security_level = level;

		enable_notifications();
	} else {
		printk("Security failed: %s level %u err %d\n", addr, level,
		       err);
	}
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed,
};

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing cancelled: %s\n", addr);
}

static void pairing_confirm(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	bt_conn_auth_pairing_confirm(conn);

	printk("Pairing confirmed: %s\n", addr);
}

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing completed: %s, bonded: %d\n", addr, bonded);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing failed conn: %s, reason %d\n", addr, reason);
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
	.cancel = auth_cancel,
	.pairing_confirm = pairing_confirm,
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed
};

/**@brief Function for printing an iOS notification.
 *
 * @param[in] p_notif  Pointer to the iOS notification.
 */
static void notif_print(bt_gatt_ancs_c_evt_notif_t *p_notif)
{
	printk("\nNotification\n");
	printk("Event:       %s\n", lit_eventid[p_notif->evt_id]);
	printk("Category ID: %s\n", lit_catid[p_notif->category_id]);
	printk("Category Cnt:%u\n", (unsigned int)p_notif->category_count);
	printk("UID:         %u\n", (unsigned int)p_notif->notif_uid);

	printk("Flags:\n");
	if (p_notif->evt_flags.silent == 1) {
		printk(" Silent\n");
	}
	if (p_notif->evt_flags.important == 1) {
		printk(" Important\n");
	}
	if (p_notif->evt_flags.pre_existing == 1) {
		printk(" Pre-existing\n");
	}
	if (p_notif->evt_flags.positive_action == 1) {
		printk(" Positive Action\n");
	}
	if (p_notif->evt_flags.negative_action == 1) {
		printk(" Negative Action\n");
	}
}

/**@brief Function for printing iOS notification attribute data.
 *
 * @param[in] p_attr Pointer to an iOS notification attribute.
 */
static void notif_attr_print(bt_gatt_ancs_c_attr_t *p_attr)
{
	if (p_attr->attr_len != 0) {
		printk("%s: %s\n", lit_attrid[p_attr->attr_id],
		       (char *)p_attr->p_attr_data);
	} else if (p_attr->attr_len == 0) {
		printk("%s: (N/A)\n", lit_attrid[p_attr->attr_id]);
	}
}

/**@brief Function for printing iOS notification attribute data.
 *
 * @param[in] p_attr Pointer to an iOS App attribute.
 */
static void app_attr_print(bt_gatt_ancs_c_attr_t *p_attr)
{
	if (p_attr->attr_len != 0) {
		printk("%s: %s\n", lit_appid[p_attr->attr_id],
		       (char *)p_attr->p_attr_data);
	} else if (p_attr->attr_len == 0) {
		printk("%s: (N/A)\n", lit_appid[p_attr->attr_id]);
	}
}

/**@brief Function for printing out errors that originated from the Notification Provider (iOS).
 *
 * @param[in] err_code_np Error code received from NP.
 */
static void err_code_print(uint8_t err_code_np)
{
	switch (err_code_np) {
	case BT_ATT_ERR_ANCS_NP_UNKNOWN_COMMAND:
		printk("Error: Command ID was not recognized by the Notification Provider.\n");
		break;

	case BT_ATT_ERR_ANCS_NP_INVALID_COMMAND:
		printk("Error: Command failed to be parsed on the Notification Provider.\n");
		break;

	case BT_ATT_ERR_ANCS_NP_INVALID_PARAMETER:
		printk("Error: Parameter does not refer to an existing object on the Notification Provider.\n");
		break;

	case BT_ATT_ERR_ANCS_NP_ACTION_FAILED:
		printk("Error: Perform Notification Action Failed on the Notification Provider.\n");
		break;

	default:
		break;
	}
}

static void bt_ancs_c_evt_handler(bt_gatt_ancs_c_evt_t *p_evt)
{
	switch (p_evt->evt_type) {
	case BT_GATT_ANCS_C_EVT_NOTIF:
		m_notification_latest = p_evt->notif;
		notif_print(&m_notification_latest);
		break;

	case BT_GATT_ANCS_C_EVT_NOTIF_ATTRIBUTE:
		m_notif_attr_latest = p_evt->attr;
		notif_attr_print(&m_notif_attr_latest);
		if (p_evt->attr.attr_id ==
		    BT_GATT_ANCS_NOTIF_ATTR_ID_APP_IDENTIFIER) {
			m_notif_attr_app_id_latest = p_evt->attr;
		}
		break;

	case BT_GATT_ANCS_C_EVT_APP_ATTRIBUTE:
		app_attr_print(&p_evt->attr);
		break;

	case BT_GATT_ANCS_C_EVT_NP_ERROR:
		err_code_print(p_evt->err_code_np);
		break;

	default:
		/* No implementation needed. */
		break;
	}
}

static int ancs_c_init(void)
{
	int err;
	struct bt_gatt_ancs_c_init_param ancs_c_init;

	ancs_c_init.evt_handler = bt_ancs_c_evt_handler;
	err = bt_gatt_ancs_c_init(&ancs_c, &ancs_c_init);

	if (!err) {
		err = bt_gatt_ancs_c_attr_add(
			&ancs_c, BT_GATT_ANCS_NOTIF_ATTR_ID_APP_IDENTIFIER,
			m_attr_appid, ATTR_DATA_SIZE);
	}

	if (!err) {
		err = bt_gatt_ancs_c_app_attr_add(
			&ancs_c, BT_GATT_ANCS_APP_ATTR_ID_DISPLAY_NAME,
			m_attr_disp_name, sizeof(m_attr_disp_name));
	}

	if (!err) {
		err = bt_gatt_ancs_c_attr_add(&ancs_c,
					      BT_GATT_ANCS_NOTIF_ATTR_ID_TITLE,
					      m_attr_title, ATTR_DATA_SIZE);
	}

	if (!err) {
		err = bt_gatt_ancs_c_attr_add(
			&ancs_c, BT_GATT_ANCS_NOTIF_ATTR_ID_MESSAGE,
			m_attr_message, ATTR_DATA_SIZE);
	}

	if (!err) {
		err = bt_gatt_ancs_c_attr_add(
			&ancs_c, BT_GATT_ANCS_NOTIF_ATTR_ID_SUBTITLE,
			m_attr_subtitle, ATTR_DATA_SIZE);
	}

	if (!err) {
		err = bt_gatt_ancs_c_attr_add(
			&ancs_c, BT_GATT_ANCS_NOTIF_ATTR_ID_MESSAGE_SIZE,
			m_attr_message_size, ATTR_DATA_SIZE);
	}

	if (!err) {
		err = bt_gatt_ancs_c_attr_add(&ancs_c,
					      BT_GATT_ANCS_NOTIF_ATTR_ID_DATE,
					      m_attr_date, ATTR_DATA_SIZE);
	}

	if (!err) {
		err = bt_gatt_ancs_c_attr_add(
			&ancs_c,
			BT_GATT_ANCS_NOTIF_ATTR_ID_POSITIVE_ACTION_LABEL,
			m_attr_posaction, ATTR_DATA_SIZE);
	}

	if (!err) {
		err = bt_gatt_ancs_c_attr_add(
			&ancs_c,
			BT_GATT_ANCS_NOTIF_ATTR_ID_NEGATIVE_ACTION_LABEL,
			m_attr_negaction, ATTR_DATA_SIZE);
	}

	return err;
}

static void button_changed(uint32_t button_state, uint32_t has_changed)
{
	uint32_t buttons = button_state & has_changed;
	int err;

	if (buttons & KEY_REQ_NOTI_ATTR) {
		err = bt_gatt_ancs_c_request_attrs(
			&ancs_c, &m_notification_latest, K_NO_WAIT);
		if (err) {
			printk("Failed requesting attributes for a notification (err: %d)\n",
			       err);
		}
	}

	if (buttons & KEY_REQ_APP_ATTR) {
		if (m_notif_attr_app_id_latest.attr_id ==
			    BT_GATT_ANCS_NOTIF_ATTR_ID_APP_IDENTIFIER &&
		    m_notif_attr_app_id_latest.attr_len != 0) {
			printk("Request for %s: \n",
			       m_notif_attr_app_id_latest.p_attr_data);
			err = bt_gatt_ancs_c_app_attr_request(
				&ancs_c, m_notif_attr_app_id_latest.p_attr_data,
				m_notif_attr_app_id_latest.attr_len, K_NO_WAIT);
			if (err) {
				printk("Failed requesting attributes for a given app (err: %d)\n",
				       err);
			}
		}
	}

	if (buttons & KEY_POS_ACTION) {
		if (m_notification_latest.evt_flags.positive_action == true) {
			printk("Performing Positive Action.\n");
			err = bt_gatt_ancs_perform_notif_action(
				&ancs_c, m_notification_latest.notif_uid,
				ACTION_ID_POSITIVE, K_NO_WAIT);
			if (err) {
				printk("Failed performing action (err: %d)\n",
				       err);
			}
		}
	}

	if (buttons & KEY_NEG_ACTION) {
		if (m_notification_latest.evt_flags.negative_action == true) {
			printk("Performing Negative Action.\n");
			err = bt_gatt_ancs_perform_notif_action(
				&ancs_c, m_notification_latest.notif_uid,
				ACTION_ID_NEGATIVE, K_NO_WAIT);
			if (err) {
				printk("Failed performing action (err: %d)\n",
				       err);
			}
		}
	}
}

static int init_button(void)
{
	int err;

	err = dk_buttons_init(button_changed);
	if (err) {
		printk("Cannot init buttons (err: %d)\n", err);
	}

	return err;
}

void main(void)
{
	int blink_status = 0;
	int err;

	printk("Starting Apple Notification Center Service client example\n");

	err = ancs_c_init();
	if (err) {
		printk("ANCS client init failed (err %d)\n", err);
		return;
	}

	err = dk_leds_init();
	if (err) {
		printk("LEDs init failed (err %d)\n", err);
		return;
	}

	err = init_button();
	if (err) {
		printk("Button init failed (err %d)\n", err);
		return;
	}

	err = bt_enable(NULL);
	if (err) {
		printk("BLE init failed (err %d)\n", err);
		return;
	}

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	bt_conn_cb_register(&conn_callbacks);
	if (err) {
		printk("Failed to register connection callbacks.\n");
		return;
	}

	err = bt_conn_auth_cb_register(&conn_auth_callbacks);
	if (err) {
		printk("Failed to register authorization callbacks.\n");
		return;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");

	for (;;) {
		dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
		k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));
	}
}
