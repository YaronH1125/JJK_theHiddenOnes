"""Follow-up verification: Slate key routing, rendered locomotion, Shift, and battle HUD."""
import unreal,time,json,traceback
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0);gm=pc.get_training_game_mode()
p1=gm.get_player_fighter();p2=gm.get_opponent_fighter();fd=p1.get_definition()
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()));out=root/'Saved/M5_followup.json'
state={'status':'running','checks':[],'started_at':time.time(),'engine':unreal.SystemLibrary.get_engine_version()}
original=[]
original_dodge_speed=fd.get_editor_property('dodge_config').dodge_speed
perf=unreal.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings')
old_throttle=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
def write():
 out.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
def check(name,value,detail=None):
 state['checks'].append({'name':name,'passed':bool(value),'details':detail});write()
def now():return unreal.GameplayStatics.get_time_seconds(world)
def wait(t):
 end=now()+t
 while now()<end:yield
def key(name,down=True):pc.debug_send_key(name,down)
def tap(name):
 key(name);yield from wait(.08);key(name,False);yield from wait(.12)
def tag(f,n):
 t=unreal.GameplayTag();t.import_text('(TagName="'+n+'")');return f.has_combat_tag(t)
def hp(f):return f.get_fighter_attribute_set().health.current_value
def ar(f):return f.get_fighter_attribute_set().action_resource.current_value
def text():
 h=pc.get_editor_property('combat_hud');h.refresh();return h.get_displayed_state()
def place(f,x,y=0,yaw=0):
 f.character_movement.stop_movement_immediately();f.set_actor_location(unreal.Vector(x,y,100),False,True);f.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
def clean():
 for k in ['W','LeftShift','F','F1']:key(k,False)
 pc.set_training_panel_open(False);gm.set_opponent_mode(unreal.OpponentMode.STATIC)
 gm.set_training_settings(unreal.TrainingSettings());gm.reset_training()
 pc.set_control_rotation(unreal.Rotator(pitch=-12,yaw=0,roll=0));unreal.WidgetLibrary.set_focus_to_game_viewport()
def shot(name):
 if '-nullrhi' not in unreal.SystemLibrary.get_command_line().lower():
  path=root/'Docs/开发过程/验收记录'/name
  unreal.SystemLibrary.execute_console_command(world,'Shot showui filename="'+path.as_posix()+'" -nosuffix')
def temp(obj,name,value):
 original.append((obj,name,obj.get_editor_property(name)));obj.set_editor_property(name,value)
