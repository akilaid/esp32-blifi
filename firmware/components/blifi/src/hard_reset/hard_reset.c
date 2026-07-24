/**
 * @file hard_reset.c
 * @brief Detects a bootloader factory reset (reset-pin, §6.1) on the next boot
 *        and dispatches the optional app-data reset callback.
 */
#include "hard_reset.h"
#include "blifi.h"

#include "esp_log.h"
#include "sdkconfig.h"

/* The bootloader factory-reset RTC flag is only available when the bootloader
 * factory-reset feature is configured (it auto-selects BOOTLOADER_RESERVE_RTC_MEM).
 * Builds without it — e.g. the Arduino/PlatformIO basic setup — skip detection. */
#if defined(CONFIG_BOOTLOADER_RESERVE_RTC_MEM)
#include "bootloader_common.h"
#define BLIFI_HARD_RESET_SUPPORTED 1
#endif

static const char *TAG = "blifi_reset";

static bool                  s_checked;
static bool                  s_was_hard_reset;
static blifi_data_reset_cb_t s_cb;
static void                 *s_cb_arg;

void blifi_hard_reset_init_on_boot(void)
{
    if (s_checked) {
        return;
    }
#if defined(BLIFI_HARD_RESET_SUPPORTED)
    /* Consume-once: the getter clears the RTC flag as it reads it. */
    s_was_hard_reset = bootloader_common_get_rtc_retain_mem_factory_reset_state();
#else
    s_was_hard_reset = false; /* feature not configured in this build */
#endif
    s_checked = true;
    if (s_was_hard_reset) {
        ESP_LOGW(TAG, "boot follows a hard (reset-pin) factory reset");
    }
}

bool blifi_was_hard_reset(void)
{
    return s_was_hard_reset;
}

esp_err_t blifi_register_data_reset_callback(blifi_data_reset_cb_t cb, void *arg)
{
    s_cb = cb;
    s_cb_arg = arg;
    return ESP_OK;
}

void blifi_hard_reset_dispatch(void)
{
    if (s_was_hard_reset && s_cb) {
        ESP_LOGI(TAG, "invoking app data-reset callback");
        s_cb(s_cb_arg);
    }
}
