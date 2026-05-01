#include <Arduino.h>
#include <BfButton.h>
#include <SPI.h>

#ifdef ESP8266
/* Fix duplicate defs of HTTP_GET, HTTP_POST, ... in ESPAsyncWebServer.h */
#define WEBSERVER_H
#endif

#include <WiFiManager.h>

#ifdef ESP32
#include <ESPmDNS.h>
#endif
#ifdef ESP8266
#include <ESP8266WiFi.h>
#endif

#include "PluginManager.h"
#include "config.h"
#include "scheduler.h"

#include "plugins/ArtNet.h"
#include "plugins/Blob.h"
#include "plugins/BigClockPlugin.h"
#include "plugins/BreakoutPlugin.h"
#include "plugins/BubblesPlugin.h"
#include "plugins/CheckerboardPlugin.h"
#include "plugins/CirclePlugin.h"
#include "plugins/ClockPlugin.h"
#include "plugins/CometPlugin.h"
#include "plugins/DDPPlugin.h"
#include "plugins/DrawPlugin.h"
#include "plugins/FireworkPlugin.h"
#include "plugins/LinesPlugin.h"
#include "plugins/MatrixRainPlugin.h"
#include "plugins/MeteorShowerPlugin.h"
#include "plugins/RadarPlugin.h"
#include "plugins/RainPlugin.h"
#include "plugins/ScanlinesPlugin.h"
#include "plugins/SnakePlugin.h"
#include "plugins/SparkleFieldPlugin.h"
#include "plugins/SpiralPlugin.h"
#include "plugins/StarsPlugin.h"
#include "plugins/TickingClockPlugin.h"
#include "plugins/WaveBarsPlugin.h"
#include "plugins/WavePlugin.h"



#ifdef ENABLE_SERVER
#include "plugins/AnimationPlugin.h"
#include "plugins/WeatherPlugin.h"
#include "plugins/EspooClockPlugin.h"
#include "plugins/CityClockPlugin.h"
#include "plugins/ForecastPlugin.h"
#endif

#include "asyncwebserver.h"
#include "messages.h"
#include "ota.h"
#include "screen.h"
#include "secrets.h"
#include "websocket.h"

BfButton btn(BfButton::STANDALONE_DIGITAL, PIN_BUTTON, true, LOW);

unsigned long previousMillis = 0;
unsigned long interval = 30000;

PluginManager pluginManager;
#ifdef ESP32
DRAM_ATTR volatile SYSTEM_STATUS currentStatus = NONE;
#else
volatile SYSTEM_STATUS currentStatus = NONE;
#endif
WiFiManager wifiManager;

unsigned long lastConnectionAttempt = 0;
const unsigned long connectionInterval = 10000;
unsigned long reconnectionBackoff = 5000;            // Start with 5 seconds
const unsigned long maxReconnectionBackoff = 300000; // Max 5 minutes
uint8_t reconnectionAttempts = 0;

void connectToWiFi()
{
  // if a WiFi setup AP was started, reboot is required to clear routes
  bool wifiWebServerStarted = false;
  wifiManager.setWebServerCallback([&wifiWebServerStarted]() { wifiWebServerStarted = true; });

  wifiManager.setHostname(WIFI_HOSTNAME);

#if defined(IP_ADDRESS) && defined(GWY) && defined(SUBNET) && defined(DNS1)
  auto ip = IPAddress();
  ip.fromString(IP_ADDRESS);

  auto gwy = IPAddress();
  gwy.fromString(GWY);

  auto subnet = IPAddress();
  subnet.fromString(SUBNET);

  auto dns = IPAddress();
  dns.fromString(DNS1);

  wifiManager.setSTAStaticIPConfig(ip, gwy, subnet, dns);
#endif

  wifiManager.setConnectRetries(10);
  wifiManager.setConnectTimeout(10);
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setWiFiAutoReconnect(true);
  wifiManager.autoConnect(WIFI_MANAGER_SSID);

#ifdef ESP32
  if (MDNS.begin(WIFI_HOSTNAME))
  {
    MDNS.addService("http", "tcp", 80);
    MDNS.setInstanceName(WIFI_HOSTNAME);
  }
  else
  {
    Serial.println("Could not start mDNS!");
  }
#endif

  if (wifiWebServerStarted)
  {
    // Reboot required, otherwise wifiManager server interferes with our server
    Serial.println("Done running WiFi Manager webserver - rebooting");
    ESP.restart();
  }

  lastConnectionAttempt = millis();
}

void pressHandler(BfButton *btn, BfButton::press_pattern_t pattern)
{
  switch (pattern)
  {
  case BfButton::SINGLE_PRESS:
    if (currentStatus != LOADING)
    {
      Scheduler.clearSchedule();
      pluginManager.activateNextPlugin();
    }
    break;

  case BfButton::LONG_PRESS:
    if (currentStatus != LOADING)
    {
      pluginManager.activatePersistedPlugin();
    }
    break;
  }
}

