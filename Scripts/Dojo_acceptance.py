"""Real PIE integration checks. No asset edits; no forced victory or fabricated human play."""
import unreal,json,time,traceback,statistics
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
pc=unreal.GameplayStatics.get_player_controller(world,0)
gm=pc.get_training_game_mode()
p1=gm.get_player_fighter();p2=gm.get_opponent_fighter()
original_speed=p1.character_movement.max_walk_speed
fd=p1.get_definition();original_energy=fd.get_editor_property('initial_energy')
out=Path(unreal.Paths.project_saved_dir())/'Dojo_acceptance.json'
state={'status':'running','map':world.get_path_name(),'game_mode':gm.get_class().get_path_name(),'checks':[],'performance':{},'audio':'enabled; not a human listening test','viewport':str(pc.get_viewport_size()),'ray_tracing':unreal.SystemLibrary.get_console_variable_int_value('r.RayTracing')}
def write():
    tmp=out.with_suffix('.tmp');tmp.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8');tmp.replace(out)
def check(name,ok,detail=None):
    state['checks'].append(dict(name=name,passed=bool(ok),detail=detail));write()
def now():return unreal.GameplayStatics.get_time_seconds(world)
def wait(t):
    end=now()+t
    while now()<end:yield
def until(fn,t):
    end=now()+t
    while not fn() and now()<end:yield
def hp(f):return f.get_fighter_attribute_set().health.current_value
def tag(f,n):
    v=unreal.GameplayTag();v.import_text('(TagName="'+n+'")');return f.has_combat_tag(v)
def place(f,x,y,yaw):
    f.character_movement.stop_movement_immediately()
    f.set_actor_location(unreal.Vector(x,y,137),False,True)
    f.set_actor_rotation(unreal.Rotator(0,yaw,0),False)
def reset(mode=unreal.OpponentMode.STATIC):
    pc.set_training_panel_open(False);gm.set_opponent_mode(mode);gm.reset_training();pc.set_training_panel_open(False)
def punch():
    p1.get_combat_input().notify_attack_pressed();p1.get_combat_input().notify_attack_released();yield from wait(.85)
def shot(super=False):
    inp=p1.get_combat_input()
    before=hp(p2)
    if not tag(p1,'Stance.Ranged'):inp.notify_stance_switch_pressed();yield from wait(.3)
    p1.get_targeting().lock_best_target()
    if super:inp.notify_kick_pressed()
    else:inp.notify_attack_pressed()
    yield from wait(2.8 if super else 1.6)
    if super:inp.notify_kick_released()
    else:inp.notify_attack_released()
    yield from until(lambda:hp(p2)<before and (tag(p1,'State.SuperBlastCooldown') if super else not tag(p1,'State.BlastCharging')),6.)
samples=[]
def sample(name,duration=60):
    global samples
    yield from wait(5)
    samples=[]
    yield from wait(duration)
    values=samples[:];s=sorted(values)
    state['performance'][name]={'frames':len(s),'mean_ms':statistics.mean(s)*1000,'p95_ms':s[int(len(s)*.95)]*1000,'max_ms':max(s)*1000,'mean_fps':1/statistics.mean(s),'seconds':sum(s)};write()
