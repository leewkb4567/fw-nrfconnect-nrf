#
# Copyright (c) 2018 - 2020 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

zephyr_sources_ifdef(CONFIG_BT_GATT_ANCS_C ancs_c.c
                                           ancs_attr_parser.c
                                           ancs_app_attr_get.c)
zephyr_sources_ifdef(CONFIG_BT_GATT_BAS_C bas_c.c)
zephyr_sources_ifdef(CONFIG_BT_GATT_BMS_SRV bms_srv.c)
zephyr_sources_ifdef(CONFIG_BT_GATT_DFU_SMP_C dfu_smp_c.c)
zephyr_sources_ifdef(CONFIG_BT_GATT_HIDS hids.c)
zephyr_sources_ifdef(CONFIG_BT_GATT_HIDS_C hids_c.c)
zephyr_sources_ifdef(CONFIG_BT_GATT_THROUGHPUT throughput.c)
zephyr_sources_ifdef(CONFIG_BT_GATT_NUS nus.c)
zephyr_sources_ifdef(CONFIG_BT_GATT_NUS_C nus_c.c)
zephyr_sources_ifdef(CONFIG_BT_GATT_LBS lbs.c)
zephyr_sources_ifdef(CONFIG_BT_GATT_LATENCY latency.c)
zephyr_sources_ifdef(CONFIG_BT_GATT_LATENCY_C latency_c.c)
zephyr_sources_ifdef(CONFIG_BT_GADGETS gadgets.c)
