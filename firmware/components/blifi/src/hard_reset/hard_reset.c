/**
 * @file hard_reset.c
 * @brief Detects a bootloader factory reset (reset-pin, §6.1) on the next boot
 *        and dispatches the optional app-data reset callback.
 */
#include "hard_reset.h"
#include "blifi.h"

#include "bootloader_common.h"
#include "esp_log.h"

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
    /* Consume-once: the getter clears the RTC flag as it reads it. */
    s_was_hard_reset = bootloader_common_get_rtc_retain_mem_factory_reset_state();
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
