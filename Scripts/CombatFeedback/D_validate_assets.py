"""Cold-start D-owned hard references, formats, loops and concurrency validation.

python Scripts/ue_python.py Scripts/CombatFeedback/D_validate_assets.py
Idle Editor, read-only. Report: Saved/FeedbackD/asset-validation.json.
"""
import json
from pathlib import Path
import unreal
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
out=Path(unreal.Paths.project_saved_dir())/'FeedbackD'
profile=unreal.load_asset('/Game/CombatFeedback/Profiles/Audio/DA_CombatAudio')
checks=[]
def check(n,b,d=None):checks.append({'name':n,'passed':bool(b),'detail':d})
check('Cold native CDO finds hard profile',unreal.get_default_object(unreal.CombatAudioConsumer).audio_profile==profile)
check('Every result/state cue exists exactly once',len(profile.cues)==18 and len({c.kind for c in profile.cues})==18)
for cue in profile.cues:
    check(str(cue.kind)+' hard settings',bool(cue.variants and cue.attenuation and cue.concurrency))
    for wave in cue.variants:
        check(wave.get_name()+' mono 48k ForceInline',wave.get_editor_property('num_channels')==1 and wave.get_editor_property('sample_rate')==48000 and wave.get_editor_property('loading_behavior')==unreal.SoundWaveLoadingBehavior.FORCE_INLINE)
        check(wave.get_name()+' loop only for charge',wave.get_editor_property('looping')==(cue.kind==unreal.CombatAudioCueKind.CHARGE_LOOP))
    if cue.kind in [unreal.CombatAudioCueKind.PUNCH_SWING,unreal.CombatAudioCueKind.KICK_SWING,unreal.CombatAudioCueKind.PUNCH_HIT,unreal.CombatAudioCueKind.KICK_HIT,unreal.CombatAudioCueKind.HEAVY_HIT,unreal.CombatAudioCueKind.GUARD]:
        check(str(cue.kind)+' two separate variant assets',len(cue.variants)>=2 and cue.variants[0]!=cue.variants[1])
    settings=cue.concurrency.concurrency
    check(str(cue.kind)+' effective MaxCount is not overridden by platform default',
          not settings.enable_max_count_platform_scaling and settings.platform_max_count.default==settings.max_count)
    if cue.kind==unreal.CombatAudioCueKind.CHARGE_LOOP:
        check('Charge concurrency limit is one per owner',settings.max_count==1 and settings.limit_to_owner)
    else:check(str(cue.kind)+' global group max four',settings.max_count==4 and not settings.limit_to_owner)
result={'status':'passed' if all(x['passed'] for x in checks) else 'failed','checks':checks}
(out/'asset-validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'status':result['status'],'checks':len(checks),'failed':[x for x in checks if not x['passed']]},ensure_ascii=False))
assert result['status']=='passed'
