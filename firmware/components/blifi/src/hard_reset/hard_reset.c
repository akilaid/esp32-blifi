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

/* ----------------------------------------- hard-reset indicator pin (§6.2) */
/* Entirely opt-in: the whole feature compiles out when the Kconfig option is off. */
#if CONFIG_BLIFI_RESET_INDICATOR_ENABLE

#include "driver/gpio.h"
#include "esp_timer.h"

static blifi_reset_indicator_config_t s_ind;
static bool                           s_ind_set;
static esp_timer_handle_t             s_ind_timer;

static inline bool indicator_usable(void)
{
    return s_ind_set && s_ind.enable && s_ind.gpio >= 0;
}

static void indicator_off_cb(void *arg)
{
    (void)arg;
    gpio_set_level((gpio_num_t)s_ind.gpio, !s_ind.active_level);
    ESP_LOGI(TAG, "hard-reset indicator: released (pulse elapsed)");
}

static void indicator_assert(void)
{
    if (!indicator_usable()) {
        return;
    }
    const gpio_config_t io = {
        .pin_bit_mask = 1ULL << s_ind.gpio,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    gpio_set_level((gpio_num_t)s_ind.gpio, s_ind.active_level);
    ESP_LOGI(TAG, "hard-reset indicator: GPIO%d asserted (level %d, pulse %ums)",
             s_ind.gpio, s_ind.active_level, (unsigned)s_ind.pulse_ms);
    if (s_ind.pulse_ms > 0) {
        if (!s_ind_timer) {
            const esp_timer_create_args_t args = {
                .callback = indicator_off_cb, .name = "blifi_ind" };
            if (esp_timer_create(&args, &s_ind_timer) != ESP_OK) {
                return;
            }
        }
        esp_timer_stop(s_ind_timer);
        esp_timer_start_once(s_ind_timer, (uint64_t)s_ind.pulse_ms * 1000);
    }
    /* pulse_ms == 0: hold until blifi_hard_reset_indicator_clear() at CONNECTED. */
}

void blifi_hard_reset_set_indicator(const blifi_reset_indicator_config_t *cfg)
{
    if (cfg) {
        s_ind = *cfg;
        s_ind_set = true;
    }
}

void blifi_hard_reset_indicator_clear(void)
{
    if (!indicator_usable()) {
        return;
    }
    if (s_ind_timer) {
        esp_timer_stop(s_ind_timer);
    }
    gpio_set_level((gpio_num_t)s_ind.gpio, !s_ind.active_level);
}

#else  /* indicator compiled out — no-ops */

static inline void indicator_assert(void) {}
void blifi_hard_reset_set_indicator(const blifi_reset_indicator_config_t *cfg) { (void)cfg; }
void blifi_hard_reset_indicator_clear(void) {}

#endif /* CONFIG_BLIFI_RESET_INDICATOR_ENABLE */

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
    if (!s_was_hard_reset) {
        return;
    }
    indicator_assert(); /* §6.2 indicator pin — no-op when compiled out */
    if (s_cb) {
        ESP_LOGI(TAG, "invoking app data-reset callback");
        s_cb(s_cb_arg);
    }
}
