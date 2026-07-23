/**
 * @file pop.h
 * @brief Proof-of-Possession generation (docs/security.md §7).
 */
#pragma once

#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** PoP string length: 8 Crockford base32 chars (~40 bits) + NUL. */
#define BLIFI_POP_LEN 8
#define BLIFI_POP_BUFSZ (BLIFI_POP_LEN + 1)

/**
 * @brief Generate a random 8-char Crockford base32 PoP into `out` (>= 9 bytes).
 */
esp_err_t blifi_pop_generate(char *out, size_t cap);

#ifdef __cplusplus
}
#endif
