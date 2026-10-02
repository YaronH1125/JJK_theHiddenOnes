# UE 构建与自动化检查

## 当前菜单试玩候选

当前源码包含石流龙角色、战斗反馈和新版菜单。包含菜单的独立构建入口为 `Scripts/build_menu_demo.ps1`；`run_menu_package_smoke.py` 只检查主菜单启动，不验证点击开始后能移动 / 战斗。

本地 `v0.1.0-alpha.1` 已整理 ZIP，但玩家反馈的启动后卡在地面和暂停背景读取问题未关闭。当前公开发行状态见 [发行说明](../Docs/releases/v0.1.0-alpha.1.md)，完整分发与版本规则见 [发布指引](../Docs/22_试玩版发布与版本管理.md)。本轮文档更新未重跑 UE 或游戏测试。

下文 M1–M5 为历史阶段工具与检查口径，后续角色 / 动作 / 菜单接入可能改变原布景；各套件的通过计数不能合并成当前包的验收结论。原始记录按用户要求本地保存，Git 仓库只包含可复用脚本与长期使用说明。

## 道场修复与独立证据

先按 [版本控制与素材依赖](../Docs/12_版本控制与素材依赖.md) 恢复两套外部原素材，执行 `python Scripts/check_external_assets.py`。基线清单随仓库提交，原素材与归档不提交。新验收 runner 自动创建本地证据目录；一次性状态探针移至 `Saved/DojoTools/`，不作为仓库工具。

参考 FPS_Start 项目的地图策略：编辑器启动默认打开白盒 `/Game/Maps/L_TrainingArena`（开发调试用）；打包默认地图为道场 `/Game/Maps/L_DojoArena`（正式包）。编辑器里调试道场需手动打开；训练面板按 **F1**（F11 是编辑器视口全屏，不是训练菜单）。

- `Dojo_fix_level.py`：停止 PIE 后，通过 `ue_python.py` 运行。仅保存道场玩法地图和 GameMode 子蓝图；当前版本删除四面中央代理墙、把 NavMeshBoundsVolume 与 GameMode 矩形边界扩到建筑内沿（ArenaCenter=(-25,0,0)、半宽深 (1250,1050)），不保存源素材包。
- `run_dojo_acceptance.py`：当前地图应为道场；新 PIE 验证无中央墙、外圈四向可达且由原墙阻挡、真实步行进外环、外圈四角 AI 追击、贴墙炮弹销毁、格挡绕背、双炮、20 次重置、三局正常资源 AI 对被动玩家以及性能采样。没有模拟人类操作，不等于真人听音/手感验收；运行中不要保存资产。
- `run_dojo_smoke.py --editor --nullrhi --map L_DojoArena`：未 Cook 独立规则 smoke；白盒改为 `L_TrainingArena`。编辑器需先关闭。去掉 `--editor --nullrhi` 验证实际 Dojo 包，默认保留声音。
- `Dojo_hash_check.py`：将道场 564 文件与 `Baselines/external_assets.json` 中已核对原导入基线的 SHA-256 比较，不再依赖被 Git 忽略的本地报告。`check_external_assets.py` 现覆盖三包：Mishima_DOJO、EnergyBeam、GoodParticleBeamAndRay。
- `build_dojo.ps1`：检查外部依赖后构建、Cook 并打包两张地图；仅代码更新可在确认 Cook 资产一致后使用 `-ReuseCook`。

每次测试保留独立时间戳证据；失败不覆盖通过记录，也不能将白盒结果写成道场结果。独立包预期位于 `Saved/Packages/Dojo/Windows`。

先打开本项目 UE 5.8.2 编辑器。以下命令在项目根目录执行，无需保持编辑器前台。

```powershell
# 检查 MCP 工具集（每条命令重新握手，编辑器重启后也可使用）
python Scripts/ue_mcp.py list_toolsets

# 在同一个编辑器中运行 Python
python Scripts/ue_python.py Scripts/M1_diag.py

# 修复/核验已有 M1 资产：先停止 PIE
python Scripts/ue_python.py Scripts/M1_finalize_assets.py

# 完整运行验收 + 3 次重新进入，结束时停止 PIE
python Scripts/run_m1_acceptance.py

# 旧 FBX 独立试导入，需停止 PIE；约 124 MB 的高模仅供评估
python Scripts/ue_python.py Scripts/M1_art_review.py
```

`ue_python.py` 使用引擎自带 Remote Execution，默认引擎目录 `F:/GameStudy/UE_5.8`，可用 `--engine` 指定；发现节点按当前项目绝对路径筛选。MCP 与 Python 通道均只连接本机。调用超时后先检查编辑器日志/结果文件，避免重复执行非幂等操作。

