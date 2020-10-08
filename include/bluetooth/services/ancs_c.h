/*
 * Copyright (c) 2012 - 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef BT_GATT_ANCS_C_H_
#define BT_GATT_ANCS_C_H_

/** @file
 *
 * @defgroup bt_gatt_ancs_c Apple Notification Service Client
 * @{
 *
 * @brief Apple Notification Center Service Client module.
 *
 * @details Disclaimer: This client implementation of the Apple Notification Center Service can
 *          be changed at any time by Nordic Semiconductor ASA. Server implementations such as the
 *          ones found in iOS can be changed at any time by Apple and may cause this client
 *          implementation to stop working.
 *
 * This module implements the Apple Notification Center Service (ANCS) client.
 * This client can be used as a Notification Consumer (NC) that receives data
 * notifications from a Notification Provider (NP). The NP is typically an iOS
 * device that is acting as a server. For terminology and up-to-date specs, see
 * http://developer.apple.com.
 *
 * The term "notification" is used in two different meanings:
 * - An <i>iOS notification</i> is the data received from the Notification Provider.
 * - A <i>GATTC notification</i> is a way to transfer data with <i>Bluetooth</i> Smart.
 * In this module, iOS notifications are received through the GATTC notifications.
 * The full term (iOS notification or GATTC notification) is used where required to avoid confusion.
 *
 * Upon initializing the module, you must add the different iOS notification attributes you
 * would like to receive for iOS notifications (see @ref bt_gatt_ancs_c_attr_add).
 *
 * Once a connection is established with a central device, the module needs a service discovery to
 * discover the ANCS server handles. If this succeeds, the handles for the ANCS server
 * must be assigned to an ANCS_C instance using the @ref bt_gatt_ancs_c_handles_assign function.
 *
 * The application can now subscribe to iOS notifications with
 * @ref bt_gatt_ancs_c_notif_source_notif_enable. The notifications arrive in the @ref BT_GATT_ANCS_C_EVT_NOTIF event.
 * @ref bt_gatt_ancs_c_request_attrs can be used to request attributes for the notifications. The attributes
 * arrive in the @ref BT_GATT_ANCS_C_EVT_NOTIF_ATTRIBUTE event.
 * Use @ref bt_gatt_ancs_c_app_attr_request to request attributes of the app that issued
 * the notifications. The app attributes arrive in the @ref BT_GATT_ANCS_C_EVT_APP_ATTRIBUTE event.
 * Use @ref bt_gatt_ancs_perform_notif_action to make the Notification Provider perform an
 * action based on the provided notification.
 *
 * @msc
 * hscale = "1.5";
 * Application, ANCS_C;
 * |||;
 * Application=>ANCS_C   [label = "bt_gatt_ancs_c_attr_add(attribute)"];
 * Application=>ANCS_C   [label = "bt_gatt_ancs_c_init(ancs_instance, event_handler)"];
 * ...;
 * Application=>ANCS_C   [label = "bt_gatt_ancs_c_handles_assign(ancs_instance, conn_handle, service_handles)"];
 * Application=>ANCS_C   [label = "bt_gatt_ancs_c_notif_source_notif_enable(ancs_instance)"];
 * Application=>ANCS_C   [label = "bt_gatt_ancs_c_data_source_notif_enable(ancs_instance)"];
 * |||;
 * ...;
 * |||;
 * Application<<=ANCS_C  [label = "BT_GATT_ANCS_C_EVT_NOTIF"];
 * |||;
 * ...;
 * |||;
 * Application=>ANCS_C   [label = "bt_gatt_ancs_c_request_attrs(attr_id, buffer)"];
 * Application<<=ANCS_C  [label = "BT_GATT_ANCS_C_EVT_NOTIF_ATTRIBUTE"];
 * |||;
 * @endmsc
 */

#include <bluetooth/gatt.h>
#include <bluetooth/conn.h>
#include <bluetooth/gatt_dm.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Apple Notification Center Service UUID. */
#define ANCS_UUID_SERVICE                                                      \
	BT_UUID_128_ENCODE(0x7905f431, 0xb5ce, 0x4e99, 0xa40f, 0x4b1e122d00d0)