def suite():
    unreal.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings').set_editor_property('bThrottleCPUWhenNotForeground',False)
    unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 60')
    yield from wait(3)
    reset();yield from wait(.5)
    check('correct_map_and_throw_disabled','L_DojoArena' in world.get_path_name() and not gm.get_editor_property('allow_conditional_throw'))
    walls=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.StaticMeshActor) if a.get_actor_label().startswith('VFXWall_')]
    check('four_invisible_colliding_walls',len(walls)==4 and all(not a.static_mesh_component.is_visible() and a.static_mesh_component.get_editor_property('hidden_in_game') and str(a.static_mesh_component.get_collision_enabled())!='CollisionEnabled.NO_COLLISION' for a in walls))
    # Swept character movement is stopped at every physical wall.
    for label,x,y in [('east',1000,0),('west',-1100,0),('north',-28,1000),('south',-28,-1000)]:
        reset();yield from wait(.2);place(p2,400,400,180);place(p1,-28,0,0)
        p1.set_actor_location(unreal.Vector(x,y,137),True,False)
        v=p1.get_actor_location()
        at_wall=(abs(v.x-732)<8 if label=='east' else abs(v.x+788)<8 if label=='west' else abs(abs(v.y)-608)<8)
        check('capsule_boundary_'+label,at_wall and -792<=v.x<=736 and abs(v.y)<=612,str(v))
    # All corners lie beyond the obsolete 600cm circle. Test actual path requests.
    for i,(x,y) in enumerate([(-728,-552),(-728,552),(672,-552),(672,552)]):
        reset();yield from wait(.2);place(p1,x,y,0);place(p2,-28,0,0)
        gm.set_opponent_mode(unreal.OpponentMode.AI);yield from wait(.3)
        ai=gm.get_opponent_ai()
        legal=ai.is_legal_destination(unreal.Vector(x,y,137))
        yield from until(lambda:(p2.get_actor_location()-p1.get_actor_location()).length()<280,12)
        distance=(p2.get_actor_location()-p1.get_actor_location()).length()
        check('AI_reaches_corner_'+str(i),legal and distance<280,{'legal':legal,'distance':distance,'state':ai.get_debug_state()})
    # Front guard still works, and disabling throws must not swallow ordinary contacts.
    reset();yield from wait(.4);place(p1,-128,0,0);place(p2,0,0,180)
    p2.get_combat_input().notify_guard_pressed();yield from wait(.2)
    health=hp(p2)
    for _ in range(5):yield from punch()
    check('front_guard_no_throw_no_damage',not p1.is_throw_paired() and not p2.is_throw_paired() and hp(p2)==health,{'health':hp(p2)})
    # Actually move around a held guard; do not teleport into a back-hit pose.
    p1.character_movement.max_walk_speed=500
    for target in [unreal.Vector(-128,250,137),unreal.Vector(160,250,137),unreal.Vector(160,0,137)]:
        deadline=now()+5
        while (p1.get_actor_location()-target).length()>25 and now()<deadline:
            delta=target-p1.get_actor_location()
            p1.add_movement_input(delta/delta.length(),1.0,True);yield
    p1.set_actor_rotation(unreal.Rotator(0,180,0),False);yield from wait(.2)
    yield from punch()
    check('held_guard_can_be_flanked',hp(p2)<health,{'health':hp(p2),'attacker':str(p1.get_actor_location()),'target':str(p2.get_actor_location())})
    p1.character_movement.max_walk_speed=original_speed
    reset();yield from wait(.4);health=hp(p2);yield from shot()
    check('full_mobile_damage',abs(health-hp(p2)-p1.get_definition().mobile_blast.max_damage)<1,health-hp(p2))
    reset();yield from wait(.4);health=hp(p2);yield from shot(True)
    check('full_super_damage_and_cooldown',abs(health-hp(p2)-p1.get_definition().super_blast.max_damage)<1 and tag(p1,'State.SuperBlastCooldown'),health-hp(p2))
    fd.set_editor_property('initial_energy',100.)
    for i in range(20):
        reset();yield from wait(.15)
        if i%2:p1.get_combat_input().notify_domain_pressed()
        else:p1.get_combat_input().notify_stance_switch_pressed();yield from wait(.1);p1.get_combat_input().notify_attack_pressed()
        yield from wait(.4);gm.reset_training();yield from wait(.2)
        check('reset_'+str(i),not any(tag(p1,n) for n in ['State.BlastCharging','State.DomainCasting','State.DomainActive']) and not unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DomainOrb))
    # Three normal-resource AI rounds: passive player, no debug kill, no damage overrides.
    fd.set_editor_property('initial_energy',original_energy)
    for i in range(3):
        reset(unreal.OpponentMode.AI);start=now()
        yield from until(lambda:gm.is_match_resolved(),180)
        check('normal_AI_round_'+str(i),gm.is_match_resolved() and p1.is_dead(),{'seconds':now()-start,'player_hp':hp(p1),'opponent_hp':hp(p2)})
    reset();yield from sample('idle')
    reset(unreal.OpponentMode.AI);yield from sample('AI')
    # Repeat the original gameplay workload, resetting between shots to avoid target death.
    global samples
    samples=[];start=now()
    while now()-start<60:
        reset();yield from wait(.3);yield from shot(True)
    s=sorted(samples)
    state['performance']['super_blast_cycles']={'frames':len(s),'mean_ms':statistics.mean(s)*1000,'p95_ms':s[int(len(s)*.95)]*1000,'max_ms':max(s)*1000,'mean_fps':1/statistics.mean(s),'seconds':sum(s)};write()
    fd.set_editor_property('initial_energy',100.)
    reset();p1.get_combat_input().notify_domain_pressed();yield from wait(1.5)
    check('domain_active',tag(p1,'State.DomainActive'))
    # Simultaneous casts isolate suppression; sequential casts can legitimately be interrupted by the first orb.
    reset();yield from wait(.3)
    p1.get_combat_input().notify_domain_pressed();p2.get_combat_input().notify_domain_pressed();yield from wait(2.)
    check('dual_domain_suppresses_orbs',tag(p1,'State.DomainActive') and tag(p2,'State.DomainActive') and not unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DomainOrb))
    reset();yield from wait(.5)
    unreal.SystemLibrary.execute_console_command(world,'HighResShot 1')
gen=suite();started=time.monotonic()
def tick(dt):
    samples.append(dt)
    try:
        if time.monotonic()-started>1000:raise TimeoutError('DOJO test timeout')
        next(gen)
    except (StopIteration,Exception) as e:
        unreal.unregister_slate_post_tick_callback(handle)
        state['status']='passed' if isinstance(e,StopIteration) and all(c['passed'] for c in state['checks']) else 'failed'
        if not isinstance(e,StopIteration):state['error']=traceback.format_exc()
        state['duration_seconds']=time.monotonic()-started
        try:
            fd.set_editor_property('initial_energy',original_energy)
            p1.character_movement.max_walk_speed=original_speed
            reset()
        finally:write()
write();handle=unreal.register_slate_post_tick_callback(tick)
