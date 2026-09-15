# UE 自动化与 M1 验收

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