/** @brief Notification Source Characteristic UUID. */
#define ANCS_UUID_CHAR_NOTIFICATION_SOURCE                                     \
	BT_UUID_128_ENCODE(0x9fbf120d, 0x6301, 0x42d9, 0x8c58, 0x25e699a21dbd)

/** @brief Control Point Characteristic UUID. */
#define ANCS_UUID_CHAR_CONTROL_POINT                                           \
	BT_UUID_128_ENCODE(0x69d1d8f3, 0x45e1, 0x49a8, 0x9821, 0x9bbdfdaad9d9)

/** @brief Data Source Characteristic UUID. */
#define ANCS_UUID_CHAR_DATA_SOURCE                                             \
	BT_UUID_128_ENCODE(0x22eac6e9, 0x24d6, 0x4bb5, 0xbe44, 0xb36ace7c7bfb)

#define BT_UUID_ANCS_SERVICE BT_UUID_DECLARE_128(ANCS_UUID_SERVICE)
#define BT_UUID_ANCS_NOTIFICATION_SOURCE                                       \
	BT_UUID_DECLARE_128(ANCS_UUID_CHAR_NOTIFICATION_SOURCE)
#define BT_UUID_ANCS_CONTROL_POINT                                             \
	BT_UUID_DECLARE_128(ANCS_UUID_CHAR_CONTROL_POINT)
#define BT_UUID_ANCS_DATA_SOURCE BT_UUID_DECLARE_128(ANCS_UUID_CHAR_DATA_SOURCE)

/** Maximum data length of an iOS notification attribute. */
#define BT_GATT_ANCS_ATTR_DATA_MAX 32
/** Number of iOS notification categories: Other, Incoming Call, Missed Call, Voice Mail, Social, Schedule, Email, News, Health and Fitness, Business and Finance, Location, Entertainment. */
#define BT_GATT_ANCS_NB_OF_CATEGORY_ID 12
/** Number of iOS notification attributes: AppIdentifier, Title, Subtitle, Message, MessageSize, Date, PositiveActionLabel, NegativeActionLabel. */
#define BT_GATT_ANCS_NB_OF_NOTIF_ATTR 8
/** Number of iOS application attributes: DisplayName. */
#define BT_GATT_ANCS_NB_OF_APP_ATTR 1
/** Number of iOS notification events: Added, Modified, Removed. */
#define BT_GATT_ANCS_NB_OF_EVT_ID 3

/** @brief Length of the iOS notification data. */
#define BT_GATT_ANCS_NOTIFICATION_DATA_LENGTH 8

/** 0b.......1 Silent: First (LSB) bit is set. All flags can be active at the same time. */
#define BT_GATT_ANCS_EVENT_FLAG_SILENT 0
/** 0b......1. Important: Second (LSB) bit is set. All flags can be active at the same time. */
#define BT_GATT_ANCS_EVENT_FLAG_IMPORTANT 1
/** 0b.....1.. Pre-existing: Third (LSB) bit is set. All flags can be active at the same time. */
#define BT_GATT_ANCS_EVENT_FLAG_PREEXISTING 2
/** 0b....1... Positive action: Fourth (LSB) bit is set. All flags can be active at the same time. */
#define BT_GATT_ANCS_EVENT_FLAG_POSITIVE_ACTION 3
/** 0b...1.... Negative action: Fifth (LSB) bit is set. All flags can be active at the same time. */
#define BT_GATT_ANCS_EVENT_FLAG_NEGATIVE_ACTION 4

/** @defgroup BT_ATT_ANCS_NP_ERROR_CODES Notification Provider (iOS) Error Codes
 * @{ */
/** The command ID is unknown to the NP. */
#define BT_ATT_ERR_ANCS_NP_UNKNOWN_COMMAND 0xA0
/** The command format is invalid. */
#define BT_ATT_ERR_ANCS_NP_INVALID_COMMAND 0xA1
/** One or more parameters do not exist in the NP. */
#define BT_ATT_ERR_ANCS_NP_INVALID_PARAMETER 0xA2
/** The action failed to be performed by the NP. */
#define BT_ATT_ERR_ANCS_NP_ACTION_FAILED 0xA3
/** @} */

