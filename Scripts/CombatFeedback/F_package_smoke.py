"""Fresh sounded graphical smoke in F package. Usage: python Scripts/CombatFeedback/F_package_smoke.py.

Existing harness uses injected DebugKill results and domain energy, plus real
projectile/sweep checks. Explicitly NOT three normal AI matches/human gameplay.
Never uses -NoSound or NullRHI. Preserves one archive per launch.
"""
from pathlib import Path
import datetime
import hashlib
import json
import subprocess
import time
ROOT=Path(__file__).resolve().parents[2]
PACKAGE=ROOT/'Saved/Packages/FeedbackF_FA2_v2/Windows'
exe=PACKAGE/'JJK_theHiddenOnes/Binaries/Win64/JJK_theHiddenOnes.exe'
assert exe.is_file()
for mapname in ['L_DojoArena','L_TrainingArena']:
    out=ROOT/'Saved/CombatFeedback/F/package-smoke'/(datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'-'+mapname)
    out.mkdir(parents=True);log=out/'runtime.log'
    cmd=[str(exe),'/Game/Maps/'+mapname,'-M5SmokeTest','-SmokeMap='+mapname,'-DojoSweepRegression',
         '-windowed','-ResX=1920','-ResY=1080','-UserDir='+out.as_posix(),'-abslog='+log.as_posix(),
         '-ExecCmds=t.MaxFPS 60,JJK.Feedback.HitStop 0,JJK.Feedback.Audio 1,JJK.Feedback.AudioGain 1']
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    start=time.time();proc=subprocess.run(cmd,cwd=PACKAGE,timeout=210,startupinfo=startup)
    text=log.read_text(encoding='utf-8',errors='replace')
    rows=[l for l in text.splitlines() if '[M5Smoke]' in l]
    state={'candidate':'FA2-20260930-v2','map':mapname,'command':cmd,'exit_code':proc.returncode,
           'seconds':time.time()-start,'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),
           'checks':rows,'sound_enabled':True,'physical_human_input':False,'human_listening':False,
           'fixture':'M5Smoke uses injected deaths/domain energy and shared-input real blasts; not normal AI games',
           'errors':[l for l in text.splitlines() if 'Error:' in l or '=== Handled ensure' in l],
           'status':'passed' if proc.returncode==0 and '[M5Smoke] COMPLETE checks=22 failures=0' in text else 'failed'}
    (out/'report.json').write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'path':str(out),'status':state['status'],'errors':state['errors']},ensure_ascii=False),flush=True)
