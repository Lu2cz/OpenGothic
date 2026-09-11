"""Exercise a private Archolos Mainland → Sewers → Mainland world transition."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

p = argparse.ArgumentParser()
for name in ("executable", "game", "save", "output"):
    p.add_argument("--" + name, type=Path, required=True)
p.add_argument("--timeout", type=int, default=720)
a = p.parse_args()
exe, game, source, out = (getattr(a, name).resolve() for name in ("executable", "game", "save", "output"))
assert not out.exists(), f"Output already exists: {out}"
out.mkdir(parents=True)
source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
with zipfile.ZipFile(source) as z:
    quests = z.read("game/quests")
records = []

def write_records():
    (out / "run.json").write_text(json.dumps(records, indent=2) + "\n")

def run(name, save, mode, evidence):
    stage = out / name
    stage.mkdir()
    shutil.copy2(save, stage / "save_slot_1.sav")
    (stage / "Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
    env = {k: v for k, v in os.environ.items() if not k.startswith("OPENGOTHIC_")}
    env.update(OPENGOTHIC_PROFILE="1", OPENGOTHIC_WORLD_PROBE=mode)
    command = [str(exe), "-g", str(game), "-game:TheChroniclesOfMyrtana.ini",
               "-window", "-rt", "0", "-gi", "0", "-bl", "0", "-save", "1"]
    record = {"stage": name, "mode": mode, "command": command,
              "source": str(save), "source_sha256": hashlib.sha256(save.read_bytes()).hexdigest(),
              "executable_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(), "result": "RUNNING"}
    records.append(record)
    write_records()
    try:
        with (stage / "terminal.log").open("w") as log:
            result = subprocess.run(command, cwd=stage, env=env, stdout=log,
                                    stderr=subprocess.STDOUT, timeout=a.timeout)
        record["game_exit_code"] = result.returncode
        trace = "\n".join((stage / n).read_text(errors="replace") for n in ("terminal.log", "log.txt") if (stage / n).exists())
        assert result.returncode == 0, f"{name}: game exited {result.returncode}"
        assert "[WORLD_PROBE] save finalized" in trace, f"{name}: incomplete save"
        assert all(x not in trace for x in ("Internal Exception", "translation failure", "Unmapped memory")), f"{name}: script/VM error"
        for marker in evidence:
            assert marker in trace, f"{name}: missing {marker!r}"
        saved = stage / "save_slot_2.sav"
        assert saved.exists() and not (stage / "save_slot_2.sav.tmp").exists(), f"{name}: save was not finalized"
        with zipfile.ZipFile(saved) as z:
            assert z.testzip() is None, f"{name}: invalid save ZIP"
            assert z.read("game/quests") == quests, f"{name}: quest archive changed"
    except subprocess.TimeoutExpired as error:
        record.update(result="TIMEOUT", error=str(error))
        write_records()
        raise
    except BaseException as error:
        record.update(result="FAIL", error=str(error))
        write_records()
        raise
    record["result"] = "PASS"
    write_records()
    return saved

try:
    sewer = run("01-to-sewers", source, "to-sewers", (
        "native_change target=ARCHOLOS_SEWERS.ZEN", "save world=archolos_sewers.zen",
        "destroyed_ref=0 callback_dispatches=1 stale_focus=0"))
    mainland = run("02-to-mainland", sewer, "to-mainland", (
        "native_change target=ARCHOLOS_MAINLAND.ZEN", "save world=ARCHOLOS_MAINLAND.ZEN",
        "returned_lock_progress=1", "destroyed_ref=0 callback_dispatches=1 stale_focus=0"))
    run("03-restart-mainland", mainland, "verify-mainland", ("restart_lock_progress=1 world=ARCHOLOS_MAINLAND.ZEN",))
finally:
    assert hashlib.sha256(source.read_bytes()).hexdigest() == source_hash, "Source save changed"
print(f"PASS world transition: {out}")
