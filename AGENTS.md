# 协作约定

本文件记录本仓库的固定操作约定，供 AI 助手与后续协作者遵循。

## 浏览器

**打开网页、HTML 产物、本地预览一律使用 Microsoft Edge，不要用 Chrome。**

- Edge 路径：`C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe`
- 打开文件：`Start-Process "<Edge 路径>" -ArgumentList "<url 或 file:/// 路径>"`
- 无头截图 / 自动化核验同样用 Edge（`--headless=new`、`--remote-debugging-port`、`--screenshot` 等参数与 Chrome 一致）

## 文档产物

- `Docs/**` 下的 Markdown 与 HTML 一律用 **UTF-8（无 BOM）** 读写。
- 在 Windows PowerShell 5.1 下**不要**用 `Get-Content -Raw` + `Set-Content` 改中文文件：前者按 ANSI 解码会毁掉中文。用 .NET 显式编码：

  ```powershell
  $enc = New-Object System.Text.UTF8Encoding($false)
  $t = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
  [System.IO.File]::WriteAllText($path, $t, $enc)
  ```

- `Docs/assets/` 下的 HTML 产物要求**自包含单文件**：内联 CSS/JS，不引用外部资源与网络字体。

## 设计稿与原型

- HUD 相关设计以 [Docs/13_对战HUD设计稿.md](Docs/13_对战HUD设计稿.md) 为准；原型数值取自 `Source/JJK_theHiddenOnes/Training/FighterDefinition.h` 与 `BlastConfig.h`，改数值时三处需同步。
- 原型画布为 1920×1080。用无头浏览器截图核验时需先把视口锁定为该尺寸（CDP `Emulation.setDeviceMetricsOverride`），否则画面会被裁切。
