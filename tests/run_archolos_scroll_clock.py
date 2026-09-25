"""Check accelerated or held Archolos world time via a native UI save."""
import argparse
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import zipfile

from archolos_test_data import copy_save

p = argparse.ArgumentParser()
for name in ("executable", "game", "save", "output"):
    p.add_argument("--" + name, required=True, type=Path)
p.add_argument("--held", action="store_true", help="Input save has HOLDTIME_ACTIVATED set")
a = p.parse_args()
exe, game, save, out = (getattr(a, name).resolve() for name in ("executable", "game", "save", "output"))
source_hash = hashlib.sha256(save.read_bytes()).hexdigest()
with zipfile.ZipFile(save) as z:
    assert z.testzip() is None
    before = struct.unpack_from("<Q", z.read("game/session"), 8)[0]
out.mkdir(parents=True, exist_ok=False)
copy_save(save, out / "save_slot_1.sav")
(out / "Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
events = []


def key(t, code, ch):
    events.extend(((t, code, ch, 1), (t + 20, code, ch, 0)))


key(8000, 53, 27)  # Pause after eight seconds of ordinary gameplay.
key(8400, 36, 13)  # Save Game.
key(8800, 125, 0xF701)  # Slot 2 keeps the source copy untouched.
key(9100, 36, 13)
for i, (code, ch) in enumerate(((34, "I"), (1, "s"), (1, "s"), (20, "3"), (22, "6"))):
    key(10000 + i * 100, code, ord(ch))
key(11000, 36, 13)
key(13000, 53, 27)
(out / "events.txt").write_text("".join(" ".join(map(str, e)) + "\n" for e in sorted(events)))
env = {k: v for k, v in os.environ.items() if not k.startswith("OPENGOTHIC_")}
env.update(OPENGOTHIC_PROFILE="1", OPENGOTHIC_UI_PROBE="save", OPENGOTHIC_UI_EVENTS=str(out / "events.txt"))
try:
    with (out / "terminal.log").open("w") as log:
        result = subprocess.run([str(exe), "-g", str(game), "-game:TheChroniclesOfMyrtana.ini",
                                 "-window", "-rt", "0", "-gi", "0", "-aa", "0", "-bl", "0", "-save", "1"],
                                cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=240)
    assert result.returncode == 0, f"Game exited {result.returncode}"
    saved = out / "save_slot_2.sav"
    with zipfile.ZipFile(saved) as z:
        assert z.testzip() is None
        assert b"Iss36" in z.read("header")
        after = struct.unpack_from("<Q", z.read("game/session"), 8)[0]
    minutes = (after - before) / 60000
    if a.held:
        assert after == before, f"Held clock advanced {minutes:.1f} game minutes"
    else:
        assert 60 < minutes < 130, f"Unexpected clock advance: {minutes:.1f} game minutes"
finally:
    assert hashlib.sha256(save.read_bytes()).hexdigest() == source_hash, "Source save changed"
print(f"PASS Riordian clock {'held' if a.held else 'advanced'} {minutes:.1f} game minutes: {out}")
