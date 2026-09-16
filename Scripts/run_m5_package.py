"""运行已归档 Development 独立包，检查真实包内闭环和退出状态。可选 --nullrhi。"""
import argparse,subprocess,hashlib,json,shutil,time
from pathlib import Path
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--nullrhi',action='store_true');args=parser.parse_args()
package=root/'Saved/Packages/M5/Windows'
exe=package/'JJK_theHiddenOnes/Binaries/Win64/JJK_theHiddenOnes.exe'
assert exe.is_file(),exe
runtime=root/'Saved/M5_PackageRuntime';runtime.mkdir(parents=True,exist_ok=True)
log=runtime/('M5_Package_nullrhi.log' if args.nullrhi else 'M5_Package_graphics.log')
cmd=[str(exe),'-M5SmokeTest','-NoSound','-unattended','-windowed','-ResX=1280','-ResY=720','-UserDir='+runtime.as_posix(),'-abslog='+log.as_posix()]
if args.nullrhi:cmd.append('-nullrhi')
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
started=time.time();proc=subprocess.run(cmd,cwd=package,timeout=300,startupinfo=startup)
text=log.read_text(encoding='utf-8',errors='replace')
assert proc.returncode==0,(proc.returncode,str(log))
assert 'Error:' not in text and '=== Handled ensure' not in text,'runtime errors: '+str(log)
assert '[M5Smoke] COMPLETE checks=14 failures=0' in text,text[-4000:]
evidence=root/'Docs/开发过程/验收记录'
shutil.copy2(log,evidence/log.name)
shots=list(runtime.rglob('M5_PackageResult.png'))
if not args.nullrhi:
 assert shots and shots[0].stat().st_mtime>=started,'packaged screenshot missing'
 shutil.copy2(shots[0],evidence/'M5_独立包胜负.png')
state={'status':'passed','checks':14,'graphics':not args.nullrhi,'command':cmd,'exit_code':proc.returncode,'duration_seconds':time.time()-started,'package_path':str(package),'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'content_hashes':{str(p.relative_to(package)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (package/'JJK_theHiddenOnes/Content/Paks').iterdir() if p.is_file()}}
(evidence/('M5_Package_nullrhi.json' if args.nullrhi else 'M5_Package_graphics.json')).write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(state,ensure_ascii=False))
