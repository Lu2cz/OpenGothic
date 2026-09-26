"""Replay Kurt's fishing scene, affected saves, or native overlay ownership checks."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import zipfile

from archolos_test_data import copy_save

p = argparse.ArgumentParser(description=__doc__)
for name in ("executable", "game", "save", "output"):
    p.add_argument("--" + name, required=True, type=Path)
p.add_argument("--mode", choices=("scene", "scene-broken", "reload", "reload-broken", "overlays"), required=True)
a = p.parse_args()
exe, game, save, out = (getattr(a, n).resolve() for n in ("executable", "game", "save", "output"))
source_hash = hashlib.sha256(save.read_bytes()).hexdigest()
out.mkdir(parents=True, exist_ok=False)
copy_save(save, out / "save_slot_1.sav")
(out / "Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
env = {k: v for k, v in os.environ.items() if not k.startswith("OPENGOTHIC_")}
env.update(OPENGOTHIC_PROFILE="1", OPENGOTHIC_FISHING_PROBE=a.mode, OPENGOTHIC_CAPTAIN_SKIP="1")
command = [str(exe), "-g", str(game), "-game:TheChroniclesOfMyrtana.ini", "-window",
           "-rt", "0", "-gi", "0", "-aa", "0", "-bl", "0", "-save", "1"]
(out / "provenance.json").write_text(json.dumps({"source": str(save), "source_sha256": source_hash,
    "executable_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(), "command": command, "mode": a.mode}, indent=2))
try:
    with (out / "terminal.log").open("w") as log:
        result = subprocess.run(command, cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=400)
    assert result.returncode == 0, f"Game exited {result.returncode}"
    trace = (out / "terminal.log").read_text(errors="replace")
    if a.mode == "overlays":
        assert "[FISHING] overlay ownership and timed renewal passed" in trace
    else:
        assert f"[FISHING] complete mode={a.mode}" in trace
        assert "[FISHING] control moved=" in trace
        if a.mode.startswith("scene"):
            assert "[FISHING] active hero=1 kurt=1 rod=1" in trace
            for choice in ("DONTTALK", "STR", "DEX", "JORN"):
                assert f"[FISHING] select=DIA_KURT_Q108_FISHINGTIME_{choice}" in trace
        for name in ("fishing-ended.png", "fishing-control.png"):
            assert (out / name).stat().st_size > 10000
        with zipfile.ZipFile(out / "save_slot_2.sav") as z:
            assert z.testzip() is None
            visual = z.read("worlds/ARCHOLOS_MAINLAND.ZEN/npc/0/visual")
            assert (b"HumanS_Fishing_Dialogue.MDS" in visual) == a.mode.endswith("broken")
            if a.mode.startswith("reload"):
                with zipfile.ZipFile(save) as original:
                    for entry in ("worlds/ARCHOLOS_MAINLAND.ZEN/npc/0/inventory", "game/quests"):
                        assert original.read(entry) == z.read(entry), f"Recovery changed {entry}"
finally:
    assert hashlib.sha256(save.read_bytes()).hexdigest() == source_hash, "Protected source save changed"
print(f"PASS fishing {a.mode}: {out}")
