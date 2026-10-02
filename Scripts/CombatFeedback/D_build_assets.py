"""Import D PCM sources and create D-owned mix/profile assets. Never save shared DAs.

Idle Editor after D C++ build: python Scripts/ue_python.py Scripts/CombatFeedback/D_build_assets.py
Writes only /Game/CombatFeedback/Audio and /Game/CombatFeedback/Profiles/Audio.
Evidence: Saved/FeedbackD/build-assets.json. Reruns update the same owned assets.
"""
import json
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
out = root / 'Saved/FeedbackD'
rows = json.loads((out/'audio-analysis.json').read_text(encoding='utf-8'))
audio = '/Game/CombatFeedback/Audio'
profiles = '/Game/CombatFeedback/Profiles/Audio'
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
for folder in [audio, profiles]: unreal.EditorAssetLibrary.make_directory(folder)

def create(name, cls, factory):
    path = audio + '/' + name
    return unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else asset_tools.create_asset(name,audio,cls,factory)

classes = {name:create('SC_D_'+name,unreal.SoundClass,unreal.SoundClassFactory()) for name in ['Swing','Impact','Charge','Danger','State']}
attenuations = {}
for name,radius,falloff in [('Melee',350,1500),('Danger',650,3500),('Charge',350,2300)]:
    asset = create('SA_D_'+name,unreal.SoundAttenuation,unreal.SoundAttenuationFactory())
    settings = asset.get_editor_property('attenuation')
    settings.set_editor_property('attenuate',True)
    settings.set_editor_property('spatialize',True)
    settings.set_editor_property('distance_algorithm',unreal.AttenuationDistanceModel.LOGARITHMIC)
    settings.set_editor_property('attenuation_shape_extents',unreal.Vector(radius,0,0))
    settings.set_editor_property('falloff_distance',float(falloff))
    asset.set_editor_property('attenuation',settings)
    attenuations[name]=asset

concurrencies = {}
for name,count,owner in [('Swing',4,False),('Impact',4,False),('Charge',1,True),('Danger',4,False),('State',4,False)]:
    asset=create('SCO_D_'+name,unreal.SoundConcurrency,unreal.SoundConcurrencyFactory())
    settings=asset.get_editor_property('concurrency')
    # UE 5.8 factory enables platform scaling (default 16), which overrides MaxCount.
    settings.set_editor_property('enable_max_count_platform_scaling',False)
    platform_count=unreal.PerPlatformInt()
    platform_count.set_editor_property('default',count)
    settings.set_editor_property('platform_max_count',platform_count)
    settings.set_editor_property('max_count',count)
    settings.set_editor_property('limit_to_owner',owner)
    settings.set_editor_property('resolution_rule',unreal.MaxConcurrentResolutionRule.STOP_LOWEST_PRIORITY)
    settings.set_editor_property('voice_steal_release_time',.015)
    asset.set_editor_property('concurrency',settings)
    concurrencies[name]=asset

waves={}
for row in rows:
    task=unreal.AssetImportTask()
    task.filename=str(root/row['path'])
    task.destination_path=audio
    task.destination_name=row['name']
    task.automated=True;task.replace_existing=True;task.save=False
    asset_tools.import_asset_tasks([task])
    wave=unreal.load_asset(audio+'/'+row['name'])
    assert isinstance(wave,unreal.SoundWave), row
    wave.set_editor_property('looping',row['loop'])
    wave.set_editor_property('volume',1.)
    wave.set_editor_property('pitch',1.)
    wave.set_editor_property('loading_behavior',unreal.SoundWaveLoadingBehavior.FORCE_INLINE)
    waves[row['name']]=wave

mapping = [
    ('PUNCH_SWING',['PunchSwing_1','PunchSwing_2'],'Swing','Melee',.48,1),
    ('KICK_SWING',['KickSwing_1','KickSwing_2'],'Swing','Melee',.52,1),
    ('PUNCH_HIT',['PunchHit_1','PunchHit_2'],'Impact','Melee',.66,3),
    ('KICK_HIT',['KickHit_1','KickHit_2'],'Impact','Melee',.66,3),
    ('HEAVY_HIT',['HeavyHit_1','HeavyHit_2'],'Impact','Melee',.72,3.5),
    ('GUARD',['Guard_1','Guard_2'],'Impact','Melee',.66,3),
    ('IMMUNE',['Immune'],'State','Melee',.40,4),
    ('CHARGE_START',['ChargeStart'],'State','Charge',.45,2),
    ('CHARGE_LOOP',['ChargeLoop'],'Charge','Charge',.65,2),
    ('CHARGE_FULL',['ChargeFull'],'State','Charge',.55,4),
    ('CHARGE_CANCEL',['ChargeCancel'],'State','Charge',.35,2),
    ('MOBILE_FIRE',['MobileFire'],'Danger','Danger',.66,4),
    ('SUPER_FIRE',['SuperFire'],'Danger','Danger',.72,4.5),
    ('RANGED_HIT',['RangedHit'],'Impact','Danger',.65,3.5),
    ('WORLD_IMPACT',['WorldImpact'],'Impact','Danger',.50,2),
    ('EXPIRE',['Expire'],'State','Danger',.35,1),
    ('DOMAIN_START',['DomainStart'],'State','Danger',.60,4),
    ('DOMAIN_END',['DomainEnd'],'State','Danger',.40,2),
]
cue_rows=[];cues=[]
for kind,names,group,atten,gain,priority in mapping:
    cue=unreal.CombatAudioCue()
    cue.set_editor_property('kind',getattr(unreal.CombatAudioCueKind,kind))
    sounds=[waves['SW_D_'+name] for name in names]
    for sound in sounds: sound.set_editor_property('sound_class_object',classes[group])
    cue.set_editor_property('variants',sounds)
    cue.set_editor_property('gain',gain)
    cue.set_editor_property('priority',priority)
    cue.set_editor_property('pitch_variation',0. if kind=='CHARGE_LOOP' else .02)
    cue.set_editor_property('max_life',max(next(r['duration'] for r in rows if r['name']==s.get_name()) for s in sounds)/.96+.05)
    cue.set_editor_property('attenuation',attenuations[atten])
    cue.set_editor_property('concurrency',concurrencies[group])
    cues.append(cue)
    cue_rows.append({'kind':kind,'sounds':[s.get_path_name() for s in sounds],'gain':gain,'priority':priority,'group':group,'attenuation':atten})

path=profiles+'/DA_CombatAudio'
profile=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
if not profile:
    factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.CombatAudioProfile)
    profile=asset_tools.create_asset('DA_CombatAudio',profiles,unreal.CombatAudioProfile,factory)
profile.set_editor_property('cues',cues)
for obj in list(classes.values())+list(attenuations.values())+list(concurrencies.values())+list(waves.values())+[profile]:
    assert obj.get_path_name().startswith('/Game/CombatFeedback/'),obj
    assert unreal.EditorAssetLibrary.save_loaded_asset(obj,only_if_is_dirty=False),obj
unreal.get_default_object(unreal.CombatAudioConsumer).set_editor_property('audio_profile',profile)
report={'waves':len(waves),'profile':profile.get_path_name(),'cues':cue_rows,
        'classes':[x.get_path_name() for x in classes.values()],
        'attenuation':{'Melee':[350,1500],'Danger':[650,3500],'Charge':[350,2300]},
        'master_gain':profile.master_gain,'no_existing_music_bus':True}
(out/'build-assets.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False))
