# -*- coding: utf-8 -*-
"""有图形 PIE 的训练闭环，通过实际 UMG 控件及其回调修改训练设置。"""
import unreal,json,time,traceback
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert world
assert '-nullrhi' not in unreal.SystemLibrary.get_command_line().lower()
pc=unreal.GameplayStatics.get_player_controller(world,0);gm=pc.get_training_game_mode()
p1,p2=gm.get_player_fighter(),gm.get_opponent_fighter()
report=Path(unreal.Paths.project_saved_dir())/'M4_demo.json'
state={'status':'running','command_line':unreal.SystemLibrary.get_command_line(),'checks':[],'stages':[],'started_at':time.time()}
perf=unreal.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings')
old_throttle=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
def now():return unreal.GameplayStatics.get_time_seconds(world)
def write():
 tmp=report.with_suffix('.tmp');tmp.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8');tmp.replace(report)
def wait(t):
 end=now()+t
 while now()<end:yield
def until(pred,t=4):
 end=now()+t
 while not pred() and now()<end:yield
def check(n,c,d=None):state['checks'].append(dict(name=n,passed=bool(c),details=d));write()
def stage(n):state['stages'].append(dict(name=n,time=now()));write()
def hp(f):return f.get_fighter_attribute_set().health.current_value
def near():
 for f,x,yaw in [(p1,-50,0),(p2,50,180)]:
  f.character_movement.stop_movement_immediately();f.set_actor_location(unreal.Vector(x,0,100),False,True);f.set_actor_rotation(unreal.Rotator(0,yaw,0),False)
def panel():return pc.get_editor_property('training_panel')
def checkbox(key,value):
 panel().get_editor_property({'infinite_health':'InfiniteHealth','infinite_resources':'InfiniteResources','no_cooldown':'NoCooldown'}[key]).set_is_checked(value);panel().call_method('SettingsChanged',(value,))
def suite():
 gm.reset_training();near();pc.set_control_rotation(unreal.Rotator(-15,0,0));yield from wait(.5)
 stage('静止木桩进入面板');pc.set_training_panel_open(True);yield from wait(.3)
 check('M4-demo_UMG_visible',panel().is_in_viewport() and pc.get_editor_property('show_mouse_cursor'))
 check('M4-demo_AI_available_M5',not panel().is_ai_option_disabled())
 panel().get_editor_property('OpponentChoice').set_selected_option('固定防御')
 check('M4-demo_mode_control',gm.get_editor_property('opponent_mode')==unreal.OpponentMode.FIXED_GUARD)
 panel().call_method('CloseClicked');yield from wait(.2)
 check('M4-demo_guard_resumes',p2.is_guard_intent())
 p1.get_combat_input().submit_light_attack();yield from until(lambda:p1.is_throw_paired(),2)
 check('M4-demo_throw_pair',p1.is_throw_paired());yield from until(lambda:not p1.is_throw_paired(),3)
 check('M4-demo_throw_damage_stats',hp(p2)==880 and gm.get_editor_property('player_stats').resolved_damage==120)
 stage('防御投技后修改训练开关');pc.set_training_panel_open(True);yield from wait(.3)
 for key in ('infinite_health','infinite_resources','no_cooldown'):checkbox(key,True)
 s=gm.get_editor_property('settings');check('M4-demo_checkbox_rules',s.infinite_health and s.infinite_resources and s.no_cooldown)
 panel().call_method('ProbeClicked');yield from wait(.2)
 check('M4-demo_probe_closes_menu',not gm.is_training_menu_open())
 check('M4-demo_probe_free_no_cooldown',p1.get_fighter_attribute_set().cursed_energy.current_value==100 and p1.get_fighter_ability_system_component().get_training_cooldown_remaining()==0)
 pc.set_training_panel_open(True);panel().get_editor_property('OpponentChoice').set_selected_option('静止木桩');panel().call_method('CloseClicked');near();yield from wait(.2)
 p1.get_combat_input().submit_kick();yield from wait(.6)
 check('M4-demo_hit_stats_update',gm.get_editor_property('player_stats').resolved_damage==165)
 yield from until(lambda:p1.can_act() and p2.can_act());p1.get_combat_input().submit_light_attack();yield from wait(.1)
 pc.set_training_panel_open(True);panel().call_method('ResetClicked');yield from wait(.2)
 check('M4-demo_action_reset',p1.can_act() and p2.can_act() and not p1.get_combat_hit().has_active_attack() and hp(p2)==1000)
 check('M4-demo_reset_settings_keep',gm.get_editor_property('settings').infinite_resources and gm.get_editor_property('player_stats').hits==0 and gm.is_training_menu_open())
 stage('关闭开关后恢复正常练习')
 for key in ('infinite_health','infinite_resources','no_cooldown'):checkbox(key,False)
 panel().call_method('CloseClicked');near();yield from wait(.2);p1.get_combat_input().submit_light_attack();yield from wait(1.3)
 check('M4-demo_normal_damage',hp(p2)==965 and p1.can_act())
 pc.set_training_panel_open(True);yield from wait(.2)
 check('M4-demo_actual_ASC_display','965/1000' in panel().get_displayed_state())
 stage('最终面板截图')
 screenshot=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))/'Docs/开发过程/验收记录/M4_训练面板.png'
 unreal.SystemLibrary.execute_console_command(world,'Shot showui filename="'+str(screenshot).replace('\\','/')+'" -nosuffix')
 yield from wait(.8)
 check('M4-demo_game_viewport_screenshot',screenshot.is_file() and screenshot.stat().st_mtime>=state['started_at'])

gen=suite();started=time.monotonic()
def tick(dt):
 try:
  if time.monotonic()-started>150:raise TimeoutError('M4 demo timeout')
  next(gen)
 except StopIteration:
  state['status']='passed' if all(c['passed'] for c in state['checks']) else 'failed';finish()
 except Exception:
  state['status']='failed';state['error']=traceback.format_exc();finish()
def finish():
 unreal.unregister_slate_post_tick_callback(handle);perf.set_editor_property('bThrottleCPUWhenNotForeground',old_throttle)
 state['duration_seconds']=time.monotonic()-started;write()
write();handle=unreal.register_slate_post_tick_callback(tick)
