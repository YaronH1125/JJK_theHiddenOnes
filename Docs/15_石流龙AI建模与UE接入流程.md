# 石流龙 AI 建模与 UE 接入流程

版本：v0.1（方案，未开工）；日期：2026-09-20；网络资料查阅日期同上。

本文回答两个问题：**用 AI 工具把石流龙这个角色做出来需要哪些环节、每步用什么工具；做出来之后怎么接进本工程替换掉默认小白人。**

状态声明：本文是流程与工具调研，**本页所有"待**"均为未执行项。资产接入的实际状态仍以 [05 开发计划与验收](05_开发计划与验收.md) 与 [10 完整性待办](10_Demo完整性与打击感待办清单.md) 的记录为准。

资料强度：Epic 官方文档与本地工程实测为一级证据；工具官网/文档为二级；博客与二手教程只作流程参照，不当结论。本次 Tripo 官网与 Meshy 文档的正文页抓取失败（网络错误），相关能力描述来自搜索结果摘要与第三方页面，标注为待核验。

---

## 1. 本工程现状：接入的硬约束

这一节是流程设计的依据，不是通用教程。数据来源为工程实测与既有验收记录。

| 事实 | 实测值 | 来源 |
| --- | --- | --- |
| 引擎 | UE 5.8.2，C++ + GAS + Behavior Tree | `JJK_theHiddenOnes.uproject` |
| 当前角色网格 | `SKM_Quinn_Simple`（文档记录），C++ 默认硬编码 `SKM_Manny_Simple` + `ABP_Manny_Combat`，蓝图可能覆盖 | [04 §5](04_旧项目评估与复用方案.md) |
| 骨架 | `SK_Mannequin`（Epic 标准骨架），另有 `FightingAnimsetPro/UE4_Mannequin/Mesh/SK_Mannequin` 一套 | 本地资产树 |
| 动画量 | `MM_*` 66 段 + `MF_*` 63 段 ≈ **131 段动画序列**，全部挂在 Manny 骨架上 | 本地资产树统计（`Content/Characters/Mannequins/Anims/**`） |
| 蒙太奇 | `AM_*` 7 个（`Training/` 5 个 + `Training/Movement/` 2 个） | 本地资产树 |
| 技能接线 | `DA_Fighter_Ishigori` 数据资产持有 Montage / 攻击定义 / Socket 名；攻击定义见 `AttackDefinition.h` | `Source/.../FighterDefinition.h` |
| 炮口 Socket | 代码约定 `Muzzle_Head_Review`（父骨 `Head`），`BlastProjectile` 回退到骨骼 `head` | `FighterCharacter.h` / `BlastProjectile.cpp` |
| 命中检测 Socket | 默认 `hand_r`（`AttackDefinition.h` TraceSocket） | 同上 |
| 已有旧模型 | `F:/GameStudy/JJKDemo/sll`：`T.fbx` 等 4 份，高 **190.479 cm**、65 骨、**999,774 三角面**、单材质槽、正面 **+Y**；含待机/走/跑，缺全部战斗动作 | [M1 素材检查](开发过程/验收记录/M1_石流龙素材检查.md) |
| 试导入资产 | `SK_Ishigori_Review.uasset` **118.0 MB**（已 gitignore，可重建） | 本地文件实测 + `.gitignore:24` |
| 已有回退补丁 | `M_FighterTint` 材质 + `SetOverlayMaterial`（受击闪白/标记），`ChargedBlastAbility` 用 Muzzle Socket 位置做贴墙检测 | `FighterCharacter.cpp` / `ChargedBlastAbility.cpp` |
| 现有验收 | 六套 PIE 套件 294 项断言 + 打包 smoke 20 项，Python 驱动（`Scripts/ue_python.py`、`run_*_acceptance.py`） | [Scripts/README](../Scripts/README.md) |

**由此推出的三条结论**（决定了后面每一步怎么做）：

