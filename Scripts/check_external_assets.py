"""Verify separately archived runtime assets; never modify Content.

Usage: python Scripts/check_external_assets.py [--package Mishima_DOJO|EnergyBeam]
"""
import argparse
import hashlib
import json
from pathlib import Path


def verify(root, manifest, selected):
    results = {}
    for name, package in manifest['packages'].items():
        if selected and name != selected:
            continue
        expected = {row['path']: row for row in package['files']}
        actual = {p.relative_to(root).as_posix(): p for p in (root / package['root']).rglob('*') if p.is_file()}
        changed = [key for key in expected.keys() & actual.keys()
                   if actual[key].stat().st_size != expected[key]['bytes']
                   or hashlib.file_digest(actual[key].open('rb'), 'sha256').hexdigest() != expected[key]['sha256']]
        results[name] = {'files': len(actual), 'missing': sorted(expected.keys() - actual.keys()),
                         'added': sorted(actual.keys() - expected.keys()), 'changed': sorted(changed)}
        results[name]['passed'] = not any(results[name][key] for key in ('missing', 'added', 'changed'))
    return {'packages': results, 'passed': bool(results) and all(row['passed'] for row in results.values())}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', choices=('Mishima_DOJO', 'EnergyBeam'))
    parser.add_argument('--project-root', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    manifest = json.loads((Path(__file__).parent / 'Baselines/external_assets.json').read_text(encoding='utf-8'))
    result = verify(args.project_root.resolve(), manifest, args.package)
    saved = args.project_root / 'Saved'
    saved.mkdir(parents=True, exist_ok=True)
    (saved / 'External_asset_integrity.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    for name, row in result['packages'].items():
        print(f"{name}: {'PASS' if row['passed'] else 'FAIL'}; files={row['files']}, "
              f"missing={len(row['missing'])}, added={len(row['added'])}, changed={len(row['changed'])}")
    if not result['passed']:
        print('Restore the matching private asset archive. See Docs/12_版本控制与素材依赖.md.')
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
