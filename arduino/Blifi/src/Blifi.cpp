/**
 * @file Blifi.cpp
 * @brief Implementation of the Arduino Blifi wrapper (see Blifi.h).
 */
#include "Blifi.h"

extern "C" {
#include "nvs_flash.h"
}

BlifiClass Blifi;

bool BlifiClass::begin() { return beginWith(nullptr); }
bool BlifiClass::begin(const char *deviceName) { return beginWith(deviceName); }

bool BlifiClass::beginWith(const char *deviceName) {
  if (_started) return true;

  // The Arduino core normally initialises NVS already; be defensive.
  esp_err_t nerr = nvs_flash_init();
  if (nerr == ESP_ERR_NVS_NO_FREE_PAGES || nerr == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }

  // Register the data-reset trampoline BEFORE blifi_init (init caches the flag).
  blifi_register_data_reset_callback(&BlifiClass::dataResetCb, this);

  blifi_config_t cfg = BLIFI_DEFAULT_CONFIG();
  if (deviceName) cfg.device_name = deviceName;
  if (blifi_init(&cfg) != ESP_OK) return false;

  esp_event_handler_instance_register(BLIFI_EVENT, ESP_EVENT_ANY_ID,
                                      &BlifiClass::eventHandler, this, nullptr);

  if (blifi_start() != ESP_OK) return false;
  _started = true;
  return true;
}

void BlifiClass::onStatusChanged(std::function<void(blifi_status_t)> cb) { _onStatus = cb; }
void BlifiClass::onProvisioned(std::function<void(IPAddress)> cb) { _onProvisioned = cb; }
void BlifiClass::onDataResetRequested(std::function<void()> cb) { _onDataReset = cb; }

bool BlifiClass::wasHardReset() { return blifi_was_hard_reset(); }
bool BlifiClass::isProvisioned() { return blifi_is_provisioned(); }
void BlifiClass::resetCredentials() { blifi_reset_credentials(); }
const char *BlifiClass::pop() { return blifi_get_pop(); }
const char *BlifiClass::statusString(blifi_status_t status) { return blifi_status_str(status); }

void BlifiClass::eventHandler(void *arg, esp_event_base_t base, int32_t id, void *data) {
  (void)base;
  BlifiClass *self = static_cast<BlifiClass *>(arg);
  const blifi_event_data_t *e = static_cast<const blifi_event_data_t *>(data);
  if (e && self->_onStatus) self->_onStatus(e->status);
  if (id == BLIFI_EVENT_WIFI_CONNECTED && e && self->_onProvisioned) {
    self->_onProvisioned(IPAddress(e->ip.addr));
  }
}

void BlifiClass::dataResetCb(void *arg) {
  BlifiClass *self = static_cast<BlifiClass *>(arg);
  if (self->_onDataReset) self->_onDataReset();
}