## 脚本职责

- `M1_create_assets.py`：从无到有创建角色定义、薄蓝图、输入和身份材质。
- `M1_create_level.py`：**重建**白盒地图，会清空此地图原有 Actor；仅在需要重建时使用。
- `M1_finalize_assets.py`：原位修复场地尺寸、墙体、出生标记、角色网格、Controller、输入；编译/保存并断言。首次创建按 create_assets → create_level → finalize_assets 顺序。
- `M1_acceptance.py`：编辑器内异步测试，输出 `Saved/M1_acceptance.json`；外部优先用 `run_m1_acceptance.py` 调度并检查最终状态。
- `M1_reentry.py`：新 PIE 的数量、属性、身份与初始位置验证。
- `run_m1_cdo_check.py`：同一编辑器会话修改 GameMode CDO、编译保存后立即 PIE 读回，再恢复原值；需先停止 PIE。
- `M1_pie_test.py`：单步调试工具，可在 UE Python 控制台带 `state/move/stats_read/modify_p1/lock/recenter/unlock/kill/respawn/reset/reinit` 参数执行。modify_p1 使用临时定义走集中重置，正式伤害不能仿照此调试入口。
- `M1_save_close.py`：检查 PIE 已停止，保存脏资产并正常退出编辑器。C++ 构建前等待编辑器进程真正退出。
- `ue_capture.py`：默认捕获编辑器视口；末尾传 `CaptureEditorImage` 可捕获实际编辑器窗口，`CaptureAssetImage` 用于资产缩略图。
- `M1_demo_view.py`：新 PIE 中取斜侧玩家视角，延后数帧请求游戏 HighResShot。截图写入有延迟，需查看文件时间与内容确认。

运行结果、参数与已知限制见本地M1 记录（`../Docs/开发过程/M1_基础训练擂台.md`，不随 Git 分发）；排障见本地处理回复（`../Docs/求救信/M1_UE编辑器自动化通道_处理回复_2026-09-15.md`，不随 Git 分发）。

## M2 输入修复与冷启动验收

```powershell
# 停止 PIE 后，幂等创建/保存 IA_Attack，修复映射与控制器
python Scripts/ue_python.py Scripts/M2_fix_inputs.py
# 正常保存退出；等待 UnrealEditor 进程完全退出后再重新打开本项目
python Scripts/ue_python.py Scripts/M1_save_close.py
# 新编辑器中运行；验收不会创建或修复输入资产
python Scripts/run_m2_acceptance.py
```

只保留一个本项目编辑器实例。`M2_fix_inputs.py` 断言保存成功、正式 Content 文件存在且未被 Git 忽略；`M2_create_assets.py` 共用此输入修复入口。

`run_m2_acceptance.py` 首先检查磁盘资产，再运行 57 项断言（原有 48 项 + T13 输入分发 9 项）。T13 核对左键映射、Controller 引用、IMC 已安装，并逐帧注入 Enhanced Input action 的按下/松开，验证会话、攻击 Montage、受击 Montage、恰好一次 35 点伤害与长按无轻拳。它不模拟操作系统鼠标，实体键鼠手感仍需人工验收。

任一失败、异常、超时或断言数量不符都会使 runner 失败；失败详情保留在 `Saved/M2_acceptance.json`，成功才更新正式证据。资产文件哈希随成功报告归档；提交时应包含 `Content/Training/IA_Attack.uasset`。


## M3 连招与核心攻防

`M3_create_assets.py` 幂等保存六个独立招式 DA、四个新输入 action、IMC 和 Controller；所有近战动画均明确使用已验证拳击占位。

```powershell
python Scripts/ue_python.py Scripts/M3_create_assets.py
python Scripts/run_m3_acceptance.py
python Scripts/run_m3_demo.py
```

规则 runner 在新 PIE 执行 118 项检查并归档源代码/资产 SHA-256；失败单独保存，不覆盖成功证据。需要验证真实 30/60/120 游戏时序时可用 `UnrealEditor.exe <uproject> -nullrhi -NoSound` 启动，再使用同一 runner；它验证真实动画姿态/碰撞/GAS/Input 但不验证渲染。`run_m3_demo.py` 必须在有图形编辑器运行，20 项闭环/边界检查并保存三张实际游戏截图；动画仍为占位，默认不发起独立打包。