/**@brief Event types that are passed from client to application on an event. */
typedef enum {
	/** An iOS notification was received on the notification source control point. */
	BT_GATT_ANCS_C_EVT_NOTIF,
	/** An iOS notification was received on the notification source control point, but the format is invalid. */
	BT_GATT_ANCS_C_EVT_INVALID_NOTIF,
	/** A received iOS notification attribute has been parsed. */
	BT_GATT_ANCS_C_EVT_NOTIF_ATTRIBUTE,
	/** An iOS app attribute has been parsed. */
	BT_GATT_ANCS_C_EVT_APP_ATTRIBUTE,
	/** An error has been sent on the ANCS Control Point from the iOS Notification Provider. */
	BT_GATT_ANCS_C_EVT_NP_ERROR,
} bt_gatt_ancs_c_evt_type_t;

/**@brief Category IDs for iOS notifications. */
typedef enum {
	/** The iOS notification belongs to the "Other" category.  */
	BT_GATT_ANCS_CATEGORY_ID_OTHER,
	/** The iOS notification belongs to the "Incoming Call" category. */
	BT_GATT_ANCS_CATEGORY_ID_INCOMING_CALL,
	/** The iOS notification belongs to the "Missed Call" category. */
	BT_GATT_ANCS_CATEGORY_ID_MISSED_CALL,
	/** The iOS notification belongs to the "Voice Mail" category. */
	BT_GATT_ANCS_CATEGORY_ID_VOICE_MAIL,
	/** The iOS notification belongs to the "Social" category. */
	BT_GATT_ANCS_CATEGORY_ID_SOCIAL,
	/** The iOS notification belongs to the "Schedule" category. */
	BT_GATT_ANCS_CATEGORY_ID_SCHEDULE,
	/** The iOS notification belongs to the "Email" category. */
	BT_GATT_ANCS_CATEGORY_ID_EMAIL,
	/** The iOS notification belongs to the "News" category. */
	BT_GATT_ANCS_CATEGORY_ID_NEWS,
	/** The iOS notification belongs to the "Health and Fitness" category. */
	BT_GATT_ANCS_CATEGORY_ID_HEALTH_AND_FITNESS,
	/** The iOS notification belongs to the "Business and Finance" category. */
	BT_GATT_ANCS_CATEGORY_ID_BUSINESS_AND_FINANCE,
	/** The iOS notification belongs to the "Location" category. */
	BT_GATT_ANCS_CATEGORY_ID_LOCATION,
	/** The iOS notification belongs to the "Entertainment" category. */
	BT_GATT_ANCS_CATEGORY_ID_ENTERTAINMENT
} bt_gatt_ancs_c_category_id_val_t;

/**@brief Event IDs for iOS notifications. */
typedef enum {
	/** The iOS notification was added. */
	BT_GATT_ANCS_EVENT_ID_NOTIFICATION_ADDED,
	/** The iOS notification was modified. */
	BT_GATT_ANCS_EVENT_ID_NOTIFICATION_MODIFIED,
	/** The iOS notification was removed. */
	BT_GATT_ANCS_EVENT_ID_NOTIFICATION_REMOVED
} bt_gatt_ancs_c_evt_id_values_t;

/**@brief Control point command IDs that the Notification Consumer can send to the Notification Provider. */
typedef enum {
	/** Requests attributes to be sent from the NP to the NC for a given notification. */
	BT_GATT_ANCS_COMMAND_ID_GET_NOTIF_ATTRIBUTES,
	/** Requests attributes to be sent from the NP to the NC for a given iOS app. */
	BT_GATT_ANCS_COMMAND_ID_GET_APP_ATTRIBUTES,
	/** Requests an action to be performed on a given notification. For example, dismiss an alarm. */
	BT_GATT_ANCS_COMMAND_ID_GET_PERFORM_NOTIF_ACTION
} bt_gatt_ancs_c_cmd_id_val_t;

/**@brief IDs for actions that can be performed for iOS notifications. */
typedef enum {
	/** Positive action. */
	ACTION_ID_POSITIVE = 0,
	/** Negative action. */
	ACTION_ID_NEGATIVE
} bt_gatt_ancs_c_action_id_values_t;

