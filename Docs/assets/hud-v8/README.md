# HUD v8 资产交付

日期：2026-09-24。视觉原型，尚未接入 Unreal / UMG。

版本管理：原型、设计板、头像母图和矢量源资产随仓库保存；`verification.json`、`preview-*.png` 和 `board-*.png` 是本地重建的检查产物，不入 Git。

- `../hud-prototype-v8.0.html`：可交互原型，1920×1080；HTML 内嵌 CSS、JS 和头像，可离线打开。
- `../od-hud-states-v8.0.html`：自包含设计板，含新头像、十个图标、状态截图和尺寸表。
- `ishigori-portrait.png`：内置 imagegen 生成的角色头像母图。使用 `../ishigori-official-20260322.jpg` 作为身份参考重新绘制，非原图裁切。P2 在展示层水平镜像。
- `punch.svg`、`heavy.svg`、`kick.svg`、`hkick.svg`、`swap.svg`、`blast.svg`、`sblast.svg`、`aim.svg`、`domain.svg`：九个技能/招式图标，24×24 viewBox；`mouse.svg` 是键位图，共十个矢量文件。
- `icons.json`：与 SVG 同源的图形字典。
- `preview-*.png`：Edge 实际渲染截图；`verification.json`：状态切换、图片加载、缩放检查结果。它不代表完整战斗逻辑回归。

图标的主色使用 `currentColor`；深色负形为 `#12212A`。导入 UMG 时可栅格化为透明纹理，或拆分主色和负形两个通道。技能当前显示 44×44，领域图标 54×54；正式导入前仍需检查纹理缩放与滤波。

## 头像生成提示词

方式：内置 `image_gen`，参考图仅用于角色身份。最终提示词如下：

```text
Use case: stylized-concept. Asset: square 1024x1024 fighting-game HUD character portrait. Create a newly illustrated head-and-shoulders portrait of Ryu Ishigori from Jujutsu Kaisen, using the attached official full-body artwork ONLY as character identity reference. Black tall sculpted pompadour fully visible, shaved sides, strong angular adult male face, narrow confident eyes, slight cocky smile, white shaggy fur collar on black jacket, necklace. Clean premium anime cel shading, bold readable silhouette, restrained ink outlines, warm skin, cool cyan rim light from upper left. Face large and readable at 96px, head top 10% margin, shoulders fill bottom, three-quarter facing slightly right, eyes at 45% height. Flat deep blue-black background with subtle teal halo. No text, no frame, no symbols, no watermark. Single finished portrait, not a sheet.
```

提示词中的 1024 是目标尺寸，生成工具实际输出以 PNG 为准；保留原始母图，不冒称已完成旧规范中的四档贴图导出。

## 重建

在仓库根目录运行 `node Scripts/build-hud-v8.cjs`。此脚本以保留的 v7 原型为状态演示基础，注入 v8 样式、图标和内嵌头像。

启动独立 Edge 无头会话（调试端口 9338）后，运行 `node Scripts/verify-hud-v8.cjs`；最后运行 `node Scripts/build-hud-v8-board.cjs`，用最新截图生成设计板。

## 演示范围

数字键切换预设状态，E 切换近远程，右键演示瞄准，Tab 显示预览面板。原型继承 v7 的演示计时与状态数据，不模拟真实 GAS 战斗；Q 也不是实际超级炮能力的完整模拟。工程数值、冷却规则、领域会话以 C++ 为准。原型中的人物与道场是构图示意。
