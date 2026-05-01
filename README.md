# IKEA OBEGRÄNSAD - 台灣新莊氣象與居家監測時鐘版

這是基於 [IKEA OBEGRÄNSAD LED 燈板](https://www.ikea.com.tw/zh/products/decoration/lighting-accessories/obegransad-led-wall-lamp-black-art-10526285) 進行改裝的 ESP32 專案。本專案針對台灣使用環境進行深度在地化優化，整合 Home Assistant 與中央氣象署 (CWA) 數據，並具備居家空氣品質監測與自動警告功能。

## 🌟 核心功能
* **在地化氣象顯示**：
    * 整合 `OpenCWA` (中央氣象署)，即時顯示新莊區天氣圖示。
    * 顯示今日最高溫與最低溫預報，並使用自定義**向上/向下三角形**增強視覺辨識。
    * 支援「明日平均氣溫趨勢」面板（可顯示升溫/降溫/持平）。
    * 日間天氣圖示顯示 UV，使用 8x8 數字字型並上移 2px；夜間改為明日天氣圖示，日夜判斷共用 cityclock Web 的 nightStart/nightEnd 設定。
* **居家環境監測**：
    * 整合 **Alpstuga Air Quality Monitor**，顯示即時室內濕度。
    * **體感溫度**顯示 (Feels Like Temperature)。
* **智慧空氣品質警告 (AQI/CO2)**：
    * 當 **PM2.5 > 35 μg/m³** 或 **CO2 > 1000 ppm** 時，自動切換至驚嘆號 `[!]` 警告畫面並顯示數值。
* **視覺優化**：
    * **統一亮度管理**：所有面板均可透過變數統一調光。
    * **時鐘亮度補償**：針對細線條時鐘字體進行視覺補償，避免看起來比圖示暗。
* **新版輪播節奏**：
    * **時鐘**顯示 46 秒 (0-45秒)。
    * **溫度面板**顯示 7 秒 (46-52秒)：
        * 白天顯示今日高/低溫。
        * 夜間顯示明日氣溫趨勢（夜間時段可在 Web 介面調整，支援跨日例如 19~6）。
    * **天氣圖示面板**顯示 7 秒 (53-59秒)：
        * 白天顯示當前天氣圖示與 UV 指數（或降雨機率）。
        * 夜間顯示明日天氣預報圖示。

## 🛠 硬體需求
* **IKEA OBEGRÄNSAD** LED 點陣燈板 (16x16)。
* **ESP32** 開發板 (取代原廠控制器)。
* **⚠️ 供電重要提醒**：
    * 請務必使用 **12W (5V/2.4A)** 以上的優質供電器（推薦使用 iPad 充電頭）。
    * **原因**：ESP32 在啟動 WiFi 抓取資料時瞬時電流較大，若使用一般 5W (5V/1A) 供電器會導致電壓掉落，造成系統反覆重啟。

## 🏠 Home Assistant 設定 (YAML)

由於新版 Home Assistant 的氣象實體不直接開放預報屬性，需在 `configuration.yaml` 中加入「觸發型樣板感測器」來提取最高/最低溫：

```yaml
template:
  - trigger:
      - platform: time_pattern
        minutes: "/15"
      - platform: homeassistant
        event: start
    action:
      - service: weather.get_forecasts
        data:
          type: hourly
        target:
          entity_id: weather.opencwa_xin_zhuang_qu
        response_variable: hourly_forecast
    sensor:
      - name: "CWA Max Temp"
        unique_id: cwa_max_temp
        unit_of_measurement: "°C"
        state: "{{ hourly_forecast['weather.opencwa_xin_zhuang_qu'].forecast[:8] | map(attribute='temperature') | max }}"
      - name: "CWA Min Temp"
        unique_id: cwa_min_temp
        unit_of_measurement: "°C"
        state: "{{ hourly_forecast['weather.opencwa_xin_zhuang_qu'].forecast[:8] | map(attribute='temperature') | min }}"
      - name: "明日氣溫趨勢"
        unique_id: tomorrow_avg_temp_trend
        unit_of_measurement: "°C"
        state: "{{ (states('sensor.tomorrow_avg_temp') | float(0)) - (states('sensor.today_avg_temp') | float(0)) }}"
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

`platformio.ini` 會將這些值注入 `HA_TOKEN` 與 `HA_SERVER`，`ForecastPlugin.cpp` 內的 `haToken`、`haServer` 會自動使用它們。請確認 `secrets.ini` 已加入 `.gitignore`，避免金鑰外洩。

如果你的 Home Assistant 實體名稱和本專案預設不同，請在 `src/plugins/ForecastPlugin.cpp` 的 `entities[]` 清單中改成你的 entity ID。預設對照如下：

- `sensor.opencwa_xin_zhuang_qu_weather_code`：OpenCWA 新莊區即時天氣代碼，用來決定螢幕上的天氣圖示
- `sensor.opencwa_xin_zhuang_qu_feels_like_temperature`：OpenCWA 新莊區體感溫度
- `sensor.opencwa_xin_zhuang_qu_uv_index`：OpenCWA 新莊區紫外線指數
- `sensor.cwa_max_temp`：今日最高溫（由上方 YAML 範例中的 HA 自動化/模板產生）
- `sensor.cwa_min_temp`：今日最低溫（由上方 YAML 範例中的 HA 自動化/模板產生）
- `sensor.alpstuga_air_quality_monitor_shi_du_2`：室內濕度
- `sensor.alpstuga_air_quality_monitor_pm2_5_2`：室內 PM2.5 濃度
- `sensor.alpstuga_air_quality_monitor_er_yang_hua_tan_2`：室內 CO2 濃度
- `sensor.tomorrow_avg_temp_trend`：明日平均氣溫趨勢（主要趨勢來源）
- `sensor.ming_ri_qi_wen_qu_shi`：明日氣溫趨勢備援來源，當主要趨勢 sensor 不可用時使用
- `sensor.opencwa_xin_zhuang_qu_tomorrow_weather_code`：OpenCWA 新莊區明日天氣代碼，用來決定明日的天氣圖示

## 🌐 Web 設定頁面

燒錄完成後，可用瀏覽器進入裝置 IP（例如 `http://192.168.1.179`）進行設定：

- `http://<裝置IP>/cityclock`
  - 城市時鐘設定（目前僅支援台北）。
  - 可調整夜間時段 `nightStart/nightEnd`，控制 Forecast 夜間顯示明日趨勢與天氣圖示切換。
  - 提供「立即套用」，通常幾秒內生效。

- `http://<裝置IP>/forecast`
  - Forecast 設定頁（已中文化，且僅保留台北）。

編譯並透過 USB 燒錄至 ESP32。

## 📂 專案結構
* `include/plugins/ForecastPlugin.h`：插件標頭檔，定義類別結構與全域變數（如 `myBrightness`）。
* `src/plugins/ForecastPlugin.cpp`：核心實作，包含 HA 資料抓取、圖形繪製與時間輪播邏輯。

## 📝 版本紀錄
v1.4 - 調整輪播節奏（46s/7s/7s），統一所有面板數字字型為系統內建字型 (`fonts[1]`) 以維持視覺一致性，優化降雨機率與 UV 指數切換邏輯。

v1.3 - 將趨勢箭頭改為高度 4px 正三角形，並優化天氣圖標位置。新增日間天氣圖示 UV 指數顯示。

v1.2 - 加入 PM2.5 與 CO2 智慧警告頁面，優化最高/最低溫箭頭視覺。

v1.1 - 加入客廳濕度感測器整合，修正時鐘亮度偏暗問題。

v1.0 - 整合 OpenCWA 新莊氣象數據，完成基礎輪播架構。