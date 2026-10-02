"""Graphical F performance fixture, launched by F_perf_run.py in exclusive PIE.

Off/default-standard each get 10s warmup + >=60s actual world frames. High
scalability, 100% screen percentage. Shared attack inputs, real sweeps and
projectiles; temporary infinite resources/HP and fixed geometry are labelled.
This is repeatable workload cost, not normal-resource AI or human gameplay.
"""
import json
import math
from pathlib import Path
import statistics
import time
import traceback
import unreal

ROOT=Path(unreal.Paths.project_dir())
request=json.loads((ROOT/'Saved/CombatFeedback/F/perf-pointer.json').read_text(encoding='utf-8'))
OUT=Path(request['path'])
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(w,0);gm=pc.get_training_game_mode()
p,q=gm.get_player_fighter(),gm.get_opponent_fighter()
scales=['sg.'+x+'Quality' for x in ['ViewDistance','AntiAliasing','Shadow','GlobalIllumination','Reflection','PostProcess','Texture','Effects','Foliage','Shading']]
keys=scales+['r.ScreenPercentage','r.VSync','t.MaxFPS','JJK.Feedback.Log','JJK.Feedback.AudioLog','JJK.Feedback.CameraStrength','JJK.Feedback.AudioGain']+['JJK.Feedback.'+x for x in ['HitStop','Reaction','Audio','RangedFX','Camera','HUD']]
old={k:unreal.SystemLibrary.get_console_variable_float_value(k) for k in keys}
settings=gm.settings.copy();mode=gm.opponent_mode
audio=[f.get_component_by_class(unreal.CombatAudioConsumer) for f in [p,q]]
visual=[f.get_component_by_class(unreal.CombatRangedVisualConsumer) for f in [p,q]]
report={'status':'running','candidate':'FA2-20260930-v2','map':w.get_path_name(),'viewport':list(pc.get_viewport_size()),
        'fixture':'static target, real shared inputs/sweeps/projectiles, temporary infinite resources/health; no saved assets',
        'scalability':2,'screen_percentage':100,'cap':0,'human_input':False,
        'measurement':'world TimeSeconds deltas, one per graphical world frame, uncapped; Python instrumentation identical in both settings',
        'default_standard_hitstop':0,'cold_samples':[],'runs':[],'frames':[]}
last=unreal.GameplayStatics.get_time_seconds(w);frames=[];label='initial';phase='idle';run=None

