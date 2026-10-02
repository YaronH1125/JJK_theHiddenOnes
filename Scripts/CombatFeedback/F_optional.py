"""T16: transient missing PunchHit sound, actual A1, no saved asset mutation."""
from pathlib import Path
import json
exec(compile((Path(__file__).parent/'A2_pie.py').read_text(encoding='utf-8').split('gen=suite();started=')[0],str(Path(__file__).parent/'A2_pie.py'),'exec'))
out=Path(json.loads((Path(unreal.Paths.project_saved_dir())/'CombatFeedback/F/optional-pointer.json').read_text(encoding='utf-8'))['path'])
report={'status':'running','candidate':'FA2-20260930-v2','checks':[],'contacts':[],
        'fixture':'transient clone of audio profile; PunchHit Variants=[None]; actor component pointer only; no package save',
        'human_input':False,'human_listening':False}
saved_audio=pa.audio_profile
clone=unreal.new_object(unreal.CombatAudioProfile)
for prop in ['master_gain','charge_min_gain','charge_max_gain','charge_min_pitch','charge_max_pitch','swing_lead_seconds','super_duck_gain','super_duck_seconds']:
    clone.set_editor_property(prop,saved_audio.get_editor_property(prop))
cues=[]
for entry in saved_audio.cues:
    cue=unreal.CombatAudioCue()
    for prop in ['kind','variants','gain','pitch_variation','max_life','priority','attenuation','concurrency']:
        cue.set_editor_property(prop,entry.get_editor_property(prop))
    if cue.kind==K.PUNCH_HIT:cue.set_editor_property('variants',[None])
    cues.append(cue)
clone.set_editor_property('cues',cues)
def suite():
    reset();yield from wait(.5)
    pa.set_editor_property('audio_profile',clone)
    missing=pa.missing_cue_count;count=fp.contact_count;before=hp(q);played=pa.get_cue_play_count(K.PUNCH_HIT)
    p.get_combat_input().submit_light_attack();yield from until(lambda:fp.contact_count>count,2)
    check('T16 missing optional PunchHit preserves one true A1 damage35',fp.contact_count==count+1 and hp(q)==before-35)
    check('T16 absent sound reports MissingCueCount and no hit voice',pa.missing_cue_count==missing+1 and pa.get_cue_play_count(K.PUNCH_HIT)==played)
    report['missing_diagnostic']={'before':missing,'after':pa.missing_cue_count,'text_log_in_consumer':False}
    yield from wait(1.5);check('T16 absent sound does not lock recovery',q.can_act() and pa.active_one_shot_count==0)
    pa.set_editor_property('audio_profile',saved_audio)
    reset();yield from wait(.5);played=pa.get_cue_play_count(K.PUNCH_HIT);count=fp.contact_count
    p.get_combat_input().submit_light_attack();yield from until(lambda:fp.contact_count>count,2)
    check('T16 restoring original profile restores actual hit voice',fp.contact_count==count+1 and pa.get_cue_play_count(K.PUNCH_HIT)==played+1)
    yield from wait(1.5)
gen=suite();started=time.monotonic()
def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    pa.set_editor_property('audio_profile',saved_audio)
    gm.set_opponent_mode(unreal.OpponentMode.STATIC);gm.reset_training()
    report['seconds']=time.monotonic()-started;report['restored']=pa.audio_profile==saved_audio;write()
def tick(dt):
    try:
        if time.monotonic()-started>30:raise TimeoutError('F optional fixture')
        next(gen)
    except StopIteration:report['status']='passed' if all(c['passed'] for c in report['checks']) else 'failed';finish()
    except Exception:report['status']='failed';report['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