def suite():
 clean();unreal.SystemLibrary.execute_console_command(world,'viewmode lit');unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 60');yield from wait(.4)
 check('HUD_initial_ASC_values','生命  1000 / 1000' in text() and '行动  5.0 / 5' in text())
 check('default_dodge_speed',fd.get_editor_property('dodge_config').dodge_speed==900.)
 check('HUD_visible_noninteractive',pc.get_editor_property('combat_hud').is_in_viewport())
 for i in range(3):
  yield from tap('F1');check('F1_opens_'+str(i),gm.is_training_menu_open())
  check('F1_stays_lit_'+str(i),not pc.is_wireframe_view())
  yield from tap('F1');check('F1_closes_'+str(i),not gm.is_training_menu_open())
 yield from tap('F1');pc.get_editor_property('training_panel').call_method('AIClicked');yield from tap('F1')
 check('AI_via_menu_stays_lit',gm.get_editor_property('opponent_mode')==unreal.OpponentMode.AI and not pc.is_wireframe_view())
 ai=gm.get_opponent_ai();params=ai.get_params();params.attack_chance=0;ai.set_params(params)
 pc.set_control_rotation(unreal.Rotator(pitch=-12,yaw=-35,roll=0));poses=[];samples=[];end=now()+.8
 while now()<end:
  a=p2.mesh.get_anim_instance()
  samples.append((p2.get_velocity().length(),p2.character_movement.get_current_acceleration().length(),bool(a.get_editor_property('ShouldMove'))))
  poses.append(p2.mesh.get_socket_transform('foot_l',unreal.RelativeTransformSpace.RTS_COMPONENT).translation)
  yield
 check('AI_speed_acceleration_animation',any(v>100 and a>0 and m for v,a,m in samples),samples[::max(1,len(samples)//5)])
 check('AI_pose_actually_changes',max((v-poses[0]).length() for v in poses)>3)
 shot('M5_修复_AI移动与HUD.png');yield from wait(.2)
 clean();place(p2,500,600);yield from wait(.3)
 start=p1.get_actor_location();forward=p1.get_actor_forward_vector();key('LeftShift');yield from wait(.08)
 check('stationary_shift_backstep',(p1.get_actor_location()-start).dot(forward)<-10 and tag(p1,'State.DodgeInvulnerable'),str((p1.get_actor_location()-start,forward)))
 check('stationary_has_pose',p1.mesh.get_anim_instance().get_current_active_montage()==fd.get_editor_property('backstep_montage'))
 check('stationary_cost_once',abs(ar(p1)-4)<.02,ar(p1))
 key('LeftShift',False);yield from wait(.8)
 check('stationary_no_sprint',not p1.is_sprinting() and p1.can_act())
 check('stationary_distance_owned_by_ability',170<(p1.get_actor_location()-start).length()<270,(p1.get_actor_location()-start).length())
 clean();place(p2,500,600);yield from wait(.2)
 key('W');yield from wait(.2);key('LeftShift');yield from wait(.08)
 check('moving_shift_dodge',tag(p1,'State.DodgeInvulnerable'))
 check('moving_shift_actual_burst',800<p1.get_velocity().length()<1000,p1.get_velocity().length())
 check('moving_has_pose','AM_Dodge' in str(p1.mesh.get_anim_instance().get_current_active_montage()))
 yield from wait(.25)
 check('recovery_sprint_without_invulnerability',p1.is_sprinting() and tag(p1,'State.DodgeRecovery') and not tag(p1,'State.DodgeInvulnerable'))
 check('recovery_still_rejects_attack', 'REJECTED_BLOCKED' in str(p1.get_combat_input().submit_light_attack()))
 yield from wait(.4)
 check('held_shift_continuous_run',p1.is_sprinting() and p1.get_velocity().length()>600,(p1.get_velocity().length(),p1.character_movement.max_walk_speed))
 check('held_shift_no_repeat_cost',abs(ar(p1)-4)<.1,ar(p1))
 check('HUD_run_state','疾跑' in text() and '行动  4.0 / 5' in text())
 shot('M5_修复_Shift持续跑.png');yield from wait(.1)
 key('LeftShift',False);yield from wait(.15)
 check('release_restores_walk_speed',not p1.is_sprinting() and abs(p1.character_movement.max_walk_speed-500)<.1)
 key('W',False);yield from wait(.1)
 clean();place(p2,500,600);yield from wait(.2);key('W');yield from wait(.15);key('LeftShift');yield from wait(.7)
 yield from tap('F1');check('menu_clears_sprint',gm.is_training_menu_open() and not p1.is_sprinting())
 yield from tap('F1');yield from wait(.2);check('menu_no_stuck_run',not p1.is_sprinting() and p1.get_velocity().length()<2)
 clean();place(p2,500,600);yield from wait(.2);key('W');yield from wait(.15);key('LeftShift');yield from wait(.7)
 pc.call_method('HandleAppActivationChanged',(False,));yield from wait(.1)
 check('focus_loss_clears_sprint',not p1.is_sprinting() and abs(p1.character_movement.max_walk_speed-500)<.1)
 clean();place(p2,500,600);yield from wait(.2);key('W');yield from wait(.15);key('LeftShift');yield from wait(.7)
 p1.jjk_debug_force_hit_react();yield from wait(.1);check('hit_clears_sprint',not p1.is_sprinting())
 clean();temp(fd,'initial_action_resource',0.);gm.reset_training();place(p2,500,600)
 key('W');yield from wait(.1);key('LeftShift');yield from wait(.12)
 check('zero_resource_rejects_dodge_and_run',not tag(p1,'State.DodgeInvulnerable') and not p1.is_sprinting())
 fd.set_editor_property('initial_action_resource',5.);clean();yield from wait(.3)
 clean();place(p2,500,600);yield from wait(.2);key('W');yield from wait(.15)
 key('LeftShift');key('LeftShift',False);yield from wait(.8)
 check('same_frame_shift_tap_no_stuck_run',not p1.is_sprinting() and abs(p1.character_movement.max_walk_speed-500)<.1)
 # Controlled real sweep: zero travel keeps the defender in the attack's hit volume.
 cfg=fd.get_editor_property('dodge_config');cfg.set_editor_property('dodge_speed',0.);fd.set_editor_property('dodge_config',cfg)
 temp(fd.get_editor_property('kick_definition'),'trace_radius',300.)
 place(p1,400);place(p2,500,yaw=180);yield from wait(.25)
 p1.get_combat_input().submit_kick();yield from wait(.18);defender_start=p2.get_actor_location();p2.request_dodge(unreal.Vector(0,1,0));yield from wait(.15)
 check('real_contact_dodge_success',p2.has_recent_dodge_success() and hp(p2)==1000,(hp(p2),gm.get_editor_property('player_stats').immunes))
 check('HUD_success_feedback','闪避成功' in text());shot('M5_修复_闪避成功.png');yield from wait(.2)
 yield from wait(.9);check('feedback_expires',not p2.has_recent_dodge_success())
 check('montage_cannot_add_travel',(p2.get_actor_location()-defender_start).length()<2,(p2.get_actor_location()-defender_start).length())
 for obj,n,v in reversed(original):obj.set_editor_property(n,v)
 original.clear();cfg=fd.get_editor_property('dodge_config');cfg.set_editor_property('dodge_speed',original_dodge_speed);fd.set_editor_property('dodge_config',cfg);clean();place(p1,0);place(p2,100,yaw=180);yield from wait(.3)
 p1.get_combat_input().submit_light_attack();yield from wait(.8)
 check('HUD_damage_from_ASC',hp(p2)<1000 and ('生命  '+str(round(hp(p2)))+' / 1000') in text(),hp(p2))
 gm.reset_training();yield from wait(.15);check('HUD_reset_restores',text().count('生命  1000 / 1000')==2)
 p2.destroy_actor();yield from wait(.15);check('HUD_destroyed_target_safe','等待角色' in text())
 pc.call_method('JJKRespawnFighters');yield from wait(.3);check('HUD_respawn_rebinds','等待角色' not in text())
 clean();pc.set_control_rotation(unreal.Rotator(pitch=-12,yaw=-35,roll=0));yield from wait(.4);shot('M5_修复_常驻战斗UI.png');yield from wait(.2)

gen=suite();started=time.monotonic()
def finish():
 unreal.unregister_slate_post_tick_callback(handle)
 cfg=fd.get_editor_property('dodge_config');cfg.set_editor_property('dodge_speed',original_dodge_speed);fd.set_editor_property('dodge_config',cfg)
 for obj,n,v in reversed(original):obj.set_editor_property(n,v)
 for k in ['W','LeftShift','F','F1']:key(k,False)
 pc.set_training_panel_open(False);gm.set_opponent_mode(unreal.OpponentMode.STATIC);gm.reset_training()
 perf.set_editor_property('bThrottleCPUWhenNotForeground',old_throttle)
 state['duration_seconds']=time.monotonic()-started;write()
def tick(dt):
 try:
  if time.monotonic()-started>150:raise TimeoutError('follow-up timeout')
  next(gen)
 except StopIteration:state['status']='passed' if all(x['passed'] for x in state['checks']) else 'failed';finish()
 except Exception:state['status']='failed';state['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
