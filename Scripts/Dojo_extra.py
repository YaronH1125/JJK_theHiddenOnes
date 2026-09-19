"""Targeted domain-suppression and real Slate keyboard routing checks."""
import unreal,time,json,traceback
from pathlib import Path
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(w,0);gm=pc.get_training_game_mode();p1=gm.get_player_fighter();p2=gm.get_opponent_fighter()
fd=p1.get_definition();old=fd.get_editor_property('initial_energy')
state={'status':'running','map':w.get_path_name(),'checks':[],'input':'Slate keyboard events, not physical human input'}
out=Path(unreal.Paths.project_saved_dir())/'Dojo_extra.json'
def write():out.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
def check(name,ok):state['checks'].append({'name':name,'passed':bool(ok)});write()
def wait(s):
    end=unreal.GameplayStatics.get_time_seconds(w)+s
    while unreal.GameplayStatics.get_time_seconds(w)<end:yield
def tag(f,n):
    v=unreal.GameplayTag();v.import_text('(TagName="'+n+'")');return f.has_combat_tag(v)
def key(k,down):pc.debug_send_key(k,down)
def tap(k):key(k,True);yield from wait(.08);key(k,False);yield from wait(.2)
def clean():
    for k in ('F1','F','W','LeftShift','E'):key(k,False)
    pc.set_training_panel_open(False);gm.set_opponent_mode(unreal.OpponentMode.STATIC);gm.reset_training()
    unreal.WidgetLibrary.set_focus_to_game_viewport()
def suite():
    clean();yield from wait(.4)
    yield from tap('F1');check('F1_opens_training_panel',gm.is_training_menu_open())
    yield from tap('F1');check('F1_closes_training_panel',not gm.is_training_menu_open())
    start=p1.get_actor_location();key('W',True);yield from wait(.4);key('W',False)
    check('W_moves_character',(p1.get_actor_location()-start).length()>20)
    key('F',True);yield from wait(.2);check('F_guard_held',p1.is_guard_intent());key('F',False);yield from wait(.2);check('F_guard_released',not p1.is_guard_intent())
    yield from tap('E');check('E_switches_to_ranged',tag(p1,'Stance.Ranged'))
    yield from tap('E');check('E_switches_back',tag(p1,'Stance.Melee'))
    # Wait for real contact AND recovery, not a fixed delay during first-use FX loading.
    for i in range(2):
        clean();yield from wait(.4)
        p1.get_combat_input().notify_stance_switch_pressed();yield from wait(.3)
        p1.get_targeting().lock_best_target()
        before=p2.get_fighter_attribute_set().health.current_value
        p1.get_combat_input().notify_kick_pressed();yield from wait(2.8)
        p1.get_combat_input().notify_kick_released()
        deadline=unreal.GameplayStatics.get_time_seconds(w)+6
        while unreal.GameplayStatics.get_time_seconds(w)<deadline:
            if p2.get_fighter_attribute_set().health.current_value<before and tag(p1,'State.SuperBlastCooldown'):break
            yield
        check('super_contact_and_cooldown_'+str(i),abs(before-p2.get_fighter_attribute_set().health.current_value-fd.super_blast.max_damage)<1 and tag(p1,'State.SuperBlastCooldown'))
    fd.set_editor_property('initial_energy',100.);clean();yield from wait(.3)
    p1.get_combat_input().notify_domain_pressed();p2.get_combat_input().notify_domain_pressed();yield from wait(2.)
    check('simultaneous_domains_active',tag(p1,'State.DomainActive') and tag(p2,'State.DomainActive'))
    check('dual_domain_no_orbs',not unreal.GameplayStatics.get_all_actors_of_class(w,unreal.DomainOrb))
    gm.reset_training();yield from wait(.4)
    check('dual_domain_reset_clean',not tag(p1,'State.DomainActive') and not tag(p2,'State.DomainActive') and not unreal.GameplayStatics.get_all_actors_of_class(w,unreal.DomainOrb))
    unreal.SystemLibrary.execute_console_command(w,'HighResShot 1')
gen=suite()
def tick(dt):
    try:next(gen)
    except Exception as e:
        unreal.unregister_slate_post_tick_callback(handle)
        state['status']='passed' if isinstance(e,StopIteration) and all(c['passed'] for c in state['checks']) else 'failed'
        if not isinstance(e,StopIteration):state['error']=traceback.format_exc()
        fd.set_editor_property('initial_energy',old);clean();write()
write();handle=unreal.register_slate_post_tick_callback(tick)