测试临时修改配置后会恢复，严禁在测试运行期间保存资产。菜单接入 `SetCombatInputEnabled`，对手停止决策接入 `SetRequestsEnabled`；训练重置仍用 `ResetTraining`。详见本地M3 验收与 M4 交接（`../Docs/开发过程/M3_连招与核心攻防.md`，不随 Git 分发）。


## M4 训练系统

先用最终 C++ 源码正式构建，启动本项目编辑器；同一时间只运行一个验收脚本，不在测试进行中保存资产。

```powershell
python Scripts/run_m4_acceptance.py  # 新 PIE：M4-T01—T12，含至少 20 次重置与 UI 换绑
python Scripts/run_m4_regression.py  # 新 PIE：原 M3 118 项；结果独立归档，不覆盖历史 M3
python Scripts/run_m4_demo.py        # 必须有图形渲染：UMG 控件回调闭环与实际面板截图
```

规则检查可用 `-nullrhi`；图形演示不可使用该参数。M4 脚本临时修改定义初值和受击时长后会恢复，最终证据写入 `Docs/开发过程/验收记录/M4_*`。报告采用临时文件替换，避免 runner 在写入中读取半份 JSON；失败报告单独保留。回归 runner 对旧 M3 脚本的非原子报告读取做重试。

游戏内 **F1** 打开训练面板。面板原生 UMG 控件无需另跑资源创建脚本；`TrainingPanelClass` 可由 Controller 薄蓝图替换。默认所有训练开关关闭。非 Shipping 构建的“开发测试技能”用真实 GE 消耗行动资源 1、咒力 10、领域能量 5，冷却 3 秒；默认领域能量 0 时正常拒绝，可先开无限资源试验。它不代表正式炮击已实现。

自动化使用真实 PIE/ASC/碰撞/动画和 UMG 控件回调，不模拟操作系统实体键鼠。规则、统计口径和 M5/M6 交接见本地M4 记录（`../Docs/开发过程/M4_训练系统.md`，不随 Git 分发）。


## M5 AI 对战

F1 面板选择 **AI 对战**（或 AI 快捷按钮），返回战斗后开始；菜单中 AI 暂停。胜负结果自动打开面板，快速重置保留模式与设置，关闭面板后开始下一局。静止木桩/固定防御仍可随时切换。`JJKDebugHud` 可查看行为树分支与任务状态。

```powershell
# 仅首次配置/修复地图时执行；先停止 PIE，普通验收不运行此修复脚本
python Scripts/ue_python.py Scripts/M5_setup_nav.py
# 已正式构建且打开编辑器：规则 / 旧阶段回归 / 三次重新进入
python Scripts/run_m5_acceptance.py
python Scripts/run_m5_regression.py M4
python Scripts/run_m5_regression.py M3
python Scripts/run_m5_reentry.py
# 有图形编辑器中，真实 UMG 操作及 AI 造成致死伤害的闭环截图
python Scripts/run_m5_demo.py
```

规则允许 `-nullrhi`，图形演示禁止该参数。M4 旧脚本仅把两项“AI 尚不可用”预期更新为 M5 的可用状态，105 项规则断言保留；报告归档为 M5_M4_Regression，不覆盖历史 M4 证据。脚本临时参数在结束时恢复，不在验收中保存资产。首次 cook 会大量编译着色器，精确时序回归应与 cook 分开运行。

独立包构建（先关闭本项目编辑器，避免编辑器 MCP 插件端口冲突；项目根目录 PowerShell）：

```powershell
& F:/GameStudy/UE_5.8/Engine/Build/BatchFiles/RunUAT.bat BuildCookRun `
  -project=F:/GameStudy/JJK_theHiddenOnes/JJK_theHiddenOnes.uproject `
  -noP4 -platform=Win64 -clientconfig=Development -build -skipbuildeditor `
  -cook -map=/Game/Maps/L_TrainingArena -stage -pak -archive `
  -archivedirectory=F:/GameStudy/JJK_theHiddenOnes/Saved/Packages/M5 -unattended -utf8output
python Scripts/run_m5_package.py
```

`-skipbuildeditor` 要求编辑器模块已经由最终源码正式构建。包位于 `Saved/Packages/M5/Windows`；普通游戏从顶层 exe 启动。`run_m5_package.py` 显式传 `-M5SmokeTest`，通过包内开发验收器检查 14 项并自动退出，包含真实伤害、结果面板、三种胜负/三次重开与模式重入。未传该参数的普通游戏不运行自动测试；Shipping 中该入口无效。`--nullrhi` 可做无图形包内规则复查，但不能替代图形启动与截图。

原生 BT/Blackboard、具体参数和 M6 技能任务接入点见本地M5 文档（`../Docs/开发过程/M5_AI对战.md`，不随 Git 分发）。


