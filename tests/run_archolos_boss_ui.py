"""Exercise Archolos's installed boss UI script through a private save."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

p = argparse.ArgumentParser()
for name in ("executable", "game", "save", "output"):
    p.add_argument("--" + name, required=True, type=Path)
p.add_argument("--mode", choices=("seed", "reload"), required=True)
a = p.parse_args()
exe, game, save, out = (getattr(a, name).resolve() for name in ("executable", "game", "save", "output"))
original = hashlib.sha256(save.read_bytes()).digest()
source_hash = original.hex()
executable_hash = hashlib.sha256(exe.read_bytes()).hexdigest()
out.mkdir(parents=True, exist_ok=False)
shutil.copy2(save, out / "save_slot_1.sav")
(out / "Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
env = {key: value for key, value in os.environ.items() if not key.startswith("OPENGOTHIC_")}
env.update(OPENGOTHIC_PROFILE="1", OPENGOTHIC_BOSS_UI_PROBE=a.mode)
try:
    with (out / "terminal.log").open("w") as log:
        result = subprocess.run([str(exe), "-g", str(game), "-game:TheChroniclesOfMyrtana.ini",
            "-window", "-rt", "0", "-gi", "0", "-aa", "0", "-bl", "0", "-save", "1"],
            cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=180)
    assert result.returncode == 0, f"Game exited {result.returncode}"
    trace = (out / "terminal.log").read_text(errors="replace")
    assert f"[BOSS_UI] {a.mode if a.mode == 'reload' else 'synthetic start'} active=1" in trace, trace[-3000:]
    background = "[BOSS_UI] draw texture=BOSSBAR_BG.TGA rect=240,-29,800,99"
    full = "[BOSS_UI] draw texture=BOSSBAR.TGA rect=274,12,732,15"
    half = "[BOSS_UI] draw texture=BOSSBAR.TGA rect=274,12,365,15"
    assert background in trace
    if a.mode == "seed":
        assert "[BOSS_UI] synthetic health active=1" in trace
        before, after = trace.split("[BOSS_UI] synthetic health active=1", 1)
        assert full in before and half in after
        assert "[BOSS_UI] synthetic save requested" in trace and (out / "save_slot_2.sav").is_file()
        with zipfile.ZipFile(out / "save_slot_2.sav") as archive:
            assert archive.testzip() is None and archive.read("game/compatibility")[:4] == b"\x03\0\0\0"
    else:
        assert half in trace
        assert "[BOSS_UI] synthetic finish active=0" in trace
        assert trace.count("[BOSS_UI] view freed=") >= 2
        assert "[BOSS_UI] draw texture=" not in trace.split("[BOSS_UI] synthetic finish active=0", 1)[1]
finally:
    assert hashlib.sha256(save.read_bytes()).digest() == original, "Source save changed"
output_hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                 for p in out.glob("save_slot_*.sav")}
(out / "manifest.json").write_text(json.dumps({"executable": executable_hash,
    "input_save": source_hash, "output_saves": output_hashes}, indent=2) + "\n")
print(f"PASS synthetic {a.mode}: {out}")
