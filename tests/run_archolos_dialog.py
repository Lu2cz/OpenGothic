"""Run the dialogue-exit regression using local game data and a copied pre-quest save."""
import argparse
import hashlib
import os
from pathlib import Path
from archolos_test_data import copy_save
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--executable", required=True, type=Path)
parser.add_argument("--game", required=True, type=Path)
parser.add_argument("--save", required=True, type=Path)
parser.add_argument("--output", required=True, type=Path)
parser.add_argument("--after-xp", action="store_true")
args = parser.parse_args()
exe, game, save, output = (p.resolve() for p in (args.executable, args.game, args.save, args.output))
assert exe.is_file() and game.is_dir() and save.is_file()
original = hashlib.sha256(save.read_bytes()).digest()
output.mkdir(parents=True, exist_ok=False)
copy_save(save, output / "save_slot_1.sav")
(output / "Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
command = [str(exe), "-g", str(game), "-game:TheChroniclesOfMyrtana.ini",
           "-window", "-rt", "0", "-gi", "0", "-aa", "0", "-bl", "0", "-save", "1"]
env = {k: v for k, v in os.environ.items() if not k.startswith("OPENGOTHIC_")}
env.update(OPENGOTHIC_PROFILE="1", OPENGOTHIC_DIALOG_PROBE="ui")
if args.after_xp:
    env["OPENGOTHIC_DIALOG_XP"] = "1"
try:
    with (output / "terminal.log").open("w") as log:
        result = subprocess.run(command, cwd=output, stdout=log, stderr=subprocess.STDOUT,
            env=env, timeout=180)
    assert result.returncode == 0, f"Game exited {result.returncode}; inspect {output}"
    trace = (output / "terminal.log").read_text(errors="replace")
    if args.after_xp:
        assert "[DIALOG_PROBE] XP delta=50" in trace, "XP award failed"
        assert any("[DIALOG_PROBE] notification=" in line and "50" in line
                   for line in trace.splitlines()), "XP notification did not reach the UI"
    assert "[ARCHOLOS_PROFILE] frames=2400" in trace, "Incomplete dialogue probe"
    assert trace.count("[DIALOG_PROBE] select exit") == 2, "Expected two exit selections"
    assert trace.count("[DIALOG_PROBE] stop instruction reached") == 2, "Exit script did not reach stop"
    assert trace.count("[DIALOG_PROBE] closed after output") == 2, "Dialogue did not close after spoken output"
    assert "[DIALOG_PROBE] finished rounds=2 active=0" in trace, "Dialogue remains active"
    measured = trace.split("[DIALOG_PROBE] open round=1", 1)[1]
    assert "MEM_GetFuncID: Unresolvable" not in measured, "Function resolution failed"

finally:
    assert hashlib.sha256(save.read_bytes()).digest() == original, "Source save changed"
print(f"Evidence: {output}")
