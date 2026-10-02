"""Convert the latest B game frame captures into GIFs and a self-contained review.

python Scripts/CombatFeedback/B_make_review.py
Requires Pillow. Output stays in Saved/FeedbackB/review.html and its run folder.
Frame durations use recorded game time. This is a silent sampled visual record,
not continuous video, sound verification, or a human playtest.
"""
import base64
from html import escape
import io
import json
from pathlib import Path
from PIL import Image, ImageOps, ImageDraw

root=Path(__file__).resolve().parents[2]/'Saved/FeedbackB'
out=Path(json.loads((root/'latest-run.json').read_text())['path'])
r=json.loads((out/'report.json').read_text())
cards=[]
for name,entries in r.get('motion_captures',{}).items():
    # Ranged beam can obscure this in-game sample; show the separately captured
    # asset preview below. Preserve the original frames in the evidence folder.
    if name=='motion-SuperRecoil-isolated-Fire':continue
    entries=[e for e in entries if Path(e['file']).exists()]
    frames=[Image.open(e['file']).convert('RGB').resize((640,360)) for e in entries]
    durations=[max(40,round((entries[i+1]['t']-e['t'])*1000)) if i<len(entries)-1 else 200 for i,e in enumerate(entries)]
    path=out/(name+'.gif')
    frames[0].save(path,save_all=True,append_images=frames[1:],duration=durations,loop=0,optimize=False)
    cards.append('<article><h2>'+escape(name)+'</h2><img src="data:image/gif;base64,'+base64.b64encode(path.read_bytes()).decode()+'"></article>')
    indices=sorted(set([0,len(frames)//4,len(frames)//2,3*len(frames)//4,len(frames)-1]))
    sheet=Image.new('RGB',(640*len(indices),390),(20,23,28));d=ImageDraw.Draw(sheet)
    for i,index in enumerate(indices):
        sheet.paste(frames[index],(640*i,30));d.text((640*i+12,8),f'{name}  t={entries[index]["t"]-entries[0]["t"]:.3f}s',fill='white')
    sheet.save(out/(name+'-strip.jpg'),quality=90)
if (root/'recoil-asset.gif').exists():
    cards.append('<article><h2>SuperRecoil · 独立资产侧面预览，待 A 接 Fire</h2><img src="data:image/gif;base64,'+base64.b64encode((root/'recoil-asset.gif').read_bytes()).decode()+'"></article>')
checks=len(r['checks']);passed=sum(c['passed'] for c in r['checks'])
html='''<!doctype html><meta charset="utf-8"><title>Agent B 动作交接预览</title>
<style>body{background:#101318;color:#e8edf4;font:16px/1.7 system-ui;margin:28px auto;max-width:1380px;padding:0 24px}h1{margin-bottom:0}p{color:#b6c4d5}main{display:grid;grid-template-columns:repeat(auto-fit,minmax(450px,1fr));gap:24px}article{background:#1b222e;border:1px solid #344355;padding:16px;border-radius:12px}h2{font-size:17px}img{width:100%;height:auto}strong{color:#89d9bb}</style>
<h1>Agent B · 动作与受击交接</h1><p>修复版石流龙 · 默认关闭顿帧 · 临时接线自测 <strong>'''+str(passed)+'/'+str(checks)+'''</strong></p>
<p>以下为游戏时间采样的无声帧序列。A1/重拳/连段使用真实输入；格挡保持及 SuperRecoil 为隔离演示，仍待 A 正式接线。不是原声录像或真人试玩。</p><main>'''+''.join(cards)+'</main>'
(root/'review.html').write_text(html,encoding='utf-8')
print(root/'review.html')
