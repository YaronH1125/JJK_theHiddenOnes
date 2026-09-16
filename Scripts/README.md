# UE 自动化与 M1 / M2 验收

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

运行结果、参数与已知限制见 [M1 记录](../Docs/开发过程/M1_基础训练擂台.md)；排障见 [处理回复](../Docs/求救信/M1_UE编辑器自动化通道_处理回复_2026-09-15.md)。

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

测试临时修改配置后会恢复，严禁在测试运行期间保存资产。菜单接入 `SetCombatInputEnabled`，对手停止决策接入 `SetRequestsEnabled`；训练重置仍用 `ResetTraining`。详见 [M3 验收与 M4 交接](../Docs/开发过程/M3_连招与核心攻防.md)。


## M4 训练系统

先用最终 C++ 源码正式构建，启动本项目编辑器；同一时间只运行一个验收脚本，不在测试进行中保存资产。

```powershell
python Scripts/run_m4_acceptance.py  # 新 PIE：M4-T01—T12，含至少 20 次重置与 UI 换绑
python Scripts/run_m4_regression.py  # 新 PIE：原 M3 118 项；结果独立归档，不覆盖历史 M3
python Scripts/run_m4_demo.py        # 必须有图形渲染：UMG 控件回调闭环与实际面板截图
```

规则检查可用 `-nullrhi`；图形演示不可使用该参数。M4 脚本临时修改定义初值和受击时长后会恢复，最终证据写入 `Docs/开发过程/验收记录/M4_*`。报告采用临时文件替换，避免 runner 在写入中读取半份 JSON；失败报告单独保留。回归 runner 对旧 M3 脚本的非原子报告读取做重试。

游戏内 **F1** 打开训练面板。面板原生 UMG 控件无需另跑资源创建脚本；`TrainingPanelClass` 可由 Controller 薄蓝图替换。默认所有训练开关关闭。非 Shipping 构建的“开发测试技能”用真实 GE 消耗行动资源 1、咒力 10、领域能量 5，冷却 3 秒；默认领域能量 0 时正常拒绝，可先开无限资源试验。它不代表正式炮击已实现。

自动化使用真实 PIE/ASC/碰撞/动画和 UMG 控件回调，不模拟操作系统实体键鼠。规则、统计口径和 M5/M6 交接见 [M4 记录](../Docs/开发过程/M4_训练系统.md)。
