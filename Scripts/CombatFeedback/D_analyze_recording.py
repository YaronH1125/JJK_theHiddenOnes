"""Check real Master WAV for signal/clipping.

python Scripts/CombatFeedback/D_analyze_recording.py
Input: Saved/FeedbackD/D-real-combat.wav and pie-report.json.
Output: recording-analysis.json in Saved/FeedbackD/.
Reports actual PCM only; this cannot certify human sound quality.
"""
import array
import json
import math
from pathlib import Path
import wave

out=Path(__file__).resolve().parents[2]/'Saved/FeedbackD'
path=out/'D-real-combat.wav'
with wave.open(str(path),'rb') as w:
    channels,rate,width=w.getnchannels(),w.getframerate(),w.getsampwidth()
    assert width==2
    samples=array.array('h',w.readframes(w.getnframes()))
    params=w.getparams()
peak=max(abs(x) for x in samples)/32768.
rms=math.sqrt(sum((x/32768.)**2 for x in samples)/len(samples))
report={'path':str(path),'seconds':len(samples)/channels/rate,'rate':rate,'channels':channels,
        'nonzero_samples':sum(x!=0 for x in samples),'peak_dbfs':20*math.log10(max(peak,1e-10)),
        'rms_dbfs':20*math.log10(max(rms,1e-10)), 'clipped_samples':sum(abs(x)>=32760 for x in samples),
        'human_listening':'pending; signal analysis does not certify timbre/comfort'}
(out/'recording-analysis.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
assert report['nonzero_samples']>0,'Silent recording; do not treat as audible acceptance'
assert report['clipped_samples']==0,'Mixer capture clipped'
