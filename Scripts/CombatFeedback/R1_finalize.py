"""Index fresh R1 evidence, keeping failures and separating environment gates."""
import datetime
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2]
out = root/'Saved/FeedbackRevisionR1'
def read(path): return json.loads(path.read_text(encoding='utf-8'))
def row(path):
    report = read(path); checks = report.get('checks', [])
    return {'path': str(path), 'status': report['status'], 'checks': len(checks),
            'passed': sum(bool(c['passed']) for c in checks),
            'failed': [c for c in checks if not c['passed']], 'error': report.get('error'),
            'contract_or_synthetic': [c['name'] for c in checks if any(t in c['name'].lower() for t in ('contract','synthetic'))]}
runs = sorted((out/'runs').glob('*/report.json'))
clock = [p for p in runs if not read(p).get('visual_only') and len(read(p)['checks']) == 80][-1]
visual = [p for p in runs if read(p).get('visual_only')][-1]
replays = []
for case in ('a-edge','c-main','e-main','run-shift','e-edges','run-integrated'):
    candidates = sorted(p for p in (out/'regressions').glob('*/report.json') if p.parent.name.endswith('-'+case))
    assert candidates, case
    replays.append({'case': case, **row(candidates[-1])})
smokes = []
for name in ('L_DojoArena','L_TrainingArena'):
    path = sorted(p for p in (out/'package-smoke').glob('*/report.json') if p.parent.name.endswith('-'+name))[-1]
    report = read(path)
    assert report['status']=='passed' and not report['errors'], report
    smokes.append({'map':name,'path':str(path),'status':report['status'],'exit_code':report['exit_code']})
cook = read(out/'cook-audit.json')
assert not cook['missing'] and len(cook['maps'])==2
preservation = read(sorted(out.glob('preservation-*.json'))[-1])
assert not preservation['protected_changes'] and not preservation['freeze_differences']
assert not preservation['freeze_additions'] and preservation['editor_binary_unchanged']
summary = {'candidate':'FR1-20261001','time':datetime.datetime.now().astimezone().isoformat(),
           'status':'playable revision; environment gates remain open',
           'clock':row(clock),'visual':row(visual),'regressions':replays,'package_smoke':smokes,
           'cook_audit':str(out/'cook-audit.json'),'package':cook['package_path'],
           'baseline_preservation':preservation,
           'editor_binary':read(out/'candidate/manifest.json')['editor_dll_sha256'],
           'cook_repair':'ParticleModuleColorScaleOverLife now owned by ParticleSystem; failed pre-repair build and freeze retained',
           'validation_scope':'R1 real-input/visual, C and integrated rerun after cook repair. A/Shift/E retain prior reports: only editor-only Cascade module ownership changed after those reports; their exact binaries remain labelled in evidence.',
           'hit_stop_default':0,'trial_launcher':'Play_R1_HitStop.cmd (HitStop=1, t.MaxFPS=60)',
           'human_new_revision_play':'not performed','human_new_revision_listening':'not performed',
           'old_candidate_user_feedback':'audio good; light/heavy weak; HitStop=1 weak; abrupt effect cutoff after enemy/wall impact',
           'remaining':['actual 60/120 FPS environment gates','PIE exact 1920x1080 viewport gate',
                        'previous cold PSO/performance/source-manifest issues not closed','human new-package feel review']}
entries = [summary['clock'],summary['visual'],*replays]
summary['latest_editor_assertions'] = sum(x['checks'] for x in entries)
summary['latest_editor_passed'] = sum(x['passed'] for x in entries)
summary['latest_editor_failures'] = [c for x in entries for c in x['failed']]
assert all(not x['error'] for x in entries)
assert all(('FPS test environment reached' in c['name'] or 'V9 actual game viewport' in c['name']) for c in summary['latest_editor_failures'])
summary['remaining_environment_gates'] = [c['name'] for c in summary['latest_editor_failures']]
docs = [root/'Docs/打击感开发/R1_试玩反馈修复记录_20261002.md', root/'Docs/打击感开发/README.md',
        root/'Docs/开发过程/验收记录/打击感首轮/F_手动试玩.md']
for path in docs:
    data = path.read_bytes(); assert not data.startswith(b'\xef\xbb\xbf'); data.decode('utf-8')
summary['docs_utf8_no_bom'] = {p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in docs}
(out/'final-summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in summary.items() if k in ('candidate','status','latest_editor_assertions','latest_editor_passed','latest_editor_failures','package')},ensure_ascii=False,indent=2))
