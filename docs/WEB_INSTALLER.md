# 網頁燒錄工具

公開入口：<https://maxxtang.github.io/TamaPoke-CYD/>

網站採 GitHub Pages 靜態託管，程式、燒錄函式庫、韌體與截圖均由同站提供。使用桌面 Chrome／Edge 的 Web Serial；HTTPS 或 localhost 才能連接。沒有帳號登入、分析追蹤或裝置資料上傳。

## 安裝與更新

- 手動選擇 ST7789／ILI9341；USB 無法辨識 LCD 型號。
- 連線後核對傳統 ESP32 與 4 MB Flash。其他晶片或容量停止操作。
- 寫入前比對下載長度、SHA-256 與 ESP32 映像 header；由 esptool-js 對實際寫入再做 MD5 校驗。
- 首次安裝：完整 4 MB factory image，偏移 `0x0`。包含全新 NVS，會清除原程式、角色與校正，需明確確認。
- 保留更新：限定相同 CYD 分割表，先讀 `0x8000` 比對，再只寫 app 至 `0x10000`；不使用 erase-all。更新前後各讀完整 `0x9000`／`0x5000` NVS，SHA-256 相同才顯示保留成功。
- 校時：先辨識 CYD `STATUS`，才送電腦 UTC epoch 的 `TIME`，不送角色選擇、清除或放生指令。

若分割表不同，工具會停止更新；不要把其他專案的 app 直接覆蓋。首次改裝前的完整 Flash 備份與還原方式見 [CYD README](cyd/README.md#首次安裝與備份)。

## 重建與測試

```sh
python3 tools/build_web.py
python3 tools/check_web.py
node --test test/web-*.test.mjs
python3 -m http.server 8765 --directory web
```

`build_web.py` 只從正式韌體套件與介面來源複製檔案，產生 `web/catalog.json`。更新 firmware 後必須重跑；`check_web.py` 會核對網站引用、套件逐位元內容、影格來源與第三方檔案雜湊。

Node 測試使用隔離假的 loader，涵蓋錯晶片／容量、損壞下載、分割表不合、同意閘門、app 寫入偏移、禁止整片清除、存檔不符、記憶體中待寫映像被改動，以及 UTC 校時與未知韌體攔截。瀏覽器 UI 與實體板子的結果分開記錄，詳見 `evidence/web-validation.json`。

底層依據 [Espressif esptool-js](https://github.com/espressif/esptool-js/tree/v0.6.0)，網站依據 [GitHub Pages 自訂 workflow](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages) 部署。
