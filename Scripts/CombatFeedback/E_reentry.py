"""E_run.py reentry: fresh PIE subscriptions and one real heavy pulse/prompt."""
from pathlib import Path
E_REENTRY_ONLY=True
exec(compile((Path(__file__).parent/'E_pie.py').read_text(encoding='utf-8'),str(Path(__file__).parent/'E_pie.py'),'exec'))