1. **工程的全部动作资产都在 Manny 骨架上。** 骨架选择不是美术细节，而是决定"131 段动画能不能直接复用"的分叉点，必须先定（见 §3）。
2. **C++ 不依赖具体网格资产。** 代码只通过 `GetMesh()`、Socket 名和 Montage 引用工作，所以换网格主要是资产替换 + Socket 校准，不需要改战斗逻辑——但 Socket 名与位置一旦变化，炮口贴墙检测和命中扫掠都要重测。
3. **旧高模不是可交付资产。** 100 万三角面 + 单材质槽 + 缺战斗动作，只能当比例、轮廓和 Hair/毛领造型的参照，不能直接进游戏（UE 里就算能进，LOD/物理资产/打包都过不去）。

---

## 2. 全流程总览

```
P0 定骨架路线 ──┐
                │
P1 概念/三视图  │  AI 生图（正/侧/背 T 或 A 姿态）
                ↓
P2 图生 3D      │  Meshy / Tripo / 混元3D / Rodin → 带贴图网格（三角面、需清理）
                ↓
P3 网格整理     │  Blender：合并碎块 → 重拓扑 → UV/烘焙 → 分件（头发/毛领/外套）→ 导出
                ↓
P4 骨骼与蒙皮   │  Blender：套 Manny 命名骨架 → 蒙皮权重 → 补物理骨 → 定参考姿势
                ↓
P5 导入 UE      │  FBX 导入 → Skeleton → 材质/LOD/物理资产/Capsule
                ↓
P6 动画复用     │  IK Rig + IK Retargeter 批量重定向 131 段 → 重建 Montage
                ↓
P7 接线替换     │  BP_Fighter 网格/AnimBP → DA 蒙太奇 → Socket 校准（炮口/手/脚）
                ↓
P8 验证交付     │  M8 验收脚本 → PIE 回归 → 打包 smoke → 素材哈希归档
```

**时间成本的重心不在建模。** AI 能把"从零到有形状"从几天压到几小时，但 P4（绑定蒙皮）与 P6/P7（重定向 + 手感校准）依旧是手工迭代，这两段决定整个接入能不能用。

---

## 3. P0 决策点：骨架走哪条路

这是**唯一一个必须先定、且定错要推倒重来的决定**。

| 路线 | 做法 | 优点 | 缺点 | 适用条件 |
| --- | --- | --- | --- | --- |
| **A 完全复用 Epic 骨架** | 新网格的骨骼层级/命名/参考姿势严格照抄 `SK_Mannequin`，导入后设为 Compatible Skeleton | 131 段动画**零重定向**直接可用；Montage/AnimBP/DA 引用改动最小；炮口与命中 Socket 立即有效 | 不能随意加骨（飞机头、外套、毛领无物理摆动）；体重比例必须接近 Manny | 想最快看到"石流龙能打" | 
| **B 新骨架 + IK 重定向** | 自建骨架（可加 `hair_01`、`coat_L` 等物理骨），建 IK Rig/IK Retargeter，把 131 段动画重定向到新骨架 | 造型自由度高；可做头发/衣摆；技术展示价值高（作品集加分） | 重定向工作量大；脚滑、手部扭曲、Root Motion 需逐项调；必须重建全部 Montage | 目标是有辨识度的正式角色 |
| **C 兼容骨架 + 追加骨** | 主体骨骼照抄 Manny（保证兼容），只额外加少量非形变用途的物理骨 | 兼顾动画零成本与少量动态 | 需要验证 UE 对"多出骨骼的兼容骨架"行为；参考姿势必须严格一致（同层级、同朝向、同参考姿势，比例不同仍可能异常） | 想同时要速度和造型 |

**推荐：先 A 后 C/B。** 用 A 路线把"石流龙外观 + 现有全部动画"跑通（这一步就能让 Demo 立刻告别小白人，且验收脚本基本不用改），确认 Socket、炮口、命中检测都对之后，再决定是否为头发/毛领升级到 C 或 B。理由与 [04 §"先迁移一个普攻闭环，验证后再做蓄力炮"](04_旧项目评估与复用方案.md) 同源：**不要同时换骨架、重写伤害、重做 UI**。

> 兼容骨架的前提是层级、命名、参考姿势都一致，仅"层级一样"不足以保证不变形（Epic 论坛讨论与官方重定向文档均指出：比例差异、twist 骨、Translation Retargeting 模式都会影响结果）。本机 UE 5.8.2 的实际行为需在 P5 用一段动画实测确认，不预设结论。

---

## 4. P1–P3：AI 建模与网格整理

### 4.1 工具矩阵

