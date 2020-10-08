.. _peripheral_ancs_client:

Bluetooth: Peripheral ANCS client
#################################

The Peripheral ANCS client sample demonstrates how to use the :ref:`ancs_c_readme`.

Overview
********

The ANCS client sample implements an Apple Notification Center Service client. This client receives iOS notifications and is therefore a Notification Consumer. It can be connected with a Notification Provider, typically an iPhone or some other Apple device, which functions as ANCS server.

When the sample is connected to a Notification Provider, it receives and prints incoming notifications on the UART.

Notifications can have positive and negative actions associated with them, depending on the app that is sending the notification. For example, a notification for an incoming call is usually associated with the positive action to answer the call and the negative action to reject it. After receiving a notification, the available actions are indicated by flags on UART. The sample can perform the positive / negative action as response to the notification.

Requirements
************

* One of the following development boards:

  * |nRF5340DK|
  * |nRF52840DK|
  * |nRF52DK|
  * |nRF51DK|

* A device running an ANCS Server to connect with (for example, an iPhone which runs iOS, or a Bluetooth Low Energy dongle and nRF Connect for Desktop)

User interface
**************

LED 1:
   * Blinks with a period of 2 seconds, duty cycle 50%, when the main loop is running.

LED 2:
   * On when connected.

Button 1:
   * Request iOS notification attributes (content) on UART.

Button 2:
   * Request iOS app attributes on UART.

Button 3:
   * Perform a positive action as response to the last received notification.

Button 4:
   * Perform a negative action as response to the last received notification.

Building and running
********************
.. |sample path| replace:: :file:`samples/bluetooth/peripheral_ancs_client`

.. include:: /includes/build_and_run.txt

.. _peripheral_ancs_client_testing:

Testing
=======

After programming the sample to your board, you can test it either by connecting to an iOS device or by using `nRF Connect for Desktop`_ that emulates an ANCS Server.

Testing with an iOS device
--------------------------

1. |connect_terminal_specific|
#. Reset the board.
#. Select the device in the iOS settings -> Bluetooth menu and connect.
#. Observe that notifications that are displayed in the iOS notification tab also show up on the UART from the sample.
#. Press Button 1 to retrieve the notification attributes and observe that you receive, among other information, the app identifier for the last received notification. For example, if you got a notification from the Calendar app and request the app identifier, it is "com.apple.mobilecal".
#. Press Button 2 to retrieve the app attributes and observe that you receive the display name for the app identifier from the previous step. For example, requesting the app attributes for "com.apple.mobilecal" yields "Calendar".
#. If the notification has a flag for a positive or negative action, perform the notification action with Button 3 or Button 4, respectively.

Testing with nRF Connect for Desktop
------------------------------------

1. |connect_terminal_specific|
#. Reset the board.
#. Start `nRF Connect for Desktop`_ and select the connected dongle that is used for communication.
#. Go to the **Server setup** tab.
   Click the dongle configuration and select **Load setup**.
   Load the ``ANCS_central.ncs`` file that is located under :file:`samples/bluetooth/peripheral_ancs_client` in the |NCS| folder structure.
#. Click **Apply to device**.
#. Go to the **Connection Map** tab.
   Click the dongle configuration and select **Security parameters**.
   Check **Perform Bonding**, and click **Apply**.
#. Connect to the device from nRF Connect. The device is advertising as "ANCS".
#. Wait until the bond is established. Verify that the UART data is received as follows::

      Connected xx:xx:xx:xx:xx:xx (random)
      The discovery procedure succeeded
      Security changed: xx:xx:xx:xx:xx:xx (random) level 2
      Pairing completed: xx:xx:xx:xx:xx:xx (random), bonded: 1

#. After bonding, verify in nRF Connect that the Client Characteristic Configuration descriptors (CCCD) for ANCS Notification Source (0x120D) and ANCS Data Source (0xC6E9) are set to ``01 00``.

Send an iOS notification to the application
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following table shows the format of a notification that you can send to the example application:

   +------------------+---------------+--------------------------+
   | Field            | Example value | Interpretation           |
   +==================+===============+==========================+
   | Event ID         | 0             | Notification added       |
   +------------------+---------------+--------------------------+
   | Flags            | 18            | Positive/negative action |
   +------------------+---------------+--------------------------+
   | Category         | 06            | Email                    |
   +------------------+---------------+--------------------------+
   | Category count   | 02            |                          |
   +------------------+---------------+--------------------------+
   | Notification UID | 01 02 03 04   | 67305985 (0x4030201)     |
   +------------------+---------------+--------------------------+

1. In nRF Connect, set the value of ANCS Notification Source (characteristic 0x120D) to ``00 18 06 02 01 02 03 04`` and click Update.
#. Verify that the UART data is received as follows::

      Notification
      Event:       Added
      Category ID: Email
      Category Cnt:2
      UID:         67305985
      Flags:
       Positive Action
       Negative Action

Perform a notification action
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The received notification has two flags: Positive Action and Negative Action. This means that you can perform these two actions on the notification.

The following table shows the format of the message that the application must send back to perform a notification action:

   +------------------+---------------+-----------------------------+
   | Field            | Example value | Interpretation              |
   +==================+===============+=============================+
   | Command ID       | 2             | Perform notification action |
   +------------------+---------------+-----------------------------+
   | Notification UID | 01 02 03 04   | 67305985 (0x4030201)        |
   +------------------+---------------+-----------------------------+
   | Action           | 00            | Positive                    |
   |                  |               |                             |
   |                  | 01            | Negative                    |
   +------------------+---------------+-----------------------------+

1. Press Button 3 to perform a positive action and verify that the ANCS Control Point (0xD8F3) is updated to ``02 01 02 03 04 00``.
#. You can also press Button 4 and observe that the server receives a negative action ``02 01 02 03 04 01``.