/**@brief App attribute ID values.
 * @details Currently, only one value is defined. However, the number of app
 * attributes might increase. For this reason, they are stored in an enumeration.
 */
typedef enum {
	/** Command used to get the display name for an app identifier. */
	BT_GATT_ANCS_APP_ATTR_ID_DISPLAY_NAME = 0
} bt_gatt_ancs_c_app_attr_id_val_t;

/**@brief IDs for iOS notification attributes. */
typedef enum {
	/** Identifies that the attribute data is of an "App Identifier" type. */
	BT_GATT_ANCS_NOTIF_ATTR_ID_APP_IDENTIFIER = 0,
	/** Identifies that the attribute data is a "Title". */
	BT_GATT_ANCS_NOTIF_ATTR_ID_TITLE,
	/** Identifies that the attribute data is a "Subtitle". */
	BT_GATT_ANCS_NOTIF_ATTR_ID_SUBTITLE,
	/** Identifies that the attribute data is a "Message". */
	BT_GATT_ANCS_NOTIF_ATTR_ID_MESSAGE,
	/** Identifies that the attribute data is a "Message Size". */
	BT_GATT_ANCS_NOTIF_ATTR_ID_MESSAGE_SIZE,
	/** Identifies that the attribute data is a "Date". */
	BT_GATT_ANCS_NOTIF_ATTR_ID_DATE,
	/** The notification has a "Positive action" that can be executed associated with it. */
	BT_GATT_ANCS_NOTIF_ATTR_ID_POSITIVE_ACTION_LABEL,
	/** The notification has a "Negative action" that can be executed associated with it. */
	BT_GATT_ANCS_NOTIF_ATTR_ID_NEGATIVE_ACTION_LABEL
} bt_gatt_ancs_c_notif_attr_id_val_t;

/**@brief Flags for iOS notifications. */
typedef struct {
	/** If this flag is set, the notification has a low priority. */
	uint8_t silent : 1;
	/** If this flag is set, the notification has a high priority. */
	uint8_t important : 1;
	/** If this flag is set, the notification is pre-existing. */
	uint8_t pre_existing : 1;
	/** If this flag is set, the notification has a positive action that can be taken. */
	uint8_t positive_action : 1;
	/** If this flag is set, the notification has a negative action that can be taken. */
	uint8_t negative_action : 1;
} bt_gatt_ancs_c_notif_flags_t;

/**@brief Parsing states for received iOS notification and app attributes. */
typedef enum {
	/** Parsing the command ID. */
	COMMAND_ID,
	/** Parsing the notification UID. */
	NOTIF_UID,
	/** Parsing app ID. */
	APP_ID,
	/** Parsing attribute ID. */
	ATTR_ID,
	/** Parsing the LSB of the attribute length. */
	ATTR_LEN1,
	/** Parsing the MSB of the attribute length. */
	ATTR_LEN2,
	/** Parsing the attribute data. */
	ATTR_DATA,
	/** Parsing is skipped for the rest of an attribute (or entire attribute). */
	ATTR_SKIP,
	/** Parsing for one attribute is done. */
	DONE
} bt_gatt_ancs_c_parse_state_t;

/**@brief iOS notification structure. */
typedef struct {
	/** Notification UID. */
	uint32_t notif_uid;
	/** Whether the notification was added, removed, or modified. */
	bt_gatt_ancs_c_evt_id_values_t evt_id;
	/** Bitmask to signal whether a special condition applies to the notification. For example, "Silent" or "Important". */
	bt_gatt_ancs_c_notif_flags_t evt_flags;
	/** Classification of the notification type. For example, email or location. */
	bt_gatt_ancs_c_category_id_val_t category_id;
	/** Current number of active notifications for this category ID. */
	uint8_t category_count;
} bt_gatt_ancs_c_evt_notif_t;

/**@brief iOS attribute structure. This type is used for both notification attributes and app attributes. */
typedef struct {
	/** Length of the received attribute data. */
	uint16_t attr_len;
	/** Classification of the attribute type. For example, "Title" or "Date". */
	uint32_t attr_id;
	/** Pointer to where the memory is allocated for storing incoming attributes. */
	uint8_t *p_attr_data;
} bt_gatt_ancs_c_attr_t;

