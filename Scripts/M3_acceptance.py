# -*- coding: utf-8 -*-
"""M3 PIE 规则验收。临时配置只在内存修改，结束恢复；不保存测试参数。"""
import json,time,traceback,math
from pathlib import Path
import unreal
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
pc=unreal.GameplayStatics.get_player_controller(world,0)
gm=pc.get_training_game_mode()
p1,p2=gm.get_player_fighter(),gm.get_opponent_fighter()
fd=p1.get_definition()
defs=[unreal.load_asset('/Game/Training/DA_M3_'+n) for n in ('A1','A2','A3','HeavyPunch','Kick','HeavyKick')]
out=Path(unreal.Paths.project_saved_dir())/'M3_acceptance.json'
state={'status':'running','engine':unreal.SystemLibrary.get_engine_version(),
       'command_line':unreal.SystemLibrary.get_command_line(),'checks':[],'fps':[]}
perf=unreal.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings')
old_throttle=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
original=[]
config_snapshot={k:fd.get_editor_property(k).export_text() for k in ['dodge_config','throw_config']}
def settemp(obj,key,value):
# M3 方向性近战语义：钉住近战软锁关闭（游戏默认开）
    if not any(o==obj and k==key for o,k,v in original):original.append((obj,key,obj.get_editor_property(key)))
    obj.set_editor_property(key,value)
def write():out.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
def check(name,condition,details=None):
    state['checks'].append(dict(name=name,passed=bool(condition),details=details));write()
    unreal.log(('[M3] PASS ' if condition else '[M3] FAIL ')+name)
def now():return unreal.GameplayStatics.get_time_seconds(world)
def wait(sec):
    end=now()+sec
    while now()<end:yield
def until(predicate,timeout=.8):
    end=now()+timeout
    while not predicate() and now()<end:yield
def attr(f,k):return f.get_fighter_attribute_set().get_editor_property(k).get_editor_property('current_value')
def hp(f):return attr(f,'health')
def resource(f):return attr(f,'action_resource')
def inp(f):return f.get_combat_input()
def hit(f):return f.get_combat_hit()
def tag(f,name):
    t=unreal.GameplayTag();t.import_text('(TagName="'+name+'")')
    return f.has_combat_tag(t)
def place(f,x,y=0,yaw=0,z=100):
    f.character_movement.stop_movement_immediately()
    f.set_actor_location(unreal.Vector(x,y,z),False,True)
    f.set_actor_rotation(unreal.Rotator(0,yaw,0),False)
def reset(close=True):
    gm.reset_training();pc.set_combat_input_enabled(True)
    place(p1,400 if close else -500);place(p2,500,yaw=180)
def light(f):return inp(f).submit_light_attack()
def attack(f,kind):return getattr(inp(f),'submit_'+kind)()
def injection(action,value):subsystem.inject_input_vector_for_action(actions[action],unreal.Vector(value,0,0),[],[])
def press(action,seconds=.06):
    for _ in wait(seconds):injection(action,1);yield
    injection(action,0);yield;yield
subsystem=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary')).call_method(
    'GetLocalPlayerSubSystemFromPlayerController',(pc,unreal.EnhancedInputLocalPlayerSubsystem.static_class()))
actions={n:unreal.load_asset('/Game/Training/IA_'+n) for n in ('Attack','Kick','Dodge','Guard','StanceSwitch')}

