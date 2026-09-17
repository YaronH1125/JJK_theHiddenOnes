"""运行已打包的 M6 Development 独立包，验证包内闭环（含 M6 双炮/领域 smoke）。"""
import argparse,subprocess,hashlib,json,shutil,time
from pathlib import Path
root=Path(__file__).resolve().parents[1]
package=root/'Saved/Packages/M6/Windows'
exe=package/'JJK_theHiddenOnes/Binaries/Win64/JJK_theHiddenOnes.exe'
assert exe.is_file(),exe
runtime=root/'Saved/M6_PackageRuntime';runtime.mkdir(parents=True,exist_ok=True)
log=runtime/'M6_Package_graphics.log'
cmd=[str(exe),'-M5SmokeTest','-NoSound','-unattended','-windowed','-ResX=1280','-ResY=720','-UserDir='+runtime.as_posix(),'-abslog='+log.as_posix()]
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
started=time.time();proc=subprocess.run(cmd,cwd=package,timeout=420,startupinfo=startup)
text=log.read_text(encoding='utf-8',errors='replace')
assert proc.returncode==0,(proc.returncode,str(log))
assert 'Error:' not in text and '=== Handled ensure' not in text,'runtime errors: '+str(log)
assert '[M5Smoke] COMPLETE checks=20 failures=0' in text,text[-4000:]
evidence=root/'Docs/开发过程/验收记录'
shutil.copy2(log,evidence/log.name)
shots=list(runtime.rglob('M5_PackageResult.png'))
if shots and shots[0].stat().st_mtime>=started:
 shutil.copy2(shots[0],evidence/'M6_独立包结果.png')
state={'status':'passed','checks':20,'command':cmd,'exit_code':proc.returncode,'duration_seconds':time.time()-started,'package_path':str(package),'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'pak_sha256':{str(p.relative_to(package)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (package/'JJK_theHiddenOnes/Content/Paks').iterdir() if p.is_file()}}
(evidence/'M6_Package_graphics.json').write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(state,ensure_ascii=False))
