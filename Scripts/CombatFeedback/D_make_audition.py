"""Make a consistent-level source timbre comparison; not an in-game mix capture.

python Scripts/CombatFeedback/D_make_audition.py
Reads D PCM without modifying it. Writes Saved/FeedbackD/D-source-audition.wav
and source-audition.json. Order: light punch, heavy impact, guard, super cannon.
Groups have a one-second gap; source variants within a group have 0.25 seconds.
Human timbre approval remains pending.
"""
import array
import json
import math
from pathlib import Path
import wave

root = Path(__file__).resolve().parents[2]
source = root / 'Content/CombatFeedback/Audio/Source'
out = root / 'Saved/FeedbackD'
rate = 48000
output = array.array('h', [0] * (rate // 2))
segments = []
for label, names in [('Light punch', ['PunchHit_1', 'PunchHit_2']),
                     ('Heavy impact', ['HeavyHit_1', 'HeavyHit_2']),
                     ('Guard', ['Guard_1', 'Guard_2']),
                     ('Super cannon', ['SuperFire'])]:
    for name in names:
        with wave.open(str(source / ('SW_D_' + name + '.wav')), 'rb') as wav:
            assert wav.getnchannels() == 1 and wav.getsampwidth() == 2 and wav.getframerate() == rate
            samples = array.array('h', wav.readframes(wav.getnframes()))
        rms = math.sqrt(sum((x / 32768.) ** 2 for x in samples) / len(samples))
        peak = max(abs(x) for x in samples) / 32768.
        # RMS-match at -24 dBFS, with -6 dBFS peak ceiling; preserve each envelope.
        gain = min(10 ** (-24 / 20) / max(rms, 1e-10), 10 ** (-6 / 20) / max(peak, 1e-10))
        start = len(output) / rate
        output.extend(round(x * gain) for x in samples)
        segments.append({'label': label, 'source': name, 'start_seconds': start,
                         'end_seconds': len(output) / rate, 'preview_gain': gain})
        output.extend([0] * (rate // 4))
    output.extend([0] * rate)
path = out / 'D-source-audition.wav'
with wave.open(str(path), 'wb') as wav:
    wav.setnchannels(1); wav.setsampwidth(2); wav.setframerate(rate)
    wav.writeframes(output.tobytes())
report = {'path': str(path), 'seconds': len(output) / rate, 'segments': segments,
          'note': 'Source comparison only. Not the game mix, temporal alignment, or human approval.',
          'clipped_samples': sum(abs(x) >= 32760 for x in output)}
(out / 'source-audition.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
assert report['clipped_samples'] == 0
print(json.dumps({'path': str(path), 'seconds': report['seconds'], 'groups': 4, 'clipped_samples': 0}))
