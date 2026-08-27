/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Battery-runtime logger (Tuna Starlink). Periodically reads the AXP2101
 * battery state and appends a CSV row to /sdcard/battlog.csv so we can
 * measure real on-battery runtime: run on battery, let it brown out, then
 * plug USB back in and read the card. See DesktopShare memory
 * project_tuna_starlink_amoled_fleet.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Start the battery-logging task. Safe to call once from app_main setup. */
void battlog_start(void);

#ifdef __cplusplus
}
#endif
