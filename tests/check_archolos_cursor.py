"""Check the opt-in real-window cursor probe after loading a copied save.

Run the existing scene profiler with OPENGOTHIC_CURSOR_PROBE=1, then pass
its terminal.log here. This checks engine cursor policy, not OS screenshots.
"""
import re
import sys
from pathlib import Path

records = re.findall(r"\[CURSOR_PROBE\] stage=(\w+) fullscreen=(\d) hidden=(\d)",
                     Path(sys.argv[1]).read_text(errors="replace"))
assert [stage for stage, _, _ in records] == ["gameplay", "resize"], records
assert all(fullscreen == "0" for _, fullscreen, _ in records), records
assert all(hidden == "1" for _, _, hidden in records), records
print("PASS: windowed gameplay and resize retain the hidden cursor")
