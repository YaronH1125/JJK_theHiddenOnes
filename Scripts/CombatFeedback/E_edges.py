"""E_run.py edges: actual right mouse/free look and next-hit lifecycle regression."""
from pathlib import Path
E_EDGE_ONLY=True
exec(compile((Path(__file__).parent/'E_pie.py').read_text(encoding='utf-8'),str(Path(__file__).parent/'E_pie.py'),'exec'))
