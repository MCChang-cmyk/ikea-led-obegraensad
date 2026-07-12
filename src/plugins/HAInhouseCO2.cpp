#include "plugins/HAInhouseCO2.h"
#include "screen.h"
#include <ArduinoJson.h>

#ifdef ESP32
#include <HTTPClient.h>
#include <WiFi.h>
#endif

static const char *haServer = HA_SERVER;
static const char *haToken = HA_TOKEN;

// sensor_2 在上 (室内), sensor_1 在下 (室外)
static const char *entityTop = "sensor.alpstuga_air_quality_monitor_er_yang_hua_tan_2";
static const char *entityBottom = "sensor.alpstuga_air_quality_monitor_er_yang_hua_tan";

void HAInhouseCO2::fetchHAData() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;

  // 抓取 sensor_2 (上方)
  String url1 = String(haServer) + "/api/states/" + String(entityTop);
  http.begin(client, url1);
  http.addHeader("Authorization", "Bearer " + String(haToken));
  http.setTimeout(3000);
  if (http.GET() == HTTP_CODE_OK) {
    JsonDocument doc;
    deserializeJson(doc, http.getString());
    String state = doc["state"].as<String>();
    if (state != "unknown" && state != "unavailable") {
      co2Sensor2 = state.toFloat();
      hasData = true;
    }
  }
  http.end();
  delay(50);

  // 抓取 sensor_1 (下方)
  String url2 = String(haServer) + "/api/states/" + String(entityBottom);
  http.begin(client, url2);
  http.addHeader("Authorization", "Bearer " + String(haToken));
  http.setTimeout(3000);
  if (http.GET() == HTTP_CODE_OK) {
    JsonDocument doc;
    deserializeJson(doc, http.getString());
    String state = doc["state"].as<String>();
    if (state != "unknown" && state != "unavailable") {
      co2Sensor1 = state.toFloat();
      hasData = true;
    }
  }
  http.end();
}

void HAInhouseCO2::drawCO2Value(int val, int y) {
  if (val < 0) val = 0;
  if (val > 9999) val = 9999;

  int digits[4];
  int numDigits = 0;

  if (val >= 1000) {
    digits[0] = val / 1000;
    digits[1] = (val / 100) % 10;
    digits[2] = (val / 10) % 10;
    digits[3] = val % 10;
    numDigits = 4;
  } else if (val >= 100) {
    digits[0] = val / 100;
    digits[1] = (val / 10) % 10;
    digits[2] = val % 10;
    numDigits = 3;
  } else if (val >= 10) {
    digits[0] = val / 10;
    digits[1] = val % 10;
    numDigits = 2;
  } else {
    digits[0] = val;
    numDigits = 1;
  }

  // 4px spacing (比標準 5px 更緊湊, 4 位數 = 16px 剛好)
  int totalWidth = numDigits * 4;
  int startX = (16 - totalWidth) / 2;

  for (int i = 0; i < numDigits; i++) {
    Screen.drawCharacter(
      startX + (i * 4), y,
      Screen.readBytes(smallNumbers[digits[i]]),
      4, myBrightness
    );
  }
}

void HAInhouseCO2::setup() {
  Screen.clear();
  lastFetch = 0;
  fetchHAData();
}

void HAInhouseCO2::loop() {
  if (millis() - lastFetch > 60000UL) { // 每 1 分鐘更新
    lastFetch = millis();
    fetchHAData();
  }

  if (displayTimer.isReady(500)) {
    Screen.clear();
    bool blinkState = (millis() / 500) % 2 == 0;

    // 上方: sensor_2 (室內)
    if (hasData && co2Sensor2 >= 0) {
      int val = (int)roundf(co2Sensor2);
      if (val < 1000 || blinkState) {
        drawCO2Value(val, 1);
      }
    }

    // 下方: sensor_1 (室外)
    if (hasData && co2Sensor1 >= 0) {
      int val = (int)roundf(co2Sensor1);
      if (val < 1000 || blinkState) {
        drawCO2Value(val, 9);
      }
    }
  }
}

const char *HAInhouseCO2::getName() const { return "HAInhouseCO2"; }
