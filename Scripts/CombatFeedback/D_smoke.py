"""Short real-charge mixer diagnosis. python Scripts/CombatFeedback/D_run.py smoke"""
import json
from pathlib import Path
import time
import traceback
import unreal
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(w,0);gm=pc.get_training_game_mode()
p,q=gm.get_player_fighter(),gm.get_opponent_fighter()
aq=q.get_component_by_class(unreal.CombatAudioConsumer)
out=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()))/'FeedbackD'
report={'status':'running','checks':[],'device':[]}
def write():(out/'smoke-report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
def now():return unreal.GameplayStatics.get_time_seconds(w)
def wait(t):
    end=now()+t
    while now()<end:yield
def suite():
    unreal.SystemLibrary.execute_console_command(w,'t.MaxFPS 60')
    gm.reset_training();pc.set_combat_input_enabled(True)
    q.get_combat_input().notify_stance_switch_pressed();yield from wait(.7)
    unreal.AudioMixerLibrary.start_recording_output(w,10.)
    q.get_combat_input().notify_attack_pressed();yield from wait(2.)
    report['device']=list(unreal.CombatAudioDiagnostics.describe_audio_device(w))
    report['checks'].append({'name':'Recording fixture has unmuted primary volume','passed':any('primary_volume=1.0000' in s and 'muted=0' in s for s in report['device'])})
    report['checks'].append({'name':'Actual single charge audio component is playing','passed':aq.is_charge_loop_playing(),'paid_q':aq.loop_paid_q,'gain':aq.loop_gain})
    yield from wait(3.)
    q.get_combat_input().notify_attack_released();yield from wait(1.)
    unreal.AudioMixerLibrary.stop_recording_output(w,unreal.AudioRecordingExportType.WAV_FILE,'D-smoke',str(out))
    yield from wait(.5)
    report['wav']=str(out/'D-smoke.wav')
gen=suite();start=time.monotonic()
def tick(dt):
    try:
        if time.monotonic()-start>30:raise TimeoutError()
        next(gen)
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle);gm.reset_training()
        report['status']='passed' if all(x['passed'] for x in report['checks']) else 'failed';write()
    except Exception:
        unreal.unregister_slate_post_tick_callback(handle);gm.reset_training()
        report['status']='failed';report['error']=traceback.format_exc();write()
write();handle=unreal.register_slate_post_tick_callback(tick)
