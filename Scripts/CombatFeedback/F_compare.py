"""Same-candidate feedback off/on sample A/B, actual combat, fixed output gain.
Launched by F_compare_run.py; not a legacy-build or human subjective comparison.
"""
from pathlib import Path
import json
exec(compile((Path(__file__).parent/'A2_pie.py').read_text(encoding='utf-8').split('gen=suite();started=')[0],str(Path(__file__).parent/'A2_pie.py'),'exec'))
out=Path(json.loads((Path(unreal.Paths.project_saved_dir())/'CombatFeedback/F/compare-pointer.json').read_text(encoding='utf-8'))['path'])
report={'status':'running','candidate':'FA2-20260930-v2','checks':[],'contacts':[],'captures':[],'timeline':[],
        'comparison':'same candidate, feedback off -> default standard; not a rebuilt pre-feedback baseline',
        'output_gain':1,'hitstop':0,'human_input':False,'human_listening':False}
last_contacts=[fp.contact_count,fq.contact_count]
def suite():
    cmd('t.MaxFPS 60');cmd('JJK.Feedback.AudioGain 1');cmd('JJKCameraStrength 2');cmd('JJK.Feedback.HitStop 0')
    # Recording and remote-execution startup vary. Wait for this exact run's
    # independent recorder, rather than assuming a fixed launch delay suffices.
    ready=Path(unreal.Paths.project_saved_dir())/'CombatFeedback/F/recorder-ready.json'
    def armed():
        try:return json.loads(ready.read_text(encoding='utf-8')).get('compare_run')==str(out)
        except (FileNotFoundError,json.JSONDecodeError):return False
    yield from until(armed,300)
    assert armed(),'F recorder did not arm this run; no incomplete A/B claim'
    report['recorder']=json.loads(ready.read_text(encoding='utf-8'));write()
    yield from wait(3)
    for enabled in [False,True]:
        for key in ['Reaction','Audio','RangedFX','Camera','HUD']:cmd('JJK.Feedback.'+key+' '+str(int(enabled)))
        setting='standard' if enabled else 'off'
        for action,method,damage in [('A1','submit_light_attack',35),('HeavyPunch','submit_heavy_punch',90),('SuperBlast',None,220)]:
            reset(600 if method is None else 120);yield from wait(.6)
            mark(setting+'-'+action)
            source,target=(p,q) if method else (q,p)
            feedback=source.get_combat_feedback();count=feedback.contact_count;before=hp(target)
            audio=source.get_component_by_class(unreal.CombatAudioConsumer);cues=audio.played_cue_count
            if method:getattr(source.get_combat_input(),method)()
            else:
                yield from stance(source);source.get_combat_input().notify_kick_pressed()
                yield from until(lambda:feedback.last_action.paid_q>.999,5)
                source.get_combat_input().notify_kick_released()
            yield from until(lambda:feedback.contact_count>count,3)
            check(setting+' '+action+' one real original damage',feedback.contact_count==count+1 and hp(target)==before-damage)
            check(setting+' '+action+' audio follows switch',audio.played_cue_count>cues if enabled else audio.played_cue_count==cues)
            shot_image(setting+'-'+action);yield from wait(1.5)
    for key in ['Reaction','Audio','RangedFX','Camera','HUD']:cmd('JJK.Feedback.'+key+' 1')
    yield from wait(20)

gen=suite();started=time.monotonic();report['script_start_wall']=time.time()
def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    gm.set_opponent_mode(unreal.OpponentMode.STATIC);gm.reset_training()
    report['seconds']=time.monotonic()-started;write()
def tick(dt):
    try:
        sample()
        if time.monotonic()-started>360:raise TimeoutError('F sample A/B')
        next(gen)
    except StopIteration:report['status']='passed' if all(c['passed'] for c in report['checks']) else 'failed';finish()
    except Exception:report['status']='failed';report['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
