"""Short D consumer boundary tests, reusing the main suite's identical contracts.

python Scripts/CombatFeedback/D_run.py contracts
Uses a real melee contact as its snapshot, then local consumer-only replay for
deduplication, shared concurrency, priority, stale generation and old End.
No damage/event injection, no recording, no long full-charge holds.
"""
from pathlib import Path
D_CONTRACT_ONLY = True
source = Path(__file__).with_name('D_pie.py')
exec(compile(source.read_text(encoding='utf-8'), str(source), 'exec'), globals())
