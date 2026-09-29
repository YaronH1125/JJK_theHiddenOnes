"""Apply the Doc-19 melee fixes to derived assets only, with a disk backup."""
import json
import shutil
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE first'
root = Path(unreal.Paths.project_dir())
backup = root / 'Saved/IG20/assets_before'
com = '/Game/Characters/Ishigori/Repaired/Combat'
base = '/Game/Characters/Ishigori/Repaired'
for folder in ['Content/Training', 'Content/Characters/Ishigori/Repaired']:
    for p in (root / folder).rglob('*.uasset'):
        dest = backup / p.relative_to(root)
        if not dest.exists():
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(p, dest)

# K2/K3 were never retargeted by the first integration.
for name in ['KB_m_MidKickStraight_R', 'KB_m_RoundhouseKickRight']:
    target = f'{com}/IG_{name}'
    if not unreal.EditorAssetLibrary.does_asset_exist(target):
        inp = unreal.IKRetargetBatchOperationInputs()
        inp.source_mesh = unreal.load_asset('/Game/FightingAnimsetPro/UE4_Mannequin/Mesh/SK_Mannequin')
        inp.target_mesh = unreal.load_asset(base + '/SK_Ishigori_Repaired')
        inp.ik_retarget_asset = unreal.load_asset(base + '/RT_FASP_Ishigori_Repaired')
        inp.assets_to_retarget = [unreal.AssetRegistryHelpers.get_asset_registry().get_asset_by_object_path(
            f'/Game/FightingAnimsetPro/Animations/InPlace/{name}.{name}')]
        inp.target_path = com
        inp.prefix = 'IG_'
        unreal.IKRetargetBatchOperation().run_batch_retarget(inp)
        assert unreal.EditorAssetLibrary.does_asset_exist(target), target

# Montage slices preserve the strike and speed up the recovery rather than
# speeding up every part equally. Heavy punch omits its long extended-fist hold.
# (DA suffix, montage suffix, sequence, slices, socket, hit source range, combo start, hold)
jobs = [
    ('A1', 'A1', 'm_Jab_L', [(0, .4, 1.15), (.4, .9, 2)], 'hand_l', (.14, .34), .32, 0),
    ('A2', 'A2', 'm_Jab_R', [(0, .45, 1.15), (.45, .933333, 2)], 'hand_r', (.25, .42), .39, 0),
    ('A3', 'A3', 'm_Hook_L', [(0, .52, 1.1), (.52, 1.266666, 1.9)], 'hand_l', (.30, .49), .50, 0),
    ('A4', 'A4', 'm_Overhand_R', [(0, .6, 1.05), (.6, 1.6, 1.9)], 'hand_r', (.34, .49), 0, 0),
    ('Kick', 'Kick1', 'm_MidKick_L', [(0, .65, 1.25), (.65, 2.1, 2.3)], 'foot_l', (.34, .56), .73, 0),
    ('Kick2', 'Kick2', 'm_MidKickStraight_R', [(0, .6, 1.3), (.6, 1.583333, 1.9)], 'foot_r', (.27, .51), .69, 0),
    ('Kick3', 'Kick3', 'm_RoundhouseKickRight', [(0, .6, 1.2), (.6, 1.6, 1.8)], 'foot_r', (.26, .49), 0, 0),
    ('HeavyPunch', 'HeavyPunch', 'Superpunch', [(0, 1.25, 2.8), (1.25, 1.65, 1.2), (2.85, 3.65, 1.8)], 'hand_r', (1.37, 1.62), 0, 1.25),
    ('HeavyKick', 'HeavyKick', 'AxeKick', [(0, .38, 1.6), (.38, .64, 1.1), (.64, 2, 2)], 'foot_r', (.43, .57), 0, .38),
]
def timeline(slices, source_time):
    elapsed = 0
    for start, end, rate in slices:
        if start <= source_time <= end + .0001:
            return elapsed + (source_time - start) / rate
        elapsed += (end - start) / rate
    raise ValueError(source_time)

def save(obj):
    assert unreal.EditorAssetLibrary.save_loaded_asset(obj, only_if_is_dirty=False), obj.get_path_name()