### M5 体验修复（2026-09-17）

- `M5_followup_assets.py` 建立明确的占位冲刺/后撤动画，仅修改 Training 下资源。
- `python Scripts/ue_python.py Scripts/M5_followup_validate_assets.py`：冷启动、非 PIE 时核对真实保存的动画引用及默认资源/闪避参数，打包前执行。
- `python Scripts/run_m5_followup.py`：图形 PIE，Slate 真实 F1/W/Shift 路由、动画姿势和常驻 HUD 检查。
- `python Scripts/run_m5_followup.py M3` / `M4` / `M5`：使用原规则脚本生成独立 Followup 报告，保留历史 M5 证据。目标帧率规则在 nullrhi 编辑器中跑；视觉与按键另用图形 PIE。
- `python Scripts/run_m5_package.py --report-prefix M5_Followup`：对更新后的同目录 Development 包生成独立报告。
- 测试配置必须恢复后再保存/打包；M3/M4 的 DodgeConfig/ThrowConfig 使用文本快照，避免可变结构留下临时参数。异常中止后先清测试回调，必要时重启编辑器，不能在持续脚本报错的会话中继续计验收结果。


## Feedback Agent A（2026-09-30）

- `run_feedback_a.py`：串行创建新 PIE、运行指定测试、等待 JSON 并停止 PIE。先打开本项目 Editor，执行 `python Scripts/run_feedback_a.py regression`；`edge_cases`、`reentry`、`baseline_samples` 同理。
- `feedback_a_regression.py`：结果/局部时钟/缓存/重置/PaidQ 回归，输出 `Saved/FeedbackA/regression.json`。临时简化渲染只为高帧率时钟检查，实际达到值写入报告，完成后恢复。
- `feedback_a_edge_cases.py`：真实互击、免疫/霸体/致死、远程各结果；单独标识合成停顿上限/播速压力。输出 `edge_cases.json`。
- `feedback_a_reentry.py`：新 PIE 与领域世界时钟/清理检查；需要三次时依次运行并复制每次报告，禁止并发 PIE。
- `feedback_a_baseline_samples.py`：真实 A1/重拳/超级炮入口与 Master Mixer 录制尝试。必须检查 `audio_file_exists`；没有 WAV 不能写成有声验收。

这些脚本只改测试会话，不保存招式/角色资产；所有输出在 `Saved/FeedbackA/`。正式 B–F 资产未接入时，测试不代表正式声画或独立包验收。

## Feedback Agent A 第二阶段（2026-09-30）

先构建当前 `JJK_theHiddenOnesEditor Win64 Development`，打开 `/Game/Maps/L_DojoArena`，确认没有已有 PIE。以下入口串行执行，不能同时操作 Editor/Cook。

- `python Scripts/CombatFeedback/A2_run.py`：从磁盘正式绑定开始，先验证 A1/重拳/超级炮，再检查四拳/三腿/重踢、格挡完整接续、移动炮/领域、20 次混合重置及重置后样板。记录真实 Master Mixer WAV 和截图，不注入接触结果。
- `python Scripts/CombatFeedback/A2_run.py extended`：超级炮 Guard/Immune/WorldImpact/Expire/炮口拒绝；真实背击和致死对格挡动作的优先级，以及 Reaction 关闭清理。
- `python Scripts/CombatFeedback/A2_run.py loop`：真实近战/格挡→合法蓄力取消→手动炮→领域→真实伤害 KO/胜负→重开后不再重置地使用三个样板。领域起始能量 100 为临时布景，生命/咒力正常；暂停 AI 决策来固定接触位置，不注入死亡，不代替正常资源真人 AI 对局。由 `A2_loop.py` 实现，录音与真实接触序列同目录保存。
- `python Scripts/CombatFeedback/A2_run.py shift`：`A2_shift.py` 检查两类炮在蓄力/前摇阶段的成功闪避中断、旧松键、下一次新会话、行动资源只扣一次和原移动速度恢复；实际空中状态拒绝闪避时原持炮仍可释放，成功 Fire 后闪避不取消在途弹体。
- `python Scripts/CombatFeedback/A2_replay.py <case>`：在正式绑定上重跑已有用例。支持 `a-clock/a-edge/a-reentry/c-main/c-extra/d-main/e-edges/e-main/e-reentry/ig20/ig21`。使用 A2 的设置保存/恢复，不调用 B–E 的临时接线脚本；旧位置参数 `Rotator` 布景在内存中改为命名参数。CONTRACT/合成停顿的标记保留，不能当作真实命中证据。
- `python Scripts/CombatFeedback/A2_validate.py`：按固定顺序运行余下回归；也可在命令末尾列出选定 case。每个子测试独占一次 PIE，确认上个测试已恢复设置后才继续，记录每次 DLL 哈希。`A2_run` 与 `A2_replay` 都把证据保存在 `Saved/FeedbackA2/` 的独立时间目录。
- `python Scripts/ue_python.py Scripts/CombatFeedback/A2_audit.py`：冷启动、非 PIE 时核对保存的角色绑定、公共 Profile、受击库、全部反馈硬依赖、近战参数和通道默认值。引用可达不代表 Cook/独立包验收通过。
- `A2_setup.py / A2_cleanup.py`：由运行器调用，保存/恢复后台声音、编辑器节流、窗口和 CVar，只重载本次测试的 Training 数据资产；不保存测试数值。`A2_pie.py / A2_extended.py` 由运行器启动 Slate 回调，不直接从终端执行。
- `python Scripts/ue_python.py Scripts/CombatFeedback/A2_bind.py`：A 的正式资产接线入口，**会保存** `DA_Fighter_Ishigori` 和 `DA_CombatFeedback_Ishigori`；已经接线的候选无需重复运行。
- `A2_snapshot.py baseline`：开发前保存当前脏工作区哈希与可恢复原件。`A2_freeze.py`：文档和回归完成后创建一次候选快照、差异和保留检查；不会覆盖已有冻结快照。`A2_release_editor.py`：确认无 PIE、无脏包后记录交接并退出 Editor。

