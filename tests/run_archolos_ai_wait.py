"""Save and reload a queued AI_WAITTILLEND outside dialogue on private copies."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

p = argparse.ArgumentParser()
for name in ("executable", "game", "save", "output"):
    p.add_argument("--" + name, required=True, type=Path)
p.add_argument("--mode", choices=("seed", "reload", "edges"), required=True)
a = p.parse_args()
exe, game, save, out = (getattr(a, name).resolve() for name in ("executable", "game", "save", "output"))
original = hashlib.sha256(save.read_bytes()).hexdigest()
out.mkdir(parents=True, exist_ok=False)
shutil.copy2(save, out / "save_slot_1.sav")
(out / "source.sha256").write_text(original)
(out / "executable.sha256").write_text(hashlib.sha256(exe.read_bytes()).hexdigest())
(out / "Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
env = {key: value for key, value in os.environ.items() if not key.startswith("OPENGOTHIC_")}
env.update(OPENGOTHIC_PROFILE="1", OPENGOTHIC_AI_WAIT_PROBE=a.mode)
try:
    with (out / "terminal.log").open("w") as log:
        result = subprocess.run([str(exe), "-g", str(game), "-game:TheChroniclesOfMyrtana.ini",
                                 "-window", "-rt", "0", "-gi", "0", "-aa", "0", "-bl", "0", "-save", "1"],
                                cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=180)
    assert result.returncode == 0, f"Game exited {result.returncode}"
    trace = (out / "terminal.log").read_text(errors="replace")
    expected = "[AI_WAIT_PROBE] save pending=1" if a.mode == "seed" else "[AI_WAIT_PROBE] edges complete" if a.mode == "edges" else "[AI_WAIT_PROBE] complete"
    assert expected in trace and "[AI_WAIT_PROBE] save finalized" in trace, trace[-2000:]
    if a.mode == "reload":
        assert "[AI_WAIT_PROBE] restored pending=1" in trace
        assert "[AI_WAIT_PROBE] still pending=1" in trace
    if a.mode == "edges":
        for marker in ("empty=1", "completed=1", "self=1", "snapshot=1", "reciprocal=1", "removed=1"):
            assert "[AI_WAIT_PROBE] " + marker in trace
    with zipfile.ZipFile(out / "save_slot_2.sav") as archive:
        header = b"AI wait edge test" if a.mode == "edges" else b"AI wait persistence test"
        assert archive.testzip() is None and header in archive.read("header")
finally:
    assert hashlib.sha256(save.read_bytes()).hexdigest() == original, "Source save changed"
print(f"Evidence: {out}")
