"""Run the journal-scrolling regression using local game data and a copied pre-quest save."""
import argparse
import hashlib
import os
import re
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--executable", required=True, type=Path)
parser.add_argument("--game", required=True, type=Path)
parser.add_argument("--save", required=True, type=Path)
parser.add_argument("--output", required=True, type=Path)
args = parser.parse_args()
exe, game, save, output = (p.resolve() for p in (args.executable, args.game, args.save, args.output))
assert exe.is_file() and game.is_dir() and save.is_file()
original = hashlib.sha256(save.read_bytes()).digest()
output.mkdir(parents=True, exist_ok=False)
shutil.copy2(save, output / "save_slot_1.sav")
(output / "Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
command = [str(exe), "-g", str(game), "-game:TheChroniclesOfMyrtana.ini",
           "-window", "-rt", "0", "-gi", "0", "-aa", "0", "-bl", "0", "-save", "1"]
env = {k: v for k, v in os.environ.items() if not k.startswith("OPENGOTHIC_")}
env.update(OPENGOTHIC_PROFILE="1", OPENGOTHIC_JOURNAL_PROBE="1")
try:
    with (output / "terminal.log").open("w") as log:
        result = subprocess.run(command, cwd=output, stdout=log, stderr=subprocess.STDOUT,
            env=env, timeout=180)
    assert result.returncode == 0, f"Game exited {result.returncode}; inspect {output}"
    trace = (output / "terminal.log").read_text(errors="replace")
    assert "[ARCHOLOS_PROFILE] frames=180" in trace, "Incomplete journal probe"
    phases = {int(i): int(n) for i, n in re.findall(r"\[JOURNAL_PROBE\] phase=(\d+) scroll=(\d+)", trace)}
    held = [int(n) for n in re.findall(r"\[JOURNAL_PROBE\] held scroll=(\d+)", trace)]
    draws = [(int(s), int(n), int(v)) for s, n, v in re.findall(
        r"\[JOURNAL_PROBE\] draw scroll=(\d+) lines=(\d+) visible=(\d+)", trace)]
    assert len(held) == 2 and held[0] > 1 and held[1] == 0, "Held scrolling failed"
    assert phases.keys() == {0, 1, 2, 3}, "Incomplete input phases"
    assert phases[0] == phases[2] == 0, "Cannot return to top"
    assert phases[1] == phases[3] and phases[1] > 1, "Keyboard/wheel bottom differs"
    assert any(s == n-v == phases[1] and n > v for s,n,v in draws), "Last page was not rendered"

finally:
    assert hashlib.sha256(save.read_bytes()).digest() == original, "Source save changed"
print(f"Evidence: {output}")