最终同版范围、未通过项和 F 入口以本地A 第二阶段交接（`../Docs/打击感开发/Agent_A_战斗底座与集成.md`，不随 Git 分发）为准。没有真人听音/键鼠、声画同步完整录像、性能对照或新 Cook 包时，报告必须保留这些待办。

## R1 试玩反馈修复（2026-10-02）

- `R1_apply_assets.py`：非 PIE 时保存本项目的顿帧 Profile、重受击动作、派生粒子及角色的两处光束引用。只操作项目派生特效，保留厂商原件和音频；不要拿来覆盖其他版本。
- `python Scripts/CombatFeedback/R1_run.py`：真实共享输入验证轻重击、局部时钟、连段、命中/撞墙尾效和实际 30/60/120 FPS。未达到目标帧率要保留失败，不能只按 `t.MaxFPS` 数值判断通过。
- `R1_run.py --capture --visual-only`：单独图形检查；截图取样会造成长帧，不计性能验收。`R1_replay.py <case>` 重跑 A/C/E 回归并独立归档，E 采用实际 1920×1080 窗口布景。
- `python Scripts/ue_python.py Scripts/CombatFeedback/R1_audit.py`：保存后的硬依赖与派生光束引用核对。`R1_snapshot.py --freeze / --verify`：分别冻结构建输入、核对厂商素材/音频及构建输入保留情况，拒绝覆盖旧冻结记录。
- `R1_release_editor.py`：核对无 PIE/脏包后退出。`powershell -File Scripts/CombatFeedback/R1_build.ps1`：独立构建到 `Saved/Packages/FeedbackR1_20261002/Windows/`，保留旧 F 包。
- `R1_cook_audit.py` 使用本轮 `package-contents.csv`；`R1_package_smoke.py` 在新包两张地图开启顿帧运行有声图形烟测，明确含 debug 布景，不代表真人体验通过。所有新证据放在 `Saved/FeedbackRevisionR1/`。

## 菜单 UI Demo

- `python -X utf8 Scripts/build_menu_ui.py`：从批准设计导出带本地 UE 桥接的运行 HTML。
- `powershell -ExecutionPolicy Bypass -File Scripts/build_menu_demo.ps1`：完整构建、cook、stage、pak 到新目录，保留旧包。
- `Scripts/menu_ui_start_pie.py`：配置 1920×1080 PIE 窗口；随后通过 MCP 启动 `PlayMode_InEditorFloating`。
- `Scripts/menu_ui_acceptance.py`：在 PIE 中检查真实菜单流程、设置生效与保存，并捕获所属游戏窗口；会恢复测试前配置文件。
- `node Scripts/verify-menu-bridge.cjs`：Edge 下验证自由训练选择 AI 对手后仍保留训练会话标签。
- `python -X utf8 Scripts/run_menu_package_smoke.py <顶层exe>`：独立启动打包结果，在隔离 UserDir 下检查菜单资源、默认页面与暂停状态，截图后退出。它不检查主菜单点击开始后的出生、移动与对战；这些需要包内完整流程验证。
