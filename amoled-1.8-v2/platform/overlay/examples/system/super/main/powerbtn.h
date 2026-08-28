/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Start the PWRON long-press power-off monitor (#265). Battery boards only --
 * see main.cpp (gated on BOARD_HAS_BATTERY). */
void powerbtn_start(void);

#ifdef __cplusplus
}
#endif
