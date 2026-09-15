"""Check beach routines, torch gravity and corpse loot in private game runs."""
import argparse
import hashlib
import os
from pathlib import Path
import re
from archolos_test_data import copy_save
import subprocess
import zipfile

p = argparse.ArgumentParser()
for name in ("executable", "game", "output"):
    p.add_argument("--" + name, type=Path, required=True)
p.add_argument("--save", type=Path)
p.add_argument("--mode", choices=("observe", "repair", "fresh"), default="observe")
p.add_argument("--expect-loot", action="store_true")
a = p.parse_args()
exe, game, out = (getattr(a, n).resolve() for n in ("executable", "game", "output"))
save = a.save.resolve() if a.save else None
assert (save is None) == (a.mode == "fresh")
original = hashlib.sha256(save.read_bytes()).hexdigest() if save else None
out.mkdir(parents=True, exist_ok=False)
args = ["-nomenu"]
if save:
    copy_save(save, out / "save_slot_1.sav")
    (out / "source.sha256").write_text(original)
    args = ["-save", "1"]
(out / "Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
env = {k: v for k, v in os.environ.items() if not k.startswith("OPENGOTHIC_")}
env.update(OPENGOTHIC_PROFILE="1", OPENGOTHIC_BEACH_PROBE=a.mode)
try:
    with (out / "terminal.log").open("w") as log:
        result = subprocess.run([str(exe), "-g", str(game), "-game:TheChroniclesOfMyrtana.ini",
            "-window", "-rt", "0", "-gi", "0", "-aa", "0", "-bl", "0"] + args,
            cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=300)
    assert result.returncode == 0, f"Game exited {result.returncode}: {out}"
    trace = (out / "log.txt").read_text(errors="replace")
    if a.mode == "fresh":
        assert "[BEACH_PROBE] fresh loot complete" in trace, "Fresh loot audit did not finish"
    else:
        assert "[BEACH_PROBE] save finalized" in trace, "Private save did not finish"
    measured = trace[trace.index("[BEACH_PROBE]"):]
    assert "Internal Exception" not in measured, "Script exception in beach check"
    if a.mode != "fresh":
        assert "torch equipped=1" in measured
        assert "torch spawned dynamic=1" in measured, "Dropped torch has no physics body"
        records = re.findall(r"\[BEACH_PROBE\] torch frame=(\d+) fall=([-\d.]+) ground_distance=([-\d.]+) ground_hit=(\d) held=(\d)", measured)
        assert records, "No dropped torch measurements"
        frame, fall, ground, hit, held = records[-1]
        assert int(frame) >= 1080 and float(fall) > 40, "Torch did not fall"
        assert hit == "1" and abs(float(ground)) < 35 and held == "0", "Torch did not settle on ground"
        positions = re.findall(r"\[BEACH_PROBE\] frame=(\d+) ezekiel=([-\d.]+),([-\d.]+),([-\d.]+) sitting=(\d) wp=(\S+)", measured)
        settled = [(float(x), float(y), float(z)) for frame, x, y, z, sitting, wp in positions
                   if int(frame) >= 720 and sitting == "1" and wp == "PART_13_DARRYL_DEAD"]
        assert len(settled) >= 4, "Ezekiel did not remain seated on the beach"
        assert all(sum((v - w) ** 2 for v, w in zip(pos, settled[-1])) < 25 for pos in settled), "Ezekiel kept moving"
    if a.mode in ("fresh", "repair") or a.expect_loot:
        for item, count in (("ITMI_POCKET", 1), ("ITSC_LIGHTHEAL", 1), ("ITMI_GOLD", 13)):
            assert f"corpse item={item} count={count}" in measured, f"Missing corpse loot: {item}"
    if a.mode != "repair":
        assert "recovered Ezekiel" not in trace, "Observation unexpectedly repaired the NPC"
    if a.mode != "fresh":
        with zipfile.ZipFile(out / "save_slot_2.sav") as z:
            assert z.testzip() is None and z.read("game/quests") and z.read("game/daedalus")
finally:
    if save:
        assert hashlib.sha256(save.read_bytes()).hexdigest() == original, "Source save changed"
print(f"PASS {a.mode}: {out}")
