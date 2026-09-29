"""Run inside a fresh PIE. Capture real HUD states; restore all temporary values, never save assets."""
import unreal, json, traceback
from pathlib import Path
from datetime import datetime

class HudV9Validation:
    def __init__(self):
        self.world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        assert self.world, 'Start PIE first'
        self.pc = unreal.GameplayStatics.get_player_controller(self.world, 0)
        self.gm = self.pc.get_training_game_mode()
        self.p1, self.p2 = self.gm.get_player_fighter(), self.gm.get_opponent_fighter()
        self.fd = self.p1.get_definition()
        self.original = {k:self.fd.get_editor_property(k) for k in ['initial_health','initial_energy']}
        self.settings, self.mode = self.gm.get_editor_property('settings'), self.gm.get_editor_property('opponent_mode')
        self.out = Path(unreal.Paths.project_saved_dir()) / 'HudV9' / datetime.now().strftime('%Y%m%d_%H%M%S')
        self.out.mkdir(parents=True)
        self.report = {'status':'running','map':self.world.get_path_name(),'checks':[],'screenshots':[]}
        self.gen = self.run()
        self.handle = unreal.register_slate_post_tick_callback(self.tick)
        print('HUD_V9_VALIDATION', str(self.out))

    def write(self):
        (self.out/'report.json').write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf-8')

    def check(self,name,ok):
        self.report['checks'].append({'name':name,'passed':bool(ok)})
        self.write()
        assert ok, name

    def wait(self,seconds):
        end = unreal.GameplayStatics.get_time_seconds(self.world)+seconds
        while unreal.GameplayStatics.get_time_seconds(self.world)<end: yield

    def shot(self,name):
        file = self.out/(name+'.png')
        unreal.SystemLibrary.execute_console_command(self.world,'Shot showui -nosuffix filename='+file.as_posix(),self.pc)
        yield from self.wait(.6)
        self.check(name+'_screenshot', file.exists())
        self.report['screenshots'].append(str(file))

    def tag(self,name):
        tag=unreal.GameplayTag();tag.import_text('(TagName="'+name+'")')
        return self.p1.has_combat_tag(tag)

    def run(self):
        self.gm.set_opponent_mode(unreal.OpponentMode.STATIC)
        self.gm.set_training_settings(unreal.TrainingSettings())
        self.gm.reset_training();self.pc.set_training_panel_open(False)
        yield from self.wait(1)
        self.check('formal_hud_visible',self.pc.get_editor_property('ArenaHud').get_visibility()==unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        self.check('all_12_textures_imported',all(unreal.load_asset('/Game/UI/HUD/V9/T_'+n) for n in ['scroll','portrait','punch','heavy','kick','hkick','swap','blast','sblast','aim','domain','mouse']))
        yield from self.shot('01_melee')
        self.fd.set_editor_property('initial_health',500.)
        self.p1.reset_to_initial_state()
        self.fd.set_editor_property('initial_health',300.)
        self.p2.reset_to_initial_state()
        self.fd.set_editor_property('initial_health',self.original['initial_health'])
        yield from self.wait(1.2)
        self.check('health_threshold_fixture',self.p1.get_fighter_attribute_set().health.current_value==500 and self.p2.get_fighter_attribute_set().health.current_value==300)
        yield from self.shot('02_yellow_red')
        self.gm.reset_training();yield from self.wait(.5)
        self.p1.get_combat_input().notify_stance_switch_pressed();yield from self.wait(.7)
        self.check('ranged_stance',self.tag('Stance.Ranged'))
        yield from self.shot('03_ranged')
        self.p1.get_combat_input().notify_kick_pressed();yield from self.wait(1)
        self.check('super_charging',self.tag('State.BlastCharging'))
        yield from self.shot('04_charge')
        yield from self.wait(1)
        self.p1.get_combat_input().notify_kick_released()
        # Cooldown begins after the release montage finishes, not on button-up.
        deadline=unreal.GameplayStatics.get_time_seconds(self.world)+4
        while not self.tag('State.SuperBlastCooldown') and unreal.GameplayStatics.get_time_seconds(self.world)<deadline: yield
        self.check('super_cooldown',self.tag('State.SuperBlastCooldown'))
        yield from self.shot('05_cooldown')
        self.fd.set_editor_property('initial_energy',100.)
        self.gm.reset_training();yield from self.wait(.6)
        for fighter,x,yaw in [(self.p1,-400,0),(self.p2,400,180)]:
            fighter.set_actor_location(unreal.Vector(x,0,137),False,True)
            fighter.set_actor_rotation(unreal.Rotator(0,yaw,0),False)
        yield from self.shot('06_domain_ready')
        self.p1.get_combat_input().notify_domain_pressed();yield from self.wait(1.5)
        self.check('domain_active',self.tag('State.DomainActive'))
        yield from self.shot('07_domain_active')
        self.pc.set_training_panel_open(True);yield from self.wait(.5)
        yield from self.shot('08_training_menu')

    def restore(self):
        for key,value in self.original.items(): self.fd.set_editor_property(key,value)
        self.gm.set_training_settings(self.settings)
        self.gm.set_opponent_mode(self.mode)
        self.gm.reset_training()
        self.pc.set_training_panel_open(False)

    def tick(self,dt):
        try: next(self.gen)
        except (StopIteration,Exception) as error:
            self.report['status']='passed' if isinstance(error,StopIteration) else 'failed'
            if not isinstance(error,StopIteration): self.report['error']=traceback.format_exc()
            try: self.restore()
            except Exception: self.report['restore_error']=traceback.format_exc();self.report['status']='failed'
            self.write()
            unreal.unregister_slate_post_tick_callback(self.handle)
            print('HUD_V9_RESULT',self.report['status'],str(self.out))

hud_v9_validation = HudV9Validation()