void baseSetup()
{
  Serial.begin(115200);

#ifdef ESP32
  // Log reset reason to diagnose unexpected reboots
  esp_reset_reason_t reason = esp_reset_reason();
  Serial.printf("\n[Boot] Reset reason: %d ", reason);
  switch (reason)
  {
  case ESP_RST_POWERON:
    Serial.println("(power-on)");
    break;
  case ESP_RST_SW:
    Serial.println("(software reset)");
    break;
  case ESP_RST_PANIC:
    Serial.println("(PANIC/exception!)");
    break;
  case ESP_RST_INT_WDT:
    Serial.println("(interrupt watchdog!)");
    break;
  case ESP_RST_TASK_WDT:
    Serial.println("(task watchdog!)");
    break;
  case ESP_RST_WDT:
    Serial.println("(other watchdog!)");
    break;
  case ESP_RST_BROWNOUT:
    Serial.println("(brownout!)");
    break;
  default:
    Serial.println("(other)");
    break;
  }
  Serial.printf("[Boot] Free heap: %u, max block: %u\n",
                ESP.getFreeHeap(), ESP.getMaxAllocHeap());
#endif

  pinMode(PIN_LATCH, OUTPUT);
  pinMode(PIN_CLOCK, OUTPUT);
  pinMode(PIN_DATA, OUTPUT);
  pinMode(PIN_ENABLE, OUTPUT);

#if !defined(ESP32) && !defined(ESP8266)
  Screen.setup();
#endif

  // Initialize configuration system (always safe)
  config.begin();

// server
#ifdef ENABLE_SERVER
  connectToWiFi();

  // set time server using config values
  // NTP server name must persist - sntp_setservername stores the pointer, not a copy
  static char ntpServerBuf[100];
  static char tzInfoBuf[100];
  strncpy(ntpServerBuf, config.getNtpServer().c_str(), sizeof(ntpServerBuf) - 1);
  ntpServerBuf[sizeof(ntpServerBuf) - 1] = '\0';
  strncpy(tzInfoBuf, config.getTzInfo().c_str(), sizeof(tzInfoBuf) - 1);
  tzInfoBuf[sizeof(tzInfoBuf) - 1] = '\0';
  configTzTime(tzInfoBuf, ntpServerBuf);

  initOTA(server);
  initWebsocketServer(server);
  initWebServer();
#endif

  pluginManager.addPlugin(new DrawPlugin());
  pluginManager.addPlugin(new BreakoutPlugin());
  pluginManager.addPlugin(new BigClockPlugin());
  pluginManager.addPlugin(new SnakePlugin());
  pluginManager.addPlugin(new StarsPlugin());
  pluginManager.addPlugin(new LinesPlugin());
  pluginManager.addPlugin(new CirclePlugin());
  pluginManager.addPlugin(new ClockPlugin());
  pluginManager.addPlugin(new TickingClockPlugin());
  pluginManager.addPlugin(new RainPlugin());
  pluginManager.addPlugin(new MatrixRainPlugin());
  pluginManager.addPlugin(new FireworkPlugin());
  pluginManager.addPlugin(new BlobPlugin());
  pluginManager.addPlugin(new SpiralPlugin());
  pluginManager.addPlugin(new WavePlugin());
  pluginManager.addPlugin(new CheckerboardPlugin());
  pluginManager.addPlugin(new RadarPlugin());
  pluginManager.addPlugin(new BubblesPlugin());
  pluginManager.addPlugin(new CometPlugin());
  pluginManager.addPlugin(new MeteorShowerPlugin());
  pluginManager.addPlugin(new ScanlinesPlugin());
  pluginManager.addPlugin(new SparkleFieldPlugin());
  pluginManager.addPlugin(new WaveBarsPlugin());

#ifdef ENABLE_SERVER
  // pluginManager.addPlugin(new WeatherPlugin());
  pluginManager.addPlugin(new EspooClockPlugin());
  pluginManager.addPlugin(new CityClockPlugin());
  pluginManager.addPlugin(new ForecastPlugin());
  pluginManager.addPlugin(new AnimationPlugin());
  pluginManager.addPlugin(new DDPPlugin());
  pluginManager.addPlugin(new ArtNetPlugin());
#endif

  Screen.clear();
  pluginManager.init();
  Scheduler.init();

  btn.onPress(pressHandler).onDoublePress(pressHandler).onPressFor(pressHandler, 1000);
}

#ifdef ESP32
TaskHandle_t screenDrawingTaskHandle = NULL;

void screenDrawingTask(void *parameter)
{
  Screen.setup();
  for (;;)
  {
    pluginManager.runActivePlugin();
    vTaskDelay(1);
  }
}

void setup()
{
  baseSetup();
  xTaskCreatePinnedToCore(screenDrawingTask,
                          "screenDrawingTask",
                          16384,
                          NULL,
                          1,
                          &screenDrawingTaskHandle,
                          0);
}
#endif
#ifdef ESP8266
void screenDrawingTask()
{
  Screen.setup();
  pluginManager.runActivePlugin();
  yield();
}

void setup()
{
  baseSetup();
  Scheduler.start();
}
#endif

void loop()
{
  static uint8_t taskCounter = 0;

  btn.read();

#ifdef ENABLE_SERVER
  ElegantOTA.loop();
#endif

#if !defined(ESP32) && !defined(ESP8266)
  pluginManager.runActivePlugin();
#endif

  if (currentStatus == NONE)
  {
    Scheduler.update();

    if ((taskCounter & 0x03) == 0)
    {
      Messages.scrollMessageEveryMinute();
    }
  }

  // Check WiFi less frequently with exponential backoff
  if (WiFi.status() != WL_CONNECTED)
  {
    unsigned long currentMillis = millis();
    if (currentMillis - lastConnectionAttempt >= reconnectionBackoff)
    {
      Serial.println("WiFi disconnected, attempting reconnection...");
      connectToWiFi();

      // Exponential backoff: double the wait time, up to max
      reconnectionAttempts++;
      reconnectionBackoff = min(reconnectionBackoff * 2, maxReconnectionBackoff);
    }
  }
  else
  {
    if (reconnectionAttempts > 0)
    {
      Serial.println("WiFi reconnected successfully");
      reconnectionAttempts = 0;
      reconnectionBackoff = 5000;
    }
  }

  taskCounter++;
  if (taskCounter > 16)
  {
    taskCounter = 0;
  }

#ifdef ENABLE_SERVER
  cleanUpClients();
#endif
#ifdef ESP32
  vTaskDelay(1);
#else
  delay(1);
#endif
}
