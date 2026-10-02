# F 验收工具

职责：冻结候选核对、串行图形回归、同版构建/Cook审计、性能与原声采集。证据全部在 `Saved/CombatFeedback/F/`，禁止并发PIE/编译/Cook。生产代码/参数/资产不在F修改范围。既有A/E脚本不改。

- `python Scripts/CombatFeedback/F_snapshot.py --verify`：核对A冻结候选、Editor DLL、F起始保护快照；首次无参数只建立一次baseline，已有则拒绝覆盖。
- `powershell -File Scripts/CombatFeedback/F_build.ps1`：关闭Editor后新build/cook/stage/archive。既有F包拒绝覆盖；另版本必须选新目录。旧外部清单Frost差异保留，不能凭build成功消除它。
- `python Scripts/CombatFeedback/F_regress.py`：MCP已就绪且无PIE时串行执行A新回归/回放，复制新证据、保留失败，不引用历史通过。
- `python Scripts/CombatFeedback/F_replay_viewport.py e-main`：只在内存适配运行器高度，使实际视口1920×1080；随后重跑e-edges/e-reentry。
- `python Scripts/CombatFeedback/F_perf_run.py`：High、真实1920×1080，off/std热身后各60秒，临时训练fixture；`cold-off`仅首发。`compare`以共享输入真实攻击比较同候选五通道开关，等待精确compare_run录制握手（最多300秒），不是历史版本/真人A/B。`optional`用瞬态clone移除PunchHit声，仅组件指针变化，验证真实伤害/恢复及恢复引用，生产Profile不保存。
- `F_package_smoke.py`：新包两地图渲染/开音频烟测，含旧debugfixture、隐藏窗口；不得计真人或正常AI三局。
- `F_cook_audit.py`：最终IoStore内容/硬依赖/包文件哈希。
- `F_capture.py`：bundled Python3.12、本地Saved依赖 PyAudioWPatch/imageio_ffmpeg；读 `capture-request.json`，系统WASAPI原声+游戏区域录制。**使用method=desktop和新截图得到的physical_box**。标题GDI捕获旧场景；DDA在当前虚拟显示上丢失访问，都不能算本轮视频。必须核验解码帧数及动作变化，再判断视频有效。工具不注入玩法输入，不证明人耳听过。
- `F_release_editor.py`：确认无PIE/无dirty内容与默认资源/通道后写交还记录并正常关闭Editor。
- `F_finalize.py`：只在最终证据/报告齐备后建立 `final-summary.json`，索引最新有效重测而保留原失败；核对UTF8无BOM与F产物/录像哈希，不重写玩法预期。

最终结论见 `Docs/开发过程/验收记录/打击感首轮/F_验收报告.md`。失败、CONTRACT、训练条件和真人未执行标记须保留。