Retrieve notification attributes
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following table shows the relevant part of a request to retrieve notification attributes:

   +------------------+---------------+-----------------------------+
   | Field            | Example value | Interpretation              |
   +==================+===============+=============================+
   | Command ID       | 0             | Get notification attributes |
   +------------------+---------------+-----------------------------+
   | Notification UID | 01 02 03 04   | 67305985 (0x4030201)        |
   +------------------+---------------+-----------------------------+
   | Attribute ID     | 00            | App identifier              |
   +------------------+---------------+-----------------------------+
   | Attribute ID     | 01            | Title                       |
   +------------------+---------------+-----------------------------+
   | Length           | 20 00         | 0x0020                      |
   +------------------+---------------+-----------------------------+
   | Attribute ID     | 03            | Message                     |
   +------------------+---------------+-----------------------------+
   | Length           | 20 00         | 0x0020                      |
   +------------------+---------------+-----------------------------+

Note that the example application will request all existing attribute types, not only a subset.

The following table shows the format of a response that contains some of the requested notification attributes:

   +------------------+---------------+-----------------------------+
   | Field            | Example value | Interpretation              |
   +==================+===============+=============================+
   | Command ID       | 0             | Get notification attributes |
   +------------------+---------------+-----------------------------+
   | Notification UID | 01 02 03 04   | 67305985 (0x4030201)        |
   +------------------+---------------+-----------------------------+
   | Attribute ID     | 01            | Title                       |
   +------------------+---------------+-----------------------------+
   | Length           | 03 00         | 0x0003                      |
   +------------------+---------------+-----------------------------+
   | Data             | 6E 52 46      | "nRF"                       |
   +------------------+---------------+-----------------------------+
   | Attribute ID     | 03            | Message                     |
   +------------------+---------------+-----------------------------+
   | Length           | 02 00         | 0x0002                      |
   +------------------+---------------+-----------------------------+
   | Data             | 35 32         | "52"                        |
   +------------------+---------------+-----------------------------+
   | Attribute ID     | 00            | App identifier              |
   +------------------+---------------+-----------------------------+
   | Length           | 03 00         | 0x0003                      |
   +------------------+---------------+-----------------------------+
   | Data             | 63 6F 6D      | "com"                       |
   +------------------+---------------+-----------------------------+

1. Press Button 1 to request notification attributes for the iOS notification that was received.
#. In nRF Connect, verify that the ANCS Control Point (0xD8F3) is updated to ``00 01 02 03 04 00 01 20 00 02 20 00 03 20 00 04 05 06 07``.
#. Respond to the request by sending two notification attributes: the title and the message. To do this, first set the Data Source (characteristic 0xC6E9) in the Server to ``00 01 02 03 04 01 03 00 6E 52 46 03 02 00 35 32``.
#. The application will print the received data on UART. Verify that the UART output is as follows::

      Title: nRF
      Message: 52

#. Update the Data Source again with the app identifier ``00 03 00 63 6F 6D``.
#. Verify that the notification is received and the UART output is as follows::

      App Identifier: com

Retrieve app attributes
^^^^^^^^^^^^^^^^^^^^^^^

With the app identifier, you can request attributes of the app that sent the notification.

The following table shows the format of a request to retrieve app attributes:

   +------------------+---------------+--------------------+
   | Field            | Example value | Interpretation     |
   +==================+===============+====================+
   | Command ID       | 1             | Get app attributes |
   +------------------+---------------+--------------------+
   | App identifier   | 6D 6F 63 00   | "com" + '\0'       |
   +------------------+---------------+--------------------+
   | Attribute ID     | 0             | Display name       |
   +------------------+---------------+--------------------+

The following table shows the format of a response that contains the requested app attributes:

   +------------------+---------------+--------------------+
   | Field            | Example value | Interpretation     |
   +==================+===============+====================+
   | Command ID       | 1             | Get app attributes |
   +------------------+---------------+--------------------+
   | App identifier   | 6D 6F 63 00   | "com" + '\0'       |
   +------------------+---------------+--------------------+
   | Attribute ID     | 0             | Display name       |
   +------------------+---------------+--------------------+
   | Length           | 04 00         | 0x0004             |
   +------------------+---------------+--------------------+
   | Data             | 4D 61 69 6C   | "Mail"             |
   +------------------+---------------+--------------------+

1. Press Button 2 to request app attributes for the app with the app identifier "com" (the last received app identifier).
#. In nRF Connect, verify that the ANCS Control Point (0xD8F3) is updated to ``01 63 6F 6D 00 00``.
#. Respond to the request by sending the app attribute. To do this, set the Data Source (characteristic 0xC6E9) in the Server to ``01 63 6F 6D 00 00 04 00 4D 61 69 6C``.
#. The application will print the received data on UART. Verify that the UART output is as follows::

      Display Name: Mail

Disconnect
^^^^^^^^^^

1. Disconnect the device in nRF Connect.
#. As bond information is preserved by nRF Connect, you can immediately reconnect to the device by clicking the Connect button again.

Dependencies
************

This sample uses the following |NCS| libraries:

* :ref:`ancs_c_readme`
* :ref:`gatt_dm_readme`
* :ref:`dk_buttons_and_leds_readme`

In addition, it uses the following Zephyr libraries:

* ``include/zephyr/types.h``
* ``lib/libc/minimal/include/errno.h``
* ``include/sys/printk.h``
* :ref:`zephyr:bluetooth_api`:

  * ``include/bluetooth/bluetooth.h``
  * ``include/bluetooth/conn.h``
  * ``include/bluetooth/uuid.h``
  * ``include/bluetooth/gatt.h``
