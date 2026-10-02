"""Index fresh F evidence and preserve final F script/document hashes."""
from pathlib import Path
import json
import hashlib
from datetime import datetime

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Saved/CombatFeedback/F'
def read(path):return json.loads((OUT/path).read_text(encoding='utf-8'))
def digest(path):return hashlib.file_digest(path.open('rb'),'sha256').hexdigest()

original=read('regressions/20260930-181841/index.json')
latest=[]
for item in original['runs']:
    entry=dict(item)
    if entry['case'] in ['e-main','e-edges','e-reentry']:
        folder=sorted((OUT/'viewport-retest').glob('*-'+entry['case']))[-1]
        result=json.loads((folder/'report.json').read_text(encoding='utf-8'))
        entry.update(evidence=str(folder),status=result['status'],
                     checks=len(result['checks']),failed=[c for c in result['checks'] if not c['passed']],
                     replaces=str(item['evidence']))
    latest.append(entry)
media=read('media/20260930-193344-PIE-off-on-desktop-complete/recording.json')
compare=read('comparison/20260930-193300/report.json')
optional=read('optional/20260930-192757/report.json')
preservation=sorted(OUT.glob('preservation-*.json'))[-1]
p=json.loads(preservation.read_text(encoding='utf-8'))
assert not p['candidate_differences'] and not p['candidate_existing_script_differences'] and not p['f_baseline_differences']
assert sum(c['checks'] for c in latest)==763
assert sum(len(c['failed']) for c in latest)==1
assert len(compare['checks'])==12 and compare['status']=='passed'
assert len(optional['checks'])==4 and optional['status']=='passed' and optional['restored']
assert media['video_valid'] and media['audio_signal']['nonzero_samples']>0
docs=[ROOT/'Docs/打击感开发/Agent_F_集成验收与打包.md']+list((ROOT/'Docs/开发过程/验收记录/打击感首轮').glob('F_*.md'))
for path in docs:
    contents=path.read_bytes();assert not contents.startswith(b'\xef\xbb\xbf')
    contents.decode('utf-8',errors='strict')
files=docs+list((ROOT/'Scripts/CombatFeedback').glob('F_*'))
report={'candidate':'FA2-20260930-v2','time':datetime.now().astimezone().isoformat(),
        'decision':'candidate delivered; technical gates open; human play/listening and legacy subjective A/B pending',
        'preservation':str(preservation),'latest_main_runs':latest,
        'main_assertions':763,'main_passed':762,'contract_or_synthetic':19,
        'additional_optional':{'path':'optional/20260930-192757','passed':4},
        'additional_comparison':{'path':'comparison/20260930-193300','passed':12},
        'performance':'performance/20260930-184555/report.json',
        'cold_off':'cold-off/20260930-184938/report.json',
        'build':'BuildCookRun.log','cook':'cook-audit.json',
        'package':str(ROOT/'Saved/Packages/FeedbackF_FA2_v2/Windows'),
        'native_package':'native-package/report.json','sound_video':'media/20260930-193344-PIE-off-on-desktop-complete/original-output.mp4',
        'normal_ai_video':'media/20260930-190937-package-normal-AI-round3/original-output.mp4',
        'sound_video_sha256':digest(OUT/'media/20260930-193344-PIE-off-on-desktop-complete/original-output.mp4'),
        'normal_ai_video_sha256':digest(OUT/'media/20260930-190937-package-normal-AI-round3/original-output.mp4'),
        'feedback_channels_default':{'HitStop':0,'Reaction':1,'Audio':1,'RangedFX':1,'Camera':1,'HUD':1,'AudioGain':1,'CameraStrength':2},
        'open_defects':['F-PKG-01','F-GATE-02','F-ASSET-03','F-PERF-04','F-UX-05','F-DIAG-06'],
        'human_input':False,'human_listening':False,
        'f_scripts_and_documents':{path.relative_to(ROOT).as_posix():digest(path) for path in files if path.is_file()}}
(OUT/'final-summary.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'decision':report['decision'],'main_passed':762,'main_assertions':763,'additional_optional_passed':4,'additional_comparison_passed':12,'preserved_source_config_assets':p['candidate_protected_count'],'docs_utf8_no_bom':len(docs),'index':str(OUT/'final-summary.json')},ensure_ascii=False,indent=2))
