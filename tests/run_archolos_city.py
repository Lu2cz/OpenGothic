"""Create or reload a private Chapter 2 city exploration save with local game data."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import time
import zipfile

p = argparse.ArgumentParser()
for name in ("executable", "game", "output"):
    p.add_argument("--" + name, type=Path, required=True)
source = p.add_mutually_exclusive_group(required=True)
source.add_argument("--seed", type=Path, help="Create the preset from a private copy of an existing game")
source.add_argument("--save", type=Path, help="Reload this city save instead of creating the preset")
p.add_argument("--persistence", choices=("seed", "reload", "completed"), help="Check linked objects and a pending one-shot callback")
p.add_argument("--reject", choices=("fingerprint", "truncated"), help="Require rejection of a damaged private compatibility snapshot")
p.add_argument("--save-failure", action="store_true", help="Verify a failed save preserves the previous slot")
a = p.parse_args()
assert sum(bool(v) for v in (a.persistence, a.reject, a.save_failure))<=1
exe, game, out = (getattr(a, name).resolve() for name in ("executable", "game", "output"))
save = (a.save or a.seed).resolve()
original = hashlib.sha256(save.read_bytes()).hexdigest()
out.mkdir(parents=True, exist_ok=False)
shutil.copy2(save, out / "save_slot_1.sav")
if a.save_failure:
    shutil.copy2(save, out / "save_slot_2.sav")
if a.reject:
    with zipfile.ZipFile(save) as z:
        entries = {n: z.read(n) for n in z.namelist()}
    data = bytearray(entries["game/compatibility"])
    if a.reject=="fingerprint":
        data[4] ^= 1
    else:
        data = data[:20]
    entries["game/compatibility"] = data
    with zipfile.ZipFile(out / "save_slot_1.sav", "w", zipfile.ZIP_DEFLATED) as z:
        for n, contents in entries.items():
            z.writestr(n, contents)
(out / "Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
env = {k: v for k, v in os.environ.items() if not k.startswith("OPENGOTHIC_")}
env.update(OPENGOTHIC_PROFILE="1", OPENGOTHIC_CITY_PROBE="reload" if a.save else "create")
if a.save_failure:
    env["OPENGOTHIC_PERSISTENCE_SAVE_FAILURE"] = "1"
if a.persistence:
    env["OPENGOTHIC_PERSISTENCE_PROBE"] = a.persistence
command = [str(exe), "-g", str(game), "-game:TheChroniclesOfMyrtana.ini",
           "-window", "-rt", "0", "-gi", "0", "-bl", "0"]
command += ["-save", "1"]
try:
    with (out / "terminal.log").open("w") as log:
        if a.reject:
            process = subprocess.Popen(command, cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic()+180
                while time.monotonic()<deadline and process.poll() is None:
                    trace = (out / "terminal.log").read_text(errors="replace")
                    if "loading error:" in trace:
                        break
                    time.sleep(0.25)
                expected = "Incompatible script/compatibility snapshot" if a.reject=="fingerprint" else "unable to read save-game file"
                assert "loading error: "+expected in trace, "Snapshot was not rejected as expected"
                assert "[CITY_PROBE] ready" not in trace and not (out / "save_slot_2.sav").exists()
                print(f"Rejected {a.reject} snapshot safely: {out}")
            finally:
                if process.poll() is None:
                    process.terminate()
                process.wait(timeout=20)
            raise SystemExit(0)
        result = subprocess.run(command, cwd=out, env=env, stdout=log,
                                stderr=subprocess.STDOUT, timeout=300)
    assert result.returncode == 0, f"Game exited {result.returncode}: {out}"
    trace = (out / "log.txt").read_text(errors="replace")
    if a.save_failure:
        assert "loading error: Injected compatibility save failure" in trace
        assert hashlib.sha256((out / "save_slot_2.sav").read_bytes()).hexdigest()==original
        assert not (out / "save_slot_2.sav.tmp").exists()
        print(f"Failed save preserved previous slot: {out}")
        raise SystemExit(0)
    if a.persistence:
        if a.persistence!="seed":
            assert "[COMPATIBILITY] Restored virtual heap and script bindings" in trace
            assert "MEM_MESSAGEBOX" not in trace
        assert f"[PERSISTENCE_PROBE] phase=start count={int(a.persistence=='completed')}" in trace
        assert f"[PERSISTENCE_PROBE] phase=finish count={int(a.persistence!='seed')}" in trace
        assert trace.count("[PERSISTENCE_PROBE] fired elapsed=")==int(a.persistence=="reload")
    assert "chapter=2 entered=1" in trace, "City chapter preset was not applied"
    assert "[CITY_PROBE] save finalized" in trace, "City save did not finish"
    assert "camera=0 dialogue=0" in trace, "Player is locked in a scene"
    moved = re.search(r"\[CITY_PROBE\] walked=([\d.]+)", trace)
    assert moved and float(moved[1]) > 50, "Normal walking input did not move Marvin"
    nearby = re.search(r"nearby=(\d+)", trace)
    assert nearby and int(nearby[1]) >= 3, "City NPC population is missing"
    with zipfile.ZipFile(out / "save_slot_2.sav") as z:
        assert z.testzip() is None and z.read("game/quests") and z.read("game/daedalus")
    print("\n".join(line for line in trace.splitlines() if "[CITY_PROBE]" in line))
finally:
    assert hashlib.sha256(save.read_bytes()).hexdigest() == original, "Source save changed"
print(f"Evidence: {out}")
