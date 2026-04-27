#pragma once
#include "PluginManager.h"
#include "timing.h"
#include <time.h>

class ForecastPlugin : public Plugin {
private:
  NonBlockingDelay displayTimer;
  
  // 數據儲存
  int maxTemp = -99;
  int minTemp = -99;
  float tomorrowTrend = 0.0f;
  bool hasTomorrowTrend = false;
  int weatherIcon = -1;
  float haFeelsLike = -99.0f;
  float haHumidity = -99.0f;
  bool hasData = false;
  unsigned long lastFetch = 0;
  float haPM25 = 0.0f;
  float haCO2 = 0.0f;          // 新增 CO2 變數
  bool showAQIWarning = false;
  int tomorrowWeatherIcon = -1;
  bool hasTomorrowWeatherIcon = false;
  
  // 顯示控制
  int displayMode = 1;
  unsigned long modeStart = 0;
  int nightStartHour = 20;
  int nightEndHour = 23;
  unsigned long lastNightConfigLoad = 0;
  
  // 統一亮度設定 (建議 150-200)
  int myBrightness = 180; 

  struct tm _internalTime;
  int _lastM = -1;
  int _lastH = -1;

  // 內部函式
  void fetchHAData();
  void drawInternalClock();
  void drawTempValue(int temp, int y);
  void drawTrendOneDecimal(float value, int y);
  void drawWeatherIcon(bool useTomorrow);
  int mapCwaCode(int code);
  void loadNightWindowConfig();

public:
  void setup() override;
  void loop() override;
  void websocketHook(JsonDocument &request) override;
  const char *getName() const override;
};