report = {}
for suffix, mn, sn, slices, bone, hit, combo, hold in jobs:
    path = f'{com}/IG_AM_KB_{mn}'
    m = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not m:
        factory = unreal.AnimMontageFactory()
        factory.target_skeleton = unreal.load_asset(base + '/SK_Ishigori_Repaired_Skeleton')
        m = unreal.AssetToolsHelpers.get_asset_tools().create_asset('IG_AM_KB_' + mn, com, unreal.AnimMontage, factory)
    seq = unreal.load_asset(f'{com}/IG_KB_{sn}')
    assert seq and m
    m.modify()
    segments, length = [], 0.0
    for start, end, rate in slices:
        end = min(end, seq.get_play_length())
        seg = unreal.AnimSegment()
        seg.set_editor_property('anim_reference', seq)
        seg.import_text(f'(StartPos={length},AnimStartTime={start},AnimEndTime={end},AnimPlayRate={rate},LoopingCount=1,CachedPlayLength={seq.get_play_length()})')
        segments.append(seg)
        length += (end - start) / rate
    track = unreal.AnimTrack()
    track.set_editor_property('anim_segments', segments)
    slot = unreal.SlotAnimationTrack()
    slot.set_editor_property('slot_name', 'DefaultSlot')
    slot.set_editor_property('anim_track', track)
    m.set_editor_property('slot_anim_tracks', [slot])
    for prop, seconds in [('blend_in', .07), ('blend_out', .10)]:
        blend = m.get_editor_property(prop)
        blend.set_editor_property('blend_time', seconds)
        m.set_editor_property(prop, blend)
    save(m)
    # SequenceLength is read-only in Python. PostLoad recomputes it from tracks.
    unreal.EditorLoadingAndSavingUtils.reload_packages([m.get_outer()], unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
    m = unreal.load_asset(path)
    assert abs(m.get_play_length() - length) < .002, (path, m.get_play_length(), length)
    save(m)
    dap = '/Game/Training/DA_M3_' + suffix
    if not unreal.EditorAssetLibrary.does_asset_exist(dap):
        assert unreal.EditorAssetLibrary.duplicate_asset('/Game/Training/DA_M3_Kick', dap)
    da = unreal.load_asset(dap)
    da.modify()
    hit_start, hit_end = [timeline(slices, t) for t in hit]
    values = {'montage': m, 'trace_socket': unreal.Name(bone), 'window_from_anim_notifies': False,
              'window_start_time': hit_start, 'window_end_time': hit_end,
              'combo_window_start_time': combo, 'combo_window_end_time': length - .06 if combo else 0.0,
              'allow_next_segment': suffix in ['A1', 'A2', 'A3', 'Kick', 'Kick2'],
              'charge_hold_time': timeline(slices, hold) if hold else 0.0,
              'cancel_window_start_time': hit_end, 'cancel_window_end_time': length,
              }
    if suffix in ['Kick', 'Kick2', 'Kick3']:
        values.update(allow_heavy_transition=False, allow_kick_transition=False,
                      segment_id={'Kick': 4, 'Kick2': 7, 'Kick3': 8}[suffix],
                      display_name=unreal.Text({'Kick': 'K1 左中段踢', 'Kick2': 'K2 右直踢', 'Kick3': 'K3 回旋踢'}[suffix]))
    for key, value in values.items():
        da.set_editor_property(key, value)
    assert unreal.ToolsetLibrary.set_object_properties(da, json.dumps({'HitEffect': 'None'}))
    assert da.get_editor_property('hit_effect') is None
    save(da)
    report[suffix] = dict(length=length, hit=[hit_start, hit_end], combo=combo, hold=values['charge_hold_time'], socket=bone)

fd = unreal.load_asset('/Game/Training/DA_Fighter_Ishigori')
fd.set_editor_property('kick_segments', [unreal.load_asset('/Game/Training/DA_M3_' + n) for n in ['Kick', 'Kick2', 'Kick3']])
fd.set_editor_property('combo_cache_lifetime', .75)
save(fd)
# Remove every melee hit FX, including the legacy fallback attack asset.
for asset in unreal.EditorAssetLibrary.list_assets('/Game/Training', recursive=True):
    if '/DA_M' not in asset:
        continue
    obj = unreal.load_asset(asset)
    if isinstance(obj, unreal.AttackDefinition):
        assert unreal.ToolsetLibrary.set_object_properties(obj, json.dumps({'HitEffect': 'None'}))
        assert obj.get_editor_property('hit_effect') is None
        save(obj)
(root / 'Saved/IG20/tuning.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report))