/**@brief iOS notification attribute content requested by the application. */
typedef struct {
	/** Boolean to determine whether this attribute will be requested from the Notification Provider. */
	bool get;
	/** Attribute ID: AppIdentifier(0), Title(1), Subtitle(2), Message(3), MessageSize(4), Date(5), PositiveActionLabel(6), NegativeActionLabel(7). */
	uint32_t attr_id;
	/** Length of the attribute. If more data is received from the Notification Provider, all the data beyond this length is discarded. */
	uint16_t attr_len;
	/** Pointer to where the memory is allocated for storing incoming attributes. */
	uint8_t *p_attr_data;
} bt_gatt_ancs_c_attr_list_t;

/**@brief ANCS client module event structure.
 *
 * @details The structure contains the event that is to be handled by the main application.
 */
typedef struct {
	/** Type of event. */
	bt_gatt_ancs_c_evt_type_t evt_type;
	/** Connection handle on which the ANCS service was discovered on the peer device. */
	uint16_t conn_handle;
	/** iOS notification. This is filled if @p evt_type is @ref BT_GATT_ANCS_C_EVT_NOTIF. */
	bt_gatt_ancs_c_evt_notif_t notif;
	/** An error coming from the Notification Provider. This is filled with @ref BT_ATT_ANCS_NP_ERROR_CODES if @p evt_type is @ref BT_GATT_ANCS_C_EVT_NP_ERROR. */
	uint8_t err_code_np;
	/** iOS notification attribute or app attribute, depending on the event type. */
	bt_gatt_ancs_c_attr_t attr;
	/** Notification UID. */
	uint32_t notif_uid;
	/** App identifier. */
	uint8_t app_id[BT_GATT_ANCS_ATTR_DATA_MAX];
} bt_gatt_ancs_c_evt_t;

/**@brief iOS notification event handler type. */
typedef void (*bt_gatt_ancs_c_evt_handler_t)(bt_gatt_ancs_c_evt_t *p_evt);

typedef struct {
	/** The current list of attributes that are being parsed. This will point to either ancs_notif_attr_list or ancs_app_attr_list in struct bt_gatt_ancs_c. */
	bt_gatt_ancs_c_attr_list_t *p_attr_list;
	/** Number of possible attributes. When parsing begins, it is set to either @ref BT_GATT_ANCS_NB_OF_NOTIF_ATTR or @ref BT_GATT_ANCS_NB_OF_APP_ATTR. */
	uint32_t nb_of_attr;
	/** The number of attributes expected upon receiving attributes. Keeps track of when to stop reading incoming attributes. */
	uint32_t expected_number_of_attrs;
	/** ANCS notification attribute parsing state. */
	bt_gatt_ancs_c_parse_state_t parse_state;
	/** Variable to keep track of what command type is being parsed ( @ref BT_GATT_ANCS_COMMAND_ID_GET_NOTIF_ATTRIBUTES or @ref BT_GATT_ANCS_COMMAND_ID_GET_APP_ATTRIBUTES). */
	bt_gatt_ancs_c_cmd_id_val_t command_id;
	/** Attribute that the parsed data is copied into. */
	uint8_t *p_data_dest;
	/** Variable to keep track of the parsing progress, for the given attribute. */
	uint16_t current_attr_index;
	/** Variable to keep track of the parsing progress, for the given app identifier. */
	uint32_t current_app_id_index;
} bt_gatt_ancs_parse_sm_t;

/**@brief iOS notification structure, which contains various status information for the client. */
struct bt_gatt_ancs_c {
	/** Connection object. */
	struct bt_conn *conn;

	/** Internal state. */
	atomic_t state;

	/** Handle of the Control Point Characteristic. */
	uint16_t handle_cp;

	/** Handle of the Notification Source Characteristic. */
	uint16_t handle_ns;

	/** Handle of the CCCD of the Notification Source Characteristic. */
	uint16_t handle_ns_ccc;

	/** Handle of the Data Source Characteristic. */
	uint16_t handle_ds;

	/** Handle of the CCCD of the Data Source Characteristic. */
	uint16_t handle_ds_ccc;

