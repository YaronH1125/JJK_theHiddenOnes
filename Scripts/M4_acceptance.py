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
out=Path(unreal.Paths.project_saved_dir())/'M4_acceptance.json'
state={'status':'running','engine':unreal.SystemLibrary.get_engine_version(),
       'command_line':unreal.SystemLibrary.get_command_line(),'checks':[],'fps':[]}
perf=unreal.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings')
old_throttle=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
original=[]
config_snapshot={k:fd.get_editor_property(k).export_text() for k in ['dodge_config','throw_config']}
def settemp(obj,key,value):
    if not any(o==obj and k==key for o,k,v in original):original.append((obj,key,obj.get_editor_property(key)))
    obj.set_editor_property(key,value)
def write():
    temp=out.with_suffix('.tmp');temp.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8');temp.replace(out)
def check(name,condition,details=None):
    state['checks'].append(dict(name=name,passed=bool(condition),details=details));write()
    unreal.log(('[M4] PASS ' if condition else '[M4] FAIL ')+name)
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


def settings(**values):
    s=gm.get_editor_property('settings')
    for k,v in values.items():s.set_editor_property(k,v)
    gm.set_training_settings(s)
def stats(f=None):return gm.get_editor_property('player_stats' if f is None or f==p1 else 'opponent_stats')
def asc(f):return f.get_fighter_ability_system_component()
def clean(close=True):
    pc.set_training_panel_open(False)
    settings(auto_recover_health=False,infinite_health=False,infinite_resources=False,no_cooldown=False,recovery_delay=.45)
    gm.set_opponent_mode(unreal.OpponentMode.STATIC)
    for k,v in [('initial_health',1000.),('initial_action_resource',5.),('initial_cursed_energy',100.),('initial_energy',50.)]:settemp(fd,k,v)
    reset(close)
def seed(**values):
    for k,v in values.items():settemp(fd,'initial_'+k,float(v))
    gm.reset_training();place(p1,400);place(p2,500,yaw=180)
def probe(f=None):return gm.request_training_probe(p1 if f is None else f)

