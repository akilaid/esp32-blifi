/**
 * @file blifi_selftest.h
 * @brief On-device self-tests for the blifi crypto and framing primitives.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Run known-answer and round-trip self-tests (X25519, HKDF, AES-GCM,
 *        session encrypt/decrypt, frame chunk/reassemble). Logs each result.
 * @return Number of failed checks (0 = all passed).
 */
int blifi_selftest(void);

#ifdef __cplusplus
}
#endif