def suite():
    settemp(fd,'melee_auto_face',False)  # M3 方向性近战语义（软锁游戏默认开）
    global p1,p2,pc,subsystem
    unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 60')
    yield from wait(.5)
    # 预热 EnhancedInput 注入路径（首次注入有 ~0.3s 初始化顿挫，会破坏后续精确时序）
    warm=unreal.load_asset('/Game/Training/IA_RecenterCamera')
    subsystem.inject_input_vector_for_action(warm,unreal.Vector(1,0,0),[],[])
    subsystem.inject_input_vector_for_action(warm,unreal.Vector(0,0,0),[],[])
    yield from wait(.3)
    # T01 / M2-T02: 一次伤害与完整三段。
    reset();yield from wait(.25);light(p1);yield from wait(1.15)
    check('M3-T01_single_A1',hp(p2)==965 and not hit(p1).has_active_attack(),hp(p2))
    check('M2-T02_dedup_single',hit(p1).get_hit_count()==1)
    reset();yield from wait(.25);light(p1);yield from wait(.3);light(p1);yield from wait(.32)
    check('M3-T01_A2',hit(p1).get_segment_id()==1,hit(p1).get_segment_id())
    yield from wait(.28);light(p1);yield from wait(.36)
    check('M3-T01_A3',hit(p1).get_segment_id()==2,hit(p1).get_segment_id())
    yield from wait(1.05)
    check('M3-T01_chain_end',not hit(p1).has_active_attack() and p1.can_act())
    check('M3-T01_three_damage',hp(p2)==870,hp(p2))
    # T02 缓存过期、后到覆盖、仅允许 A2 转招。
    reset(False);yield from wait(.2);light(p1);light(p1);yield from wait(.7)
    check('M3-T02_expired',hit(p1).get_segment_id()==0)
    reset(False);yield from wait(.2);light(p1);yield from wait(.3);light(p1);yield from wait(.35)
    attack(p1,'heavy_punch');attack(p1,'kick');yield from wait(.55)
    check('M3-T02_latest_wins',hit(p1).get_segment_id()==4,hit(p1).get_segment_id())
    yield from wait(1.2);check('M3-T02_consumed_once',not hit(p1).has_active_attack())
    reset(False);yield from wait(.2);light(p1);yield from wait(.3);attack(p1,'heavy_punch');yield from wait(.4)
    check('M3-T02_forbidden_A1_heavy',hit(p1).get_segment_id()==0)
    # T03 缓存遇到死亡、中断、重置；不得残留。
    for method in ('jjk_debug_force_hit_react','jjk_debug_kill','reset_to_initial_state'):
        reset(False);yield from wait(.2);light(p1);yield from wait(.3);light(p1);getattr(p1,method)();yield from wait(.9)
        check('M3-T03_cache_clear_'+method,'NONE' in str(inp(p1).peek_cached_action()) and not hit(p1).has_active_attack())
    # T04/T05 前后方向与硬直松开。
    reset();yield from wait(.25);inp(p2).notify_guard_pressed();attack(p1,'kick');yield from until(lambda:tag(p2,'State.GuardStun'))
    check('M3-T04_front_guard',hp(p2)==1000 and tag(p2,'State.GuardStun'),hp(p2))
    inp(p2).notify_guard_released();yield from wait(.6)
    check('M3-T05_guard_stun_release',not inp(p2).is_guard_intent() and p2.can_act())
    reset();place(p2,500,yaw=0);yield from wait(.25);inp(p2).notify_guard_pressed();attack(p1,'kick');yield from wait(.55)
    check('M3-T04_back_hit',hp(p2)==955,hp(p2))
    # T06/T14：固定位置以确保真实接触，无敌结束后同一段不能补伤害。
    dodge_cfg=fd.get_editor_property('dodge_config');old_speed=dodge_cfg.dodge_speed
    dodge_cfg.set_editor_property('dodge_speed',0.);settemp(fd,'dodge_config',dodge_cfg)
    settemp(defs[4],'window_end_time',.75)
    reset();yield from wait(.25);attack(p1,'kick');yield from wait(.18);p2.request_dodge(unreal.Vector(0,1,0));yield from wait(.15)
    check('M3-T06_invulnerable_contact',hp(p2)==1000 and tag(p2,'State.DodgeInvulnerable'),hp(p2))
    yield from wait(.25)
    check('M3-T06_recovery_vulnerable',not tag(p2,'State.DodgeInvulnerable') and tag(p2,'State.DodgeRecovery'))
    yield from wait(.35);check('M3-T14_no_late_bypass',hp(p2)==1000,hp(p2))
    yield from wait(.35);attack(p1,'kick');yield from wait(.5)
    check('M3-T06_new_attack_hits',hp(p2)==955,hp(p2))
    defs[4].set_editor_property('window_end_time',.45)
    dodge_cfg.set_editor_property('dodge_speed',old_speed);fd.set_editor_property('dodge_config',dodge_cfg)
    # T07/T15 取消四种组合，失败保留原招。
    for inside,enough in ((False,False),(False,True),(True,False),(True,True)):
        settemp(fd,'initial_action_resource',5. if enough else 1.)
        reset(False);yield from wait(.2);light(p1);yield from wait(.2 if inside else .02)
        before=resource(p1);iid=hit(p1).get_active_instance_id();result=p1.request_dodge(unreal.Vector(0,1,0))
        check('M3-T07_cancel_'+str((inside,enough)),result==(inside and enough),result)
        check('M3-T15_cost_'+str((inside,enough)),abs(resource(p1)-(before-(2 if result else 0)))<.01,resource(p1))
        check('M3-T15_old_action_'+str((inside,enough)),not hit(p1).has_active_attack() if result else hit(p1).has_active_attack() and hit(p1).get_active_instance_id()==iid)
    fd.set_editor_property('initial_action_resource',5.)
    reset(False);yield from wait(.2);x=p1.get_actor_location();p1.request_dodge(unreal.Vector(0,1,0));yield from wait(.3)
    check('M3-T11_direction_displacement',p1.get_actor_location().y-x.y>100,p1.get_actor_location().y-x.y)
    check('M3-T16_recovery_rejects_E_attack',not p1.request_stance_switch() and 'REJECTED_BLOCKED' in str(light(p1)))
    yield from wait(.5);check('M3-T06_dodge_restores',p1.can_act())
    # T08 受击、死亡禁止取消。
    for method in ('jjk_debug_force_hit_react','jjk_debug_kill'):
        reset(False);yield from wait(.2);getattr(p1,method)();yield from wait(.05)
        check('M3-T08_reject_'+method,not p1.request_dodge(unreal.Vector()) and not p1.request_stance_switch())
    # T09 合法投技与配对一次伤害。
    reset();yield from wait(.25);inp(p2).notify_guard_pressed();light(p1);yield from until(lambda:p1.is_throw_paired())
    check('M3-T09_pair_started',p1.is_throw_paired() and p2.is_throw_paired())
    check('M3-T09_no_double_damage',hp(p2)==1000,hp(p2))
    check('M3-T08_throw_rejects_actions',not p1.request_dodge(unreal.Vector()) and not p2.request_stance_switch() and 'REJECTED_BLOCKED' in str(light(p2)))
    yield from wait(1.2)
    check('M3-T09_pair_damage_release',hp(p2)==880 and not p1.is_throw_paired() and p1.can_act() and p2.can_act(),hp(p2))
    # T10 每一方的死亡、销毁、重置与停止动画。
    for who in (1,2):
        for operation in ('death','reset','stop','destroy'):
            reset();yield from wait(.25);inp(p2).notify_guard_pressed();light(p1);yield from until(lambda:p1.is_throw_paired())
            check(f'M3-T10_setup_{who}_{operation}',p1.is_throw_paired())
            target=p1 if who==1 else p2;other=p2 if who==1 else p1
            if operation=='death':target.jjk_debug_kill()
            elif operation=='reset':target.reset_to_initial_state()
            elif operation=='stop':target.stop_anim_montage()
            else:target.destroy_actor()
            yield from wait(.18)
            check(f'M3-T10_release_{who}_{operation}',not other.is_throw_paired() and other.can_act() and 'NONE' not in str(other.character_movement.movement_mode))
            yield from wait(1.1)
            check(f'M3-T10_no_delayed_damage_{who}_{operation}',hp(other)==1000,hp(other))
            if operation=='destroy':
                gm.ensure_fighters_spawned();yield from wait(.25)
                p1,p2=gm.get_player_fighter(),gm.get_opponent_fighter()
    # T11 边缘非法配对保持防御；闪避碰撞不越界。
    cfg=fd.get_editor_property('throw_config');cfg.set_editor_property('pair_distance',300.);settemp(fd,'throw_config',cfg)
    reset();place(p1,-1000);place(p2,-900,yaw=180);yield from wait(.25);inp(p2).notify_guard_pressed();light(p1);yield from wait(.5)
    check('M3-T11_boundary_throw_falls_back',not p1.is_throw_paired() and hp(p2)==1000,hp(p2))
    cfg.set_editor_property('pair_distance',90.);fd.set_editor_property('throw_config',cfg)
    reset(False);place(p1,1000);yield from wait(.25);p1.request_dodge(unreal.Vector(1,0,0));yield from wait(.8)
    check('M3-T11_dodge_wall',p1.get_actor_location().x<1125,p1.get_actor_location().x)
    # T12 霸体扣血与等级突破；倒地保护再起身。
    settemp(defs[3],'bGrantsSuperArmor',True)
    reset();yield from wait(.25);attack(p2,'heavy_punch');light(p1);yield from wait(.5)
    check('M3-T12_armor_damage_no_interrupt',hp(p2)==965 and hit(p2).has_active_attack(),hp(p2))
    reset();yield from wait(.25);attack(p2,'heavy_punch');attack(p1,'heavy_kick');yield from wait(.5)
    check('M3-T12_high_interrupt_knockdown',tag(p2,'State.KnockedDown') and not hit(p2).has_active_attack())
    defs[3].set_editor_property('bGrantsSuperArmor',False)
    reset();yield from wait(.25);attack(p1,'heavy_kick');yield from wait(.5)
    check('M3-T12_knockdown',hp(p2)==900 and tag(p2,'State.KnockedDown'),hp(p2))
    place(p2,500,yaw=180);yield from wait(.55);place(p1,400);light(p1);yield from wait(.35)
    check('M3-T12_knockdown_protection',hp(p2)==900,hp(p2))
    yield from wait(1.2);check('M3-T12_getup_action',p2.can_act() and 'EXECUTED' in str(light(p2)))
    # T14 防住后松开也不被本段剩余扫掠击中。
    settemp(defs[4],'window_end_time',.75)
    reset();yield from wait(.25);inp(p2).notify_guard_pressed();attack(p1,'kick');yield from until(lambda:tag(p2,'State.GuardStun'));inp(p2).notify_guard_released();yield from wait(.6)
    check('M3-T14_guard_dedup',hp(p2)==1000,hp(p2));defs[4].set_editor_property('window_end_time',.45)
    # T16/T17 真实 Enhanced Input。
    reset(False);yield from wait(.25);yield from press('Dodge',.8)
    check('M3-T16_hold_shift_one_dodge',abs(resource(p1)-4.)<.01,resource(p1))
    reset(False);yield from wait(.25);light(p1);yield from wait(.12)
    for _ in wait(.06):injection('Attack',1);yield
    injection('Attack',0);injection('Dodge',1);yield;injection('Dodge',0);yield from wait(.08)
    check('M3-T16_same_frame_success',not hit(p1).has_active_attack() and not inp(p1).is_session_active() and abs(resource(p1)-3)<.01,resource(p1))
    settemp(fd,'initial_action_resource',0.)
    reset(False);yield from wait(.25)
    for _ in wait(.06):injection('Attack',1);yield
    injection('Attack',0);injection('Dodge',1);yield;injection('Dodge',0);yield from wait(.08)
    check('M3-T16_failed_shift_keeps_release',hit(p1).has_active_attack())
    fd.set_editor_property('initial_action_resource',5.)
    reset(False);yield from wait(.2);light(p1);yield from wait(.2)
    for _ in wait(.1):injection('Guard',1);yield
    check('M3-T17_F_not_cancel',hit(p1).has_active_attack() and inp(p1).is_guard_intent())
    injection('Guard',0);yield from wait(.1)
    check('M3-T17_F_release',not inp(p1).is_guard_intent())
    for mode in ('menu','focus'):
        reset(False);yield from wait(.2);inp(p1).notify_guard_pressed();inp(p1).notify_attack_pressed();inp(p1).notify_kick_pressed()
        if mode=='menu':pc.set_combat_input_enabled(False)
        else:pc.call_method('HandleAppActivationChanged',(False,))
        inp(p1).notify_attack_released();inp(p1).notify_kick_released();yield from wait(.1)
        check('M3-T17_SLL-T02_'+mode,not inp(p1).is_guard_intent() and not hit(p1).has_active_attack())
        pc.set_combat_input_enabled(True)
    # SLL-T01 轻重拳腿四分流；SLL-T03 切形态与会话。
    for action,hold,segment in [('Attack',.06,0),('Attack',.25,3),('Kick',.06,4),('Kick',.25,5)]:
        reset(False);yield from wait(.25);yield from press(action,hold);yield from wait(.05)
        check('SLL-T01_'+action+str(hold),hit(p1).has_active_attack() and hit(p1).get_segment_id()==segment,hit(p1).get_segment_id())
    reset(False);yield from wait(.25);inp(p1).notify_attack_pressed()
    check('SLL-T03_hold_rejects_E',not p1.request_stance_switch());inp(p1).invalidate_session(unreal.Text('test'))
    yield from press('StanceSwitch');yield from wait(.3)
    check('SLL-T03_ranged',str(p1.get_stance()).endswith('RANGED') or 'RANGED' in str(p1.get_stance()),str(p1.get_stance()))
    check('SLL-T03_ranged_no_melee','REJECTED_RANGED_STANCE' in str(light(p1)))
    yield from wait(.6);yield from press('StanceSwitch');yield from wait(.3)
    check('SLL-T03_back_melee','MELEE' in str(p1.get_stance()))
    # SLL-T04 正常命中 +3 去重；防御/空挥无收益，拳脚不消耗。
    settemp(fd,'initial_cursed_energy',50.)
    reset();yield from wait(.25);attack(p1,'kick');yield from wait(1.1)
    check('SLL-T04_hit_gain_once',attr(p1,'cursed_energy')==53,attr(p1,'cursed_energy'))
    reset();yield from wait(.25);inp(p2).notify_guard_pressed();attack(p1,'kick');yield from wait(1.1)
    check('SLL-T04_block_no_gain',attr(p1,'cursed_energy')==50)
    reset(False);yield from wait(.25);attack(p1,'heavy_punch');yield from wait(1.1)
    check('SLL-T04_whiff_no_cost',attr(p1,'cursed_energy')==50)
    reset();yield from wait(.25);inp(p2).notify_guard_pressed();light(p1);yield from wait(1.6)
    check('SLL-T04_throw_gain_once',attr(p1,'cursed_energy')==53,attr(p1,'cursed_energy'))
    fd.set_editor_property('initial_cursed_energy',100.)
    # 缓存消费重新核验锁定目标；资源归零不禁止零成本拳脚。
    reset(False);yield from wait(.25);p1.get_targeting().lock_target(p2);light(p1);yield from wait(.3);light(p1)
    p2.jjk_debug_kill();yield from wait(.45)
    check('M3-T03_dead_cached_target',hit(p1).get_segment_id()==0)
    # 普通闪避无方向后撤；恢复准确封顶。
    reset(False);yield from wait(.25);start_x=p1.get_actor_location().x
    p1.request_dodge(unreal.Vector());yield from wait(.8)
    check('M3-T11_default_backstep',p1.get_actor_location().x<start_x-100)
    yield from wait(2.0)
    check('M3-T15_regen_clamped',abs(resource(p1)-5.)<.01,resource(p1))
    # M2-T08：双方真实接触换血，不能被输入阶段提前取消。
    for n in range(5):
        reset();yield from wait(.3);light(p2);light(p1);yield from wait(1.3)
        check('M2-T08_trade_'+str(n),hp(p1)==965 and hp(p2)==965,[hp(p1),hp(p2)])
    # M2-T05/T06 起手/有效/恢复：停止动画、受击、死亡、重置无旧检测。
    for phase,delay in [('windup',.1),('active',.32),('recovery',.65)]:
        for operation in ('stop','hit','kill','reset'):
            reset(False);yield from wait(.2);light(p1);yield from wait(delay)
            if operation=='stop':p1.stop_anim_montage()
            elif operation=='hit':p1.jjk_debug_force_hit_react()
            elif operation=='kill':p1.jjk_debug_kill()
            else:gm.reset_training()
            yield from wait(.15);place(p1,400);yield from wait(1.1)
            check('M2-T05-T06_'+phase+'_'+operation,not hit(p1).has_active_attack() and hp(p2)==1000,hp(p2))
    # T13 测量实际 game delta；每档连段和窄取消窗口。
    for fps in (30,60,120):
        unreal.SystemLibrary.execute_console_command(world,f't.MaxFPS {fps}')
        reset(False);yield from wait(.3)
        samples=[];segments=set();light(p1)
        for _ in wait(1.9):
            samples.append(unreal.GameplayStatics.get_world_delta_seconds(world))
            seg=hit(p1).get_segment_id();segments.add(seg)
            if hit(p1).has_active_attack() and seg<2 and .28<hit(p1).get_segment_elapsed_time()<.4:light(p1)
            yield
        check(f'M3-T13_combo_{fps}',{0,1,2}.issubset(segments),sorted(segments))
        measured=1/(sum(samples)/len(samples));state['fps'].append(dict(requested=fps,measured=measured,min_delta=min(samples),max_delta=max(samples)))
        settemp(defs[0],'cancel_window_start_time',.2);settemp(defs[0],'cancel_window_end_time',.3)
        reset(False);yield from wait(.2);light(p1);yield from wait(.22)
        elapsed=hit(p1).get_segment_elapsed_time();result=p1.request_dodge(unreal.Vector(0,1,0))
        check(f'M3-T13_narrow_cancel_{fps}',result,dict(elapsed=elapsed,result=result))
        defs[0].set_editor_property('cancel_window_start_time',.1);defs[0].set_editor_property('cancel_window_end_time',.8)
    unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 60')
    reset(False)

gen=suite();started=time.monotonic()
def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    for obj,k,v in reversed(original):obj.set_editor_property(k,v)
    for k,value in config_snapshot.items():
        cfg=fd.get_editor_property(k);cfg.import_text(value);fd.set_editor_property(k,cfg)
    perf.set_editor_property('bThrottleCPUWhenNotForeground',old_throttle)
    unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 0')
    gm.reset_training();pc.set_combat_input_enabled(True)
    state['duration_seconds']=time.monotonic()-started;write()
def tick(dt):
    try:
        if time.monotonic()-started>480:raise TimeoutError('M3 timeout')
        next(gen)
    except StopIteration:
        state['status']='passed' if state['checks'] and all(c['passed'] for c in state['checks']) else 'failed';finish()
    except Exception:
        state['status']='failed';state['error']=traceback.format_exc();unreal.log_error(state['error']);finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
print('M3 acceptance started:',str(out))
