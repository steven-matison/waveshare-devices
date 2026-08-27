/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Battery-runtime logger (Tuna Starlink). This board carries a LiPo but has no
 * fuel gauge, so runtime can only be measured empirically: log the AXP2101 raw
 * battery voltage (VBAT ADC) over time, run on battery until the rail collapses
 * at the splash (brownout), then read the CSV back off the SD card.
 *
 * We reuse the board's own, already-registered AXP2101 driver rather than
 * re-rolling the register reads: power_manager_get_battery_state() enables the
 * ADC channels and does the 14-bit VBAT/VBUS/VSYS masking correctly. That
 * symbol is compiled into the image (CUSTOM_DEVICE_IMPLEMENT in the board's
 * power_manager.c); its header isn't on a clean include path from this
 * component, so we forward-declare exactly the struct + prototype we need and
 * fetch the live handle by name via esp_board_manager.
 */
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_board_device.h"

#include "battlog.h"

/* --- Mirror of the bits we need from the board's axp2101_power_manager.h.
 * These are stable ABI (register-derived); keep in sync if the board driver's
 * struct layout ever changes. Only the leading fields are read here. */
extern "C" {
typedef enum {
    PM_SRC_UNKNOWN = 0,
    PM_SRC_BATTERY,
    PM_SRC_EXTERNAL,
} pm_power_source_t;

typedef struct {
    bool     is_present;
    int      power_source;   /* pm_power_source_t */
    int      charge_state;   /* power_manager_battery_charge_state_t */
    bool     has_voltage_mv;
    uint32_t voltage_mv;
    bool     has_percentage;
    uint8_t  percentage;
    bool     has_vbus_voltage_mv;
    uint32_t vbus_voltage_mv;
    bool     has_system_voltage_mv;
    uint32_t system_voltage_mv;
} pm_battery_state_t;

/* Defined in the board's power_manager.c (CUSTOM_DEVICE_IMPLEMENT). */
esp_err_t power_manager_get_battery_state(void *device_handle, pm_battery_state_t *state);
}

static const char *TAG = "BATTLOG";

#define BATTLOG_PATH        "/sdcard/battlog.csv"
#define BATTLOG_INTERVAL_MS 15000   /* 15 s: ~480 rows over a 2 h run, good resolution near the cliff */
#define BATTLOG_HEADER \
    "# seq,uptime_ms,vbat_mv,vbus_mv,vsys_mv,percent,source,charge_state,present\n"

static const char *source_str(int src)
{
    switch (src) {
    case PM_SRC_BATTERY:  return "BAT";
    case PM_SRC_EXTERNAL: return "USB";
    default:              return "UNK";
    }
}

/* Append one line, flushing all the way to the card each time so an abrupt
 * brownout can lose at most the current row, never the ones before it. */
static bool battlog_append(const char *line)
{
    FILE *f = fopen(BATTLOG_PATH, "a");
    if (!f) {
        ESP_LOGW(TAG, "open %s failed: %s", BATTLOG_PATH, strerror(errno));
        return false;
    }
    fputs(line, f);
    fflush(f);
    fsync(fileno(f));
    fclose(f);
    return true;
}

/* Dump the whole log to the serial console once, wrapped in markers, so a
 * completed on-battery run can be retrieved over USB without pulling the card.
 * Retries the open briefly in case the SD mount is still coming up at boot. */
static void battlog_dump(void)
{
    FILE *f = nullptr;
    for (int i = 0; i < 10 && f == nullptr; i++) {
        f = fopen(BATTLOG_PATH, "r");
        if (!f) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    printf("\n----BATTLOG-DUMP-BEGIN----\n");
    if (!f) {
        printf("(no file %s: %s)\n", BATTLOG_PATH, strerror(errno));
    } else {
        char buf[256];
        while (fgets(buf, sizeof(buf), f)) {
            fputs(buf, stdout);
        }
        fclose(f);
    }
    printf("----BATTLOG-DUMP-END----\n");
    fflush(stdout);
}

static void battlog_task(void *arg)
{
    /* Dump any prior run's log to serial first (retrieval over USB). */
    battlog_dump();

    /* Resolve the AXP2101 handle. The board manager auto-inits devices at boot,
     * but this task may race that, so retry a few seconds before giving up. */
    void *pm = nullptr;
    for (int i = 0; i < 30 && pm == nullptr; i++) {
        if (esp_board_device_get_handle("axp2101_power_manager", &pm) != ESP_OK) {
            pm = nullptr;
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    if (pm == nullptr) {
        ESP_LOGE(TAG, "AXP2101 handle unavailable; battery logging disabled");
        vTaskDelete(nullptr);
        return;
    }

    /* Write the CSV header once (only when the file is new/empty), then a boot
     * marker with the reset reason so separate runs are distinguishable and a
     * brownout reset is visible in the log itself. */
    struct stat st;
    if (stat(BATTLOG_PATH, &st) != 0 || st.st_size == 0) {
        battlog_append(BATTLOG_HEADER);
    }
    {
        char boot[96];
        snprintf(boot, sizeof(boot), "# boot reset_reason=%d\n", (int)esp_reset_reason());
        battlog_append(boot);
    }

    uint32_t seq = 0;
    for (;;) {
        pm_battery_state_t s;
        memset(&s, 0, sizeof(s));
        esp_err_t err = power_manager_get_battery_state(pm, &s);
        int64_t up_ms = esp_timer_get_time() / 1000;

        if (err != ESP_OK) {
            ESP_LOGW(TAG, "read failed: %s", esp_err_to_name(err));
        } else {
            char line[160];
            snprintf(line, sizeof(line),
                     "%lu,%lld,%lu,%lu,%lu,%d,%s,%d,%d\n",
                     (unsigned long)seq, (long long)up_ms,
                     (unsigned long)(s.has_voltage_mv ? s.voltage_mv : 0),
                     (unsigned long)(s.has_vbus_voltage_mv ? s.vbus_voltage_mv : 0),
                     (unsigned long)(s.has_system_voltage_mv ? s.system_voltage_mv : 0),
                     s.has_percentage ? (int)s.percentage : -1,
                     source_str(s.power_source),
                     s.charge_state, s.is_present ? 1 : 0);
            battlog_append(line);
            /* Also to serial: useful as a live sanity check while still on USB,
             * before unplugging for the real (serial-less) discharge run. */
            ESP_LOGI(TAG, "seq=%lu vbat=%lumV vbus=%lumV src=%s pct=%d chg=%d",
                     (unsigned long)seq,
                     (unsigned long)(s.has_voltage_mv ? s.voltage_mv : 0),
                     (unsigned long)(s.has_vbus_voltage_mv ? s.vbus_voltage_mv : 0),
                     source_str(s.power_source),
                     s.has_percentage ? (int)s.percentage : -1,
                     s.charge_state);
        }
        seq++;
        vTaskDelay(pdMS_TO_TICKS(BATTLOG_INTERVAL_MS));
    }
}

void battlog_start(void)
{
    xTaskCreate(battlog_task, "battlog", 4 * 1024, nullptr, 4, nullptr);
}
