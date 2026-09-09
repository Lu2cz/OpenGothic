"""Run the recipe-journal regression using local game data and a copied pre-quest save."""
import argparse
import hashlib
import os
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
env.update(OPENGOTHIC_PROFILE="1", OPENGOTHIC_RECIPE_PROBE="1")
try:
    with (output / "terminal.log").open("w") as log:
        result = subprocess.run(command, cwd=output, stdout=log, stderr=subprocess.STDOUT,
            env=env, timeout=180)
    assert result.returncode == 0, f"Game exited {result.returncode}; inspect {output}"
    trace = (output / "terminal.log").read_text(errors="replace")
    assert "[ARCHOLOS_PROFILE] frames=180" in trace, "Incomplete recipe probe"
    measured = trace.split("[RECIPE_PROBE] begin learning", 1)[1].split("[RECIPE_PROBE] end", 1)[0]
    assert "[RECIPE_PROBE] learned=1" in measured, "Recipe was not learned"
    cooking = measured.split("[RECIPE_PROBE] topic=Cooking", 1)[1].split("[RECIPE_PROBE] topic=", 1)[0]
    assert "[RECIPE_PROBE] entry=Rat on a stick" in cooking, "Recipe is absent from Cooking"
    for ingredient in ["- Heavy Branch", "- 2x Fried rat meat", "- Salt", "- A bag of pepper"]:
        assert ingredient in cooking, f"Missing ingredient: {ingredient}"
    assert "[RECIPE_PROBE] preserved=1 reread_duplicate=0" in measured, "Other state changed or reread duplicated the log"
    assert measured.count("[RECIPE_PROBE] document=") == 2, "Reading did not display a document twice"
    for doc in measured.split("[RECIPE_PROBE] document=")[1:]:
        assert "Hitpoint restore: 35" in doc and "Value: 6" in doc, "Recipe document stats are missing"
        assert "A mediocre meal of rat meat," in doc, "Recipe document description is missing"

    assert "Internal Exception" not in measured, "Recipe script raised an exception"
    assert "translation failure" not in measured, "Recipe used unmapped memory"

finally:
    assert hashlib.sha256(save.read_bytes()).digest() == original, "Source save changed"
print(f"Evidence: {output}")
