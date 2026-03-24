# ESP32 AC Telegram Controller

Control your air conditioner remotely via Telegram, using an ESP32 and a direct UART connection to the AC unit. No cloud app, no proprietary dongle, no subscription — just your phone and a few euros of hardware.

This project was built and tested on a **Bosch Climate 3000i** (a Midea-based unit), and leverages the [MideaUART](https://github.com/dudanov/MideaUART) library by [@dudanov](https://github.com/dudanov) to communicate with the AC over its internal serial interface.

---

## How it works

Modern Midea-based ACs (sold under many brand names including Bosch, Electra, Comfee and others) expose a UART serial interface on their internal control board, originally intended for Wi-Fi dongles. This project connects an ESP32 Dev Kit V1 directly to that interface using a stripped USB-A cable, giving full bidirectional control over the unit — the same way the official dongle would, but locally and without any cloud dependency.

The ESP32 connects to your home Wi-Fi and polls the Telegram Bot API every 5 seconds. When a command arrives, it builds a control frame and sends it to the AC over UART. State changes made with the physical remote (power, mode, temperature) are automatically reflected back via a UART status callback, keeping the bot's state always in sync.

---

## Hardware

- ESP32 development board (any variant with 2 UARTs)
- USB-A cable (old one you can cut up)
- The internal USB port on your AC's control board

The AC's USB port exposes a standard UART interface at 5V. The wiring is:

| USB pin | Signal | ESP32 pin |
|---------|--------|-----------|
| 1 (VCC) | 5V power | VIN or 5V |
| 2 (D-) | TX from AC | GPIO 16 (RX2) |
| 3 (D+) | RX to AC | GPIO 17 (TX2) |
| 4 (GND) | Ground | GND |

> ⚠️ The AC's USB port runs at 5V logic. Most ESP32 boards are 3.3V tolerant on GPIO but are powered fine from the 5V USB line via VIN. Check your specific board's datasheet before wiring. Also, there is a 1kΩ resistor connected to AC TX → ESP RX GPIO 16 (RX2) to protect the ESP32.

---

## Dependencies

Install these through the Arduino Library Manager:

| Library | Purpose |
|---------|---------|
| [MideaUART](https://github.com/dudanov/MideaUART) | UART communication with the AC |
| [UniversalTelegramBot](https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot) | Telegram Bot API client |
| [ArduinoJson](https://arduinojson.org/) | Required by UniversalTelegramBot |

---

## Setup

**1. Create a Telegram bot**

Open Telegram and message [@BotFather](https://t.me/BotFather). Send `/newbot`, follow the prompts, and copy the token it gives you.

To get your chat ID, message [@userinfobot](https://t.me/userinfobot) or type this in browser https://api.telegram.org/bot<YourBOTToken>/getUpdates

**2. Fill in your credentials**

At the top of the sketch:

```cpp
const char* ssid     = "your_wifi_ssid";
const char* password = "your_wifi_password";
const char* botToken = "your_telegram_bot_token";
const char* chatID   = "your_telegram_chat_id";
```

> ⚠️ Never commit real credentials to a public repo. Use `secrets.h` or environment variables if you plan to share your fork.

**3. Flash the ESP32**

Open the sketch in Arduino IDE, select your board under Tools → Board, and upload.

**4. Wire up and power on**

Connect the ESP32 to the AC's USB port as described in the hardware section. The ESP32 can draw power from the AC's USB 5V line so no separate power supply is needed.

---

## Telegram Commands

| Command | Description |
|---------|-------------|
| `/help` | Show all available commands |
| `/cool` | Switch to cooling mode |
| `/heat` | Switch to heating mode |
| `/dry` | Switch to dry mode |
| `/fan` | Switch to fan only mode |
| `/off` | Turn the AC off |
| `/tempup` | Increase target temperature by 1°C |
| `/tempdown` | Decrease target temperature by 1°C |
| `/settemp 22` | Set exact target temperature (16–30°C) |
| `/temp` | Get current target, indoor and outdoor temperatures |
| `/fan_auto` | Set fan speed to auto |
| `/fan20` | Set fan speed to 20% (silent) |
| `/fan40` | Set fan speed to 40% (low) |
| `/fan60` | Set fan speed to 60% (medium) |
| `/fan80` | Set fan speed to 80% (high) |
| `/fan100` | Set fan speed to 100% (turbo) |
| `/swingoff` | Disable swing |
| `/swingboth` | Enable swing in both directions |
| `/swingvertical` | Enable vertical swing only |
| `/swinghorizontal` | Enable horizontal swing only |
| `/eco` | Activate eco preset |
| `/turbo` | Activate turbo preset |
| `/sleep` | Activate sleep preset |
| `/fp` | Activate freeze protection preset |
| `/none` | Clear active preset |
| `/led` | Toggle the AC's display on/off |
| `/reboot` | Reboot the ESP32 |
| `/status` | Show uptime, free heap, WiFi signal and AC state |

---

## Stability features

The sketch has been tested running continuously for multiple days. Several reliability measures are built in:

- **Google DNS** (`8.8.8.8`) is forced at connection time so the bot works from any network, not just the local one
- **SSL connection closed** before each poll cycle to prevent `WiFiClientSecure` memory fragmentation
- **Heap fragmentation check** — reboots automatically when the largest free memory block drops below 25KB (the threshold where SSL handshakes start failing silently)
- **WiFi auto-reconnect** with a fallback reboot if reconnection fails
- **Physical remote sync** — a UART state callback keeps the bot's power state accurate even when the AC is controlled with the physical remote

---

## Credits

- **[@dudanov](https://github.com/dudanov)** — for the MideaUART library that makes all of this possible
- **[@witnessmenow](https://github.com/witnessmenow)** — for the UniversalTelegramBot library