def now():return unreal.GameplayStatics.get_time_seconds(w)
def cmd(s):unreal.SystemLibrary.execute_console_command(w,s,pc)
def write():(OUT/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
def wait(t):
    end=now()+t
    while now()<end:yield
def until(fn,t=4):
    end=now()+t
    while not fn() and now()<end:yield
def place(gap=120):
    # The workload repeats identical geometry; this is not player locomotion.
    for f,x,yaw in [(p,0,0),(q,gap,180)]:
        f.character_movement.stop_movement_immediately()
        f.set_actor_location(unreal.Vector(x,0,100),False,True)
        f.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
def reset():
    pc.handle_app_activation_changed(True);pc.set_training_panel_open(False);gm.set_training_menu_open(False)
    gm.set_opponent_mode(unreal.OpponentMode.STATIC)
    fixture=unreal.TrainingSettings();fixture.set_editor_property('infinite_health',True)
    fixture.set_editor_property('infinite_resources',True)
    gm.set_training_settings(fixture);gm.reset_training();pc.set_combat_input_enabled(True)
    pc.set_control_rotation(unreal.Rotator(pitch=-8,yaw=55,roll=0));place()
def feedback(enabled):
    for key in ['Reaction','Audio','RangedFX','Camera','HUD']:cmd('JJK.Feedback.'+key+' '+str(int(enabled)))
    cmd('JJK.Feedback.HitStop 0');cmd('JJK.Feedback.CameraStrength 2');cmd('JJK.Feedback.AudioGain 1')
def attack(kind):
    global label
    label=kind;place(650 if kind in ('Mobile','Super','Domain') else 120)
    if kind in ('A1','HeavyPunch','HeavyKick'):
        getattr(p.get_combat_input(),{'A1':'submit_light_attack','HeavyPunch':'submit_heavy_punch','HeavyKick':'submit_heavy_kick'}[kind])()
        yield from wait(1.35)
    elif kind=='Guard':
        q.get_combat_input().notify_guard_pressed();yield from wait(.25)
        p.get_combat_input().submit_light_attack();yield from wait(.8)
        q.get_combat_input().notify_guard_released();yield from wait(.5)
    elif kind in ('Mobile','Super'):
        p.get_combat_input().notify_stance_switch_pressed();yield from wait(.5)
        if kind=='Mobile':
            p.get_combat_input().notify_attack_pressed();yield from wait(.8);p.get_combat_input().notify_attack_released()
        else:
            p.get_combat_input().notify_kick_pressed();yield from wait(2.15);p.get_combat_input().notify_kick_released()
        yield from wait(1.8);p.get_combat_input().notify_stance_switch_pressed();yield from wait(.5)
    elif kind=='Domain':
        p.get_combat_input().notify_domain_pressed();yield from wait(5.8)
    elif kind=='Combo':
        for _ in range(4):p.get_combat_input().submit_light_attack();yield from wait(.30)
        yield from wait(1.2)

def summary(samples):
    values=sorted(x['ms'] for x in samples)
    return {'frames':len(values),'seconds':sum(values)/1000,'mean_ms':statistics.mean(values),
            'mean_fps':1000/statistics.mean(values),'p95_ms':values[min(len(values)-1,math.ceil(len(values)*.95)-1)],
            'p99_ms':values[min(len(values)-1,math.ceil(len(values)*.99)-1)],'max_ms':max(values),
            'spikes_over_50ms':[x for x in samples if x['ms']>50]}
def suite():
    global phase,run,frames,label
    for k in scales:cmd(k+' 2')
    for k,v in [('r.ScreenPercentage',100),('r.VSync',0),('t.MaxFPS',0),('JJK.Feedback.Log',0),('JJK.Feedback.AudioLog',0)]:cmd(k+' '+str(v))
    feedback(not request.get('cold_off',False));reset();yield from wait(3)
    report['cold_feedback_setting']='off' if request.get('cold_off') else 'standard'
    report['environment_cvars']={k:unreal.SystemLibrary.get_console_variable_float_value(k) for k in keys}
    for kind in ['A1','HeavyPunch','Super','Mobile','Domain']:
        reset();yield from wait(.6);frames=[];phase='cold'
        before=p.get_combat_feedback().contact_count
        yield from attack(kind)
        phase='idle'
        report['cold_samples'].append({'action':kind,'contacts':p.get_combat_feedback().contact_count-before,**summary(frames),
                                     'note':'first invocation in fresh PIE after production BeginPlay preload, not OS/disk cold package'});write()
    if request.get('cold_off'):return
    sequence=['A1','Combo','Guard','HeavyPunch','HeavyKick','Mobile','Super','Domain']
    for enabled in [False,True]:
        phase='warmup';feedback(enabled);reset()
        end=now()+10;i=0
        while now()<end:
            yield from attack(sequence[i%len(sequence)]);i+=1
        # One reset before sampling gives the same resource/mode/round state.
        reset();yield from wait(1)
        run={'setting':'standard' if enabled else 'off','warmup_seconds':10,'samples':[],
             'component_max':{'audio_loops':0,'audio_oneshots':0,'visual_cues':0,'trail_points':0},
             'contact_start':p.get_combat_feedback().contact_count,'fire_start':p.get_combat_feedback().fire_count}
        report['runs'].append(run);frames=[];phase='sample';start=now();i=0
        while now()-start<60:
            yield from attack(sequence[i%len(sequence)]);i+=1
        phase='idle';run['duration_game_seconds']=now()-start
        run['contact_delta']=p.get_combat_feedback().contact_count-run['contact_start']
        run['fire_delta']=p.get_combat_feedback().fire_count-run['fire_start']
        run.update(summary(frames));run['samples']=frames;write()
    a,b=report['runs'];report['comparison']={'p95_delta_ms':b['p95_ms']-a['p95_ms'],
        'budget_p95_1ms_met':b['p95_ms']-a['p95_ms']<=1,
        'note':'single off -> on pair; spikes require correlation, not automatic attribution; baseline 60fps status uses measured FPS'}

gen=suite();started=time.monotonic()
def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    for k,v in old.items():cmd(k+' '+str(v))
    gm.set_training_settings(settings);gm.set_opponent_mode(mode);gm.reset_training();write()
def tick(_):
    global last
    try:
        current=now();delta=current-last;last=current
        if delta>0 and phase in ('sample','cold'):
            counts={'audio_loops':sum(c.active_loop_count for c in audio),
                    'audio_oneshots':sum(c.active_one_shot_count for c in audio),
                    'visual_cues':sum(c.active_cue_count for c in visual),
                    'trail_points':sum(c.active_orb_trail_count for c in visual)}
            sample={'game_seconds':current,'wall_seconds':time.monotonic()-started,'ms':delta*1000,'action':label,**counts}
            frames.append(sample)
            if phase=='sample':
                for k,v in counts.items():run['component_max'][k]=max(run['component_max'][k],v)
        if time.monotonic()-started>480:raise TimeoutError('F performance fixture')
        next(gen)
    except StopIteration:report['status']='sampled';finish()
    except Exception:report['status']='failed';report['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
