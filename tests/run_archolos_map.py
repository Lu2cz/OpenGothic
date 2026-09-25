"""Replay the Silbach noticeboard map from an independent campaign save copy."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import zipfile

from archolos_test_data import copy_save

p = argparse.ArgumentParser()
for name in ("executable", "game", "save", "output"):
    p.add_argument("--" + name, required=True, type=Path)
p.add_argument("--mode", choices=("window", "fullscreen", "reopen", "restart"), default="window")
a = p.parse_args()
exe, game, save, out = (getattr(a, name).resolve() for name in ("executable", "game", "save", "output"))
source_hash = hashlib.sha256(save.read_bytes()).hexdigest()
if a.mode == "restart":
    with zipfile.ZipFile(save) as z:
        assert b"Silbach map verification" in z.read("header"), "Restart requires the saved map result"
out.mkdir(parents=True, exist_ok=False)
copy_save(save, out / "save_slot_1.sav")
(out / "Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
env = {k: v for k, v in os.environ.items() if not k.startswith("OPENGOTHIC_")}
env.update(OPENGOTHIC_PROFILE="1", OPENGOTHIC_MAP_PROBE=a.mode)
(out / "provenance.json").write_text(json.dumps({
    "source_sha256": source_hash,
    "executable_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
    "mode": a.mode,
}, indent=2))
try:
    with (out / "terminal.log").open("w") as log:
        result = subprocess.run([str(exe), "-g", str(game), "-game:TheChroniclesOfMyrtana.ini",
                                 "-window", "-rt", "0", "-gi", "0", "-aa", "0", "-bl", "0", "-save", "1"],
                                cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=1000)
    trace = (out / "log.txt").read_text(errors="replace")
    assert result.returncode == 0, f"Game exited {result.returncode}"
    for expected in ("[SPRITEMAP] dialogue_select=PC_NEWSBOARD_SILBACH_SHOWMAP_INFO",
                     "[SPRITEMAP] draw texture=MAP_SILBACH.tga", "[SPRITEMAP] visible stage=2",
                     "[SPRITEMAP] closed stage=2", "[SPRITEMAP] control moved=", "[SPRITEMAP] complete"):
        assert expected in trace, expected
    if a.mode == "reopen":
        for expected in ("[SPRITEMAP] visible stage=4", "[SPRITEMAP] closed stage=4"):
            assert expected in trace, expected
    assert "illegal access of unbound member ZCVOB.TRAFOOBJTOWORLD" not in trace
    assert "mem_readint:  address translation failure: 0x00000000008d" not in trace
    assert (out / "map-open.png").exists()
    if a.mode == "reopen":
        assert (out / "map-reopen.png").exists()
    with zipfile.ZipFile(out / "save_slot_2.sav") as z:
        assert z.testzip() is None
        assert b"Silbach map verification" in z.read("header")
finally:
    assert hashlib.sha256(save.read_bytes()).hexdigest() == source_hash, "Source save changed"
print(f"PASS Silbach map {a.mode}: {out}")
