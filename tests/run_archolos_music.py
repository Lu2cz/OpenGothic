"""Check native Archolos music during a private city save/load run."""
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
    p.add_argument("--" + name, type=Path, required=True)
p.add_argument("--save", type=Path, required=True, help="City save to test on a private copy")
p.add_argument("--expect-track", help="Require this track on the first playback after loading")
p.add_argument("--full", action="store_true", help="Test settings, day/night, zones, overrides and a complete overlap loop")
a = p.parse_args()
exe, game, out = (getattr(a, name).resolve() for name in ("executable", "game", "output"))
save = a.save.resolve()
original = hashlib.sha256(save.read_bytes()).hexdigest()
out.mkdir(parents=True, exist_ok=False)
shutil.copy2(save, out / "save_slot_1.sav")
(out / "Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
env = {k: v for k, v in os.environ.items() if not k.startswith("OPENGOTHIC_")}
env.update(OPENGOTHIC_PROFILE="1", OPENGOTHIC_CITY_PROBE="reload",
           OPENGOTHIC_MUSIC_PROBE="full" if a.full else "reload")
command = [str(exe), "-g", str(game), "-game:TheChroniclesOfMyrtana.ini",
           "-window", "-rt", "0", "-gi", "0", "-bl", "0"]
command += ["-save", "1"]
try:
    with (out / "terminal.log").open("w") as log:
        result = subprocess.run(command, cwd=out, env=env, stdout=log,
                                stderr=subprocess.STDOUT, timeout=300)
    assert result.returncode == 0, f"Game exited {result.returncode}: {out}"
    trace = (out / "log.txt").read_text(errors="replace")
    assert "[MUSIC_PROBE] playing file=" in trace, "No native Archolos track reached playback"
    states = re.findall(r"state file=(\S+) position=(\d+) gain=([\d.]+) enabled=(\d) tails=(\d+) finished=(\d)", trace)
    assert any(int(pos)>1000 and float(gain)>0 and done=="0" for _,pos,gain,_,_,done in states), "Native music source did not advance at nonzero gain"
    lengths = re.findall(r"playing file=(\S+) length=(\d+)", trace)
    if a.expect_track:
        assert lengths and lengths[0][0].lower()==a.expect_track.lower(), "Saved override/zone selected the wrong initial track"
    for name, length in lengths:
        if name.lower()=="03.ogg":
            assert abs(int(length)-116500)<100, "Long-track duration overflowed (03.ogg is 116.5 seconds)"
    if a.full:
        assert "stage=12" in trace, "Music scenario did not complete"
        cancelled = trace.split("[MUSIC_PROBE] stage=1\n",1)[1].split("[MUSIC_PROBE] stage=2\n",1)[0]
        assert "background=1" in cancelled and "playing file=11.ogg" not in cancelled, "Cancelled background track reached playback"
        assert any(float(gain)==0 and enabled=="0" for _,_,gain,enabled,_,_ in states), "Mute failed"
        assert any(abs(float(gain)-0.2)<0.001 for _,_,gain,_,_,_ in states), "Volume setting failed"
        assert "tags=1 override=0" in trace, "Night music was not selected"
        assert "zone=VIL tags=0 override=0" in trace, "Village selection failed"
        assert "zone=VIL tags=2 override=0" in trace, "Daytime combat selection failed"
        assert "zone=VIL tags=3 override=0" in trace, "Nighttime combat selection failed"
        assert re.search(r"playing file=35.ogg .*loop=1",trace), "Track did not loop through its overlap point"
        assert any(int(tails)>0 for _,_,_,_,tails,_ in states), "No transition/loop tail was observed"
    assert "chapter=2 entered=1" in trace, "City chapter preset was not applied"
    assert "[CITY_PROBE] save finalized" in trace, "City save did not finish"
    assert "camera=0 dialogue=0" in trace, "Player is locked in a scene"
    moved = re.search(r"\[CITY_PROBE\] walked=([\d.]+)", trace)
    assert moved and float(moved[1]) > 50, "Normal walking input did not move Marvin"
    nearby = re.search(r"nearby=(\d+)", trace)
    assert nearby and int(nearby[1]) >= 3, "City NPC population is missing"
    with zipfile.ZipFile(out / "save_slot_2.sav") as z:
        assert z.testzip() is None and z.read("game/quests") and z.read("game/daedalus")
    print("\n".join(line for line in trace.splitlines() if "[CITY_PROBE]" in line or ("[MUSIC_PROBE]" in line and "state file=" not in line)))
finally:
    assert hashlib.sha256(save.read_bytes()).hexdigest() == original, "Source save changed"
print(f"Evidence: {out}")
