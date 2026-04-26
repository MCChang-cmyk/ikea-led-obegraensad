#pragma once

#include "PluginManager.h"
#include "timing.h"

struct CityInfo
{
  const char *name;
  const char *timezone;
  float latitude;
  float longitude;
};

class CityClockPlugin : public Plugin
{
private:
  // Cities configuration
  static constexpr CityInfo cities[] = {
      {"Taipei", "CST-8", 25.03, 121.56}};
  static constexpr int cityCount = 1;

  struct tm timeinfo;
  NonBlockingDelay secondTimer;

  int currentCityIndex = 0;
  char savedTz[100] = {0};

  int cachedTemperature = -99;
  int weatherIcon = -1;
  bool hasWeatherData = false;
  unsigned long lastWeatherUpdate = 0;

  int displayMode = 0;
  unsigned long modeStartTime = 0;
  bool colonVisible = true;
  int cityScrollX = -16;

  void loadConfig();
  void fetchWeather();
  void drawClock();
  void drawTemperature();
  int mapWmoCode(int code, bool isNight);
  void switchToCity(int index);
  String getCurrentCityName();

public:
  void setup() override;
  void loop() override;
  void teardown() override;
  void websocketHook(JsonDocument &request) override;
  const char *getName() const override;
};
