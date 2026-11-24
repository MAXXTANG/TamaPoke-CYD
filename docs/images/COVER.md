# 封面來源

`cover.png` 以內建 **image_gen** 製作的場景為底，依使用者明確授權，直接合成專案既有的小皮卡丘像素素材。這是專案封面插畫，並非韌體截圖或硬體照片。

基礎封面提示詞：

> Create a wide landscape 16:9 pixel-art cover for an open-source ESP32 handheld pixel game project. Full bleed opaque scene with a crisp uniform square pixel grid, pastel sky blue, sage green grass, layered green hills, a small pixel tree, cream-colored pixel clouds, tiny flowers and a few golden pixel stars. Charming calm retro game mood. Large readable cream pixel-font title in the left half, exactly "TamaPoke CYD"; smaller supporting text exactly "ESP32 PIXEL PET". Spacious balanced composition. Right half shows the landscape and a single bright golden pixel star. No characters, people, mascots, animals, physical electronics or device frames. No brand marks or watermarks. Avoid gradients, smooth curves and realistic rendering. This is a title cover illustration, not a screenshot.

`cover-landscape.png` 保存已核准的基礎封面。`pikachu-idle.png` 為專案既有的原生 19×25 像素角色影格，來源是 `assets/p025.bin`；素材權利與非商業條件見 [第三方聲明](../../THIRD_PARTY_NOTICES.md)。

合成使用最近鄰整數 7 倍放大，角色為 133×175 像素，放在右側草地、樹木左邊。角色之外的每個像素與基礎圖完全一致；標題、背景、配色與 1672×941 畫布均保留。輸入與輸出雜湊、座標及驗證範圍見 [cover-composition.json](cover-composition.json)。合成圖另存 PNG，原始生成圖與原有來源資訊完整保留於基礎圖檔。

可重做合成（Pillow 僅用於這個選用工具，韌體與網站整理無此依賴）：

```sh
python3 -m pip install Pillow==13.0.0
python3 tools/compose_cover.py
python3 tools/build_web.py
```
