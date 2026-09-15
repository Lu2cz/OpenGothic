"""Replay the forest trialogue on a private copy; never edit the player's save."""
import argparse
import hashlib
import os
from pathlib import Path
from archolos_test_data import copy_save
import subprocess
import zipfile

p = argparse.ArgumentParser()
for name in ("executable", "game", "save", "output"):
    p.add_argument("--" + name, required=True, type=Path)
p.add_argument("--baseline", action="store_true")
p.add_argument("--full-dialogue", action="store_true")
a = p.parse_args()
exe, game, save, out = (getattr(a, n).resolve() for n in ("executable", "game", "save", "output"))
original = hashlib.sha256(save.read_bytes()).hexdigest()
out.mkdir(parents=True, exist_ok=False)
copy_save(save, out / "save_slot_1.sav")
(out / "source.sha256").write_text(original)
(out / "executable.sha256").write_text(hashlib.sha256(exe.read_bytes()).hexdigest())
(out / "Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
env = {k: v for k, v in os.environ.items() if not k.startswith("OPENGOTHIC_")}
env.update(OPENGOTHIC_PROFILE="1", OPENGOTHIC_FOREST_PROBE="1", OPENGOTHIC_TRIALOG_TRACE="1")
if not a.full_dialogue:
    env["OPENGOTHIC_CAPTAIN_SKIP"] = "1"
try:
    with (out / "terminal.log").open("w") as log:
        result = subprocess.run([str(exe), "-g", str(game), "-game:TheChroniclesOfMyrtana.ini",
            "-window", "-rt", "0", "-gi", "0", "-aa", "0", "-bl", "0", "-save", "1"],
            cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=500)
    assert result.returncode == 0, f"Game exited {result.returncode}"
    trace = (out / "terminal.log").read_text(errors="replace")
    assert "[FOREST_PROBE] complete" in trace, "Cutscene did not return control"
    assert "[FOREST_PROBE] save finalized" in trace
    if not a.baseline:
        assert "[FOREST_PROBE] player control verified" in trace
        assert "[FOREST_PROBE] speaker reset verified" in trace
        assert "Go bother someone else." not in trace
        assert "_TRIA_Copy: Invalid NPC" not in trace
        for suffix, speaker in (("05_01", "Fabio"), ("01_02", "Jorn"), ("05_03", "Fabio"),
                                ("05_04", "Fabio"), ("01_05", "Jorn"), ("05_06", "Fabio"),
                                ("01_07", "Jorn"), ("01_08", "Jorn"), ("05_09", "Fabio"),
                                ("05_10", "Jorn"), ("05_11", "Fabio"),
                                ("Question2_05_02", "Fabio"), ("Question1_01_02", "Jorn"),
                                ("QuestionEnd_05_02", "Fabio"), ("QuestionEnd_01_07", "Jorn"),
                                ("QuestionEnd_01_09", "Jorn")):
            assert f"output=TRIA_Fabio_Q102_JornTrialog_{suffix} actor=Fabio label={speaker} " in trace
        assert "QuestionEnd_15_06 actor=Marvin label=player " in trace
    with zipfile.ZipFile(out / "save_slot_2.sav") as z:
        assert z.testzip() is None
        assert b"Forest dialogue test" in z.read("header")
finally:
    assert hashlib.sha256(save.read_bytes()).hexdigest() == original, "Source save changed"
print(f"Evidence: {out}")
