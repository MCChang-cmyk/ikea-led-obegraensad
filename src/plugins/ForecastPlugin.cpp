#include "plugins/ForecastPlugin.h"
#include <ArduinoJson.h>

#ifdef ESP32
#include <HTTPClient.h>
#include <WiFi.h>
#endif

// ==========================================
// Home Assistant 連線設定
// ==========================================
const char* haServer = "http://192.168.1.30:8123";
const char* haToken  = HA_TOKEN;

// ==========================================
// CWA 天氣代碼轉換
// ==========================================
int ForecastPlugin::mapCwaCode(int code) {
  if (code == 1) return 2;             // 晴
  if (code >= 2 && code <= 3) return 3; // 多雲
  if (code >= 4 && code <= 7) return 0; // 陰
  if (code >= 8 && code <= 22) return 4;// 雨
  return 1; // 雷雨
}

// ==========================================
// 數據抓取 (包含氣象、濕度、PM2.5、CO2)
// ==========================================
void ForecastPlugin::fetchHAData() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClient client;
  HTTPClient http;
  
  const char* entities[] = {
    "sensor.opencwa_xin_zhuang_qu_weather_code",
    "sensor.opencwa_xin_zhuang_qu_feels_like_temperature",
    "sensor.cwa_max_temp",
    "sensor.cwa_min_temp",
    "sensor.alpstuga_air_quality_monitor_shi_du_2",
    "sensor.alpstuga_air_quality_monitor_pm2_5_2",
    "sensor.alpstuga_air_quality_monitor_er_yang_hua_tan_2"
  };

  for (int i = 0; i < 7; i++) {
    String url = String(haServer) + "/api/states/" + String(entities[i]);
    http.begin(client, url);
    http.addHeader("Authorization", "Bearer " + String(haToken));
    http.setTimeout(3000);

    if (http.GET() == HTTP_CODE_OK) {
      JsonDocument doc;
      deserializeJson(doc, http.getString());
      String state = doc["state"].as<String>();
      
      if (state != "unknown" && state != "unavailable") {
        if (i == 0) weatherIcon = mapCwaCode(state.toInt());
        if (i == 1) haFeelsLike = state.toFloat();
        if (i == 2) maxTemp = (int)roundf(state.toFloat());
        if (i == 3) minTemp = (int)roundf(state.toFloat());
        if (i == 4) haHumidity = state.toFloat();
        if (i == 5) haPM25 = state.toFloat();
        if (i == 6) haCO2 = state.toFloat();
        
        // 警告邏輯：PM2.5 > 35 或 CO2 > 1000
        showAQIWarning = (haPM25 > 35.0f || haCO2 > 1000.0f);
        hasData = true;
      }
    }
    http.end();
    delay(100); 
  }
}

// ==========================================
// 繪製數值 (支援 CO2 四位數)
// ==========================================
void ForecastPlugin::drawTempValue(int val, int y) {
  int absV = abs(val);
  if (val >= 1000) { 
    Screen.drawNumbers(2, y, {absV/1000, (absV/100)%10, (absV/10)%10, absV%10});
  } else if (val >= 100 || val <= -100) {
    Screen.drawNumbers(4, y, {absV/100, (absV/10)%10, absV%10});
  } else if (val >= 10 || val <= -10) {
    Screen.drawNumbers(6, y, {absV/10, absV%10});
    if (val < 0) Screen.setPixel(5, y + 2, 1, myBrightness);
  } else {
    if (val < 0) Screen.setPixel(6, y + 2, 1, myBrightness);
    Screen.drawNumbers(8, y, {absV});
  }
}

// ==========================================
// 繪製時鐘 (亮度補償)
// ==========================================
void ForecastPlugin::drawInternalClock() {
  if (getLocalTime(&_internalTime)) {
    if (_lastH != _internalTime.tm_hour || _lastM != _internalTime.tm_min) {
      Screen.clear();
      int clockB = (myBrightness + 25 > 255) ? 255 : myBrightness + 25;
      Screen.drawCharacter(2, 0, Screen.readBytes(fonts[1].data[_internalTime.tm_hour/10]), 8, clockB);
      Screen.drawCharacter(9, 0, Screen.readBytes(fonts[1].data[_internalTime.tm_hour%10]), 8, clockB);
      Screen.drawCharacter(2, 9, Screen.readBytes(fonts[1].data[_internalTime.tm_min/10]), 8, clockB);
      Screen.drawCharacter(9, 9, Screen.readBytes(fonts[1].data[_internalTime.tm_min%10]), 8, clockB);
      _lastH = _internalTime.tm_hour; _lastM = _internalTime.tm_min;
    }
  }
}

