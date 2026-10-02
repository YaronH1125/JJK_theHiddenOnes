"""Record actual A1/heavy/super-blast master audio before the feedback build.

Run with python Scripts/run_feedback_a.py baseline_samples.
This is game-mixer output, not a claim of human listening or a desktop video.
"""
import json
import time
import traceback
from pathlib import Path
import unreal

w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc = unreal.GameplayStatics.get_player_controller(w, 0)
gm = pc.get_training_game_mode()
p, q = gm.get_player_fighter(), gm.get_opponent_fighter()
out = Path(unreal.Paths.project_saved_dir()) / 'FeedbackA'
report = {'status': 'running', 'samples': [], 'audio': 'master mixer, no human listening'}
perf = unreal.load_object(None, '/Script/UnrealEd.Default__EditorPerformanceSettings')
old = perf.get_editor_property('bThrottleCPUWhenNotForeground')
perf.set_editor_property('bThrottleCPUWhenNotForeground', False)
def now(): return unreal.GameplayStatics.get_time_seconds(w)
def wait(t):
    end = now()+t
    while now()<end: yield
def hp(f): return f.get_fighter_attribute_set().get_editor_property('health').get_editor_property('current_value')
def reset(gap=150):
    gm.reset_training(); pc.set_combat_input_enabled(True)
    p.set_actor_location(unreal.Vector(0,0,100),False,True)
    q.set_actor_location(unreal.Vector(gap,0,100),False,True)
    p.set_actor_rotation(unreal.Rotator(pitch=0,yaw=0,roll=0),False)
    q.set_actor_rotation(unreal.Rotator(pitch=0,yaw=180,roll=0),False)
def suite():
    unreal.SystemLibrary.execute_console_command(w, "au.NeverDisableSubmixes 1")
    yield from wait(.2)
    unreal.AudioMixerLibrary.start_recording_output(w, 15)
    for label,fn in [('A1','submit_light_attack'),('HeavyPunch','submit_heavy_punch')]:
        reset(); yield from wait(.3)
        before=hp(q); getattr(p.get_combat_input(),fn)(); yield from wait(1.5)
        report['samples'].append({'move':label,'damage':before-hp(q)})
    reset(600); yield from wait(.3)
    q.request_stance_switch(); yield from wait(.6)
    before=hp(p); q.get_combat_input().notify_kick_pressed()
    yield from wait(3.3); q.get_combat_input().notify_kick_released(); yield from wait(3)
    report['samples'].append({'move':'SuperBlast','damage':before-hp(p)})
    unreal.AudioMixerLibrary.stop_recording_output(w, unreal.AudioRecordingExportType.WAV_FILE, 'baseline-samples', str(out))
    yield from wait(.5)
    report['audio_file_exists']=(out/'baseline-samples.wav').exists()
    report['status']='passed' if all(s['damage']>0 for s in report['samples']) else 'failed'
gen=suite(); started=time.monotonic()
def write(): (out/'baseline_samples.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
def tick(dt):
    try: next(gen)
    except StopIteration: finish()
    except Exception:
        report['status']='failed';report['error']=traceback.format_exc();finish()
def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    perf.set_editor_property('bThrottleCPUWhenNotForeground',old)
    unreal.SystemLibrary.execute_console_command(w, 'au.NeverDisableSubmixes 0')
    gm.reset_training();pc.set_combat_input_enabled(True)
    report['seconds']=time.monotonic()-started;write()
write();handle=unreal.register_slate_post_tick_callback(tick)
