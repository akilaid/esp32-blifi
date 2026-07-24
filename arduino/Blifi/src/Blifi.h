/**
 * @file Blifi.h
 * @brief Arduino wrapper for the blifi BLE Wi-Fi provisioning component.
 *
 * A thin C++ layer over the `blifi` ESP-IDF component (vendored under src/blifi):
 * it turns the component's esp_event callbacks into Arduino std::function
 * callbacks. No provisioning/crypto/BLE logic is duplicated here.
 *
 * Requires the arduino-esp32 core 3.x. Usage:
 *
 *   #include <Blifi.h>
 *   void setup() {
 *     Serial.begin(115200);
 *     Blifi.onProvisioned([](IPAddress ip){ Serial.println(ip); });
 *     Blifi.begin();
 *   }
 *
 * Note: the reset-pin (bootloader) hard reset is NOT available in the Arduino IDE
 * build — see the README. wasHardReset() therefore returns false here; the
 * software resetCredentials() works everywhere.
 */
#pragma once

#include <Arduino.h>
#include <IPAddress.h>
#include <functional>

extern "C" {
// Angle brackets (not quotes) so the preprocessor skips this file's own
// directory — on case-insensitive filesystems `"blifi.h"` would otherwise
// collide with this wrapper's `Blifi.h`. Resolves to the blifi component's header.
#include <blifi.h>
}

class BlifiClass {
 public:
  /** Initialise + start provisioning. Uses the auto "blifi-XXXX" BLE name. */
  bool begin();
  /** Initialise + start with a custom BLE device name. */
  bool begin(const char *deviceName);

  /** Called on every status change (connecting, connected, errors…). */
  void onStatusChanged(std::function<void(blifi_status_t)> cb);
  /** Called once the device is online, with its IP address. */
  void onProvisioned(std::function<void(IPAddress)> cb);
  /** Called on the boot after a hard reset so you can wipe your own data.
   *  (Fires only under the PlatformIO/IDF build — see the README.) */
  void onDataResetRequested(std::function<void()> cb);

  /** True if this boot followed a reset-pin hard reset (always false in the
   *  Arduino IDE build; functional under PlatformIO/IDF). */
  bool wasHardReset();
  /** True if Wi-Fi credentials are stored. */
  bool isProvisioned();
  /** Erase stored credentials and return to provisioning (software reset). */
  void resetCredentials();
  /** The device's Proof-of-Possession string (show it to the user). */
  const char *pop();
  /** Human-readable name for a status code (for logs). */
  const char *statusString(blifi_status_t status);

 private:
  bool beginWith(const char *deviceName);
  static void eventHandler(void *arg, esp_event_base_t base, int32_t id, void *data);
  static void dataResetCb(void *arg);

  std::function<void(blifi_status_t)> _onStatus;
  std::function<void(IPAddress)> _onProvisioned;
  std::function<void()> _onDataReset;
  bool _started = false;
};

/** Global instance, à la `WiFi` / `Serial`. */
extern BlifiClass Blifi;
