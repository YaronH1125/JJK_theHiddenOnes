# HUD v9：火影条组与鸣潮式技能图标

日期：2026-09-24。当前视觉稿：[原型](../hud-prototype-v9.0.html) · [设计板](../od-hud-states-v9.0.html)。v8 保留为上一版。

版本管理：原型、设计板、SVG/PNG 源资产与生成/导入脚本随仓库保存；`verification.json`、`preview-*.png` 和 `board-*.png` 是本地重建的检查产物，不入 Git。

## 本轮内容

血条组按照 [PlayStation 官方《究极风暴4》实机截图](https://blog.ja.playstation.com/2016/02/02/20160202-naruto4/) 重绘：570px 细长条，生命 16px、咒力 12px，咒力向头像方向错位 10px，红色卷轴背板、浅金回钩、橙色行动短槽。姓名使用本地书法字体，隐藏常驻 HP 数字和状态标签。

鸣潮图标参考仓库 `ref-wuthering-waves-ui.png`，重做九个招式徽记和鼠标键位图。采用白色镂空、弧线、尖端与粗细对比。HUD 普通图标 62px，领域 76px；槽位与按键沿用 LMB/Q/E/R。

每个图标同时提供 `.svg` 与 256×256 透明 `.png`。SVG 使用 `currentColor`，负形为真实透明，无上版深色填洞。PNG 为白色，UMG 导入后可用控件 tint 控制冷却/封印/就绪颜色。尚未导入 Unreal。

| 文件名 | 招式 |
| --- | --- |
| punch / heavy | 四段拳 / 重拳 |
| kick / hkick | 腿击 / 重踢 |
| blast / sblast | 蓄力炮 / 超级炮 |
| swap | 切换形态 |
| aim | 瞄准 |
| domain | 领域展开 |
| mouse | 鼠标左键标识 |

头像复用 [v8 生成母图](../hud-v8/ishigori-portrait.png)，在原型中内嵌，92×100 椭圆显示。未重做头像图像。装饰回钩为纯装饰，不表示额外资源；仍保留石流龙、五点行动、单条 1000 生命与项目战斗规则。这是参照重绘版本，不是提取原游戏 HUD 贴图的逐像素复制。

## 重建与检查

1. `node Scripts/build-hud-v9.cjs`：基于保存的 v8 原型生成 v9，并导出 SVG。
2. 启动 Microsoft Edge 调试会话，端口 9338；执行 `node Scripts/verify-hud-v9.cjs`：1920×1080 截图、12 个预设状态、三色血量、P2 74% 填充、E 切换、1280×720 缩放及透明 PNG 导出。
3. `node Scripts/build-hud-v9-board.cjs`：生成自包含设计板。
4. `node Scripts/check-hud-v9-board.cjs`：核验设计板图片与布局。

检查记录 `verification.json`；预览图 `preview-*.png`；图标展板 `board-assets.png`。HTML 均内联 CSS/JS/图像，不依赖网络。数值与状态演示沿用旧原型，不等同 GAS 战斗模拟或引擎回归。
# Unreal Demo 实装

十二张纹理已导入 `/Game/UI/HUD/V9`，正式 `UArenaCombatHudWidget` 默认启用。
编辑器目标编译通过；2026-09-24 实机验证覆盖近战、黄/红生命、远程、蓄力、冷却、领域就绪、领域展开与训练菜单。
验证记录：`Saved/HudV9/20260924_154159/report.json`（15 项通过、8 张实机截图）。
测试临时数值已恢复，并重新载入原角色定义；磁盘角色资产未由本次验证写入。
独立包暂未更新：`build_dojo.ps1` 在已有 `PS_GPBAR_Frost.uasset` 外部素材校验处停止。
