"""强制重载被删建循环污染的消费者资产 → 重编译 ABP → 保存。

    python Scripts/ue_python.py Scripts/M8_reload_consumers.py
"""
import json
import unreal

R = {}

paths = ['/Game/Characters/Ishigori/ABP_Ishigori',
         '/Game/Characters/Ishigori/BS_Ishigori_Idle_Walk_Run',
         '/Game/Training/AM_M2_A1',
         '/Game/Training/AM_M2_HitReact',
         '/Game/Training/Movement/AM_Dodge']

pkgs = []
for p in paths:
    obj = unreal.load_asset(p)
    if obj:
        pkgs.append(obj.get_package())
R['packages'] = [pkg.get_name() for pkg in pkgs]

try:
    ok = unreal.EditorLoadingAndSavingUtils.reload_packages(pkgs, False)
    R['reload'] = ok
except Exception as e:
    R['reload'] = f'<ERR {e}>'

# 重编译
abp = unreal.load_asset('/Game/Characters/Ishigori/ABP_Ishigori')
try:
    R['compile'] = unreal.BlueprintEditorLibrary.compile_blueprint(abp)
except Exception as e:
    R['compile'] = f'<ERR {e}>'

# 保存全部消费者
for p in paths:
    if unreal.EditorAssetLibrary.does_asset_exist(p):
        unreal.EditorAssetLibrary.save_asset(p, only_if_is_dirty=False)
R['saved'] = True

print('M8RL_BEGIN')
print(json.dumps(R, ensure_ascii=False, indent=1, default=str))
print('M8RL_END')
