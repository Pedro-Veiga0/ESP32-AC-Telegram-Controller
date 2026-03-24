#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Appliance/AirConditioner/AirConditioner.h>

#define RXD2 16
#define TXD2 17

const char* ssid     = "your_wifi_ssid";
const char* password = "your_wifi_password";
const char* botToken = "your_telegram_bot_token";
const char* chatID   = "your_telegram_chat_id";

bool acPowerState = false;
bool ledState = true;

WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);
unsigned long lastCheck = 0;
unsigned long lastHeapLog = 0;

using namespace dudanov::midea::ac;
HardwareSerial acSerial(2);
AirConditioner mideaAC;

// ─── Helpers ──────────────────────────────────────────────────────────────────
void applyMode(Mode mode, float temp) {
  acPowerState = true;
  mideaAC.setPowerState(true);
  Control ctrl;
  ctrl.mode = mode;
  ctrl.targetTemp = temp;
  mideaAC.control(ctrl);
}
void applyFanSpeed(FanMode speed)    { Control ctrl; ctrl.fanMode   = speed;  mideaAC.control(ctrl); }
void applySwingMode(SwingMode swing) { Control ctrl; ctrl.swingMode = swing;  mideaAC.control(ctrl); }
void applyPreset(Preset preset)      { Control ctrl; ctrl.preset    = preset; mideaAC.control(ctrl); }

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  acSerial.begin(9600, SERIAL_8N1, RXD2, TXD2);
  mideaAC.setStream(&acSerial);
  mideaAC.setBeeper(true);
  mideaAC.setup();

  // ✅ Sync acPowerState whenever the AC reports its real state (remote included)
  mideaAC.addOnStateCallback([](){
    acPowerState = mideaAC.getPowerState();
  });

  client.setInsecure();
  client.setTimeout(15);

  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, IPAddress(8, 8, 8, 8));
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (++attempts > 40) ESP.restart();
  }
  Serial.println("\nWiFi Connected!");

  bot.sendMessage(chatID, "✅ ESP32 online! ");
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  mideaAC.loop(); // ✅ This triggers the state callback when AC responds

  if (millis() - lastHeapLog > 60000) {
    lastHeapLog = millis();
    Serial.println("Heap free: " + String(ESP.getFreeHeap()) +
                   " | Max block: " + String(ESP.getMaxAllocHeap()));
  }

  if (ESP.getMaxAllocHeap() < 25000) {
    Serial.println("Heap fragmented. Rebooting...");
    bot.sendMessage(chatID, "🔄 Heap fragmented, rebooting to recover...", "");
    delay(500);
    ESP.restart();
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost, reconnecting...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      mideaAC.loop();
      delay(500);
      attempts++;
    }
    if (WiFi.status() != WL_CONNECTED) ESP.restart();
    Serial.println("WiFi reconnected!");
  }

  if (millis() - lastCheck > 5000) {
    lastCheck = millis();
    client.stop();

    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      for (int i = 0; i < numNewMessages; i++) {
        mideaAC.loop();

        String text     = bot.messages[i].text;
        String senderID = bot.messages[i].chat_id;

        if (text.length() == 0) continue;

        if (senderID != chatID) {
          bot.sendMessage(senderID, "🚫 Unauthorized user.");
          continue;
        }

        // ── Help ──────────────────────────────────────────────────────────────
        if (text == "/start" || text == "/help") {
          bot.sendMessage(senderID,
            "🕹️ AC Controller Commands\n\n"
            "🌡️ Modes\n"
            "❄️ /cool — Cool mode\n"
            "🔥 /heat — Heat mode\n"
            "💧 /dry — Dry mode\n"
            "🌀 /fan — Fan only\n"
            "🔴 /off — Turn off AC\n\n"
            "🌡️ Temperature\n"
            "⬆️ /tempup — +1°C\n"
            "⬇️ /tempdown — -1°C\n"
            "🎯 /settemp 22 — Set exact temp [16–30°C]\n"
            "📊 /temp — Current temperatures\n\n"
            "💨 Fan Speed\n"
            "/fan_auto /fan20 /fan40\n"
            "/fan60 /fan80 /fan100\n\n"
            "🔄 Swing\n"
            "/swingoff /swingboth\n"
            "/swingvertical /swinghorizontal\n\n"
            "✨ Presets\n"
            "/eco /turbo /sleep /fp /none\n\n"
            "💡 /led — Toggle AC display\n"
            "🔁 /reboot — Reboot ESP32\n"
            "📟 /status — Heap & uptime info\n"
          );
        }

        // ── Modes ─────────────────────────────────────────────────────────────
        else if (text == "/cool") {
          applyMode(Mode::MODE_COOL, mideaAC.getTargetTemp());
          bot.sendMessage(senderID, "❄️ Cooling mode at " + String(mideaAC.getTargetTemp(), 0) + "°C");
        }
        else if (text == "/heat") {
          applyMode(Mode::MODE_HEAT, mideaAC.getTargetTemp());
          bot.sendMessage(senderID, "🔥 Heating mode at " + String(mideaAC.getTargetTemp(), 0) + "°C");
        }
        else if (text == "/dry") {
          applyMode(Mode::MODE_DRY, mideaAC.getTargetTemp());
          bot.sendMessage(senderID, "💧 Dry mode activated");
        }
        else if (text == "/fan") {
          applyMode(Mode::MODE_FAN_ONLY, mideaAC.getTargetTemp());
          bot.sendMessage(senderID, "🌀 Fan only mode activated");
        }

        // ── Temperature ───────────────────────────────────────────────────────
        else if (text == "/tempup") {
          float current = mideaAC.getTargetTemp();
          if (current < 30.0) {
            Control ctrl; ctrl.targetTemp = current + 1.0; mideaAC.control(ctrl);
            bot.sendMessage(senderID, "⬆️ Temperature set to " + String(current + 1.0, 0) + "°C");
          } else {
            bot.sendMessage(senderID, "⚠️ Already at maximum (30°C)");
          }
        }
        else if (text == "/tempdown") {
          float current = mideaAC.getTargetTemp();
          if (current > 16.0) {
            Control ctrl; ctrl.targetTemp = current - 1.0; mideaAC.control(ctrl);
            bot.sendMessage(senderID, "⬇️ Temperature set to " + String(current - 1.0, 0) + "°C");
          } else {
            bot.sendMessage(senderID, "⚠️ Already at minimum (16°C)");
          }
        }
        else if (text.startsWith("/settemp")) {
          int spaceIndex = text.indexOf(' ');
          if (spaceIndex == -1) {
            bot.sendMessage(senderID, "⚠️ Usage: /settemp 22\nValid range: 16–30°C");
          } else {
            float newTemp = text.substring(spaceIndex + 1).toFloat();
            if (newTemp < 16.0 || newTemp > 30.0) {
              bot.sendMessage(senderID, "⚠️ Must be between 16–30°C");
            } else {
              Control ctrl; ctrl.targetTemp = newTemp; mideaAC.control(ctrl);
              bot.sendMessage(senderID, "🎯 Temperature set to " + String(newTemp, 0) + "°C");
            }
          }
        }
        else if (text == "/temp") {
          float target  = mideaAC.getTargetTemp();
          float indoor  = mideaAC.getIndoorTemp();
          float outdoor = mideaAC.getOutdoorTemp();
          String status = acPowerState ? "🟢 ON" : "🔴 OFF"; // ✅ Now always accurate
          String msg = "📊 Status: " + status + "\n🎯 Target: " + String(target, 0) + "°C";
          msg += indoor  != 0.0 ? "\n🏠 Indoor:  " + String(indoor,  1) + "°C" : "\n🏠 Indoor:  not available";
          msg += outdoor != 0.0 ? "\n🌤️ Outdoor: " + String(outdoor, 1) + "°C" : "\n🌤️ Outdoor: not available";
          bot.sendMessage(senderID, msg);
        }

        // ── Fan speed ─────────────────────────────────────────────────────────
        else if (text == "/fan_auto") { applyFanSpeed(FanMode::FAN_AUTO);   bot.sendMessage(senderID, "💨 Fan: Auto"); }
        else if (text == "/fan20")    { applyFanSpeed(FanMode::FAN_SILENT);  bot.sendMessage(senderID, "💨 Fan: 20% (Silent)"); }
        else if (text == "/fan40")    { applyFanSpeed(FanMode::FAN_LOW);     bot.sendMessage(senderID, "💨 Fan: 40% (Low)"); }
        else if (text == "/fan60")    { applyFanSpeed(FanMode::FAN_MEDIUM);  bot.sendMessage(senderID, "💨 Fan: 60% (Medium)"); }
        else if (text == "/fan80")    { applyFanSpeed(FanMode::FAN_HIGH);    bot.sendMessage(senderID, "💨 Fan: 80% (High)"); }
        else if (text == "/fan100")   { applyFanSpeed(FanMode::FAN_TURBO);   bot.sendMessage(senderID, "💨 Fan: 100% (Turbo)"); }

        // ── Swing ─────────────────────────────────────────────────────────────
        else if (text == "/swingoff")        { applySwingMode(SwingMode::SWING_OFF);        bot.sendMessage(senderID, "🔒 Swing off"); }
        else if (text == "/swingboth")       { applySwingMode(SwingMode::SWING_BOTH);       bot.sendMessage(senderID, "🔄 Swing: both"); }
        else if (text == "/swingvertical")   { applySwingMode(SwingMode::SWING_VERTICAL);   bot.sendMessage(senderID, "↕️ Swing: vertical"); }
        else if (text == "/swinghorizontal") { applySwingMode(SwingMode::SWING_HORIZONTAL); bot.sendMessage(senderID, "↔️ Swing: horizontal"); }

        // ── Presets ───────────────────────────────────────────────────────────
        else if (text == "/eco")   { applyPreset(Preset::PRESET_ECO);               bot.sendMessage(senderID, "🍃 Eco preset activated"); }
        else if (text == "/turbo") { applyPreset(Preset::PRESET_TURBO);             bot.sendMessage(senderID, "🚀 Turbo preset activated"); }
        else if (text == "/sleep") { applyPreset(Preset::PRESET_SLEEP);             bot.sendMessage(senderID, "😴 Sleep preset activated"); }
        else if (text == "/fp")    { applyPreset(Preset::PRESET_FREEZE_PROTECTION); bot.sendMessage(senderID, "🧊 Freeze protection activated"); }
        else if (text == "/none")  { applyPreset(Preset::PRESET_NONE);              bot.sendMessage(senderID, "✅ Preset cleared"); }

        // ── LED ───────────────────────────────────────────────────────────────
        else if (text == "/led") {
          ledState = !ledState;
          mideaAC.displayToggle();
          bot.sendMessage(senderID, ledState ? "🟢 LED turned ON" : "🔴 LED turned OFF");
        }

        // ── Off ───────────────────────────────────────────────────────────────
        else if (text == "/off") {
          mideaAC.setPowerState(false); // callback will set acPowerState = false
          bot.sendMessage(senderID, "🔴 AC turned OFF");
        }

        // ── Reboot ────────────────────────────────────────────────────────────
        else if (text == "/reboot") {
          bot.sendMessage(senderID, "🔁 Rebooting...");
          delay(500);
          ESP.restart();
        }

        // ── Status ────────────────────────────────────────────────────────────
        else if (text == "/status") {
          unsigned long upSec = millis() / 1000;
          String msg = "📟 System Status\n";
          msg += "🕐 Uptime: " + String(upSec / 3600) + "h " + String((upSec % 3600) / 60) + "m\n";
          msg += "💾 Free heap: "  + String(ESP.getFreeHeap())    + " bytes\n";
          msg += "📦 Max block: "  + String(ESP.getMaxAllocHeap()) + " bytes\n";
          msg += "📶 WiFi RSSI: "  + String(WiFi.RSSI())           + " dBm\n";
          msg += "⚡ AC status: "  + String(acPowerState ? "🟢 ON" : "🔴 OFF");
          bot.sendMessage(senderID, msg);
        }

        else {
          bot.sendMessage(senderID, "❓ Unknown command. Send /help for the list.");
        }
      }
      mideaAC.loop();
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
  }
}