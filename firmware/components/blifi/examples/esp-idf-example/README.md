# blifi ESP-IDF example - Wi-Fi core (Phase 2)

Interactive serial-console demo for the `blifi` component's Wi-Fi manager: scan,
connect, persist credentials to NVS, and watch the retry/backoff state machine -
**no BLE yet**.

## Build & flash

```bash
get_idf                     # activate ESP-IDF (see repo setup)
cd firmware/components/blifi/examples/esp-idf-example
idf.py set-target esp32
idf.py -p <PORT> flash monitor      # e.g. /dev/cu.usbserial-240
```

## Console commands

At the `blifi>` prompt (type `help` for the list):

| Command | Description |
|---------|-------------|
| `scan` | List nearby Wi-Fi networks (SSID, RSSI, channel, auth) |
| `set <ssid> [password]` | Store credentials in NVS |
| `connect [ssid password]` | Connect with the given (or stored) credentials |
| `status` | Show current status and IP |
| `info` | Show stored credentials |
| `erase` | Erase stored credentials |

Status transitions are also logged as they happen (`[event] WIFI_CONNECTING`,
`WIFI_CONNECTED ip=…`, `WIFI_AUTH_ERROR`, etc.).

## Try it

1. `scan` - confirm your network appears.
2. `connect "Your SSID" yourpassword` - watch it reach `WIFI_CONNECTED` with an IP.
3. Press the reset button - it auto-reconnects from NVS on boot.
4. `connect "Your SSID" wrongpassword` - see `WIFI_AUTH_ERROR` (fast-fail).
5. `erase` then reset - it stays idle with no stored credentials.
