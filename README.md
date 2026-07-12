# IKEA OBEGRÄNSAD - Home Assistant 氣象監測時鐘版

![預覽圖](images/IKEA-OBEGRÄNSAD-picture.jpg)

這是基於 [IKEA OBEGRÄNSAD LED 燈板](https://www.ikea.com.hk/zh/products/luminaires/wall-lamps-and-wall-spotlights/obegransad-art-80526254) 進行改裝的 ESP32 專案。本專案針對個人使用環境進行深度在地化優化，利用 Home Assistant 整合各種感測器、Google Weather 提供的數據進行資訊顯示。

## 🖼 畫面顯示範例

本面板除了時鐘面板以外，另外設計了氣溫與天氣面板，使三種面板自動輪撥切換。並且設計了日間模式與夜間模式，以符合日常生活的資訊需求：

### ☀ 日間模式 (Day Mode)

![日間模式範例](images/IKEA-OBEGRÄNSAD-readme-pattern-day.jpg)

日間模式會顯示當天的最高溫與最低溫預報，並且在即時天氣圖示中，根據當時的天氣預報改變顯示數值，當為晴天、晴時多雨時，數字顯示紫外線指數。而當天氣為雷、雨天或陰雨天，且降雨機率高於 30％ 時，顯示降雨機率。

### 🌙 夜間模式 (Night Mode)

![夜間模式範例](images/IKEA-OBEGRÄNSAD-readme-pattern-night.jpg)

夜間模式僅顯示今明兩天氣溫平均值的差異以及明日天氣預報圖示。


## 🌟 核心功能
* **在地化氣象顯示**：
    * 整合 `Google Weather` 即時顯示本地天氣圖示。
    * 顯示今日最高溫與最低溫預報，並使用自定義**向上/向下三角形**增強視覺辨識。
    * 顯示今日即時天氣圖示，並且依據天氣類型顯示 UV 或是降雨機率。
    * 夜間時顯示明日預報，使用「明日平均氣溫趨勢」面板，顯示升溫/降溫/持平預報。
* **輪播節奏**：
    * **時鐘**顯示 46 秒 (0-45秒)。
    * **溫度面板**顯示 7 秒 (46-52秒)：
        * 白天顯示今日高/低溫。
        * 夜間顯示明日氣溫趨勢（夜間時段可在 Web 介面調整，支援跨日例如 19~6）。
    * **天氣圖示面板**顯示 7 秒 (53-59秒)：
        * 白天顯示當前天氣圖示與 UV 指數，如果目前的天氣代碼是 0 (陰天)、4 (雨天) 或 1 (雷雨)，且降雨機率高於 30% 時，顯示降雨機率。
        * 夜間顯示明日天氣預報圖示。     

## 🛠 硬體需求
* **IKEA OBEGRÄNSAD** LED 點陣燈板 (16x16)。
* **ESP32** 開發板 (取代 IKEA 原廠控制器)。
* 硬體改裝與接線配置請參考[原專案](https://github.com/PiotrMachowski/ikea-led-obegraensad)
* 接入後的硬體控制由 [IKEA OBEGRÄNSAD LED 整合](https://github.com/lucaam/ikea-obegransad-led)進行控制
* **⚠️ 供電重要提醒**：
    * 請務必使用 **12W (5V/2.4A)** 以上的優質供電器（推薦使用 iPad 充電頭）。
    * **原因**：ESP32 在啟動 WiFi 抓取資料時瞬時電流較大，若使用一般 5W (5V/1A) 供電器會導致電壓掉落，造成系統反覆重啟。

## 🏠 Home Assistant 設定 (YAML)

由於 Home Assistant 的氣象實體不直接開放預報屬性，需在 `configuration.yaml` 中加入「觸發型樣板感測器」來提取最高/最低溫：

```yaml
template:
  - trigger:
      - platform: time_pattern
        minutes: "/15"
      - platform: homeassistant
        event: start
      - platform: event
        event_type: "force_update_weather"
    action:
      - service: weather.get_forecasts
        data:
          type: daily
        target:
          entity_id: weather.wo_de_jia
        response_variable: google_daily_data
    sensor:
      # 1. 今日最高溫預報
      - name: "Google Weather Max Temp"
        unique_id: google_today_max_temp
        unit_of_measurement: "°C"
        state: >
          {% set entity = 'weather.wo_de_jia' %}
          {% if google_daily_data is defined and google_daily_data[entity] is defined %}
            {{ google_daily_data[entity].forecast[0].temperature | float(0) }}
          {% else %} unknown {% endif %}

      # 2. 今日最低溫預報
      - name: "Google Weather Min Temp"
        unique_id: google_today_min_temp
        unit_of_measurement: "°C"
        state: >
          {% set entity = 'weather.wo_de_jia' %}
          {% if google_daily_data is defined and google_daily_data[entity] is defined %}
            {{ google_daily_data[entity].forecast[0].templow | float(0) }}
          {% else %} unknown {% endif %}

      # 3. 明天氣溫趨勢 (平均值比較)
      - name: "Tomorrow Temp Trend"
        unique_id: tomorrow_temp_trend_google
        unit_of_measurement: "°C"
        state: >
          {% set entity = 'weather.wo_de_jia' %}
          {% if google_daily_data is defined and google_daily_data[entity] is defined %}
            {% set fc = google_daily_data[entity].forecast %}
            {% if fc | count >= 2 %}
              {% set today_avg = (fc[0].temperature + fc[0].templow) / 2 %}
              {% set tomorrow_avg = (fc[1].temperature + fc[1].templow) / 2 %}
              {{ (tomorrow_avg - today_avg) | round(1) }}
            {% else %} 0 {% endif %}
          {% else %} unknown {% endif %}
        attributes:
          advice: >
            {% set diff = states('sensor.tomorrow_qi_wen_qu_shi') | float(0) %}
            {% if diff > 1.5 %} "明天平均氣溫上升 {{ diff }}°C，會明顯變熱。"
            {% elif diff < -1.5 %} "明天平均氣溫下降 {{ diff | abs }}°C，會明顯轉冷。"
            {% else %} "氣溫波動不大，體感與今日相仿。"
            {% endif %}
          today_avg: >
            {% set entity = 'weather.wo_de_jia' %}
            {% if google_daily_data is defined and google_daily_data[entity] is defined %}
              {% set fc = google_daily_data[entity].forecast %}
              {{ (fc[0].temperature + fc[0].templow) / 2 }}
            {% else %} N/A {% endif %}

      # 4 輸出給 ESP32 的代碼 (今日)
      - name: "esp32_current_weather_code"
        unique_id: esp32_current_weather_code
        state: >
          {% set cond = states('sensor.wo_de_jia_weather_condition') %}
          {% set mapper = {
            'sunny': 2, 'clear-night': 2, '晴': 2,
            'cloudy': 0, 'overcast': 0, '陰': 0,
            'partlycloudy': 3, '多雲': 3, '多雲時陰': 0, '多雲時晴': 3,
            'rainy': 4, 'pouring': 4, 'rain': 4, '雨': 4, '陣雨': 4,
            'lightning': 1, 'lightning-rainy': 1, '雷雨': 1,
            'windy': 5, '強風': 5
          } %}
          {% if cond in mapper %} {{ mapper[cond] }}
          {% elif '雨' in cond %} 4
          {% elif '陰' in cond or '雲' in cond %} 0
          {% else %} 3 {% endif %}

      # 5 輸出給 ESP32 的代碼 (明日)
      - name: "esp32_tomorrow_weather_code"
        unique_id: esp32_tomorrow_weather_code
        state: >
          {% set entity = 'weather.wo_de_jia' %}
          {% if google_daily_data is defined and google_daily_data[entity] is defined %}
            {% set cond = google_daily_data[entity].forecast[1].condition %}
            {% set mapper = {
              'sunny': 2, 'clear-night': 2, 'cloudy': 0, 'overcast': 0,
              'partlycloudy': 3, 'rainy': 4, 'pouring': 4, 'lightning': 1, 
              'lightning-rainy': 1, 'windy': 5
            } %}
            {% if cond in mapper %} {{ mapper[cond] }}
            {% else %} 3 {% endif %}
          {% else %}
            3
          {% endif %}
```

## 👨‍💻 安裝與編譯

複製本專案至本地。

使用 Visual Studio Code + PlatformIO 開啟。

在專案根目錄建立 `secrets.ini`，填入 Home Assistant 連線資訊（`platformio.ini` 會透過 `extra_configs = secrets.ini` 載入）：

```ini
[tokens]
ha_token = "YOUR_LONG_LIVED_TOKEN"
ha_server = "http://YOUR_HA_IP:8123"
```

`platformio.ini` 會將這些值注入 `HA_TOKEN` 與 `HA_SERVER`，`HAForecastClockPlugin.cpp` 內的 `haToken`、`haServer` 會自動使用它們。請確認 `secrets.ini` 已加入 `.gitignore`，避免金鑰外洩。

如果你的 Home Assistant 實體名稱和本專案預設不同，請在 `src/plugins/HAForecastClockPlugin.cpp` 的 `entities[]` 清單中改成你的 entity ID。預設對照如下：

- `sensor.esp32_current_weather_code`: Google Weather 即時天氣代碼，用來決定天氣圖示（由 HA 自動化/模板產生）
- `sensor.esp32_tomorrow_weather_code`：Google Weather 明日天氣代碼，用來決定天氣圖示（由 HA 自動化/模板產生）
- `sensor.wo_de_jia_precipitation_probability`: Google Weather 即時降雨機率
- `sensor.wo_de_jia_uv_index`: Google Weather 紫外線指數
- `sensor.jin_ri_zui_gao_wen_yu_bao`：今日最高溫（由 HA 自動化/模板產生）
- `sensor.jin_ri_zui_di_wen_yu_bao`：今日最低溫（由 HA 自動化/模板產生）
- `sensor.ming_ri_qi_wen_qu_shi`：明日氣溫趨勢（由 HA 自動化/模板產生）

## 🌐 Web 設定頁面

燒錄完成後，可用瀏覽器進入裝置 IP 進行設定：

- `http://<裝置IP>/cityclock`
  - 城市時鐘設定（目前僅支援台北）。
  - 可調整夜間時段 `nightStart/nightEnd`，控制 Forecast 夜間顯示明日趨勢與天氣圖示切換。

- `http://<裝置IP>/forecast`
  - Forecast 設定頁（已中文化，且僅保留台北）。

使用 PlatformIO 編譯並燒錄至 ESP32。

## 📂 專案結構
* `src/signs.cpp`: 定義與繪製天氣圖示
* `src/weather_animation.cpp`：定義動態天氣圖示的繪製邏輯（陰天、雨天、雷雨等動畫）。
* `src/plugins/WeatherIcon.cpp`：專用的 WeatherIcon 面板插件，用於循環預覽所有動態天氣圖示。
* `include/plugins/HAForecastClockPlugin.h`：插件標頭檔，定義類別結構與全域變數（如 `myBrightness`）。
* `src/plugins/HAForecastClockPlugin.cpp`：核心實作，包含 HA 資料抓取、圖形繪製與時間輪播邏輯。

## ☁️ 天氣圖示 (Index 0 - 7)
索引 (Index),描述 (根據註釋/代碼),建議對應的天氣狀態
0,Cloudy (陰天),陰天、多雲時陰
1,Thunderstorm (雷雨),雷雨、強降雨
2,Clear (晴天),晴朗 (白天)
3,Mostly/Partly Cloudy (多雲),多雲、晴時多雲
4,Rain (下雨),雨天、陣雨
5,Snow (下雪),下雪 
6,Fog (霧),霧、霾
7,Moon (晴朗夜晚),晴朗 (夜晚)

## 📝 版本紀錄

v1.6.0 - 移除部分動畫面板，新增 `HAInhouseCO2` Plugin，用以顯示室內 CO2 警報數值。當 CO2 超過 1000 時增加閃爍警示功能，並移除數值間的分隔線。

v1.5.2 - 全面更替資訊來源為 Google Weather 作為資訊來源。

v1.5.1 - 新增 Google Weather 作為資訊來源，優化資訊顯示正確度。

v1.5 - 新增 `weather_animation` 動態圖示支援，並加入 `WeatherIcon` 預覽面板。優化天氣圖示的視覺動態效果。

v1.4 - 調整輪播節奏（46s/7s/7s），統一所有面板數字字型為系統內建字型 (`fonts[1]`) 以維持視覺一致性，優化降雨機率邏輯與 UV 指數切換邏輯。

v1.3 - 將趨勢箭頭改為高度 4px 正三角形，並優化天氣圖標位置。新增 OpenUV 整合數據顯示。

v1.2 - 加入日夜區分畫面，優化最高/最低溫箭頭視覺。

v1.1 - 加入個人濕度感測器整合，修正不同頁面亮度問題。

v1.0 - 移除原始專案透過 ESP32 直接擷取雲端資訊（Open-Meteo）的依賴，整合 Home Assistant 本地數據，完成基礎輪播架構。