# IKEA OBEGRÄNSAD - 台灣新莊氣象與居家監測時鐘版

這是基於 [IKEA OBEGRÄNSAD LED 燈板](https://www.ikea.com.tw/zh/products/decoration/lighting-accessories/obegransad-led-wall-lamp-black-art-10526285) 進行改裝的 ESP32 專案。本專案針對台灣使用環境進行深度在地化優化，整合 Home Assistant 與中央氣象署 (CWA) 數據，並具備居家空氣品質監測與自動警告功能。

## 🌟 核心功能
* **在地化氣象顯示**：
    * 整合 `OpenCWA` (中央氣象署)，即時顯示新莊區天氣圖示。
    * 顯示今日最高溫與最低溫預報，並使用自定義**向上/向下箭頭**增強視覺辨識。
* **居家環境監測**：
    * 整合 **Alpstuga Air Quality Monitor**，顯示即時室內濕度。
    * **體感溫度**顯示 (Feels Like Temperature)。
* **智慧空氣品質警告 (AQI/CO2)**：
    * 當 **PM2.5 > 35 μg/m³** 或 **CO2 > 1000 ppm** 時，自動切換至驚嘆號 `[!]` 警告畫面並顯示數值。
* **視覺優化**：
    * **統一亮度管理**：所有面板均可透過變數統一調光。
    * **時鐘亮度補償**：針對細線條時鐘字體進行視覺補償，避免看起來比圖示暗。

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
```

## 👨‍💻 安裝與編譯

複製本專案至本地。

使用 Visual Studio Code + PlatformIO 開啟。

開啟 src/plugins/ForecastPlugin.cpp：

修改 haServer 為你的 Home Assistant IP。

修改 haToken 為你的長期存取權杖。

(建議) 在 include/ 目錄建立 secrets.h 來管理 Token 並加入 .gitignore 以防洩露。

編譯並透過 USB 燒錄至 ESP32。

## 📂 專案結構
include/plugins/ForecastPlugin.h：插件標頭檔，可在此調整 myBrightness 全域亮度。

src/plugins/ForecastPlugin.cpp：核心邏輯，包含資料抓取、箭頭繪製與輪播邏輯。

## 📝 版本紀錄
v1.2 - 加入 PM2.5 與 CO2 智慧警告頁面，優化最高/最低溫箭頭視覺。

v1.1 - 加入客廳濕度感測器整合，修正時鐘亮度偏暗問題。

v1.0 - 整合 OpenCWA 新莊氣象數據，完成基礎輪播架構。