def suite():
    global p1,p2
    unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 60')
    clean();yield from wait(.3)
    check('M4-T01_static',not p2.is_guard_intent())
    check('M4-T01_select_guard',gm.set_opponent_mode(unreal.OpponentMode.FIXED_GUARD))
    check('M4-T01_guard_intent',p2.is_guard_intent() and tag(p2,'State.Guarding'))
    gm.set_opponent_mode(unreal.OpponentMode.STATIC)
    check('M4-T01_release',not p2.is_guard_intent() and not tag(p2,'State.Guarding'))
    p2.jjk_debug_force_hit_react();yield from wait(.05);gm.set_opponent_mode(unreal.OpponentMode.FIXED_GUARD)
    check('M4-T01_passive_continues',tag(p2,'State.HitStun') and not p2.is_guard_intent())
    yield from wait(1.1);check('M4-T01_guard_after_recovery',p2.can_act() and p2.is_guard_intent())
    p2.jjk_debug_force_hit_react();yield from wait(.05);gm.set_opponent_mode(unreal.OpponentMode.STATIC);yield from wait(1.1)
    check('M4-T01_static_after_recovery',p2.can_act() and not p2.is_guard_intent())
    check('M4-T02_AI_available_M5',gm.is_mode_available(unreal.OpponentMode.AI))
    pc.set_training_panel_open(True);yield from wait(.1)
    panel=pc.get_editor_property('training_panel')
    check('M4-T02_AI_available_UMG_M5',not panel.is_ai_option_disabled())
    check('M4-T11_initial_values', '1000/1000' in panel.get_displayed_state(),panel.get_displayed_state())
    pc.set_training_panel_open(False)
    # Recovery restarts after new contact. A long test delay leaves a hittable interval.
    clean();settings(auto_recover_health=True,recovery_delay=1.2);yield from wait(.2)
    light(p1);yield from wait(.5);check('M4-T03_damaged_before_heal',hp(p2)==965)
    yield from wait(.65);place(p1,400);place(p2,500,yaw=180);light(p1);yield from wait(.55)
    check('M4-T03_second_hit',hp(p2)==930,hp(p2))
    yield from wait(.45);check('M4-T03_no_old_timer_heal',hp(p2)==930,hp(p2))
    yield from wait(1.2);check('M4-T03_delayed_heal',hp(p2)==1000,hp(p2))
    # Infinite HP records uncapped resolved output, prevents death before application.
    clean();seed(health=10);settings(infinite_health=True);yield from wait(.2);light(p1);yield from wait(1.2)
    s=stats();check('M4-T04_survives',hp(p2)==1 and not p2.is_dead() and p2.can_act())
    check('M4-T04_damage_ledger',s.raw_damage==35 and s.resolved_damage==35 and s.health_lost==9 and s.hits==1,dict(raw=s.raw_damage,resolved=s.resolved_damage,lost=s.health_lost))
    settings(infinite_health=False);place(p1,400);place(p2,500,yaw=180);light(p1);yield from wait(.6)
    check('M4-T04_disable_allows_death',p2.is_dead() and hp(p2)==0)
    settings(auto_recover_health=True,recovery_delay=.1);yield from wait(.4);check('M4-T03_autoheal_no_resurrection',p2.is_dead() and hp(p2)==0)
    clean();seed(action_resource=0,cursed_energy=0,energy=0);yield from wait(.15)
    check('M4-T05_zero_rejected',not probe())
    settings(infinite_resources=True,no_cooldown=True)
    before=[attr(p1,k) for k in ('action_resource','cursed_energy','energy')]
    check('M4-T05_zero_infinite_executes',probe())
    check('M4-T05_zero_no_deduction',before==[attr(p1,k) for k in ('action_resource','cursed_energy','energy')])
    check('M4-T05_zero_dodge',p1.request_dodge(unreal.Vector(0,1,0)))
    check('M4-T05_dodge_state_still_blocks',not probe());yield from wait(.8)
    p1.jjk_debug_force_hit_react();yield from wait(.05);check('M4-T05_stun_still_blocks',not probe());yield from wait(1.1)
    p1.jjk_debug_kill();yield from wait(.05);check('M4-T05_death_still_blocks',not probe())
    clean();yield from wait(.2);check('M4-T06_normal_probe',probe())
    check('M4-T06_real_GE_cost',attr(p1,'cursed_energy')==90 and attr(p1,'energy')==45 and abs(resource(p1)-4)<.01)
    check('M4-T06_cooldown_active',asc(p1).get_training_cooldown_remaining()>2.8 and not probe())
    p1.jjk_debug_force_hit_react();yield from wait(.05);settings(no_cooldown=True)
    check('M4-T06_only_cooldown_removed',asc(p1).get_training_cooldown_remaining()==0 and tag(p1,'State.HitStun'))
    yield from wait(1.1);check('M4-T06_repeat_no_cooldown',probe() and probe() and asc(p1).get_training_cooldown_remaining()==0)
    settings(infinite_resources=True);before=[attr(p1,k) for k in ('action_resource','cursed_energy','energy')];probe()
    settings(infinite_resources=False,no_cooldown=False)
    check('M4-T07_no_retroactive_cost',before==[attr(p1,k) for k in ('action_resource','cursed_energy','energy')])
    check('M4-T07_regular_rules_restore',probe() and not probe() and attr(p1,'cursed_energy')==before[1]-10 and asc(p1).get_training_cooldown_remaining()>2.8)
    # Real outcome classification and recovery boundary, no time-based combo guess.
    clean();yield from wait(.2);light(p1);yield from wait(.42)
    check('M4-T08_hit_current_combo',stats().hits==1 and stats().combo_hits==1)
    yield from wait(.8);check('M4-T08_recovery_ends_combo',stats().combo_hits==0 and stats().last_combo_hits==1)
    place(p1,400);place(p2,500,yaw=180);light(p1);yield from wait(.42)
    check('M4-T08_new_hit_new_combo',stats().hits==2 and stats().combo_hits==1)
    clean();yield from wait(.2);gm.set_opponent_mode(unreal.OpponentMode.FIXED_GUARD);attack(p1,'kick');yield from wait(.6)
    check('M4-T08_guard_not_hit',stats().guards==1 and stats().hits==0 and stats().resolved_damage==0 and hp(p2)==1000)
    clean();cfg=fd.get_editor_property('dodge_config');old_speed=cfg.dodge_speed;cfg.set_editor_property('dodge_speed',0.);settemp(fd,'dodge_config',cfg);yield from wait(.25);attack(p1,'kick');yield from wait(.18);p2.request_dodge(unreal.Vector(0,1,0));yield from wait(.22)
    check('M4-T08_immune_not_hit',stats().immunes==1 and stats().hits==0,dict(immune=stats().immunes,hits=stats().hits))
    cfg.set_editor_property('dodge_speed',old_speed);fd.set_editor_property('dodge_config',cfg)
    clean(False);yield from wait(.2);light(p1);yield from wait(.6)
    check('M4-T08_whiff',stats().whiffs==1 and stats().hits==0)
    clean();yield from wait(.2);gm.set_opponent_mode(unreal.OpponentMode.FIXED_GUARD);light(p1);yield from wait(.45)
    check('M4-T09_throw_setup',p1.is_throw_paired() and p2.is_throw_paired())
    settings(infinite_health=True,infinite_resources=True,no_cooldown=True);gm.reset_training();yield from wait(.15)
    check('M4-T09_throw_reset',not p1.is_throw_paired() and not p2.is_throw_paired() and p1.can_act() and p2.can_act())
    check('M4-T09_settings_mode_preserved',gm.get_editor_property('settings').infinite_health and p2.is_guard_intent())
    check('M4-T09_stats_reset',stats().raw_damage==0 and stats().hits==0 and stats().combo_hits==0)
    # 20 resets, interleaving attack / dodge / hit react / death and mode changes.
    clean(False);yield from wait(.2)
    ability_counts=[asc(f).get_granted_ability_count() for f in (p1,p2)]
    bindings=gm.get_attribute_binding_count()
    for i in range(20):
        gm.set_opponent_mode(unreal.OpponentMode.FIXED_GUARD if i%2 else unreal.OpponentMode.STATIC)
        if i%4==0:light(p1)
        elif i%4==1:p1.request_dodge(unreal.Vector(0,1,0))
        elif i%4==2:p1.jjk_debug_force_hit_react()
        else:p1.jjk_debug_kill()
        yield from wait(.13);gm.reset_training();yield from wait(.08)
        valid=all(not f.is_dead() and f.can_act() and not hit(f).has_active_attack() and f.get_stance()==unreal.FighterStance.MELEE for f in (p1,p2))
        check(f'M4-T10_reset_{i+1:02}',valid and [asc(f).get_granted_ability_count() for f in (p1,p2)]==ability_counts and gm.get_attribute_binding_count()==bindings)
        check(f'M4-T10_effects_{i+1:02}',asc(p1).get_active_effect_count()==0 and asc(p2).get_active_effect_count()==(1 if i%2 else 0))
    # Subscriptions should be one per attribute (8 each); widget exactly one refresh for single GM signal.
    check('M4-T11_attribute_binding_count',bindings==16,bindings)
    clean();yield from wait(.15)
    inp(p1).notify_attack_pressed();inp(p1).notify_guard_pressed()
    pc.set_training_panel_open(True);yield from wait(.1)
    check('M4-T11_menu_clears_input',not inp(p1).is_session_active() and not p1.is_guard_intent() and light(p1)==unreal.ActionRequestResult.REJECTED_BLOCKED)
    check('M4-T11_menu_blocks_shared',light(p1)==unreal.ActionRequestResult.REJECTED_BLOCKED and not probe())
    for i in range(5):
        pc.set_training_panel_open(False);pc.set_training_panel_open(True);pc.rebuild_training_panel();yield from wait(.03)
    panel=pc.get_editor_property('training_panel');before=panel.get_refresh_count()
    gm.set_opponent_mode(unreal.OpponentMode.STATIC)
    check('M4-T11_widget_single_subscription',panel.get_refresh_count()==before+1,dict(before=before,after=panel.get_refresh_count()))
    pc.set_training_panel_open(False);inp(p1).notify_attack_released();yield from wait(.1)
    check('M4-T11_old_release_no_attack',not hit(p1).has_active_attack() and not gm.is_training_menu_open() and not pc.get_editor_property('show_mouse_cursor'))
    oldpanel=panel;before=oldpanel.get_refresh_count();gm.set_opponent_mode(unreal.OpponentMode.STATIC)
    check('M4-T11_hidden_widget_unbound',oldpanel.get_refresh_count()==before)
    pc.set_training_panel_open(True);p2.destroy_actor();yield from wait(.1)
    check('M4-T11_destroy_unbind',gm.get_attribute_binding_count()==8 and '待重生' in panel.get_displayed_state())
    gm.ensure_fighters_spawned();p2=gm.get_opponent_fighter();yield from wait(.1)
    check('M4-T11_respawn_rebind',gm.get_attribute_binding_count()==16 and '待重生' not in panel.get_displayed_state())
    pc.set_training_panel_open(False);gm.reset_training();place(p1,400);place(p2,500,yaw=180);yield from wait(.2);light(p1);yield from wait(.5)
    pc.set_training_panel_open(True);yield from wait(.1)
    check('M4-T11_respawn_actual_health_display',hp(p2)==965 and '965/1000' in panel.get_displayed_state())
    # Combined switches, reset from menu keeps gates until close.
    settings(infinite_health=True,infinite_resources=True,no_cooldown=True);gm.reset_training()
    check('M4-T12_menu_reset_gated',light(p1)==unreal.ActionRequestResult.REJECTED_BLOCKED and not probe())
    pc.set_training_panel_open(False);seed(action_resource=0,cursed_energy=0,energy=0);yield from wait(.1)
    check('M4-T12_combined_executes',probe() and probe())
    p1.jjk_debug_force_hit_react();yield from wait(.05);check('M4-T12_combined_stun_blocks',not probe());yield from wait(1.1)
    settings(infinite_health=False,infinite_resources=False,no_cooldown=False)
    check('M4-T12_all_off_cost_returns',not probe() and not gm.get_editor_property('settings').infinite_health)
    clean();yield from wait(.2);light(p1);yield from wait(1.2)
    check('M4-T12_normal_combat_after_settings',hp(p2)==965 and p1.can_act() and p2.can_act())
    clean();yield from wait(.25);light(p1);yield from wait(.3);light(p1);yield from wait(.6);light(p1);yield from wait(1.5)
    check('M4-T08_gapped_chain_not_true_combo',stats().hits==3 and stats().last_combo_hits==1 and stats().resolved_damage==130,dict(hits=stats().hits,last_combo=stats().last_combo_hits,damage=stats().resolved_damage))
    for d in defs[:3]:settemp(d,'hit_stun_duration',.8)
    clean();yield from wait(.25);light(p1);yield from wait(.3);light(p1);yield from wait(.6);light(p1);yield from wait(1.8)
    check('M4-T08_true_three_hit_combo',stats().hits==3 and stats().last_combo_hits==3 and stats().last_combo_damage==130,dict(hits=stats().hits,combo=stats().last_combo_hits,damage=stats().last_combo_damage))
    for d in defs[:3]:d.set_editor_property('hit_stun_duration',.5)
    clean();yield from wait(.25);attack(p1,'heavy_kick');yield from wait(.6)
    check('M4-T08_knockdown_ends_combo',tag(p2,'State.KnockedDown') and stats().combo_hits==0 and stats().last_combo_hits==1,dict(down=tag(p2,'State.KnockedDown'),combo=stats().combo_hits,last=stats().last_combo_hits))
    # Additional boundaries: pending damage outranks probe; player respawn remains menu-gated.
    clean();p1.jjk_debug_force_hit_react();check('M4-T05_pending_hit_blocks_probe',not probe());yield from wait(.7)
    pc.set_training_panel_open(True);p1.destroy_actor();yield from wait(.08);gm.ensure_fighters_spawned();p1=gm.get_player_fighter();yield from wait(.15)
    check('M4-T11_player_respawn_menu_gate',light(p1)==unreal.ActionRequestResult.REJECTED_BLOCKED and pc.get_player_fighter()==p1 and gm.get_attribute_binding_count()==16)
    pc.set_training_panel_open(False);check('M4-T11_player_respawn_resume',light(p1)==unreal.ActionRequestResult.EXECUTED)
    clean();yield from wait(.2);gm.set_opponent_mode(unreal.OpponentMode.FIXED_GUARD);light(p1);yield from wait(1.55)
    s=stats(p1);check('M4-T08_throw_ledger_once',s.hits==1 and s.raw_damage==120 and s.resolved_damage==120 and hp(p2)==880)
    clean();seed(health=10);yield from wait(.2);light(p1);yield from wait(.6)
    s=stats(p1);check('M4-T08_overkill_ledger',s.raw_damage==35 and s.resolved_damage==35 and s.health_lost==10 and p2.is_dead())
    clean();yield from wait(.2);inp(p2).submit_light_attack();yield from wait(.15);gm.set_opponent_mode(unreal.OpponentMode.STATIC)
    check('M4-T01_switch_cancels_active_intent',not hit(p2).has_active_attack() and not p2.is_guard_intent())
    clean()
    state['binding_count']=gm.get_attribute_binding_count();state['ability_counts']=[asc(f).get_granted_ability_count() for f in (p1,p2)]

