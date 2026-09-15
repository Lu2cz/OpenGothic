"""Check native Archolos music during a private city save/load run."""
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
p.add_argument("--save", type=Path, required=True, help="City save to test on a private copy")
p.add_argument("--expect-track", help="Require this track on the first playback after loading")
p.add_argument("--full", action="store_true", help="Test settings, day/night, zones, overrides and a complete overlap loop")
p.add_argument("--kmlib", action="store_true", help="Check zone gameplay events and local services")
p.add_argument("--menu", action="store_true", help="Check initial menu music, loading, and session exit back to menu")
p.add_argument("--settings", type=Path, help="Private settings for local-services persistence checks")
a = p.parse_args()
exe, game, out = (getattr(a, name).resolve() for name in ("executable", "game", "output"))
save = a.save.resolve()
original = hashlib.sha256(save.read_bytes()).hexdigest()
out.mkdir(parents=True, exist_ok=False)
copy_save(save, out / "save_slot_1.sav")
(out / "Gothic.ini").write_text(a.settings.read_text() if a.settings else "[INTERNAL]\nvidResIndex=0\n")
env = {k: v for k, v in os.environ.items() if not k.startswith("OPENGOTHIC_")}
env.update(OPENGOTHIC_PROFILE="1", OPENGOTHIC_CITY_PROBE="reload",
           OPENGOTHIC_MUSIC_PROBE="full" if a.full else "reload")
if a.kmlib:
    env["OPENGOTHIC_KMLIB_PROBE"] = "1"
command = [str(exe), "-g", str(game), "-game:TheChroniclesOfMyrtana.ini",
           "-window", "-rt", "0", "-gi", "0", "-bl", "0"]
if a.menu:
    env["OPENGOTHIC_KMLIB_MENU_PROBE"] = "1"
else:
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
    if a.menu:
        assert "global wav=GAMESTART.WAV" not in trace, "Gothic startup WAV overlaps the Archolos soundtrack"
        assert "background=2048x2048" in trace, "Archolos menu background was not selected"
        assert "menu image=menu_km_archolos.tga" in trace.lower(), "Archolos menu logo was not loaded"
        assert lengths[0][0]=="02.ogg" and lengths[-1][0]=="02.ogg", "Menu track missing on launch or return"
        for stage in (0,1):
            state = trace.split(f"[KMLIB_PROBE] menu stage={stage}\n",1)[1].splitlines()[0]
            assert re.search(r"state file=02.ogg position=[1-9]\d{3,} gain=0.500000",state), "Menu audio clock/gain failed"
            assert "tails=0 finished=0 legacy=0" in state, "Another music source remains active in the menu"
    if a.kmlib:
        assert "[KMLIB_PROBE] natural=CIT_" in trace, "Natural city zone notification missing"
        assert "[KMLIB_PROBE] haven=1 scenes=1 theme=HAV_DAY_STD edx_preserved=1" in trace, "Muted region entry hook failed"
        assert "[KMLIB_PROBE] gated=0" in trace, "Location-entry suppression ignored"
        assert "repeat_scenes=1 night=HAV_NGT_FGT" in trace, "Theme update/one-shot scene guard failed"
        assert "armor_restricted=1 armor_released=0" in trace, "Water Circle region logic missing"
        import configparser
        settings = configparser.ConfigParser()
        if a.settings:
            settings.read(a.settings)
        before = settings.getint("KMLIB_STATS", "STAT_ACHIEVEMENT_18", fallback=0)
        after = before + 2
        assert f"[KMLIB_PROBE] stat={after} unlocked={int(after>=5)}" in trace, "Local stat/achievement update failed"
        settings.read(out / "Gothic.ini")
        assert settings.getint("KMLIB_STATS", "STAT_ACHIEVEMENT_18")==after, "Local stats not persisted"
    assert "chapter=2 entered=1" in trace, "City chapter preset was not applied"
    assert "[CITY_PROBE] save finalized" in trace, "City save did not finish"
    assert "camera=0 dialogue=0" in trace, "Player is locked in a scene"
    moved = re.search(r"\[CITY_PROBE\] walked=([\d.]+)", trace)
    assert moved and float(moved[1]) > 50, "Normal walking input did not move Marvin"
    nearby = re.search(r"nearby=(\d+)", trace)
    assert nearby and int(nearby[1]) >= 3, "City NPC population is missing"
    with zipfile.ZipFile(out / "save_slot_2.sav") as z:
        assert z.testzip() is None and z.read("game/quests") and z.read("game/daedalus")
    print("\n".join(line for line in trace.splitlines() if "[CITY_PROBE]" in line or "[KMLIB_PROBE]" in line or ("[MUSIC_PROBE]" in line and "state file=" not in line)))
finally:
    assert hashlib.sha256(save.read_bytes()).hexdigest() == original, "Source save changed"
print(f"Evidence: {out}")
