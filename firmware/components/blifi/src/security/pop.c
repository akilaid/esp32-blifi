#include "pop.h"

#include <stdint.h>
#include <string.h>

#include "esp_random.h"

/* Crockford base32 alphabet (excludes I, L, O, U). */
static const char CROCKFORD[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

esp_err_t blifi_pop_generate(char *out, size_t cap)
{
    if (!out || cap < BLIFI_POP_BUFSZ) {
        return ESP_ERR_INVALID_ARG;
    }
    /* 8 chars * 5 bits = 40 bits = 5 random bytes. */
    uint8_t raw[5];
    esp_fill_random(raw, sizeof(raw));

    uint64_t bits = 0;
    for (int i = 0; i < 5; i++) {
        bits = (bits << 8) | raw[i];
    }
    for (int i = 0; i < BLIFI_POP_LEN; i++) {
        int shift = (BLIFI_POP_LEN - 1 - i) * 5;
        out[i] = CROCKFORD[(bits >> shift) & 0x1f];
    }
    out[BLIFI_POP_LEN] = '\0';
    return ESP_OK;
}

esp_err_t blifi_pop_validate(const char *pop)
{
    if (!pop || strlen(pop) != BLIFI_POP_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < BLIFI_POP_LEN; i++) {
        if (strchr(CROCKFORD, pop[i]) == NULL) {
            return ESP_ERR_INVALID_ARG;   /* not an uppercase Crockford symbol */
        }
    }
    return ESP_OK;
}
