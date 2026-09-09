"""Run the ship-stair regression using local game data and a copied pre-quest save."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument("--executable", required=True, type=Path)
parser.add_argument("--game", required=True, type=Path)
parser.add_argument("--save", required=True, type=Path)
parser.add_argument("--output", required=True, type=Path)
parser.add_argument("--state", choices=("closed", "open"), default="closed")
args = parser.parse_args()
exe, game, save, output = (p.resolve() for p in (args.executable, args.game, args.save, args.output))
assert exe.is_file() and game.is_dir() and save.is_file()
original = hashlib.sha256(save.read_bytes()).digest()
output.mkdir(parents=True, exist_ok=False)
shutil.copy2(save, output / "save_slot_1.sav")
(output / "Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
command = [str(exe), "-g", str(game), "-game:TheChroniclesOfMyrtana.ini",
           "-window", "-rt", "0", "-gi", "0", "-aa", "0", "-bl", "0", "-save", "1"]
try:
    with (output / "terminal.log").open("w") as log:
        result = subprocess.run(command, cwd=output, stdout=log, stderr=subprocess.STDOUT,
            env={**os.environ, "OPENGOTHIC_PROFILE": "1",
                 "OPENGOTHIC_GATE_PROBE": "open" if args.state == "open" else "walk"}, timeout=180)
    assert result.returncode == 0, f"Game exited {result.returncode}; inspect {output}"
    trace = (output / "terminal.log").read_text(errors="replace")
    assert "[GATE_STATE]" in trace and "[ARCHOLOS_PROFILE] frames=600" in trace, "Incomplete gate probe"
    subprocess.run([sys.executable, str(Path(__file__).with_name("check_archolos_gate.py")),
                    str(output / "terminal.log"), "--state", args.state], check=True)
finally:
    assert hashlib.sha256(save.read_bytes()).digest() == original, "Source save changed"
print(f"Evidence: {output}")
