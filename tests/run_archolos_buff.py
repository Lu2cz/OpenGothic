"""Exercise Archolos's installed speed potion through the native inventory path."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

p = argparse.ArgumentParser()
for name in ("executable", "game", "output"):
    p.add_argument("--" + name, required=True, type=Path)
p.add_argument("--save", type=Path, help="Use a private save copy; omit for a fresh game")
p.add_argument("--mode", choices=("seed", "reload", "natural", "repeat"), required=True)
a = p.parse_args()
exe, game, out = (getattr(a, name).resolve() for name in ("executable", "game", "output"))
save = a.save.resolve() if a.save else None
if a.mode == "reload":
    assert save is not None, "reload requires --save"
source_hash = hashlib.sha256(save.read_bytes()).hexdigest() if save else None
out.mkdir(parents=True, exist_ok=False)
if save:
    shutil.copy2(save, out / "save_slot_1.sav")
(out / "Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
env = {key: value for key, value in os.environ.items() if not key.startswith("OPENGOTHIC_")}
env.update(OPENGOTHIC_PROFILE="1", OPENGOTHIC_BUFF_PROBE=a.mode)
with (out / "terminal.log").open("w") as log:
    result = subprocess.run([str(exe), "-g", str(game), "-game:TheChroniclesOfMyrtana.ini",
                             "-window", "-rt", "0", "-gi", "0", "-aa", "0", "-bl", "0"] + (["-save", "1"] if save else ["-nomenu"]),
                            cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT,
                            timeout=360 if a.mode in ("natural", "reload") else 90)
assert result.returncode == 0, f"Game exited {result.returncode}"
trace = (out / "terminal.log").read_text(errors="replace")
assert "Internal Exception" not in trace, trace[-3000:]
if a.mode == "seed":
    assert "[BUFF_UI] activate inventory=1" in trace and "[BUFF_UI] save active=1 sprint=1" in trace
    with zipfile.ZipFile(out / "save_slot_2.sav") as archive:
        assert archive.testzip() is None
elif a.mode == "repeat":
    assert "[BUFF_UI] activate inventory=1" in trace and "[BUFF_UI] repeat handle=" in trace and "same=1 sprint=1" in trace
else:
    assert "[BUFF_UI] activate inventory=1" in trace if a.mode == "natural" else "[BUFF_UI] reload handle=" in trace
    assert "[BUFF_UI] expired elapsed_ms=" in trace
    active, fade, expired = (out / f"buff-ui-{phase}.png" for phase in ("active", "fade", "expired"))
    assert active.is_file() and fade.is_file() and expired.is_file()
    before, after = trace.split("[BUFF_UI] expired elapsed_ms=", 1)
    assert "[BUFF_UI] draw texture=ItPo_Speed2.TGA" in before
    assert "[BUFF_UI] draw texture=ItPo_Speed2.TGA" not in after
if save:
    assert hashlib.sha256(save.read_bytes()).hexdigest() == source_hash, "Source save changed"
manifest = {"mode": a.mode, "executable": hashlib.sha256(exe.read_bytes()).hexdigest(), "input_save": source_hash,
            "outputs": {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in out.iterdir() if path.is_file()}}
(out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(f"PASS timed buff {a.mode}: {out}")