void ForecastPlugin::setup() {
  Screen.clear(); displayMode = 1; modeStart = millis();
  _lastH = -1; _lastM = -1; lastFetch = millis();
  fetchHAData();
}

void ForecastPlugin::loop() {
  if (millis() - lastFetch > 600000UL) {
    lastFetch = millis();
    fetchHAData();
  }

  unsigned long elapsed = millis() - modeStart;

  switch (displayMode) {
    case 1: // 天氣圖示
      if (displayTimer.isReady(1000)) { Screen.clear(); if(hasData) Screen.drawWeather(0, 4, weatherIcon); }
      if (elapsed >= 5000) { displayMode = 2; modeStart = millis(); displayTimer.forceReady(); }
      break;

    case 2: // 最高/最低溫 (優化後的箭頭)
      if (displayTimer.isReady(1000)) {
        Screen.clear();
        // --- 最高溫：向上箭頭 ---
        Screen.setPixel(2, 1, 1, myBrightness); // 尖端
        Screen.setPixel(1, 2, 1, myBrightness); Screen.setPixel(2, 2, 1, myBrightness); Screen.setPixel(3, 2, 1, myBrightness); // 橫向加寬
        Screen.setPixel(2, 3, 1, myBrightness); Screen.setPixel(2, 4, 1, myBrightness); // 箭身
        drawTempValue(maxTemp, 2);

        // --- 最低溫：向下箭頭 ---
        Screen.setPixel(2, 9, 1, myBrightness); Screen.setPixel(2, 10, 1, myBrightness); // 箭身
        Screen.setPixel(1, 11, 1, myBrightness); Screen.setPixel(2, 11, 1, myBrightness); Screen.setPixel(3, 11, 1, myBrightness); // 橫向加寬
        Screen.setPixel(2, 12, 1, myBrightness); // 尖端
        drawTempValue(minTemp, 9);
      }
      if (elapsed >= 5000) { displayMode = 3; modeStart = millis(); displayTimer.forceReady(); }
      break;

    case 3: // 體感
      if (displayTimer.isReady(1000)) {
        Screen.clear();
        Screen.setPixel(2, 4, 1, myBrightness); Screen.setPixel(1, 5, 1, myBrightness); Screen.setPixel(3, 5, 1, myBrightness); Screen.setPixel(2, 6, 1, myBrightness);
        drawTempValue((int)roundf(haFeelsLike), 4);
      }
      if (elapsed >= 5000) { displayMode = 4; modeStart = millis(); displayTimer.forceReady(); }
      break;

    case 4: // 濕度
      if (displayTimer.isReady(1000)) {
        Screen.clear();
        Screen.setPixel(2, 4, 1, myBrightness); Screen.setPixel(1, 5, 1, myBrightness); Screen.setPixel(3, 5, 1, myBrightness); Screen.setPixel(2, 6, 1, myBrightness);
        drawTempValue((int)roundf(haHumidity), 4);
      }
      if (elapsed >= 5000) { displayMode = (showAQIWarning) ? 6 : 5; modeStart = millis(); displayTimer.forceReady(); }
      break;

    case 6: // 空氣品質警告
      if (displayTimer.isReady(1000)) {
        Screen.clear();
        // 驚嘆號 [!]
        Screen.setPixel(1, 3, 1, myBrightness); Screen.setPixel(1, 4, 1, myBrightness); Screen.setPixel(1, 5, 1, myBrightness); Screen.setPixel(1, 6, 1, myBrightness); Screen.setPixel(1, 8, 1, myBrightness);
        if (haPM25 > 35.0f) {
           drawTempValue((int)roundf(haPM25), 4);
        } else {
           drawTempValue((int)roundf(haCO2), 4);
        }
      }
      if (elapsed >= 7000) { displayMode = 5; modeStart = millis(); _lastM = -1; }
      break;

    case 5: // 時鐘
      drawInternalClock();
      if (elapsed >= 40000) { displayMode = 1; modeStart = millis(); displayTimer.forceReady(); Screen.clear(); }
      break;
  }
}

void ForecastPlugin::websocketHook(JsonDocument &request) {}
const char *ForecastPlugin::getName() const { return "CWA HA Clock Plus"; }