	/** GATT write parameters for Control Point Characteristic. */
	struct bt_gatt_write_params cp_write_params;

	/** Semaphore for Control Point GATT write parameters. */
	struct k_sem cp_write_params_sem;

	/** Data buffer for Control Point GATT write. */
	uint8_t cp_data[CONFIG_BT_GATT_ANCS_C_CP_BUFF_SIZE];

	/** GATT subscribe parameters for Notification Source Characteristic. */
	struct bt_gatt_subscribe_params ns_notif_params;

	/** GATT subscribe parameters for Data Source Characteristic. */
	struct bt_gatt_subscribe_params ds_notif_params;

	/** Event handler to be called for handling events in the Apple Notification client application. */
	bt_gatt_ancs_c_evt_handler_t evt_handler;

	/** For all attributes: contains information about whether the attributes are to be requested upon attribute request, and the length and buffer of where to store attribute data. */
	bt_gatt_ancs_c_attr_list_t
		ancs_notif_attr_list[BT_GATT_ANCS_NB_OF_NOTIF_ATTR];

	/** For all app attributes: contains information about whether the attributes are to be requested upon attribute request, and the length and buffer of where to store attribute data. */
	bt_gatt_ancs_c_attr_list_t
		ancs_app_attr_list[BT_GATT_ANCS_NB_OF_APP_ATTR];

	/** The number of attributes that are to be requested when an iOS notification attribute request is made. */
	uint32_t number_of_requested_attr;

	/** Structure containing different information used to parse incoming attributes correctly (from data_source characteristic). */
	bt_gatt_ancs_parse_sm_t parse_info;

	/** Allocate memory for the event here. The event is filled with several iterations of the @ref ancs_parse_get_attrs_response function when requesting iOS notification attributes. */
	bt_gatt_ancs_c_evt_t evt;
};

/**@brief Apple Notification client init structure, which contains all options and data needed for
 *        initialization of the client. */
struct bt_gatt_ancs_c_init_param {
	/** Event handler to be called for handling events in the Apple Notification client application. */
	bt_gatt_ancs_c_evt_handler_t evt_handler;
};

/**@brief Function for initializing the ANCS client.
 *
 * @param[out] ancs_c       ANCS client structure. This structure must be
 *                          supplied by the application. It is initialized by this function
 *                          and will later be used to identify this particular client instance.
 * @param[in]  ancs_c_init  Information needed to initialize the client.
 *
 * @retval 0 If the client was initialized successfully.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_gatt_ancs_c_init(struct bt_gatt_ancs_c *ancs_c,
			const struct bt_gatt_ancs_c_init_param *ancs_c_init);

/**@brief Function for clearing the ANCS client connection.
 *
 * @param[in] ancs_c  iOS notification structure. This structure must be supplied by
 *                    the application. It identifies the particular client instance to use.
 */
void bt_gatt_ancs_c_on_disconnected(struct bt_gatt_ancs_c *ancs_c);