gen=suite();started=time.monotonic()
def finish():
    write()
    unreal.unregister_slate_post_tick_callback(handle)
    cleanup_errors=[]
    # 即使 PIE 被外部终止，也先独立恢复资产配置，不能将测试初值保存到正式资产。
    for obj,k,v in reversed(original):
        try:obj.set_editor_property(k,v)
        except Exception:cleanup_errors.append(traceback.format_exc())
    perf.set_editor_property('bThrottleCPUWhenNotForeground',old_throttle)
    for k,value in config_snapshot.items():
        cfg=fd.get_editor_property(k);cfg.import_text(value);fd.set_editor_property(k,cfg)
    try:
        pc.set_training_panel_open(False)
        settings(auto_recover_health=False,infinite_health=False,infinite_resources=False,no_cooldown=False)
        gm.set_opponent_mode(unreal.OpponentMode.STATIC)
        unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 0')
        gm.reset_training();pc.set_combat_input_enabled(True)
    except Exception:cleanup_errors.append(traceback.format_exc())
    if cleanup_errors:state['cleanup_errors']=cleanup_errors;state['status']='failed'
    state['duration_seconds']=time.monotonic()-started;write()

def tick(dt):
    try:
        if time.monotonic()-started>480:raise TimeoutError('M4 timeout')
        next(gen)
    except StopIteration:
        state['status']='passed' if state['checks'] and all(c['passed'] for c in state['checks']) else 'failed';finish()
    except Exception:
        state['status']='failed';state['error']=traceback.format_exc();unreal.log_error(state['error']);finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
print('M4 acceptance started:',str(out))
