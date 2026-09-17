
import unreal,json
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 60')
L=[]
def suite():
 end=unreal.GameplayStatics.get_time_seconds(world)+4
 n=0;t0=end-4
 while unreal.GameplayStatics.get_time_seconds(world)<end:
  n+=1;yield
 L.append('fps60_measured=%.1f ticks=%d game=%.2f'%(n/(unreal.GameplayStatics.get_time_seconds(world)-t0),n,unreal.GameplayStatics.get_time_seconds(world)-t0))
 unreal.log('FPSRES '+json.dumps(L))
gen=suite()
def tick(dt):
 try: next(gen)
 except Exception as e: unreal.log('FPSERR '+str(e));unreal.unregister_slate_post_tick_callback(handle)
handle=unreal.register_slate_post_tick_callback(tick)
