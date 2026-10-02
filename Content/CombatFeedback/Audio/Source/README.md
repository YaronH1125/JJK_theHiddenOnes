# D 音频源文件

这些 48 kHz / 16-bit / 单声道 WAV 是本项目首轮战斗声音样板。`manifest.json` 记录来源、处理、时长、峰值、RMS 和 SHA-256；对应 SoundWave 位于上一级 Audio 目录。

- PunchHit、KickHit、HeavyHit、Guard 各两种：D 用确定性噪声、短瞬态与衰减共振编写，属于程序合成样板；未声称是录音棚 Foley。
- PunchSwing、KickSwing：项目已有 GoodParticleBeamAndRay 的 Release_Wind_01 裁剪、单声道、滤波与包络变体。
- MobileFire、SuperFire、RangedHit、WorldImpact、Expire、ChargeStart、DomainStart/End：从项目已有 GoodParticleBeamAndRay / EnergyBeam 的 WAV 导出后派生，不改源包。
- ChargeLoop：已有 Electric 纹理与自制周期谐波组合，修复接缝；只有此文件在 UE 中开启循环。
- Full、Immune、Cancel：自制不同音高的短状态提示。资源受限不会触发 Full。

重建：先在空闲 UE 中执行 `python Scripts/ue_python.py Scripts/CombatFeedback/D_inventory.py`，再从项目根目录运行 `python Scripts/CombatFeedback/D_make_audio.py` 和 `python Scripts/ue_python.py Scripts/CombatFeedback/D_build_assets.py`。源文件不能替代 UE 的资产导入；运行期由 D 的硬引用 Profile 预加载。

原素材授权沿项目已有第三方包许可；交付、公开或分发派生素材时沿用该许可。没有使用影视音轨或下载外部音频。声音风格与耳机/扬声器听感仍需真人确认。
