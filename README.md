# JJK: The Hidden Ones — 咒术回战 · 训练擂台

> UE 5.8.2 · C++ · GAS · 单机 1v1 训练擂台 · 首个角色：石流龙

基于《咒术回战》IP 的第三人称动作战斗 Demo，用于求职作品集。项目以 **Gameplay Ability System** 为核心，完整实现了双形态战斗、蓄力炮、原创领域机制与 AI 对战，并配套了一套 **自动化 PIE 验收体系**（294 项检查 + 打包 smoke）。

> 声明：本项目为个人学习/求职用途，与《咒术回战》官方无关。角色当前使用占位模型（Mannequin），正式角色素材接入中。

## 功能特性

### 双形态战斗
- **近战**：轻拳三段连招、重拳、腿击、重踢（倒地）、条件投技；攻击带磁吸（范围内自动面向/滑步贴近目标）
- **远程**：移动蓄力炮 + 定点超级蓄力炮（最低 1.2s / 满蓄 2.4s / 冷却 10s），发射可见弹体，蓄力时长决定伤害与咒力成本

### 原创领域机制
- 领域展开（结印 1s → 持续 6s）：自动生成满威力曲线追踪球，普通闪避不免疫
- 双领域并存互相压制（销毁在飞球、暂停生成，一方结束按原调度恢复）
- 倒地/死亡/重置保护始终生效

### 攻防与手感
- 共享攻防结算：正面格挡（削血+防御硬直）、闪避无敌帧、倒地保护、攻击方向去重
- 出招磁吸：范围内自动面向/滑步贴近目标（镜头不参与索敌）；索敌键硬锁持续面向
- 相机三档姿态驱动：近战居中 / 远程过肩 / 瞄准贴肩（PUBG/COD 式开火转身）

### AI 对战
- 原生 Behavior Tree + Blackboard，九分支决策（近战连击/远程炮/领域/防御/闪避/绕行）
- AI 与玩家走**同一套输入请求与合法性检查**（无作弊路径）
- 对手模式：木桩 / 固定防御 / AI

### 自动化验收
- 六套 PIE 验收套件，**294 项断言**（远程执行 Python 驱动，见 `Scripts/`）
- Development 独立包 + 包内 smoke 20 项（退出码校验 + SHA256 归档）
- 覆盖：规则回归、重新进入、图形闭环、打包验证

## 操作

| 输入 | 近战形态 | 远程形态 |
| --- | --- | --- |
| WASD | 移动 | 移动（侧移） |
| Shift | 移动=加速跑 / 原地=后撤闪避（无敌帧，可取消攻击） | 同左 |
| 鼠标左键 | 轻拳（三段连招）/ 长按=重拳 | 按住蓄力，松开发射 |
| Q | 腿击 / 长按=重踢（倒地） | 按住蓄力，松开发射（超级炮） |
| E | 切换近战 ⇄ 远程（0.1s，不打断移动） | 同左 |
| F | 防御（正面格挡） | 同左 |
| R | 领域展开 | 同左 |
| 鼠标中键 | 锁定/解除目标（持续面向） | — |
| F1 | 训练面板（对手模式/资源开关/统计/重置） | — |

## 构建与运行

1. 安装 **Unreal Engine 5.8.2**
2. 双击 `JJK_theHiddenOnes.uproject`（首次打开会自动编译 C++），或右键 → Generate Visual Studio project 后构建 `Development Editor`
3. 打开地图 `L_TrainingArena`，PIE 运行；F1 打开训练面板

打包：`RunUAT BuildCookRun -platform=Win64 -clientconfig=Development -cook -map=/Game/Maps/L_TrainingArena -stage -pak -archive ...`（参考 `Scripts/run_m6_package.py`）

## 架构一览

```
Source/JJK_theHiddenOnes/Training/
├── FighterCharacter / FighterAttributeSet   角色、四资源（生命/行动/咒力/领域能量）
├── CombatInputComponent                     共享输入请求入口（玩家与 AI 同一套合法性）
├── CombatHitComponent                       命中窗口/去重/防御/投技结算
├── ChargedBlastAbility (+Mobile/Stationary) 蓄力炮：成本曲线/门槛/冷却/弹体
├── DomainExpansionAbility / DomainOrb       领域：结印/捕获/调度/压制/曲线追踪球
├── FighterAIController / ArenaBTNodes       原生 BT：九分支决策（近战/远程/领域/压制）
├── ArenaCrosshairHud / TrainingGameMode     准星、训练流程与胜负仲裁
└── M5SmokeHarness                           打包内自动 smoke（20 项，退出码校验）
```

## 文档

- [01 需求说明](Docs/01_需求说明.md)
- [08 石流龙战斗系统设计](Docs/08_石流龙战斗系统设计.md)
- [Scripts 自动化说明](Scripts/README.md)

## 已知限制

- 角色为占位模型（Mannequin + 占位动画集），正式角色素材接入中
- 演示视频待录制
