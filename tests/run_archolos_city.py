"""Create or reload a private Chapter 2 city exploration save with local game data."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import zipfile

p = argparse.ArgumentParser()
for name in ("executable", "game", "output"):
    p.add_argument("--" + name, type=Path, required=True)
source = p.add_mutually_exclusive_group(required=True)
source.add_argument("--seed", type=Path, help="Create the preset from a private copy of an existing game")
source.add_argument("--save", type=Path, help="Reload this city save instead of creating the preset")
a = p.parse_args()
exe, game, out = (getattr(a, name).resolve() for name in ("executable", "game", "output"))
save = (a.save or a.seed).resolve()
original = hashlib.sha256(save.read_bytes()).hexdigest()
out.mkdir(parents=True, exist_ok=False)
shutil.copy2(save, out / "save_slot_1.sav")
(out / "Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
env = {k: v for k, v in os.environ.items() if not k.startswith("OPENGOTHIC_")}
env.update(OPENGOTHIC_PROFILE="1", OPENGOTHIC_CITY_PROBE="reload" if a.save else "create")
command = [str(exe), "-g", str(game), "-game:TheChroniclesOfMyrtana.ini",
           "-window", "-rt", "0", "-gi", "0", "-bl", "0"]
command += ["-save", "1"]
try:
    with (out / "terminal.log").open("w") as log:
        result = subprocess.run(command, cwd=out, env=env, stdout=log,
                                stderr=subprocess.STDOUT, timeout=300)
    assert result.returncode == 0, f"Game exited {result.returncode}: {out}"
    trace = (out / "log.txt").read_text(errors="replace")
    assert "chapter=2 entered=1" in trace, "City chapter preset was not applied"
    assert "[CITY_PROBE] save finalized" in trace, "City save did not finish"
    assert "camera=0 dialogue=0" in trace, "Player is locked in a scene"
    moved = re.search(r"\[CITY_PROBE\] walked=([\d.]+)", trace)
    assert moved and float(moved[1]) > 50, "Normal walking input did not move Marvin"
    nearby = re.search(r"nearby=(\d+)", trace)
    assert nearby and int(nearby[1]) >= 3, "City NPC population is missing"
    with zipfile.ZipFile(out / "save_slot_2.sav") as z:
        assert z.testzip() is None and z.read("game/quests") and z.read("game/daedalus")
    print("\n".join(line for line in trace.splitlines() if "[CITY_PROBE]" in line))
finally:
    assert hashlib.sha256(save.read_bytes()).hexdigest() == original, "Source save changed"
print(f"Evidence: {out}")
