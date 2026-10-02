"""Only during E's own failed callback: persist failure and restore its transient fixture.

python Scripts/ue_python.py Scripts/CombatFeedback/E_abort.py
Does not stop unrelated PIE; E_run performs the owned StopPIE and editor cleanup.
"""
import gc
import json
from pathlib import Path
import types
import unreal
latest=Path(json.loads((Path(unreal.Paths.project_saved_dir())/'FeedbackE/latest-run.json').read_text(encoding='utf-8'))['path'])
matches={id(o.__globals__):o.__globals__ for o in gc.get_objects() if isinstance(o,types.FunctionType)
         and o.__name__=='tick' and o.__code__.co_filename.replace('\\','/').endswith('/CombatFeedback/E_pie.py')
         and o.__globals__.get('archive')==latest}
assert len(matches)<=1,len(matches)
if matches:
    state=next(iter(matches.values()))
    assert state['report']['fixture'].startswith('D audio consumer temporarily attached')
    state['gen'].close()
    state['report']['status']='failed'
    state['report']['abort_note']='Replacement fixture saw transient None before the next GameMode spawn tick; preserved failure and restored current live actors'
    state['write']()
    state['q']=state['gm'].get_opponent_fighter()
    state['finish']()
else:
    # A callback that unregistered before failing to write can already have been collected.
    # Persist the known fixture failure; E_run still owns StopPIE and E_cleanup restoration.
    file=latest/'report.json';data=json.loads(file.read_text(encoding='utf-8'))
    assert data['status']=='running' and data['fixture'].startswith('D audio consumer temporarily attached')
    data['status']='failed'
    data['error']='Replacement fixture read a transient None before GameMode respawn; old restore callback also encountered None. Full traceback retained in the Editor log.'
    file.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
print('Preserved E failure; E_run owns StopPIE and settings restoration')
