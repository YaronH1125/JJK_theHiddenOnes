"""Author melee prototypes and trim existing vendor energy sounds; standard Python only.

python Scripts/CombatFeedback/D_make_audio.py (after D_inventory.py exports sources)
Writes rebuildable PCM sources to Content/CombatFeedback/Audio/Source/ and QA to
Saved/FeedbackD/audio-analysis.json. Original packages/exports stay read-only.
Authored melee sounds are audition prototypes, not claimed studio Foley recordings.
"""
import array
import hashlib
import json
import math
import random
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Content/CombatFeedback/Audio/Source'
SAVED = ROOT / 'Saved/FeedbackD'
OUT.mkdir(parents=True, exist_ok=True)
SR = 48000
manifest = []

def read(name, start=0., length=1.):
    with wave.open(str(SAVED / 'source-wav' / (name + '.wav')), 'rb') as w:
        assert w.getsampwidth() == 2
        rate, channels = w.getframerate(), w.getnchannels()
        a = array.array('h', w.readframes(w.getnframes()))
    mono = [sum(a[i:i+channels]) / (channels * 32768.) for i in range(0, len(a), channels)]
    n = int(length * SR)
    result = []
    for i in range(n):
        p = (start + i / SR) * rate
        j = int(p)
        f = p - j
        result.append(mono[j] * (1-f) + mono[min(j+1, len(mono)-1)] * f if j < len(mono) else 0.)
    return result

def lowpass(x, cutoff):
    alpha = 1. - math.exp(-2 * math.pi * cutoff / SR)
    y, v = [], 0.
    for s in x:
        v += alpha * (s-v)
        y.append(v)
    return y

def save(name, x, source, peak=.50, loop=False):
    dc = sum(x) / len(x)
    x = [s-dc for s in x]
    if not loop:
        fade_in, fade_out = int(.002*SR), int(.025*SR)
        for i in range(fade_in): x[i] *= i/fade_in
        for i in range(fade_out): x[-i-1] *= i/fade_out
    scale = peak / max(max(abs(s) for s in x), 1e-8)
    pcm = array.array('h', [round(max(-.99, min(.99, s*scale)) * 32767) for s in x])
    path = OUT / (name + '.wav')
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm.tobytes())
    rms = math.sqrt(sum((s/32768.)**2 for s in pcm)/len(pcm))
    manifest.append({'name': name, 'path': str(path.relative_to(ROOT)), 'source': source,
                     'sample_rate': SR, 'channels': 1, 'duration': len(pcm)/SR, 'loop': loop,
                     'peak_dbfs': 20*math.log10(max(abs(s) for s in pcm)/32768.),
                     'rms_dbfs': 20*math.log10(max(rms,1e-9)),
                     'clipped_samples': sum(abs(s)>=32760 for s in pcm),
                     'boundary_delta': abs(pcm[-1]-pcm[0])/32768.,
                     'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})

def impact(kind, variant):
    rng = random.Random(9103 + variant*103 + {'Punch':1,'Kick':2,'Heavy':3,'Guard':4}[kind])
    dur = {'Punch':.23,'Kick':.29,'Heavy':.40,'Guard':.22}[kind]
    noise = [rng.uniform(-1,1) for _ in range(int(dur*SR))]
    body = lowpass(noise, 520 if kind=='Heavy' else 900)
    snap = lowpass(noise, 3600)
    f = {'Punch':145,'Kick':110,'Heavy':72,'Guard':320}[kind] * (1+variant*.055)
    x=[]
    for i in range(len(noise)):
        t=i/SR
        attack=min(t/.002,1.)
        thud=math.sin(2*math.pi*f*(t + .035*(1-math.exp(-t/.035)))) * math.exp(-t/(.09 if kind=='Heavy' else .052))
        crack=(snap[i]-body[i]) * math.exp(-t/.017)
        texture=body[i]*math.exp(-t/(.12 if kind=='Heavy' else .065))
        value=(thud*.65+crack*.85+texture*1.4)*attack
        if kind=='Guard':
            value=(thud*.2+crack*.55+texture*.8 + .23*math.sin(2*math.pi*760*t)*math.exp(-t/.04))*attack
        x.append(value)
    return x

for kind in ['Punch','Kick','Heavy','Guard']:
    for variant in [1,2]:
        save('SW_D_'+kind+'Hit_'+str(variant) if kind!='Guard' else 'SW_D_Guard_'+str(variant),
             impact(kind,variant), 'D authored deterministic noise/resonance prototype', peak=.48 if kind=='Guard' else .56)

for kind in ['Punch','Kick']:
    for variant in [1,2]:
        dur=.16 if kind=='Punch' else .23
        x=read('SW_GPBAR_Release_Wind_01', .12 + variant*.035, dur)
        x=lowpass(x,3200)
        x=[s*math.sin(math.pi*i/(len(x)-1))**1.8 for i,s in enumerate(x)]
        save('SW_D_'+kind+'Swing_'+str(variant),x,'GoodParticleBeamAndRay: SW_GPBAR_Release_Wind_01; cropped/mono/filtered',peak=.32)

derived = {
    'MobileFire': ('SW_GPBAR_Release_Energy_01',0.,.48,.48),
    'SuperFire': ('SW_GPBAR_Shipboard_Railgun',0.,.68,.56),
    'RangedHit': ('SW_GPBAR_Release_Impact_01',0.,.45,.52),
    'WorldImpact': ('SW_GPBAR_Release_Impact_02',0.,.33,.42),
    'Expire': ('SW_GPBAR_Bit_Fade',.08,.22,.20),
    'ChargeStart': ('sfx_AttackUp',.02,.26,.32),
    'DomainStart': ('sfx_CritUp',.04,.65,.42),
    'DomainEnd': ('SW_GPBAR_Bit_Fade',.10,.28,.24),
}
for kind,(source,start,dur,peak) in derived.items():
    save('SW_D_'+kind,lowpass(read(source,start,dur),4800),source+'; mono/trim/lowpass/peak-normalized',peak=peak)

for kind,freq,dur in [('ChargeFull',880,.16),('Immune',1240,.11),('ChargeCancel',220,.13)]:
    x=[(.7*math.sin(2*math.pi*freq*i/SR)+.3*math.sin(2*math.pi*freq*1.5*i/SR))*math.exp(-i/SR/.045) for i in range(int(dur*SR))]
    save('SW_D_'+kind,x,'D authored state cue; distinct full/immune/cancel pitches',peak=.25)

# Periodic harmonic hum has matching value and derivative at the 1 s seam.
# Energy texture uses a Hann overlap of a vendor loop, so it is periodic too.
n=SR
texture=read('Sfx_Loop_Eletric',.30,2.)
periodic=[texture[i]*math.sin(math.pi*i/n)**2 + texture[i+n]*math.cos(math.pi*i/n)**2 for i in range(n)]
# Remove texture seam drift over 512 samples without inserting silence.
delta=periodic[-1]-periodic[0]
for i in range(512): periodic[-512+i]-=delta*(.5-.5*math.cos(math.pi*i/511))
hum=[.65*math.sin(2*math.pi*96*i/SR)+.18*math.sin(2*math.pi*192*i/SR)+.07*periodic[i] for i in range(n)]
save('SW_D_ChargeLoop',hum,'EnergyBeam Sfx_Loop_Eletric texture + D authored periodic hum; 1 s seam repaired',peak=.28,loop=True)

(SAVED/'audio-analysis.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
(OUT/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'waves':len(manifest),'clipped_samples':sum(x['clipped_samples'] for x in manifest),
                  'source':str(OUT),'loop':manifest[-1]},ensure_ascii=False,indent=2))
