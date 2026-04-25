<!-- markdownlint-disable MD024 -->

# IKEA OBEGRÄNSAD LED Wall Lamp — Extended Firmware

Alternative firmware for the IKEA OBEGRÄNSAD LED Wall Lamp with **44 plugins**, city clock, weather forecast, scrolling marquee, and more.

> This project is based on the excellent work by [ph1p/ikea-led-obegraensad](https://github.com/ph1p/ikea-led-obegraensad). The original project provides the core framework — plugin system, web GUI, OTA updates, and hardware abstraction. I added a number of my own plugins and features on top because I wanted them sooner rather than later. This is not a competition — just a personal fork with extra stuff. All credit for the foundation goes to the original author and contributors.

> **Disclaimer**: Use at your own risk. Modifying the lamp may void its warranty and could damage the device if done improperly.

---

## What's Different from the Original

- **19 additional plugins**: animations, games, character animations, clocks with weather
- **City Clock** with automatic timezone, weather display, and night mode detection for multiple cities
- **Weather Forecast** showing tomorrow's conditions and temperatures for a selectable city
- **Scrolling Marquee** with Latin + Cyrillic character support and web control
- **Espoo Clock** with weather from Open-Meteo API
- **Per-pixel brightness gradients** in sprite-based plugins (Batman, Mario, Goose)
- **Scheduler stability fixes** (heap fragmentation from repeated SSL allocations)
- **Various bug fixes**: Bresenham line drawing, NTP pointer lifetime, cross-core race conditions, NVS handle leaks

---

## Table of Contents

- [Hardware](#hardware)
- [Software Setup](#software-setup)
- [All Plugins](#all-plugins)
- [Custom Plugins in Detail](#custom-plugins-in-detail)
- [Web UI Pages](#web-ui-pages)
- [HTTP API Reference](#http-api-reference)
- [Configuration](#configuration)
- [OTA Updates](#ota-updates)
- [DDP Protocol](#ddp-display-data-protocol)
- [Home Assistant](#home-assistant-integration)
- [Plugin Development](#plugin-development)
- [Architecture](#architecture)
- [Troubleshooting](#troubleshooting)

---

## Hardware

**Display**: 16×16 LED matrix (256 pixels), per-pixel brightness 0–255

**Supported boards**:
- ESP32 Dev Board (recommended)
- ESP32-C3, ESP32-S3 (XIAO)
- ESP8266 NodeMCU / D1 Mini (limited: no per-pixel brightness with storage enabled)

### Pin Configuration

| LCD Pin | ESP32 | ESP8266 |
|---------|-------|---------|
| GND | GND | GND |
| VCC | 5V | VIN |
| EN (PIN_ENABLE) | GPIO26 | GPIO16 |
| IN (PIN_DATA) | GPIO27 | GPIO13 |
| CLK (PIN_CLOCK) | GPIO14 | GPIO14 |
| CLA (PIN_LATCH) | GPIO12 | GPIO0 |
| BUTTON | GPIO16 | GPIO2 |

Pins are configured in `include/constants.h`. For full wiring diagrams and lamp disassembly instructions, see the [original project](https://github.com/ph1p/ikea-led-obegraensad#hardware-setup).

---

## Software Setup

### Prerequisites

- [Visual Studio Code](https://code.visualstudio.com/) with [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) extension
- USB cable for initial flash

### Build & Upload

```bash
git clone <this-repo-url>
cd ikea-led-obegraensad
```

1. Open the project folder in VS Code — PlatformIO will auto-detect `platformio.ini` and install dependencies
2. Copy `include/secrets.h` and edit OTA credentials if needed (defaults work fine for ESP32 with WiFi Manager)
3. Click **PlatformIO: Build** (checkmark icon in bottom toolbar)
4. Click **PlatformIO: Upload** to flash via USB

### WiFi Configuration (ESP32)

Uses [WiFiManager](https://github.com/tzapu/WiFiManager):

1. On first boot, the device creates a WiFi network named `IKEA`
2. Connect to it from your phone/laptop
3. A captive portal lets you pick your home WiFi and enter the password
4. Device reboots and connects — find its IP in your router or serial monitor

The AP name can be changed via `WIFI_MANAGER_SSID` in `include/constants.h`.

---

## All Plugins

44 active plugins in total. Plugins marked with **★** are custom additions.

### Animations & Visual Effects

| # | Plugin | Description |
|---|--------|-------------|
| 1 | Draw | Live drawing canvas via web GUI |
| 2 | Game of Life | Conway's cellular automaton |
| 3 | Stars | Twinkling starfield |
| 4 | Lines | Animated line patterns |
| 5 | Circle | Expanding/contracting circles |
| 6 | Rain | Falling rain drops |
| 7 | Matrix Rain | Matrix-style green code rain |
| 8 | Firework | Firework explosions |
| 9 | Blob | Organic blob movement |
| 10 | Spiral | Spiral patterns |
| 11 | Wave | Sine wave animation |
| 12 | Checkerboard | Animated checkerboard |
| 13 | Radar | Radar sweep effect |
| 14 | Bubbles | Floating bubble particles |
| 15 | Comet | Comet trail across the screen |
| 16 | Fireflies | Glowing firefly particles |
| 17 | Meteor Shower | Multiple falling meteors |
| 18 | Scanlines | Horizontal scan effect |
| 19 | Sparkle Field | Random sparkle particles |
| 20 | Wave Bars | Vertical wave bar animation |
| 21 | **★ Plasma** | Classic plasma color cycling |
| 22 | **★ Perlin Noise** | Smooth animated noise |
| 23 | **★ Droplet** | Water droplet ripple effect |
| 24 | **★ Flocking** | Boid flocking simulation |
| 25 | **★ Sand** | Falling sand physics |
| 26 | **★ Maze** | Procedural maze generation & solve |
| 27 | **★ Heartbeat** | Pulsing heart with double-beat rhythm |
| 28 | **★ Lava Lamp** | Lava lamp blob simulation |
| 29 | **★ Rotating Cube** | 3D wireframe cube rotation |
| 30 | **★ Spectrum** | Audio spectrum visualizer |
| 31 | **★ DNA Helix** | Double helix animation |

### Games

| # | Plugin | Description |
|---|--------|-------------|
| 32 | Breakout | Classic brick breaker |
| 33 | Snake | Snake game |
| 34 | **★ Tetris** | Tetris with web controls |

### Character Animations

| # | Plugin | Description |
|---|--------|-------------|
| 35 | **★ Mario** | Pixel art Mario animation |
| 36 | **★ Batman** | Batman with bat-signal, drop-in, cape flutter |
| 37 | **★ Goose** | Untitled Goose — walks, honks, runs away |

### Clocks & Weather

| # | Plugin | Description |
|---|--------|-------------|
| 38 | **★ Espoo Clock** | Time + weather for Espoo via Open-Meteo |
| 39 | **★ City Clock** | Multi-city clock with timezone/weather/night mode |
| 40 | **★ Forecast** | Tomorrow's weather forecast for a selectable city |

### Text & Communication

| # | Plugin | Description |
|---|--------|-------------|
| 41 | **★ Marquee** | Scrolling text with Latin + Cyrillic support |

### Network & Integration

| # | Plugin | Description |
|---|--------|-------------|
| 42 | Animation | Custom animations from web UI creator |
| 43 | DDP | Display Data Protocol (UDP pixel control) |
| 44 | ArtNet | ArtNet DMX protocol support |

---

## Custom Plugins in Detail

### City Clock ★

Displays time, date, and weather for 4 configurable cities:
- **Helsinki** (UTC+2/+3) — default
- **Omsk** (UTC+6)
- **Berlin** (UTC+1/+2)
- **Moscow** (UTC+3)

Features:
- Automatic timezone with DST handling
- Weather from [wttr.in](https://wttr.in) (updates every 10 minutes)
- Weather icons: sun, clouds, rain, snow, thunderstorm, fog, moon (night)
- Night mode detection with month-based sunrise/sunset tables for ~55°N
- Persistent city selection via NVS (survives reboots and scheduler cycling)
- Web UI at `/cityclock` for city selection

### Weather Forecast ★

Displays tomorrow's weather forecast for a selectable city:
- **Espoo** — default
- **Helsinki**
- **St. Petersburg**
- **Berlin**

Features:
- Weather data from [Open-Meteo API](https://open-meteo.com/) (free, no API key)
- Updates every 30 minutes, 60-second retry on failure
- 3-phase display cycle: city name scroll → weather icon → max/min temperatures
- Up/down arrows indicate high and low temps with degree symbols
- Persistent city selection via NVS (works with scheduler)
- Web UI at `/forecast` for city selection

### Marquee ★

Scrolling text display with web control:
- Full Latin character set from built-in 5×7 font
- Custom Cyrillic glyphs (А–Я + Ё) — 33 characters hand-drawn as 5×7 bitmaps
- UTF-8 decoding for seamless mixing of Latin and Cyrillic text
- Adjustable scroll speed (10–200ms per pixel)
- Web UI at `/marquee` with text input and speed slider
- WebSocket control: `{"event":"marquee", "text":"Привет!", "speed":50}`

### Character Animations ★

**Batman**: Multi-phase animation — bat-signal glowing over a city skyline, Batman drops in with acceleration + bounce, standing pose with cape flutter animation, breathing brightness, and fade out. Two 16×16 sprite frames with brightness gradients (6 levels).

**Mario**: Pixel art Mario with walking animation and brightness shading.

**Goose** (Untitled Goose Game): 5-phase animation cycle — walks in from the left, stands and looks around, HONKS with blinking exclamation mark, runs away to the right, pause, repeat. 4 sprite frames (walk A/B, stand, honk) with 7 brightness gradient levels for body shading, beak, legs, and eye detail.

### Heartbeat ★

A 12×10 pixel heart centered on the display with realistic double-beat pulsing (lub-dub rhythm). The heart scales smoothly using inverse-mapped rendering with `floorf()` for pixel-perfect symmetry. Brightness oscillates between beats with a rest phase.

---

## Web UI Pages

| URL | Description |
|-----|-------------|
| `/` | Main control panel — plugin list, brightness, drawing canvas, scheduler |
| `/marquee` | Marquee text input with speed control |
| `/cityclock` | City Clock city selection with save |
| `/forecast` | Weather Forecast city selection with save |
| `/config` | Device configuration (weather location, NTP server, timezone) |
| `/update` | OTA firmware update (ElegantOTA) |

---

## HTTP API Reference

Base URL: `http://<device-ip>/api`

### Device Info

```http
GET /api/info
```

Returns device state, active plugin, brightness, schedule, and full plugin list.

### Plugin Control

```http
PATCH /api/plugin?id={plugin_id}
```

### Brightness

```http
PATCH /api/brightness?value={0-255}
```

### Display Data

```http
GET /api/data
```

Returns 256-byte array of current pixel brightness values.

### Scrolling Message

```http
GET /api/message?text={text}&graph={csv}&repeat={n}&delay={ms}&id={id}
```

Parameters: `text`, `graph` (comma-separated 0–15 values), `miny`/`maxy`, `repeat` (-1 = infinite), `delay` (ms, default 50), `id`.

```http
GET /api/removemessage?id={id}
```

### Scheduler

```http
POST /api/schedule
# Body: schedule=[{"pluginId":10,"duration":2},{"pluginId":8,"duration":5}]

GET /api/schedule/start
GET /api/schedule/stop
GET /api/schedule/clear
```

### City Clock

```http
GET  /api/cityclock          # Returns {"cityIndex": N}
POST /api/cityclock          # Body: {"cityIndex": N}  — saves without switching plugin
```

### Weather Forecast

```http
GET  /api/forecast           # Returns {"cityIndex": N}
POST /api/forecast           # Body: {"cityIndex": N}  — saves city for scheduler
```

### Configuration

```http
GET  /api/config             # Get current config
POST /api/config             # Update config (JSON body)
POST /api/config/reset       # Reset to defaults
```

### Storage

```http
GET /api/storage/clear       # Clear NVS storage
```

---

## Configuration

Edit `include/constants.h` for build-time settings:

```cpp
#define ENABLE_SERVER           // Comment out to disable WiFi features
#define WEATHER_LOCATION "Espoo"  // Default weather city
#define NTP_SERVER "pool.ntp.org"
#define TZ_INFO "EET-2EEST,M3.5.0/3,M10.5.0/4"  // POSIX timezone
#define WIFI_MANAGER_SSID "IKEA"  // WiFi setup AP name
```

Edit `include/secrets.h` for credentials:

```cpp
#define WIFI_HOSTNAME "ikea-led"
#define OTA_USERNAME "admin"
#define OTA_PASSWORD "ikea-led-wall"

// ESP8266 only (ESP32 uses WiFi Manager):
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
```

Runtime configuration is available at `/config` in the web UI, stored in NVS.

---

## OTA Updates

Powered by [ElegantOTA](https://github.com/ayushsharma82/ElegantOTA).

1. Navigate to `http://<device-ip>/update`
2. Login with OTA credentials (default: `admin` / `ikea-led-wall`)
3. Select `.pio/build/esp32dev/firmware.bin`
4. Wait for upload and automatic reboot

Visual feedback on the LED matrix: **U** = uploading, **R** = rebooting.

For PlatformIO CLI upload, configure `upload_protocol = custom` and `custom_upload_url` in `platformio.ini`.

---

## DDP (Display Data Protocol)

Control the LED matrix in real-time via UDP packets on port 4048.

**Packet**: 10-byte header + 768 bytes RGB data (3 bytes per pixel × 256 pixels).
**Brightness**: `(R + G + B) / 3`

```python
import socket

header = bytearray([0x41, 0x00] + [0x00] * 8)
pixels = bytearray([128, 128, 128] * 256)  # Mid-brightness fill
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.sendto(header + pixels, ("192.168.1.100", 4048))
```

A `ddp.py` helper script is included — see `python3 ddp.py --help`.

ArtNet (DMX over UDP) is also supported via the ArtNet plugin.

---

## Home Assistant Integration

### HACS Integration

Use [this HA integration](https://github.com/HennieLP/ikea-led-obegraensad-Home-Assistant-Integration) for seamless control with WebSocket support.

### REST API

```yaml
# configuration.yaml
rest_command:
  led_brightness_high:
    url: "http://<device-ip>/api/brightness/"
    method: PATCH
    content_type: "application/x-www-form-urlencoded"
    payload: "value=100"
  led_brightness_low:
    url: "http://<device-ip>/api/brightness/"
    method: PATCH
    content_type: "application/x-www-form-urlencoded"
    payload: "value=1"
```

---

## Plugin Development

### 1. Create header (`include/plugins/MyPlugin.h`)

```cpp
#pragma once
#include "PluginManager.h"
#include "timing.h"

class MyPlugin : public Plugin {
private:
    NonBlockingDelay frameTimer;
public:
    void setup() override;
    void loop() override;
    const char *getName() const override;
};
```

### 2. Create implementation (`src/plugins/MyPlugin.cpp`)

```cpp
#include "plugins/MyPlugin.h"
#include "screen.h"

void MyPlugin::setup() {
    Screen.clear();
    frameTimer.forceReady();
}

void MyPlugin::loop() {
    if (!frameTimer.isReady(50))  // 20 FPS
        return;

    Screen.clear();
    Screen.setPixel(8, 8, 1, 255);  // Single bright pixel at center
}

const char *MyPlugin::getName() const {
    return "My Plugin";
}
```

### 3. Register in `src/main.cpp`

```cpp
#include "plugins/MyPlugin.h"
// ...
pluginManager.addPlugin(new MyPlugin());
```

### Key APIs

- `Screen.setPixel(x, y, value, brightness)` — set pixel (0–15 coords, brightness 0–255)
- `Screen.clear()` — clear framebuffer
- `NonBlockingDelay::isReady(ms)` — non-blocking timer (returns true every N ms)
- `NonBlockingDelay::forceReady()` — force timer to fire immediately on next check
- Plugins with WiFi features should be guarded with `#ifdef ENABLE_SERVER`

### Important Notes

- **Never use `delay()`** — it blocks the rendering loop. Use `NonBlockingDelay` from `timing.h`.
- **ESP32 dual-core**: Rendering runs on Core 0, main loop (WiFi/WebSocket) on Core 1. Don't share mutable state without synchronization.
- **PROGMEM**: Store large const arrays in flash with `PROGMEM`, read with `pgm_read_byte()`.
- Plugin objects are created once (`new`) and persist across activations. Member variables survive `teardown()` → `setup()` cycles.

---

## Architecture

```
include/
├── constants.h          # Pins, display size, feature flags
├── config.h             # Runtime configuration
├── PluginManager.h      # Plugin base class & manager
├── screen.h             # LED matrix driver
├── timing.h             # NonBlockingDelay utility
├── secrets.h            # WiFi/OTA credentials (not committed)
└── plugins/             # Plugin headers (43 files)

src/
├── main.cpp             # Entry point, plugin registration
├── screen.cpp           # 16×16 LED rendering with SPI shift registers
├── PluginManager.cpp    # Plugin lifecycle management
├── config.cpp           # NVS-backed configuration
├── asyncwebserver.cpp   # HTTP server & REST API routes
├── websocket.cpp        # WebSocket event handling
├── webgui.cpp           # Embedded web UI (generated from frontend/)
├── scheduler.cpp        # Plugin auto-rotation scheduler
├── signs.cpp            # Font rendering & weather icons
├── messages.cpp         # Scrolling message system
├── storage.cpp          # NVS persistent storage
├── ota.cpp              # OTA update handling
└── plugins/             # Plugin implementations (43 files)

frontend/               # SolidJS web UI (pnpm build → webgui.cpp)
```

**Resource usage** (ESP32, 44 plugins):
- RAM: 16.3% (53 KB / 320 KB)
- Flash: 80.1% (1.52 MB / 1.90 MB)

---

## Troubleshooting

- **Flickering display**: Check soldering points, especially VCC. Ensure adequate power supply.
- **WiFi won't connect**: Hold the button for 3+ seconds to reset WiFi config (opens setup portal).
- **Weather not updating**: Verify internet connectivity. Weather uses HTTPS — the ESP32 needs a working SSL stack. Check serial monitor for error messages.
- **Plugin crashes on scheduler rotation**: If you see task watchdog resets, ensure plugins don't block in `setup()` with heavy network operations.

---

## Credits

- **Original project**: [ph1p/ikea-led-obegraensad](https://github.com/ph1p/ikea-led-obegraensad) — the core framework, web GUI, plugin system, and most of the original plugins
- **Home Assistant integration**: [HennieLP](https://github.com/HennieLP/ikea-led-obegraensad-Home-Assistant-Integration)
- **WiFi Manager**: [tzapu/WiFiManager](https://github.com/tzapu/WiFiManager)
- **OTA**: [ElegantOTA](https://github.com/ayushsharma82/ElegantOTA)

---

## License

See the [original project](https://github.com/ph1p/ikea-led-obegraensad) for license information.
