"""Check the Fabio/Rupert gate or replay arrival using native walking and UI choices."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import json
import zipfile

from archolos_test_data import copy_save

p = argparse.ArgumentParser()
for name in ("executable", "game", "save", "output"):
    p.add_argument("--" + name, required=True, type=Path)
p.add_argument("--expect", choices=("available", "unavailable"), default="available")
p.add_argument("--story", choices=("seed", "reload", "sleep", "sleep-reload", "sleep-again", "routines"))
p.add_argument("--placement", choices=("broken", "placed"))
a = p.parse_args()
exe, game, save, out = (getattr(a, name).resolve() for name in ("executable", "game", "save", "output"))
source_hash = hashlib.sha256(save.read_bytes()).hexdigest()
out.mkdir(parents=True, exist_ok=False)
copy_save(save, out / "save_slot_1.sav")
(out / "Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
env = {k: v for k, v in os.environ.items() if not k.startswith("OPENGOTHIC_")}
env.update(OPENGOTHIC_PROFILE="1", OPENGOTHIC_SILBACH_PROBE="1")
if a.story:
    env.pop("OPENGOTHIC_SILBACH_PROBE")
    env.update(OPENGOTHIC_SILBACH_STORY=a.story, OPENGOTHIC_TRIALOG_TRACE="1", OPENGOTHIC_CAPTAIN_SKIP="1")
(out / "provenance.json").write_text(json.dumps({"source_sha256": source_hash, "executable_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(), "mode": a.story or a.expect}, indent=2))
try:
    with (out / "terminal.log").open("w") as log:
        result = subprocess.run([str(exe), "-g", str(game), "-game:TheChroniclesOfMyrtana.ini",
                                 "-window", "-rt", "0", "-gi", "0", "-aa", "0", "-bl", "0", "-save", "1"],
                                cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=1000 if a.story else 300)
    trace = "\n".join((out / name).read_text(errors="replace")
                      for name in ("terminal.log", "log.txt") if (out / name).exists())
    assert result.returncode == 0, f"Game exited {result.returncode}"
    if a.story == "routines":
        assert "[ROUTINE_EXCHANGE] complete nearby=1 distant=1 hidden=1 dead=1 player=1" in trace
        assert not (out / "save_slot_2.sav").exists()
    elif a.story:
        if a.story.startswith("sleep"):
            observed = [line.rsplit("=", 1)[1] for line in trace.splitlines()
                        if "[SILBACH_SLEEP] complete placed=" in line]
            actual = observed[-1].strip().lower() if observed else ""
            if a.story != "sleep-reload":
                assert "[SILBACH_SLEEP] bed=" in trace, "Sleep bed was not used"
            assert actual in {"0", "1", "false", "true"}, observed
            if a.placement:
                assert (actual in {"1", "true"}) == (a.placement == "placed"), observed
        else:
            assert f"[SILBACH_STORY] complete reload={int(a.story == 'reload')}" in trace, "Story did not complete; inspect stage log"
            assert "[SILBACH_STORY] control moved=" in trace
        if a.story == "sleep" and a.placement == "placed":
            assert "[SILBACH_SLEEP] activated=52 misplaced=0" in trace
            for choice in ("DIA_VIKTOR_WAKEUP_WHERE", "DIA_VIKTOR_WAKEUP_KURT"):
                assert f"[SILBACH_STORY] select={choice}" in trace, choice
        if a.story == "sleep-again":
            assert "[SILBACH_STORY] select=PC_SLEEPTIME_NOON_INFO" in trace
        if a.story == "seed":
            for choice in ("DIA_MARTHA_Q103_TRIALOG_FABIOWAY_SPLITUP", "DIA_JORN_Q103_ALLRIGHT_SPLITUP", "DIA_JORN_Q103_ALLRIGHT_KURT"):
                assert f"[SILBACH_STORY] select={choice}" in trace, choice
            for suffix, speaker in (("03_01", "Martha"), ("03_02", "Viktor"), ("03_03", "Viktor"),
                                    ("03_04", "Martha"), ("03_05", "Viktor"), ("03_06", "Viktor"), ("03_07", "Viktor")):
                assert f"output=DIA_Martha_Q103_Trialog_FabioWay_{suffix} actor=Martha label={speaker} " in trace
            assert "output=DIA_Jorn_Q103_Allright_Kurt_01_04 actor=Jorn label=Jorn " in trace
            assert "Go bother someone else." not in trace
        with zipfile.ZipFile(out / "save_slot_2.sav") as z:
            assert z.testzip() is None
            assert (b"Silbach sleep verification" if a.story.startswith("sleep") else b"Silbach story verification") in z.read("header")
    else:
        observed = [line.rsplit("=", 1)[1] for line in trace.splitlines()
                    if "[SILBACH_PROBE] fabio_trialog_available=" in line]
        actual = observed[-1].strip().lower() if observed else ""
        assert actual in {"0", "1", "false", "true"}, observed
        assert (actual in {"1", "true"}) == (a.expect == "available"), observed
        assert not (out / "save_slot_2.sav").exists(), "Read-only probe unexpectedly saved"

finally:
    assert hashlib.sha256(save.read_bytes()).hexdigest() == source_hash, "Source save changed"
print(f"PASS Silbach {a.story or a.expect}: {out}")