| 环节 | 候选工具 | 用途 | 备注（资料强度） |
| --- | --- | --- | --- |
| 概念三视图 | 即梦 / Midjourney / Niji / Stable Diffusion + ControlNet | 生成正/侧/背三视图，锁定飞机头、敞胸外套、毛领、体格 | 工程内已有 AI 概念图先例（[08 §2](08_石流龙战斗系统设计.md) 领域概念图，图内已标"非官方设定"），沿用同一标注规范 |
| 图生 3D | **Meshy**、**Tripo**、**腾讯混元3D / 混元3D Studio**、**Rodin**、TRELLIS（自托管） | 三视图 → 带 PBR 贴图的网格 | 二级证据：厂商文档与第三方对比文；各家都宣传 quad/重拓扑输出，实际质量需自测 |
| 重拓扑 | Blender（Quad Remesher / RetopoFlow）、InstaLOD、Simplygon | 三角面汤 → 四边形布线 + LOD | 游戏可用性的关键一步；[官方/第三方](https://www.tripo3d.ai/blog/can-ai-make-usable-3d-models) 均承认 AI 拓扑需人工整理 |
| 烘焙 | Blender / Substance / xNormal | 高模细节烘到游戏 UV | AI 输出常有多视图贴图不一致，需统一 UV 后重烘 |
| 自动绑定 | Mixamo、Blender Auto-Rig Pro / Rigify、Meshy 的自动绑骨、Tripo 的 auto-rig | 快速得到可动骨架 | **注意授权**：Mixamo 内容的使用范围受 Adobe 条款约束，作品集用途需自行确认；混元3D Studio 可本地跑（[腾讯开源报道](https://cloud.tencent.cn/developer/article/2572758)） |
| 动画/关键帧 | Blender 手工关键帧、Cascadeur（物理辅助、可导出 FBX）、Move.ai / Rokoko Vision（视频动捕） | 补工程缺失的战斗动作（结印、领域展开姿势等） | Cascadeur 2026.x 持续更新（[CG Channel](https://www.cgchannel.com/2026/04/nekki-releases-cascadeur-2026-1/)）；视频动捕适合走跑与基础拳脚，不适合高风格化招式 |

### 4.2 AI 能做什么、不能做什么（先对齐预期）

| 能省力 | 仍旧要人做 |
| --- | --- |
| 从三视图出一个**比例和轮廓可用**的网格 | 拓扑布线、UV 展开、贴图通道打包、LOD |
| 快速铺贴图底子（albedo 初稿） | 面部/飞机头的可读性修型；毛领与外套的分件 |
| 自动绑骨给出草稿骨架 | 蒙皮权重（腋下、肩、胯、毛领最容易穿帮）、参考姿势对齐 |
| 生成一批通用动作 | **拳腿时序、打击帧、Root Motion、脚底锁定、动作通知时点** —— 这些直接决定手感，必须手工迭代 |

工程内部已有同类结论：[10](10_Demo完整性与打击感待办清单.md) 明确写了"重定向只解决骨架映射，不自动解决力量感、脚底锁定和人物性格"。

### 4.3 必须守住的工程约定

- **单位与朝向**：模型高度对齐 190.479 cm 基准；Blender 侧与 UE 侧都用厘米，导出时缩放保持 1.0，不做额外 0.01/100 转换。
- **正视方向**：UE 里角色以 **+X 为前**；旧 FBX 实测正面向 **+Y**，旧记录给的校准起点是 **Yaw = −90°**（[M1 素材检查](开发过程/验收记录/M1_石流龙素材检查.md)）。新模型建议直接在 DCC 里转正，不要留到引擎里扭。
- **参考姿势**：目标骨架的参考姿势必须与 `SK_Mannequin` 的姿势一致（UE5 的 Manny 是 A-pose 站立），否则动画会整体跑偏。
- **分件策略**：主体（身体+外套）、头发（飞机头）、毛领建议分成 2–3 个 Skeletal Mesh 或同网格多材质槽——飞机头和毛领是石流龙的第一辨识特征，值得单独留出可做物理/附加骨的余地。
- **材质槽数量**：控制在 2–4 个（皮肤/衣服/头发/毛领），AI 输出的单槽贴图要拆通道，不要 1 个槽塞一张 4K 大图。

---

## 5. P4–P5：绑定与导入

### 5.1 Blender 侧交付检查单（导出 FBX 之前）

- [ ] 变换已应用（Apply All Transforms），原点在世界 0，脚底在 Z=0
- [ ] 骨架层级/命名与 `SK_Mannequin` 一致（选 A/C 路线时）；父级链无多余关节
- [ ] 蒙皮权重检查：抬臂、抬腿、深蹲三个极限姿势下无破面与穿帮
- [ ] 顶点权重最多 4 骨影响（UE 默认），无零权重孤立顶点
- [ ] UV 全部展开、无重叠（除镜像件），贴图通道按 UE 约定打包
- [ ] 导出设置：FBX、Skeletal Mesh、不烘焙动画、`Y up` 交给导出器处理、单位厘米

### 5.2 UE 导入检查单

- [ ] Skeleton 指向目标骨架；Import Uniform Scale = 1.0 后实测身高 ≈ 190 cm
- [ ] 参考姿势与源骨架一致（用 Pose Asset 或直接对比）
- [ ] 材质：albedo 勾 sRGB、法线用 NormalMap 压缩、ORM 不加 sRGB；`M_FighterTint` 的 `SetOverlayMaterial` 路径仍可复用
- [ ] 生成 LOD（AI 高模必须做，建议 LOD0 ≈ 4–6 万三角面，逐级 50%）
- [ ] 物理资产：躯干/头/四肢胶囊，用于受击位置与死亡布娃娃（`Variant_Combat` 有 `SetSimulatePhysics` 逻辑）
- [ ] Capsule 半高/半径按新身高重设，并复核相机三档 `AimSocketOffsetY/Z` 等偏移（`FighterDefinition.h` 中的相机参数）
- [ ] Socket：`Muzzle_Head_Review`（父骨 Head）位置/朝向重校；`hand_r` 命中检测点复核；双脚 Socket（若后续做 FootIK）

---

## 6. P6：动画复用与重定向

选择路线 A/C（兼容骨架）时，本节大部分可以跳过——这正是先走 A 的价值。

选择路线 B 时的顺序：

1. **先建 Retarget Pose**：为目标骨架摆一个与源骨架参考姿势对位的姿势，存成 Pose Asset。
2. **建 IK Rig ×2**：源 `SK_Mannequin` 一套，目标骨架一套，链定义按 Epic 的 [IK Rig 文档](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine) 配好（Spine/Head/LeftArm/RightArm/LeftLeg/RightLeg + 手指链）。
3. **建 IK Retargeter**：链映射用 `Map All (Exact)` 起步，手指链用 **One to One** 旋转模式，躯干链用 **Interpolated**。操作栈里的 Pelvis Motion / Floor Constraint / FK Chains / Root Motion / Remap Curves 逐项按 [UE 5.8 重定向操作栈文档](https://dev.epicgames.com/documentation/unreal-engine/retargeting-operation-stack-in-unreal-engine-5-8?lang=en-US) 调；地面约束用 Crotch Offset 对齐胯部高度，防止悬空或陷地。
4. **批量导出动画**：在 Retargeter 里批量选 131 段序列导出为 **只含 Animation 的资产**。工程实践与外部经验一致：**不要顺手导出蒙太奇、AnimBP、角色蓝图**——链接会错，后面极难排障。蒙太奇一律在新骨架上重建。
5. **重建 7 个蒙太奇**：`AM_M2_A1`、`AM_M2_HitReact`、`AM_M3_*`、`AM_Backstep`、`AM_Dodge`，逐个重放攻击窗口通知（`UAnimNotify_AttackWindowOpen/Close`）并复核时点；`DA_Fighter_Ishigori` / `AttackDefinition` 里的 Montage 引用与 `TraceSocket` 同步更新。
6. **Root Motion**：走跑与闪避检查是否仍由根骨驱动；重定向器里 Root Motion 用 `Copy From Source Root` 还是 `Generate From Target Pelvis` 要实测，错了会脚滑或位移翻倍。

### 6.1 蒙太奇与通知的坑（工程实测结论，来自既有记录）

- 命中窗口**优先由动画通知驱动**，无通知时才回退时间窗（`AttackDefinition.h`）。重定向后的序列会丢通知，必须逐条补。
- 腿击当前复用拳动画 + 手部 Socket 占位（[M3 记录](开发过程/M3_连招与核心攻防.md)），正式动作接入后**腿击检测点要从 `hand_r` 改到脚部 Socket**，这是最容易漏的一处逻辑-资产耦合。
- 占位动作替换后必须重测：Socket、Root Motion、通知时点、打断与取消窗口。

---

## 7. P7：替换默认小白人的执行清单

| 步骤 | 操作 | 备注 |
| --- | --- | --- |
| 1 | `BP_Fighter` 的 Mesh 换成新 Skeletal Mesh，Anim Class 指向新 AnimBP（或兼容骨架时保留原 AnimBP） | 代码不硬编码具体资产，蓝图覆盖即可 |
| 2 | 若走 B 路线：重定向 AnimBP 或重建（推荐重建，重定向的 AnimBP 易出状态机错位） | 外部经验明确反对直接导出 AnimBP |
| 3 | 校准 `Muzzle_Head_Review` 局部偏移/旋转 | 直接决定炮口位置、贴墙 30 cm 拒绝、光束起点 |
| 4 | 复核 `hand_r` / 手部 Socket 的命中扫掠路径 | `CombatHitComponent` 用帧间 Sweep 采样 |
| 5 | 核对 `DA_Fighter_Ishigori` 全部 Montage 引用与通知 | 双形态两套攻击定义都要过 |
| 6 | 保留双方识别标记：`M_FighterTint` + `SetOverlayMaterial`，比对 190 cm 下 HUD 位置与遮挡 | [14 §10.1](14_HUD开发指引.md) 要求正式模型接入后复核 HUD |
| 7 | 头发/毛领动态（可选）：加骨 + 物理，或先做刚性 LOD | 布料/长发属高成本项 |
| 8 | 与 `Mishima_DOJO` 只读约定共存 | 不因接角色而改动美术包（[11](11_日式道场场景接入说明.md)） |

---

## 8. P8：验收与交付

沿用现有自动化体系，新增一套角色接入专项（建议命名 `M8_art_swap`，脚本放 `Scripts/`，走 `ue_python.py` 驱动）。

| 检查项 | 手段 | 通过标准 |
| --- | --- | --- |
| 导入正确性 | UE Python 读资产 | 身高 190±2 cm、骨架名匹配、LOD 数量、材质槽数、Socket 存在 |
| 动画零丢失 | 脚本枚举引用 | 5 个攻击定义 + 2 个移动蒙太奇的 Montage 均非空且 Skeleton 匹配 |
| 通知完整性 | 脚本读序列通知 | 每个攻击 Montage 至少一对 Open/Close，时点与 `AttackDefinition` 窗口一致 |
| 炮口行为 | 复用 M7_A 用例 | 贴墙 30 cm 拒绝、墙外拦截、终点与命中一致 |
| 命中结算 | 复用 M3/M5 断言 | 同一段不重复命中、腿击改用脚部 Socket 后仍正确 |
| 视觉 | 无头截图（Edge，1920×1080 锁视口） | 三档相机下不穿模、HUD 不遮挡角色关键部位 |
| 性能 | `Dojo_perf_high.py` 同法 | 记录编辑器/High 档 FPS，不高调宣称"稳定 60" |
| 打包 | `build_dojo.ps1` + `run_dojo_smoke.py` | 包内 smoke 全绿 |
| 素材完整性 | 前后文件清单 + SHA-256 | `Mishima_DOJO` 564 文件哈希不变；新角色资产单独归档并登记来源 |

---

## 9. 风险与开放问题

| 风险 | 影响 | 应对 |
| --- | --- | --- |
| 骨架参考姿势/比例与 Manny 不一致 | 动画整体错位、关节扭曲 | 严格照抄层级与参考姿势；用一段动画先验证再批处理 |
| 炮口 Socket 位置变更 | 光束起点偏移、贴墙判定失效 | 每次改网格都重跑 M7_A 遮挡用例 |
| AI 拓扑与权重不过关 | 抬臂穿帮、打包体积爆炸 | 重拓扑与权重必须人工过一遍；LOD 强制生成 |
| 重定向后手感退化 | 连招节奏、脚滑、打击感崩 | 保留时间窗回退机制；关键动作手工关键帧 |
| 视频动捕/AI 动作质量不稳 | 风格化招式不可用 | 结印、领域展开等少量姿势手工做，不追求全套 |
| 素材授权与 IP | 不能进正式作品集/发布版 | 沿用 [12](12_版本控制与素材依赖.md)、[14 §10.1](14_HUD开发指引.md) 的口径：官方立绘与二手素材只作参考，正式交付需自有或可商用素材，AI 生成内容记录来源 |
| 时间估算 | 影响作品集节奏 | 见 §10 |

时间量级（按单人、已有 294 项回归可用估计，用于排期而非承诺）：

| 阶段 | 乐观 | 现实 |
| --- | --- | --- |
| P1–P3 概念 → 可用网格 | 1 天 | 2–3 天（重拓扑反复） |
| P4 绑定蒙皮 | 0.5 天（自动） | 1–2 天（权重修正） |
| P5–P6 导入 + 重定向 | 0.5 天（路线 A） | 2–3 天（路线 B） |
| P7 接线 + Socket 校准 | 0.5 天 | 1–2 天 |
| P8 验收 + 打包 + 录像 | 1 天 | 1–2 天 |
| 合计 | ≈ 3.5 天 | **≈ 8–12 天** |

---

## 10. 需要用户确认的决策点（开工前）

| # | 决策 | 选项 | 影响 |
| --- | --- | --- | --- |
| D1 | 骨架路线 | A 兼容骨架（快）/ B 新骨架+重定向（自由度高）/ C 兼容+追加骨（折中） | 决定 P6 工作量与动画是否零成本复用 |
| D2 | 造型来源 | ①AI 从三视图重新生成 ②旧高模（100 万面）重拓扑精修 ③两者结合：AI 出主体、旧模出比例参照 | 决定 P1–P3 路线与时间 |
| D3 | 工具与预算 | Meshy / Tripo / 混元3D（本地）/ Rodin 的选择；订阅还是按次 | 影响成本与出图迭代速度 |
| D4 | 动作范围 | 只接外观 + 复用现有动画 / 另做少量专属动作（结印、领域展开） | 决定是否引入 Cascadeur/动捕 |
| D5 | 验收强度 | 只做 PIE 目视复核 / 全套 M8 自动化 + 打包 | 决定交付证据完整度 |

---

## 11. 案例参照：一条已验证的 AI 角色流水线（10 步）

来源：3D 概念设计师 Stefan Vaskevich 的教程，经其 6 个月、多角色压力测试，案例角色为"瘟疫医生恶魔猎人"，全部使用免费或低价工具，声称把传统 1–2 周的角色流程压到**一个工作日**。原文经[火星时代转载](https://www.hxsd.com/content/59520/)，原始出处 [top3d.ai](https://www.top3d.ai/zh/learn/create-3d-character-with-ai-full-lesson)。**属二级证据，其速度结论与"无穿模"宣称不适用于本项目，仅作流程骨架参照。**

### 11.1 步骤与工具

| # | 步骤 | 工具 | 关键做法 |
| --- | --- | --- | --- |
| 1 | 概念图 | Nano Banana（Google AI Studio，免费） | 对话迭代提示词；**把角色拆成独立部件**分别生成；每部件备正/3-4/侧/背四视角 |
| 2 | 生成网格 | 混元3D Studio 3.1（国际版免费 20 次/天，国内 30 次/天） | 角色工作流，单图输入为主；**此阶段只出几何、不要贴图**；输出四边面基础网格，称 80–90% 可用度 |
| 3 | 雕刻清理 | Blender 雕刻模式 | Grab/Elastic Grab（占 80% 时间）、遮罩+"切割遮罩"做干净切口、Smooth 抹平被遮挡面、Inflate+Remesh 封洞 |
| 4 | 重拓扑 | 混元自动重拓扑 + Retopoflow（Blender 插件，非商用免费） | 每对象跑 3–4 次挑最干净线框；精修重点只有**关节循环边（肩/肘/膝）与眼周** |
| 5 | UV | 混元自动展开 + UV Packmaster（免费） | 自动展开对简单件效果好，靴子这类复杂件手动标缝合线；**打包要按目标分辨率留边紧密排**，松散会浪费一半像素 |
| 6 | 烘焙 | Blender Cycles（Marmoset 可选） | 烘 Normal + AO；高模先选、低模后选（低模为活动项），勾 Selected to Active |
| 7 | 贴图 | Modddif + Blender 绘制 | **选 Albedo 模式，不要 Realistic**（多数 AI 工具把光照烘进漫反射，进 UE5 真实光照会打架）；**选 Single-View 投射，不要 Multi-View**（多视投射会让眼睛漂移、肤色乱）；用完修补刷逐角度修，脸单独用一张参考 |
| 8 | 贴图合成 | Blender Principled BSDF | 组 Albedo/Roughness/Metallic/Normal/AO（+可选 Emissive）完整 PBR 集 |
| 9 | 绑定 | **AccuRig（免费）** + Blender 权重绘制 | 角色单独导出（道具拿开）、T/A-pose；打标记约 15 分钟；**标完必须跑 Calibration**，否则骨架偏离身体（这是 Mixamo 最常见的失败模式）；导出时直接选目标引擎骨架；权重绘制约 30 分钟，重点腋下与胯；物理部位（外套/尾巴/头发）**额外加骨**并用梯度权重分配 |
| 10 | 引擎集成 | UE5 | 导入 → IK Retargeter 指向 Manny/Quinn 骨架与 AccuRig 骨架，自动映射 → 第三人称蓝图换网格 → **物理资产里给外套骨骼加物理体**，跑动时布料自然摆动 |

### 11.2 这套流程里对本工程最有价值的三点

1. **"拆件"是 AI 建模的核心技巧**：不要指望一次生成整个角色。石流龙拆成【身体 / 双手 / 靴子 / 敞开的外套 / 飞机头 / 毛领】六个件分别生成再组装，每件单独重拓扑和贴图——这直接解决"飞机头糊成一坨"的问题，也符合 §4.3 建议的分件策略。
2. **贴图必须走 Albedo + Single-View**：这两条是作者踩坑后的明确结论。工程已有 `M_FighterTint` 覆盖材质，正式贴图若把光照烘进去，受击闪白与轮廓光叠加后会脏。
3. **AccuRig 是比 Mixamo 更适合本项目的免费绑骨选择**：操作 15 分钟、免费、可直接导出目标引擎骨架，且作者实测在非标准比例（长袖/宽檐/笨重靴）上比 Mixamo 稳，前提是跑 Calibration。**但导出骨架是否严格等于 `SK_Mannequin` 层级与命名，必须在本机实测**——若不等于，就退回到 §3 路线 B 的重定向流程。

### 11.3 与本工程现实的差距（不要照抄的部分）

| 案例做法 | 本工程的问题 |
| --- | --- |
| "一个工作日完成" | 案例角色**没有战斗需求**。本工程要 131 段动画复用、5 组攻击定义的 Montage 与通知时点、Root Motion、炮口 Socket 校准、294 项回归与打包 smoke，这些一个都不在案例范围内 |
| 只做外观与走路 | 本工程的验收标准是"能对打"，P6/P7/P8 才是主战场 |
| 引擎里只换网格 | 本工程还要同步 HUD 位置、Capsule/相机偏移、腿击检测点、`Mishima_DOJO` 只读约束 |
| 物理布料靠物理资产 | 石流龙是飞机头 + 毛领，摆动幅度大，物理资产容易抖穿，可能需要退化为"少量骨 + 手工动画" |

---

## 12. 参考资料（查阅日期 2026-09-20）

**Epic 官方（一级）**

- [UE 5.8 重定向操作栈](https://dev.epicgames.com/documentation/unreal-engine/retargeting-operation-stack-in-unreal-engine-5-8?lang=en-US) — Pelvis Motion / Floor Constraint / FK Chains / Root Motion / Remap Curves 各参数含义
- [IK Rig Animation Retargeting](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine)
- [Auto Retargeting](https://dev.epicgames.com/documentation/unreal-engine/auto-retargeting-in-unreal-engine?application_version=5.5) — 一键重定向的源/目标/自动生成重定向器
- [Retarget Manager](https://dev.epicgames.com/documentation/unreal-engine/documentation/zh-cn/unreal-engine/retarget-manager-in-unreal-engine?application_version=5.6)
- [UE 5.8 Release Notes](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes) — 重定向重载集（单个 IK Retargeter 处理不同重定向关系）
- [Control Rig](https://dev.epicgames.com/documentation/en-us/unreal-engine/control-rig-in-unreal-engine) — 后续做程序化姿势/手部修正
- [Compatible Skeletons 论坛讨论](https://forums.unrealengine.com/t/compatible-skeletons-questions/2657451/6) — 层级一致仍可能因比例/twist/Translation Retargeting 出问题

**AI 工具（二级，能力描述需自测）**

- [Meshy 自动绑骨文档](https://docs.meshy.ai/en/webapp/guides/3d-model/rigging)
- [Meshy 游戏资产指南（官方仓库）](https://github.com/meshy-dev/Meshy-guide/blob/main/meshy-game-asset-guide.md)
- [Meshy AI 动画生成（600+ 动作，FBX/GLB）](https://www.meshy.ai/zh-Hant/features/ai-animation-generator)
- [Tripo：从提示到可玩——UE 中的 AI 3D 角色完整指南](https://www.tripo3d.ai/blog/auto-rig-3d-characters-in-unreal-engine)（正文抓取失败）
- [Tripo：AI 生成的角色如何绑定骨骼](https://www.tripo3d.ai/zh/blog/how-to-rig-an-ai-generated-character)（正文抓取失败）
- [Tripo：AI 模型是否可用于实际生产](https://www.tripo3d.ai/blog/can-ai-make-usable-3d-models)
- [腾讯混元3D 开源与游戏建模定位](https://cloud.tencent.cn/developer/article/2572758)
- [混元3D Studio（专业级 AI 3D 工作台）](https://www.stdaily.com/web/gdxw/2025-09/25/content_407381.html)
- [Cascadeur 2026.1](https://www.cgchannel.com/2026/04/nekki-releases-cascadeur-2026-1/) / [2026.2 动画层](https://www.cgchannel.com/2026/08/nekki-releases-cascadeur-2026-2-with-animation-layers/)
- [Move.ai 评测（2026）](https://us.fitgap.com/products/009160/move-ai)
- [Mixamo 与 Meshy 配合的分步指南](https://www.meshy.ai/zh/tutorials/how-to-use-mixamo-with-meshy)
- [Make-It-Animatable（CVPR 2025，自动生成可动画角色）](https://github.com/jasongzy/Make-It-Animatable) — 研究向，可关注

**教程与案例参照（二级，流程骨架来源）**

- [十步用 AI 打造可绑定 3D 角色（火星时代转载，含完整工具链与参数）](https://www.hxsd.com/content/59520/) — §11 案例来源
- [用 AI 创建游戏级 3D 角色：完整教程（top3d.ai 原文）](https://www.top3d.ai/zh/learn/create-3d-character-with-ai-full-lesson)
- [Blender + AI 完整工作流搭建](https://www.uisdc.com/ai-3d-pipeline)
- [AccuRig 免费自动绑骨](https://actorcore.reallusion.com/auto-rig/accurig) / [AccuRig 导出说明](https://manual.reallusion.com/AccuRig-2/2.0/09-add-motions/export.htm)
- [Meshy：如何用 AI 自动绑定 3D 角色（2026 指南）](https://www.meshy.ai/zh/tutorials/character-auto-rigging-workflow)
- [虚幻5 替代小白人角色的完整逻辑：两次动画重定向（博客，流程参照）](https://www.cnblogs.com/zhangbo2008/p/22647557)
- [虚幻基础：将角色的模型替换为自己的模型（博客，流程参照）](https://blog.csdn.net/qq_42863961/article/details/155760936)

**工程内既有记录**

- [06 石流龙角色方案](06_石流龙角色方案.md)、[08 战斗系统设计](08_石流龙战斗系统设计.md)
- [M1 石流龙素材检查](开发过程/验收记录/M1_石流龙素材检查.md)、[M1_ArtReview.json](开发过程/验收记录/M1_ArtReview.json)
- [10 完整性与打击感待办](10_Demo完整性与打击感待办清单.md)（§5.2 正式素材到位后接入清单）
- [12 版本控制与素材依赖](12_版本控制与素材依赖.md)、[14 HUD 开发指引 §10.1](14_HUD开发指引.md)
