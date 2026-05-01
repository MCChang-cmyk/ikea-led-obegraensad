#include "plugins/ForecastPlugin.h"
#include <ArduinoJson.h>

#ifdef ESP32
#include <HTTPClient.h>
#include <WiFi.h>
#endif
#ifdef ENABLE_STORAGE
#include <Preferences.h>
#endif

// ==========================================
// Home Assistant 連線設定
// ==========================================
const char* haServer = HA_SERVER;
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

void ForecastPlugin::loadNightWindowConfig() {
#ifdef ENABLE_STORAGE
  Preferences prefs;
  prefs.begin("cityclock", true);
  int start = prefs.getInt("nightStart", 20);
  int end = prefs.getInt("nightEnd", 23);
  prefs.end();

  if (start >= 0 && start <= 23) nightStartHour = start;
  if (end >= 0 && end <= 23) nightEndHour = end;
#endif
}

// ==========================================
// 數據抓取 (包含氣象、濕度、PM2.5、CO2、明日趨勢)
// ==========================================
void ForecastPlugin::fetchHAData() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClient client;
  HTTPClient http;
  
  const char* entities[] = {
    "sensor.opencwa_xin_zhuang_qu_weather_code",           // OpenCWA 新莊區即時天氣代碼，用於顯示天氣圖示
    "sensor.opencwa_xin_zhuang_qu_feels_like_temperature", // OpenCWA 新莊區體感溫度
    "sensor.openuv_current_uv_index",                      // OpenUV 紫外線指數
    "sensor.cwa_max_temp",                                 // 今日最高溫（由自訂 HA 自動化/模板產生）
    "sensor.cwa_min_temp",                                 // 今日最低溫（由自訂 HA 自動化/模板產生）
    "sensor.alpstuga_air_quality_monitor_shi_du_2",        // 室內濕度
    "sensor.alpstuga_air_quality_monitor_pm2_5_2",         // 室內 PM2.5 濃度
    "sensor.alpstuga_air_quality_monitor_er_yang_hua_tan_2", // 室內 CO2 濃度
    "sensor.tomorrow_avg_temp_trend",                      // 明日平均氣溫趨勢（可選備援）
    "sensor.ming_ri_qi_wen_qu_shi",                        // 明日氣溫趨勢備援，當主要趨勢 sensor 不可用時使用
    "sensor.opencwa_xin_zhuang_qu_tomorrow_weather_code"  // OpenCWA 新莊區明日天氣代碼，用於明日天氣圖示
    "sensor.xin_zhuang_ji_shi_jiang_yu_ji_lu"              //新莊即時降雨機率  
  };

  bool trendUpdated = false;
  float newTrend = tomorrowTrend;
  int entityCount = sizeof(entities) / sizeof(entities[0]);

  for (int i = 0; i < entityCount; i++) {
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
        if (i == 2) haUVIndex = state.toFloat();
        if (i == 3) maxTemp = (int)roundf(state.toFloat());
        if (i == 4) minTemp = (int)roundf(state.toFloat());
        if (i == 5) haHumidity = state.toFloat();
        if (i == 6) haPM25 = state.toFloat();
        if (i == 7) haCO2 = state.toFloat();
        if (i == 8 || i == 9) { newTrend = state.toFloat(); trendUpdated = true; }
        if (i == 10) { tomorrowWeatherIcon = mapCwaCode(state.toInt()); hasTomorrowWeatherIcon = true; }
        if (i == 11) haRainProb = state.toFloat();
        
        // 警告邏輯：PM2.5 > 35 或 CO2 > 1000
        showAQIWarning = (haPM25 > 35.0f || haCO2 > 1000.0f);
        hasData = true;
      }
    }
    http.end();
    delay(100); 
  }

  if (trendUpdated) {
    tomorrowTrend = newTrend;
    hasTomorrowTrend = true;
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

void ForecastPlugin::drawTrendOneDecimal(float value, int y) {
  int rounded = (int)roundf(fabsf(value));
  if (rounded > 99) rounded = 99; // guard for screen width

  if (rounded < 10) {
    // Display single digit, centered (x=5)
    Screen.drawCharacter(5, y, Screen.readBytes(fonts[1].data[rounded]), 8, myBrightness);
  } else {
    // Display two digits (rare case)
    Screen.drawNumbers(0, y, {rounded / 10, rounded % 10});
  }
}

void ForecastPlugin::drawUVIndex(int y) {
  int uv = (int)roundf(haUVIndex);
  if (uv < 0) uv = 0;
  if (uv > 99) uv = 99;

  if (uv < 10) {
    // Display single digit, centered (x=5)
    Screen.drawCharacter(5, y, Screen.readBytes(fonts[1].data[uv]), 8, myBrightness);
  } else {
    Screen.drawCharacter(2, y, Screen.readBytes(fonts[1].data[uv / 10]), 8, myBrightness);
    Screen.drawCharacter(9, y, Screen.readBytes(fonts[1].data[uv % 10]), 8, myBrightness);
  }
}

void ForecastPlugin::drawRainProb(int y) {
  int prob = (int)roundf(haRainProb);
  if (prob < 0) prob = 0;
  if (prob > 99) prob = 99;

  if (prob < 10) {
    // Display single digit, centered (x=5)
    Screen.drawCharacter(5, y, Screen.readBytes(fonts[1].data[prob]), 8, myBrightness);
  } else {
    Screen.drawCharacter(2, y, Screen.readBytes(fonts[1].data[prob / 10]), 8, myBrightness);
    Screen.drawCharacter(9, y, Screen.readBytes(fonts[1].data[prob % 10]), 8, myBrightness);
  }
}

void ForecastPlugin::drawWeatherIcon(bool useTomorrow) {
  int icon = weatherIcon;
  if (useTomorrow && hasTomorrowWeatherIcon) {
    icon = tomorrowWeatherIcon;
  }

  if (icon >= 0 && icon < (int)weatherIcons.size()) {
    int iconY = useTomorrow ? 5 : 0;
    Screen.drawWeather(0, iconY, icon, myBrightness);
    if (!useTomorrow) {
      if ((icon == 0 || icon == 4 || icon == 1) && haRainProb > 30.0f) {
        drawRainProb(8);
      } else {
        drawUVIndex(8);
      }
    }
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
  Screen.clear(); displayMode = 5; modeStart = millis();
  _lastH = -1; _lastM = -1; lastFetch = millis();
  loadNightWindowConfig();
  lastNightConfigLoad = millis();
  fetchHAData();
}

void ForecastPlugin::loop() {
  if (millis() - lastFetch > 600000UL) {
    lastFetch = millis();
    fetchHAData();
  }
  if (millis() - lastNightConfigLoad > 5000UL) {
    loadNightWindowConfig();
    lastNightConfigLoad = millis();
  }

  // 獲取當前秒數，基於整分鐘的秒數決定顯示模式
  int currentSec = 0;
  int currentHour = 0;
  if (getLocalTime(&_internalTime)) {
    currentSec = _internalTime.tm_sec;
    currentHour = _internalTime.tm_hour;
  } else {
    time_t now = time(nullptr);
    struct tm fallbackTime;
    localtime_r(&now, &fallbackTime);
    currentSec = fallbackTime.tm_sec;
    currentHour = fallbackTime.tm_hour;
  }

  bool showNightTrend = false;
  if (nightStartHour == nightEndHour) {
    showNightTrend = (currentHour == nightStartHour);
  } else if (nightStartHour < nightEndHour) {
    showNightTrend = (currentHour >= nightStartHour && currentHour <= nightEndHour);
  } else {
    showNightTrend = (currentHour >= nightStartHour || currentHour <= nightEndHour);
  }

  // 根據秒數設置顯示模式
  if (currentSec < 46) {
    displayMode = 5; // 時鐘 (0-45 秒)
  } else if (currentSec < 53) {
    displayMode = 2; // 溫度面板 (46-52 秒)
  } else {
    displayMode = 3; // 天氣圖示面板 (53-59 秒)
  }

  switch (displayMode) {
    case 2: // 溫度面板：白天高低溫、晚間明日趨勢
      if (displayTimer.isReady(1000)) {
        static unsigned long lastDebugPrint = 0;
        if (millis() - lastDebugPrint > 30000UL) {
          Serial.printf("[ForecastPlugin] sec=%d displayMode=%d\n", currentSec, displayMode);
          lastDebugPrint = millis();
        }
        Screen.clear();
        if (showNightTrend && hasTomorrowTrend) {
          // 晚間顯示明日趨勢（正數：上箭頭下數值；負數：上數值下箭頭）
          auto drawUpArrowLarge = [&]() {
            // Centered up triangle (height 4px), moved down by 1 pixel
            Screen.setPixel(7, 1, 1, myBrightness); // Row 0: 1 pixel
            Screen.setPixel(6, 2, 1, myBrightness); Screen.setPixel(7, 2, 1, myBrightness); Screen.setPixel(8, 2, 1, myBrightness); // Row 1: 3 pixels
            Screen.setPixel(5, 3, 1, myBrightness); Screen.setPixel(6, 3, 1, myBrightness); Screen.setPixel(7, 3, 1, myBrightness); Screen.setPixel(8, 3, 1, myBrightness); Screen.setPixel(9, 3, 1, myBrightness); // Row 2: 5 pixels
            Screen.setPixel(4, 4, 1, myBrightness); Screen.setPixel(5, 4, 1, myBrightness); Screen.setPixel(6, 4, 1, myBrightness); Screen.setPixel(7, 4, 1, myBrightness); Screen.setPixel(8, 4, 1, myBrightness); Screen.setPixel(9, 4, 1, myBrightness); Screen.setPixel(10, 4, 1, myBrightness); // Row 3: 7 pixels
          };
          auto drawDownArrowLarge = [&]() {
            // Centered down triangle (height 4px), moved up by 1 pixel
            Screen.setPixel(4, 10, 1, myBrightness); Screen.setPixel(5, 10, 1, myBrightness); Screen.setPixel(6, 10, 1, myBrightness); Screen.setPixel(7, 10, 1, myBrightness); Screen.setPixel(8, 10, 1, myBrightness); Screen.setPixel(9, 10, 1, myBrightness); Screen.setPixel(10, 10, 1, myBrightness); // Row 0: 7 pixels
            Screen.setPixel(5, 11, 1, myBrightness); Screen.setPixel(6, 11, 1, myBrightness); Screen.setPixel(7, 11, 1, myBrightness); Screen.setPixel(8, 11, 1, myBrightness); Screen.setPixel(9, 11, 1, myBrightness); // Row 1: 5 pixels
            Screen.setPixel(6, 12, 1, myBrightness); Screen.setPixel(7, 12, 1, myBrightness); Screen.setPixel(8, 12, 1, myBrightness); // Row 2: 3 pixels
            Screen.setPixel(7, 13, 1, myBrightness); // Row 3: 1 pixel
          };

          if (tomorrowTrend > 0) {
            // 上方箭頭
            drawUpArrowLarge();
            // 下方顯示絕對值（含小數點一位）
            drawTrendOneDecimal(tomorrowTrend, 8);
          } else if (tomorrowTrend < 0) {
            // 上方顯示絕對值（不顯示負號，含小數點一位）
            drawTrendOneDecimal(tomorrowTrend, 0);
            // 下方箭頭
            drawDownArrowLarge();
          } else {
            // 持平：置中短橫 + 下方顯示 0.0
            Screen.setPixel(5, 3, 1, myBrightness); Screen.setPixel(6, 3, 1, myBrightness); Screen.setPixel(7, 3, 1, myBrightness); Screen.setPixel(8, 3, 1, myBrightness); Screen.setPixel(9, 3, 1, myBrightness);
            drawTrendOneDecimal(tomorrowTrend, 8);
          }
        } else {
          // ==========================================
          // 白天顯示：今日高低溫 (位置微調)
          // ==========================================
          
          // --- 上方：最高溫圖示 (向上實心三角形，位置下移 1px) ---
          // 頂點 (2,2), 底邊 (0,4) 到 (4,4)
          Screen.setPixel(2, 2, 1, myBrightness); 
          Screen.setPixel(1, 3, 1, myBrightness); Screen.setPixel(2, 3, 1, myBrightness); Screen.setPixel(3, 3, 1, myBrightness);
          Screen.setPixel(0, 4, 1, myBrightness); Screen.setPixel(1, 4, 1, myBrightness); Screen.setPixel(2, 4, 1, myBrightness); Screen.setPixel(3, 4, 1, myBrightness); Screen.setPixel(4, 4, 1, myBrightness);
          
          // 數值座標也同步微調，確保與三角形間距適中
          drawTempValue(maxTemp, 2); 

          // --- 下方：最低溫圖示 (向下實心三角形) ---
          // 位置保持不變，維持與上方的對稱感
          Screen.setPixel(0, 11, 1, myBrightness); Screen.setPixel(1, 11, 1, myBrightness); Screen.setPixel(2, 11, 1, myBrightness); Screen.setPixel(3, 11, 1, myBrightness); Screen.setPixel(4, 11, 1, myBrightness);
          Screen.setPixel(1, 12, 1, myBrightness); Screen.setPixel(2, 12, 1, myBrightness); Screen.setPixel(3, 12, 1, myBrightness);
          Screen.setPixel(2, 13, 1, myBrightness);
          
          drawTempValue(minTemp, 10); 
        }
      }
      break;

    case 3: // 天氣圖示面板
      if (displayTimer.isReady(1000)) {
        Screen.clear();
        // 日夜模式使用 cityclock web 介面儲存的 nightStart/nightEnd 設定
        drawWeatherIcon(showNightTrend);
      }
      break;

    case 5: // 時鐘
      drawInternalClock();
      break;
  }
}

void ForecastPlugin::websocketHook(JsonDocument &request) {}
const char *ForecastPlugin::getName() const { return "CWA HA Clock Plus"; }