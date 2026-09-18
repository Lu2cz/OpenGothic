"""Inspect the Fabio/Rupert Silbach arrival gate on an independent save copy."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess

from archolos_test_data import copy_save

p = argparse.ArgumentParser()
for name in ("executable", "game", "save", "output"):
    p.add_argument("--" + name, required=True, type=Path)
p.add_argument("--expect", choices=("available", "unavailable"), required=True)
a = p.parse_args()
exe, game, save, out = (getattr(a, name).resolve() for name in ("executable", "game", "save", "output"))
source_hash = hashlib.sha256(save.read_bytes()).hexdigest()
out.mkdir(parents=True, exist_ok=False)
copy_save(save, out / "save_slot_1.sav")
(out / "Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
env = {k: v for k, v in os.environ.items() if not k.startswith("OPENGOTHIC_")}
env.update(OPENGOTHIC_PROFILE="1", OPENGOTHIC_SILBACH_PROBE="1")
try:
    with (out / "terminal.log").open("w") as log:
        result = subprocess.run([str(exe), "-g", str(game), "-game:TheChroniclesOfMyrtana.ini",
                                 "-window", "-rt", "0", "-gi", "0", "-aa", "0", "-bl", "0", "-save", "1"],
                                cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=300)
    trace = "\n".join((out / name).read_text(errors="replace")
                      for name in ("terminal.log", "log.txt") if (out / name).exists())
    assert result.returncode == 0, f"Game exited {result.returncode}"
    observed = [line.rsplit("=", 1)[1] for line in trace.splitlines()
                if "[SILBACH_PROBE] fabio_trialog_available=" in line]
    actual = observed[-1].strip().lower() if observed else ""
    assert actual in {"0", "1", "false", "true"}, observed
    assert (actual in {"1", "true"}) == (a.expect == "available"), observed
    assert not (out / "save_slot_2.sav").exists(), "Read-only probe unexpectedly saved"
finally:
    assert hashlib.sha256(save.read_bytes()).hexdigest() == source_hash, "Source save changed"
print(f"PASS Fabio trialogue {a.expect}: {out}")
