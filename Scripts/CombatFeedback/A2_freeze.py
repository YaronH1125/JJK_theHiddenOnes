"""Freeze the reviewed A2 workspace and compare every pre-existing file to its baseline.

Run once after tests, documentation, settings restoration and Editor shutdown.
No staging, commits, checkout, or deletion of project files.
"""
import difflib
import hashlib
import json
from pathlib import Path
import subprocess
import sys

root=Path(__file__).resolve().parents[2];out=root/'Saved/FeedbackA2'
subprocess.run([sys.executable,str(root/'Scripts/CombatFeedback/A2_snapshot.py'),'candidate'],check=True,cwd=root)
base=json.loads((out/'baseline/manifest.json').read_text(encoding='utf-8'))
final=json.loads((out/'candidate/manifest.json').read_text(encoding='utf-8'))
changed=[p for p,h in base['files'].items() if final['files'].get(p)!=h]
added=[p for p in final['files'] if p not in base['files']]
allowed={'Content/Training/DA_Fighter_Ishigori.uasset','Source/JJK_theHiddenOnes/Training/CombatFeedbackProfile.h',
 'Source/JJK_theHiddenOnes/Training/DodgeAbility.cpp','Source/JJK_theHiddenOnes/Training/ChargedBlastAbility.cpp',
 'Scripts/feedback_a_regression.py','Scripts/feedback_a_baseline_samples.py','Scripts/README.md',
 'Docs/打击感开发/Agent_A_战斗底座与集成.md','Docs/打击感开发/README.md','Docs/20_打击感首轮设计需求与分工.md'}
unexpected=[p for p in changed if p not in allowed]
patch=[]
for rel in changed+added:
    if Path(rel).suffix not in ('.h','.cpp','.py','.md'):continue
    prior=out/'baseline/files'/rel
    before=prior.read_text(encoding='utf-8').splitlines(True) if prior.exists() else []
    after=(root/rel).read_text(encoding='utf-8').splitlines(True)
    patch.extend(difflib.unified_diff(before,after,fromfile='baseline/'+rel,tofile='candidate/'+rel))
(out/'delta-from-baseline.patch').write_text(''.join(patch),encoding='utf-8')
content=[p for p in base['files'] if p.startswith('Content/')]
protected=[p for p in base['files'] if p.startswith(('Source/JJK_theHiddenOnes/Training/Feedback/','Content/CombatFeedback/','Content/Characters/Ishigori/Repaired/Feedback/'))]
preservation={'baseline_files':len(base['files']),'unchanged':len(base['files'])-len(changed),'changed':changed,'added':added,
              'unexpected_changes':unexpected,'existing_content_files':len(content),
              'existing_content_unchanged':sum(final['files'].get(p)==base['files'][p] for p in content),
              'protected_B_C_D_E_files':len(protected),'protected_B_C_D_E_unchanged':sum(final['files'].get(p)==base['files'][p] for p in protected)}
(out/'preservation.json').write_text(json.dumps(preservation,ensure_ascii=False,indent=2),encoding='utf-8')
assert not unexpected,preservation
assert preservation['protected_B_C_D_E_files']==preservation['protected_B_C_D_E_unchanged']
summary={'candidate':'FA2-20260930-v2','head_plus_dirty_workspace':final['head'],'editor_dll_sha256':final['editor_dll_sha256'],
         'manifest_sha256':hashlib.sha256((out/'candidate/manifest.json').read_bytes()).hexdigest(),
         'map':'/Game/Maps/L_DojoArena','binding':json.loads((out/'binding.json').read_text(encoding='utf-8')),
         'dependency_audit':json.loads((out/'dependency-audit.json').read_text(encoding='utf-8'))['status'],
         'runs':[],'replays':[],'latest_cases':{},'harness_failures':[]}
for folder,category in [('runs','runs'),('replays','replays')]:
    for path in sorted((out/folder).iterdir()):
        report=path/'report.json'
        launch=path/'launch.json'
        if launch.exists() and not json.loads(launch.read_text(encoding='utf-8')).get('success',False):
            summary['harness_failures'].append({'path':str(path),'launch':str(launch)})
        if not report.exists():continue
        data=json.loads(report.read_text(encoding='utf-8'));checks=data.get('checks',[])
        summary[category].append({'path':str(path),'status':data['status'],'checks':len(checks),
          'failed':[c for c in checks if not c['passed']],'contract_or_synthetic':[c['name'] for c in checks if 'CONTRACT' in c['name'] or 'synthetic' in c['name']],
          'error':data.get('error'),'fps':data.get('fps')})
        meta_file=path/'meta.json'
        if meta_file.exists():
            meta=json.loads(meta_file.read_text(encoding='utf-8'))
            summary[category][-1]['binary_sha256']=meta.get('binary_sha256')
            if 'case' in meta:summary['latest_cases'][meta['case']]=summary[category][-1]
assert all(row.get('binary_sha256')==final['editor_dll_sha256'] for row in summary['latest_cases'].values()), 'Mixed binaries in latest cases'
(out/'candidate-summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(preservation,ensure_ascii=False,indent=2))
