"""Run in PIE: exercise the rendered menu and verify real gameplay state, then restore prefs."""
import json
import time
import traceback
from pathlib import Path
import unreal


class MenuAcceptance:
    def __init__(self):
        self.world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        assert self.world, 'Start PIE in L_DojoArena first'
        self.pc = unreal.GameplayStatics.get_player_controller(self.world, 0)
        self.gm = self.pc.get_training_game_mode()
        self.menu = self.pc.get_editor_property('game_menu')
        self.out = Path(unreal.Paths.project_saved_dir()) / 'MenuValidation' / time.strftime('%Y%m%d_%H%M%S')
        self.out.mkdir(parents=True)
        self.prefs = Path(unreal.Paths.project_saved_dir()) / 'Menu/UserSettings.json'
        self.original_prefs = self.prefs.read_bytes() if self.prefs.exists() else None
        self.original_engine = Path(unreal.Paths.project_saved_dir()) / 'Config/WindowsEditor/GameUserSettings.ini'
        self.original_engine_bytes = self.original_engine.read_bytes() if self.original_engine.exists() else None
        self.report = {'status': 'running', 'map': self.world.get_path_name(), 'checks': [], 'screenshots': []}
        self.gen = self.run()
        self.handle = unreal.register_slate_post_tick_callback(self.tick)
        self.write()
        print('MENU_VALIDATION', str(self.out))

    def write(self):
        (self.out / 'report.json').write_text(json.dumps(self.report, ensure_ascii=False, indent=2), encoding='utf-8')

    def check(self, name, passed):
        self.report['checks'].append({'name': name, 'passed': bool(passed)})
        self.write()
        assert passed, name

    def wait(self, seconds):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            yield

    def js(self, script):
        self.menu.debug_javascript(script)

    def click(self, selector):
        self.js(f'document.querySelector({json.dumps(selector)}).click();')
        yield from self.wait(.8)

    def state(self):
        self.js('window.menuUE.probe();')
        yield from self.wait(.5)
        self.last = json.loads(self.menu.get_last_probe())

    def shot(self, name):
        path = self.out / (name + '.png')
        self.check(name + '_owned_window_capture', self.menu.debug_capture_screenshot(path.as_posix()))
        yield from self.wait(.8)
        self.check(name + '_screenshot', path.exists())
        self.report['screenshots'].append(str(path))

    def key(self, name):
        self.pc.debug_send_key(name, True)
        self.pc.debug_send_key(name, False)
        yield from self.wait(.8)

    def run(self):
        deadline = time.monotonic() + 30
        while not self.menu.is_ready() and time.monotonic() < deadline:
            yield
        self.check('local_design_ready', self.menu.is_ready())
        self.check('default_main_menu', self.menu.get_current_page() == 'main')
        self.check('main_freezes_game', unreal.GameplayStatics.is_game_paused(self.world))
        yield from self.state()
        self.baseline = self.last['saved']
        yield from self.shot('01_main')
        yield from self.click('[data-action="start"]')
        self.check('start_page', self.menu.get_current_page() == 'start')
        yield from self.click('[data-action="enter-battle"]')
        self.check('real_game_unpaused', not unreal.GameplayStatics.is_game_paused(self.world))
        self.check('ai_mode_applied', self.gm.get_editor_property('opponent_mode') == unreal.OpponentMode.AI)
        self.check('both_real_fighters_spawned', bool(self.gm.get_player_fighter() and self.gm.get_opponent_fighter()))
        self.check('native_hud_visible', self.pc.get_editor_property('arena_hud').get_visibility() == unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        yield from self.shot('02_game')
        yield from self.key('Escape')
        self.check('escape_opens_pause', self.menu.get_current_page() == 'pause')
        self.check('pause_freezes_game', unreal.GameplayStatics.is_game_paused(self.world))
        frozen = unreal.GameplayStatics.get_time_seconds(self.world)
        yield from self.wait(1)
        self.check('world_time_stops', unreal.GameplayStatics.get_time_seconds(self.world) == frozen)
        yield from self.shot('03_pause')
        yield from self.key('Escape')
        self.check('escape_resumes_real_game', self.menu.get_current_page() == 'battle' and not unreal.GameplayStatics.is_game_paused(self.world))
        yield from self.key('F1')
        self.check('f1_opens_new_training_settings', self.menu.get_current_page() == 'settings')
        yield from self.state()
        self.check('training_tab_source', self.last['tab'] == 'training' and self.last['settingsSource'] == 'pause')
        yield from self.click('[data-toggle="infiniteEnergy"]')
        yield from self.click('[data-action="back"]')
        yield from self.state()
        self.check('unsaved_changes_confirmation', self.last['modal'] == 'unsaved')
        yield from self.click('[data-modal="discard"]')
        self.check('discard_returns_to_pause', self.menu.get_current_page() == 'pause')
        self.check('discard_does_not_apply', self.gm.get_editor_property('settings').infinite_resources == self.baseline['infiniteEnergy'])
        yield from self.click('[data-action="training-settings"]')
        self.js("setField('infiniteEnergy',true);setField('noCooldown',true);setField('opponent','固定防御');")
        yield from self.wait(.3)
        yield from self.click('[data-action="apply"]')
        self.check('real_infinite_resources_applied', self.gm.get_editor_property('settings').infinite_resources)
        self.check('real_no_cooldown_applied', self.gm.get_editor_property('settings').no_cooldown)
        self.check('real_guard_opponent_applied', self.gm.get_editor_property('opponent_mode') == unreal.OpponentMode.FIXED_GUARD)
        self.check('settings_persisted', json.loads(self.prefs.read_text(encoding='utf-8'))['infiniteEnergy'])
        yield from self.shot('04_training_settings')
        yield from self.click('[data-tab="audio"]')
        self.js("setField('master',40);setField('sfx',30);")
        yield from self.wait(.3)
        yield from self.click('[data-action="apply"]')
        self.check('real_sfx_gain_applied', abs(float(unreal.SystemLibrary.get_console_variable_float_value('JJK.Feedback.AudioGain'))-.3) < .001)
        yield from self.click('[data-tab="input"]')
        self.js("setField('sensitivity',75);setField('invert',true);setField('shake',false);")
        yield from self.wait(.3)
        yield from self.click('[data-action="apply"]')
        self.check('real_camera_shake_disabled', self.pc.get_camera_feedback_strength() == 0)
        yield from self.click('[data-tab="graphics"]')
        yield from self.shot('05_graphics_settings')
        yield from self.click('[data-action="back"]')
        yield from self.click('[data-action="controls"]')
        self.check('controls_page', self.menu.get_current_page() == 'controls')
        yield from self.shot('06_controls')
        yield from self.click('[data-action="back"]')
        yield from self.click('[data-action="restart"]')
        yield from self.click('[data-modal="cancel"]')
        self.check('restart_cancel_keeps_pause', self.menu.get_current_page() == 'pause')
        round_id = self.gm.get_feedback_round_id()
        yield from self.click('[data-action="restart"]')
        yield from self.click('[data-modal="confirm"]')
        self.check('real_match_restarted', self.gm.get_feedback_round_id() > round_id and not unreal.GameplayStatics.is_game_paused(self.world))
        yield from self.key('Escape')
        yield from self.click('[data-action="return-main"]')
        yield from self.click('[data-modal="confirm"]')
        self.check('return_main_pauses_real_game', self.menu.get_current_page() == 'main' and unreal.GameplayStatics.is_game_paused(self.world))
        yield from self.click('[data-action="settings"]')
        yield from self.click('[data-action="back"]')
        self.check('main_settings_return_source', self.menu.get_current_page() == 'main')
        yield from self.click('[data-action="training-start"]')
        yield from self.click('[data-action="enter-battle"]')
        self.check('training_starts_with_saved_opponent', self.gm.get_editor_property('opponent_mode') == unreal.OpponentMode.FIXED_GUARD)
        yield from self.key('Escape')
        # Restore preferences through the same bridge; retain actual display settings.
        self.menu.command(json.dumps({'action': 'apply', 'settings': self.baseline}, ensure_ascii=False))
        self.pc.open_game_menu('main', 'graphics')
        yield from self.wait(.5)

    def restore_files(self):
        for path, original in [(self.prefs, self.original_prefs), (self.original_engine, self.original_engine_bytes)]:
            if original is None:
                path.unlink(missing_ok=True)
            else:
                path.write_bytes(original)

    def tick(self, dt):
        try:
            next(self.gen)
        except (StopIteration, Exception) as error:
            self.report['status'] = 'passed' if isinstance(error, StopIteration) else 'failed'
            if not isinstance(error, StopIteration):
                self.report['error'] = traceback.format_exc()
            self.restore_files()
            self.write()
            unreal.unregister_slate_post_tick_callback(self.handle)
            print('MENU_RESULT', self.report['status'], str(self.out))


menu_acceptance = MenuAcceptance()
