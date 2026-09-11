"""Replay Jorn/captain progression on a private pre-captain save."""
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
    p.add_argument("--" + name, required=True, type=Path)
p.add_argument("--save", type=Path)
p.add_argument("--prepare", action="store_true", help="Start a fresh private ship game and save the narrow pre-captain fixture")
p.add_argument("--skip-dialogue", action="store_true")
a = p.parse_args()
exe, game, out = (getattr(a, n).resolve() for n in ("executable", "game", "output"))
save = a.save.resolve() if a.save else None
assert (save is None) == a.prepare
original = hashlib.sha256(save.read_bytes()).hexdigest() if save else None
out.mkdir(parents=True, exist_ok=False)
if save:
    shutil.copy2(save, out / "save_slot_1.sav")
    (out / "source.sha256").write_text(original)
(out / "Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
env = {k: v for k, v in os.environ.items() if not k.startswith("OPENGOTHIC_")}
env.update(OPENGOTHIC_PROFILE="1", OPENGOTHIC_CAPTAIN_PROBE="prepare" if a.prepare else "1", OPENGOTHIC_TRIALOG_TRACE="1")
if a.skip_dialogue:
    env["OPENGOTHIC_CAPTAIN_SKIP"] = "1"
try:
    with (out / "terminal.log").open("w") as log:
        result = subprocess.run([str(exe), "-g", str(game), "-game:TheChroniclesOfMyrtana.ini",
            "-window", "-rt", "0", "-gi", "0", "-aa", "0", "-bl", "0"] + (["-nomenu"] if a.prepare else ["-save", "1"]),
            cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=700)
    assert result.returncode == 0, f"Game exited {result.returncode}"
    trace = (out / "terminal.log").read_text(errors="replace")
    if a.prepare:
        assert "[CAPTAIN_PROBE] fixture ready" in trace and "[CAPTAIN_PROBE] fixture finalized" in trace
        with zipfile.ZipFile(out / "save_slot_2.sav") as z:
            assert z.testzip() is None and b"Captain fixture" in z.read("header")
        print(f"Evidence: {out}")
        raise SystemExit
    for fn in ("DIA_JORN_Q101_WHATSUP_INFO", "DIA_JORN_Q101_WHATSUP_YES",
               "TRIA_CAPTAIN_Q101_JORNTRIALOG_1", "TRIA_CAPTAIN_Q101_TIMOTRIALOG_NOTNECESSARY"):
        assert "[CAPTAIN_PROBE] select " + fn in trace, "Missing dialogue choice: " + fn
    assert "Go bother someone else." not in trace, "Empty dialogue selected an unrelated subtitle"
    assert "_TRIA_Copy: Invalid NPC" not in trace, "Legacy speaker swapping still ran"
    for message, speaker in (("TRIA_Jorn_Q101_JornTrialog_01_03", "Jorn"),
                             ("TRIA_Timo_Q101_TimoTrialog_06_03", "Timo")):
        assert re.search(r"output=" + re.escape(message) + r" actor=.*? label=" + speaker + r" running=", trace), message
    assert re.search(r"output=TRIA_Captain_Q101_TimoTrialog_08_04 actor=(.+?) label=\1 running=", trace)
    assert "[CAPTAIN_PROBE] complete" in trace, "Cutscene did not return control"
    assert "[CAPTAIN_PROBE] save finalized" in trace, "Save did not finish"
    assert "camera=0 dialogue=0 flag=11 fade=0 alpha=0 tria=0" in trace
    assert "[CAPTAIN_PROBE] registered animation tick" not in trace, "Test injected the fix"
    ezekiel = re.findall(r"npc=NONE_3_EZEKIEL pos=([^ ]+) bs=(\d+) wp=(\S+)", trace)
    assert ezekiel and int(ezekiel[-1][1]) & 31 == 11, "Ezekiel did not sit after departure"
    assert ezekiel[-1][2] == "PART_13_DARRYL_DEAD", "Ezekiel retained his ship routine"
    with zipfile.ZipFile(out / "save_slot_2.sav") as z:
        assert z.testzip() is None
        assert b"Captain sequence test" in z.read("header")
        assert z.read("game/quests") and z.read("game/daedalus")
finally:
    if save:
        assert hashlib.sha256(save.read_bytes()).hexdigest() == original, "Source save changed"
print(f"Evidence: {out}")
