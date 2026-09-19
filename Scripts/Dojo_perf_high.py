"""Compare project-side High scalability without editing any source art."""
import unreal,json,time,statistics
from pathlib import Path
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(w,0);gm=pc.get_training_game_mode()
keys=['sg.ViewDistanceQuality','sg.AntiAliasingQuality','sg.ShadowQuality','sg.GlobalIlluminationQuality','sg.ReflectionQuality','sg.PostProcessQuality','sg.TextureQuality','sg.EffectsQuality','sg.FoliageQuality','sg.ShadingQuality']
old={k:unreal.SystemLibrary.get_console_variable_int_value(k) for k in keys}
for k in keys:unreal.SystemLibrary.execute_console_command(w,k+' 2')
pc.set_training_panel_open(False);gm.set_opponent_mode(unreal.OpponentMode.STATIC);gm.reset_training()
out=Path(unreal.Paths.project_saved_dir())/'Dojo_perf_high.json';out.write_text('{"status":"running"}',encoding='utf-8')
started=time.monotonic();samples=[]
def tick(dt):
    elapsed=time.monotonic()-started
    if elapsed>10:samples.append(dt)
    if elapsed<70:return
    unreal.unregister_slate_post_tick_callback(handle)
    s=sorted(samples)
    result={'status':'passed','map':w.get_path_name(),'viewport':str(pc.get_viewport_size()),'scalability':2,'frames':len(s),'seconds':sum(s),'mean_ms':statistics.mean(s)*1000,'p95_ms':s[int(len(s)*.95)]*1000,'max_ms':max(s)*1000,'mean_fps':1/statistics.mean(s),'note':'Measurement completed; not a claim of meeting a frame-rate budget.'}
    for k,v in old.items():unreal.SystemLibrary.execute_console_command(w,k+' '+str(v))
    out.write_text(json.dumps(result,indent=2),encoding='utf-8')
handle=unreal.register_slate_post_tick_callback(tick)
