"""Stop only D's live Python callback after a failed fixture; runner then restores PIE.

python Scripts/ue_python.py Scripts/CombatFeedback/D_abort.py
Uses D_pie's module callback globals, asserts ownership before touching them.
"""
import gc
import types
matches=[o.__globals__ for o in gc.get_objects() if isinstance(o,types.FunctionType)
         and o.__name__=='tick' and o.__code__.co_filename.replace('\\','/').endswith('/CombatFeedback/D_pie.py')]
assert len(matches)==1,len(matches)
state=matches[0]
assert state['report'].get('test_binding','').startswith('temporary D_setup')
state['gen'].close()
state['report']['status']='failed'
state['report']['error']='D suite stopped by operator; this is not a passing run'
state['finish']()