/**@brief Function for writing to the CCCD to enable notifications from the Apple Notification Service.
 *
 * @param[in] ancs_c  iOS notification structure. This structure must be supplied by
 *                    the application. It identifies the particular client instance to use.
 *
 * @retval 0 If writing to the CCCD was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_gatt_ancs_c_notif_source_notif_enable(struct bt_gatt_ancs_c *ancs_c);

/**@brief Function for writing to the CCCD to enable data source notifications from the ANCS.
 *
 * @param[in] ancs_c iOS notification structure. This structure must be supplied by
 *                   the application. It identifies the particular client instance to use.
 *
 * @retval 0 If writing to the CCCD was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_gatt_ancs_c_data_source_notif_enable(struct bt_gatt_ancs_c *ancs_c);

/**@brief Function for writing to the CCCD to disable notifications from the ANCS.
 *
 * @param[in] ancs_c  iOS notification structure. This structure must be supplied by
 *                    the application. It identifies the particular client instance to use.
 *
 * @retval 0 If writing to the CCCD was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_gatt_ancs_c_notif_source_notif_disable(struct bt_gatt_ancs_c *ancs_c);

/**@brief Function for writing to the CCCD to disable data source notifications from the ANCS.
 *
 * @param[in] ancs_c  iOS notification structure. This structure must be supplied by
 *                    the application. It identifies the particular client instance to use.
 *
 * @retval 0 If writing to the CCCD was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_gatt_ancs_c_data_source_notif_disable(struct bt_gatt_ancs_c *ancs_c);

/**@brief Function for registering attributes that will be requested when @ref bt_gatt_ancs_c_request_attrs
 *        is called.
 *
 * @param[in] ancs_c ANCS client instance on which the attribute is registered.
 * @param[in] id     ID of the attribute that is added.
 * @param[in] p_data Pointer to the buffer where the data of the attribute can be stored.
 * @param[in] len    Length of the buffer where the data of the attribute can be stored.
 *
 * @retval 0 If all operations are successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_gatt_ancs_c_attr_add(struct bt_gatt_ancs_c *ancs_c,
			    bt_gatt_ancs_c_notif_attr_id_val_t const id,
			    uint8_t *p_data, uint16_t const len);

/**@brief Function for registering attributes that will be requested when @ref bt_gatt_ancs_c_app_attr_request
 *        is called.
 *
 * @param[in] ancs_c ANCS client instance on which the attribute is registered.
 * @param[in] id     ID of the attribute that is added.
 * @param[in] p_data Pointer to the buffer where the data of the attribute can be stored.
 * @param[in] len    Length of the buffer where the data of the attribute can be stored.
 *
 * @retval 0 If all operations are successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_gatt_ancs_c_app_attr_add(struct bt_gatt_ancs_c *ancs_c,
				bt_gatt_ancs_c_app_attr_id_val_t const id,
				uint8_t *p_data, uint16_t const len);

/**@brief Function for requesting attributes for a notification.
 *
 * @param[in] ancs_c   iOS notification structure. This structure must be supplied by
 *                     the application. It identifies the particular client instance to use.
 * @param[in] p_notif  Pointer to the notification whose attributes will be requested from
 *                     the Notification Provider.
 * @param[in] timeout  Time-out for taking Control Point semaphore.
 *
 * @retval 0 If all operations are successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_gatt_ancs_c_request_attrs(struct bt_gatt_ancs_c *ancs_c,
				 bt_gatt_ancs_c_evt_notif_t const *p_notif,
				 k_timeout_t timeout);

/**@brief Function for requesting attributes for a given app.
 *
 * @param[in] ancs_c   iOS notification structure. This structure must be supplied by
 *                     the application. It identifies the particular client instance to use.
 * @param[in] p_app_id App identifier of the app for which the app attributes are requested.
 * @param[in] len      Length of the app identifier.
 * @param[in] timeout  Time-out for taking Control Point semaphore.
 *
 * @retval 0 If all operations are successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_gatt_ancs_c_app_attr_request(struct bt_gatt_ancs_c *ancs_c,
				    uint8_t const *p_app_id, uint32_t len,
				    k_timeout_t timeout);

/**@brief Function for performing a notification action.
 *
 * @param[in] ancs_c    iOS notification structure. This structure must be supplied by
 *                      the application. It identifies the particular client instance to use.
 * @param[in] uuid      The UUID of the notification for which to perform the action.
 * @param[in] action_id Perform a positive or negative action.
 * @param[in] timeout   Time-out for taking Control Point semaphore.
 *
 * @retval 0 If the operation is successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_gatt_ancs_perform_notif_action(
	struct bt_gatt_ancs_c *ancs_c, uint32_t uuid,
	bt_gatt_ancs_c_action_id_values_t action_id, k_timeout_t timeout);

/**@brief Function for assigning a handle to this instance of ancs_c.
 *
 * @details Call this function when a link has been established with a peer to
 *          associate the link to this instance of the module. This makes it
 *          possible to handle several links and associate each link to a particular
 *          instance of this module.
 *
 * @param[in]     dm     Discovery object.
 * @param[in,out] ancs_c Pointer to the ANCS client structure instance for associating the link.
 *
 * @retval 0 If the operation is successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_gatt_ancs_c_handles_assign(struct bt_gatt_dm *dm,
				  struct bt_gatt_ancs_c *ancs_c);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* BT_GATT_ANCS_C_H_ */
