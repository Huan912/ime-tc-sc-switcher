# IME 繁簡切換器 (ime-tc-sc-switcher)

> 用全域快捷鍵一鍵切換**微軟注音輸入法**的「簡體輸出」設定,切換後立即生效。
## 緣起

本專案是 [aa846301/ime-tc-sc-switcher](https://github.com/aa846301/ime-tc-sc-switcher)(AutoHotkey v2.0 腳本)的 C++ 復刻版。

由於 Windows 11在切換注音繁簡過於麻煩，在網上衝浪時發現有上述repo然後又懶的下載 AHK2.0，所以用C++復刻了一版

## 快捷鍵

| 快捷鍵 | 功能 |
| --- | --- |
| <kbd>Shift</kbd>+<kbd>Alt</kbd>+<kbd>F</kbd> | 切換繁體 / 簡體輸出 |
| <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Alt</kbd>+<kbd>X</kbd> | 開關右下角 OSD 通知 |

> 啟動時會彈出一個使用說明對話框,內容與上表相同。

## 需求

- Windows 10 / 11(僅支援 Windows)
- 已安裝**微軟注音輸入法**(程式讀寫 `HKCU\SOFTWARE\Microsoft\IME\15.0\IMETC`)

## 下載與使用

### 方法一:直接下載預編譯版本

至本專案的 [Releases](../../releases) 頁面下載 `ime-tc-sc-switcher.zip`，解壓縮後直接雙擊執行即可。

### 方法二:從原始碼建置

### 讓它開機自動啟動

按 <kbd>Win</kbd>+<kbd>R</kbd> 輸入 `shell:startup` 開啟「啟動」資料夾,把 `ime-tc-sc-switcher.exe` 的捷徑放進去即可。

## 運作原理

**`reg`——註冊表讀寫**
   切換的本質是改 `HKCU\SOFTWARE\Microsoft\IME\15.0\IMETC` 底下的 `REG_SZ` 值
   `Enable Simplified Chinese Output`:
   - `"0x00000000"` = 繁體輸出
   - `"0x00000001"` = 簡體輸出
   
## 注意事項與已知限制

- ⚠️ **執行檔未簽署**,首次執行可能被 SmartScreen / 防毒軟體攔截,請自行判斷是否信任。
- 🎯 只支援**微軟注音輸入法**(IMETC);其他輸入法(如微軟拼音、新注音舊版)的登錄位置不同,不適用。
- 🔁 **單一執行個體**:程式用 mutex 鎖定,若已有實例在背景執行,再次開啟只會提示「程式已在執行中」。測試新版本前請先從工作管理員結束舊進程。

## 致謝
- [LinYa](https://home.gamer.com.tw/artwork.php?sn=6002596)
- [aa846301/ime-tc-sc-switcher](https://github.com/aa846301/ime-tc-sc-switcher)
