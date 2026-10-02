"""Capture the isolated recoil asset from the side without beam occlusion.

python Scripts/CombatFeedback/B_capture_recoil.py
Requires idle Editor; creates then removes B-tagged transient preview actors.
Output: Saved/FeedbackB/recoil-asset.gif and recoil-asset-frames/*.png.
This is a sampled asset preview, not a recording of a bound Fire consumer.
"""
import base64
import json
from pathlib import Path
import sys
from PIL import Image
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from ue_mcp import UnrealMCP
from ue_python import run
from ue_capture import find_image
root=Path(__file__).resolve().parents[2];out=root/'Saved/FeedbackB'
folder=out/'recoil-asset-frames';folder.mkdir(exist_ok=True)
request=out/'preview_request.json';preview=root/'Scripts/CombatFeedback/B_preview.py'
c=UnrealMCP();toolset='EditorToolset.EditorAppToolset'
previous=json.loads(c.tool(toolset,'GetCameraTransform')['content'][0]['text'])['returnValue']
try:
    frames=[]
    for i in range(12):
        request.write_text(json.dumps({'items':[dict(asset='/Game/Characters/Ishigori/Repaired/Feedback/IG_AS_FB_SuperRecoil',time=i/30.,x=350,y=0,yaw=0)]}),encoding='utf-8')
        result=run(preview);assert result['success'],result
        shot=c.tool(toolset,'CaptureViewport',dict(captureTransform={'location':{'x':660,'y':0,'z':130},'rotation':{'pitch':-5,'yaw':180,'roll':0}},annotations=None,bShowUI=False))
        data=find_image(shot);assert data
        p=folder/(str(i).zfill(3)+'.png');p.write_bytes(base64.b64decode(data));frames.append(Image.open(p).convert('RGB'))
    frames[0].save(out/'recoil-asset.gif',save_all=True,append_images=frames[1:],duration=[34]*11+[400],loop=0)
finally:
    request.write_text('{"items":[]}',encoding='utf-8')
    result=run(preview);assert result['success'],result
    c.tool(toolset,'SetCameraTransform',{'transform':previous})
print(out/'recoil-asset.gif')
