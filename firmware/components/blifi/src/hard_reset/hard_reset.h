/**
 * @file hard_reset.h
 * @brief Internal helpers for the reset-pin hard-reset flow (docs/plan.md §6.1).
 *
 * The GPIO factory reset itself is handled by the ESP-IDF bootloader (before
 * app_main); this module just detects, on the next boot, that it happened and
 * dispatches the optional app-data reset callback. The public entry points
 * (::blifi_was_hard_reset, ::blifi_register_data_reset_callback) live in blifi.h.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read and cache the bootloader's RTC-retained factory-reset flag.
 *
 * The underlying flag is consumed (cleared) on first read, so this must be
 * called exactly once, early in ::blifi_init. Safe to call when the factory
 * reset feature is disabled — it simply caches false.
 */
void blifi_hard_reset_init_on_boot(void);

/**
 * @brief If this boot follows a hard reset, invoke the registered app-data
 *        reset callback (if any). No-op otherwise.
 */
void blifi_hard_reset_dispatch(void);

#ifdef __cplusplus
}
#endif
