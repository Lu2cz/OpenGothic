"""Replay both crafting-station missing-item messages from a private save."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import zipfile

from archolos_test_data import copy_save

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
copy_save(save, output / "save_slot_1.sav")
(output / "Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
command = [str(exe), "-g", str(game), "-game:TheChroniclesOfMyrtana.ini",
           "-window", "-rt", "0", "-gi", "0", "-aa", "0", "-bl", "0"]
env = {k: v for k, v in os.environ.items() if not k.startswith("OPENGOTHIC_")}
env["OPENGOTHIC_PROFILE"] = "1"
try:
    for slot, probe in [(1, True), (2, False)]:
        run_env = dict(env)
        if probe:
            run_env["OPENGOTHIC_MOB_FEEDBACK_PROBE"] = "1"
        with (output / f"terminal-{slot}.log").open("w") as log:
            result = subprocess.run(command + ["-save", str(slot)], cwd=output,
                                    env=run_env, stdout=log, stderr=subprocess.STDOUT, timeout=180)
        assert result.returncode == 0, f"Game exited {result.returncode} on slot {slot}"
        trace = (output / f"terminal-{slot}.log").read_text(errors="replace")
        assert "[ARCHOLOS_PROFILE] frames=" in trace
        assert "mem_readint:  address translation failure: 0x0000000000ab2684" not in trace
        if probe:
            for phrase in ("I don't have the right item: Lab water bottle",
                           "I don't have the right item: Feather"):
                assert f"[MOB_FEEDBACK] text={phrase}" in trace, phrase
                assert f"[MOB_FEEDBACK] drawn={phrase}" in trace, phrase
            assert "station=MOBNAME_LAB item=ITMI_FLASK supplied=0 attached=0" in trace
            assert "station=MOBNAME_SCROLLWRITING item=ITFS_FEATHER supplied=0 attached=0" in trace
            assert "station=MOBNAME_LAB item=ITMI_FLASK supplied=1 attached=1" in trace
            assert "[MOB_FEEDBACK] usable interaction=1" in trace
            for name in ("missing-flask.png", "missing-quill.png", "usable-lab.png"):
                assert (output / name).stat().st_size > 10000
            with zipfile.ZipFile(output / "save_slot_2.sav") as archive:
                assert archive.testzip() is None
        else:
            assert "[COMPATIBILITY] Restored virtual heap and script bindings" in trace
finally:
    assert hashlib.sha256(save.read_bytes()).digest() == original, "Source save changed"
print(f"Evidence: {output}")
