"""Map-specific smoke with isolated logs and immutable timestamped results.
Use --editor for uncooked standalone checks, otherwise run the Dojo package.
"""
import argparse,subprocess,json,time,hashlib,shutil
from pathlib import Path
root=Path(__file__).resolve().parents[1]
(root/'Docs/开发过程/验收记录').mkdir(parents=True,exist_ok=True)
ap=argparse.ArgumentParser();ap.add_argument('--editor',action='store_true');ap.add_argument('--map',default='L_DojoArena');ap.add_argument('--nullrhi',action='store_true');ap.add_argument('--default-map',action='store_true');args=ap.parse_args()
stamp=time.strftime('%Y%m%d_%H%M%S');runtime=root/'Saved/DojoValidation'/f'{args.map}_{stamp}';runtime.mkdir(parents=True)
if args.editor:
    exe=Path('F:/GameStudy/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe');cmd=[str(exe),str(root/'JJK_theHiddenOnes.uproject'),'/Game/Maps/'+args.map,'-game']
else:
    exe=root/'Saved/Packages/Dojo/Windows/JJK_theHiddenOnes/Binaries/Win64/JJK_theHiddenOnes.exe'
    cmd=[str(exe)] if args.default_map else [str(exe),'/Game/Maps/'+args.map]
cmd+=['-M5SmokeTest','-DojoSweepRegression','-SmokeMap='+args.map,'-unattended','-windowed','-ResX=1280','-ResY=720','-ExecCmds=stat fps','-UserDir='+runtime.as_posix(),'-abslog='+(runtime/'runtime.log').as_posix()]
if args.nullrhi:cmd+=['-nullrhi','-NoSound']
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
started=time.time();code=None
try:code=subprocess.run(cmd,cwd=root,timeout=600,startupinfo=startup).returncode
finally:
    log=(runtime/'runtime.log').read_text(encoding='utf-8',errors='replace') if (runtime/'runtime.log').exists() else ''
    checks=[line.split('[M5Smoke] ',1)[1] for line in log.splitlines() if '[M5Smoke] ' in line]
    result={'command':cmd,'map':args.map,'exit_code':code,'duration_seconds':time.time()-started,'checks':checks,'log':str(runtime/'runtime.log'),'binary_sha256':hashlib.file_digest(exe.open('rb'),'sha256').hexdigest(),'passed':code==0 and 'COMPLETE checks=22 failures=0' in checks}
    out=root/'Docs/开发过程/验收记录'/f'Dojo_smoke_{args.map}_{stamp}.json';out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(result,ensure_ascii=False))
assert result['passed']
