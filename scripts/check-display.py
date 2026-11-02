from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
rows=[line.split('|') for line in (root/'firetv/app/src/main/assets/team-logos.txt').read_text().splitlines()]
header=(root/'include/TeamLogos.h').read_text()
assert len(rows)==32 and len({r[0] for r in rows})==32
for team,palette,pixels in rows:
 assert re.fullmatch('[0-9a-f]{64}',palette)
 assert re.fullmatch('[0-9a-f]{1024}',pixels)
 assert '{"'+team+'","'+palette+'","'+pixels+'"}' in header
 assert len(set(pixels))>1
print('PASS: 32 matching, valid indexed team logos on both platforms')
