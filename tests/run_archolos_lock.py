"""Reproduce Open Lock on a supplied pre-chest save; all writes use private copies."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import time
import zipfile

p = argparse.ArgumentParser()
for name in ["executable", "game", "save", "output"]:
    p.add_argument("--"+name, type=Path, required=True)
p.add_argument("--mode", choices=["cast","ordinary","ordinary-reload","reload"], default="cast")
a = p.parse_args()
exe, game, save, out = (x.resolve() for x in [a.executable,a.game,a.save,a.output])
original = hashlib.sha256(save.read_bytes()).digest()
out.mkdir(parents=True,exist_ok=False)
shutil.copy2(save,out/"save_slot_1.sav")
(out/"Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
env = {k:v for k,v in os.environ.items() if not k.startswith("OPENGOTHIC_")}
env.update(OPENGOTHIC_PROFILE="1",OPENGOTHIC_LOCK_PROBE=a.mode)
try:
    with (out/"terminal.log").open("w") as log:
        command = [str(exe),"-g",str(game),"-game:TheChroniclesOfMyrtana.ini","-window","-rt","0","-gi","0","-bl","0","-save","1"]
        if a.mode=="ordinary":
            process = subprocess.Popen(command,cwd=out,env=env,stdout=log,stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic()+180
                while time.monotonic()<deadline and not (out/"save_slot_2.sav").exists():
                    if process.poll() is not None:
                        raise AssertionError(f"Game exited {process.returncode}: {out}")
                    time.sleep(.1)
                assert (out/"save_slot_2.sav").exists(), f"Ordinary lockpick save missing: {out}"
            finally:
                if process.poll() is None:
                    process.terminate()
                process.wait(timeout=20)
        else:
            result = subprocess.run(command,cwd=out,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
            assert result.returncode==0, f"Game exited {result.returncode}: {out}"
    text = (out/"terminal.log").read_text(errors="replace")
    if a.mode!="ordinary":
        assert "[ARCHOLOS_PROFILE] frames=900" in text, "Incomplete run"
    measured = text if a.mode=="ordinary" else text.split("[LOCK_PROBE] loaded",1)[1]
    if a.mode!="ordinary":
        assert "unarmed focus=Chest mob=Q101_CHEST_01" in measured, "Wrong reproduction position/chest"
    if a.mode=="cast":
        for expected in ["spell=103 focus=Chest mob=Q101_CHEST_01", "away_rejected=1", "no_mana_rejected=1", "unlocked_rejected=1", "cast input held", "complete cracked=1 scroll_used=1 mana_used=1", "chest_ui=2"]:
            assert expected in measured, f"Missing {expected}: {out}"
        with zipfile.ZipFile(out/"save_slot_2.sav") as z, zipfile.ZipFile(save) as source:
            assert z.testzip() is None
            assert b"Open Lock test" in z.read("header")
            assert z.read("game/quests")==source.read("game/quests"), "Unlock unexpectedly changed existing quests"
    elif a.mode=="ordinary":
        for expected in ["ordinary target=Q101_CHEST_01", "ordinary broken=1", "ordinary cracked=1 chest_ui=2"]:
            assert expected in measured, f"Missing {expected}: {out}"
        with zipfile.ZipFile(out/"save_slot_2.sav") as z:
            assert z.testzip() is None
            assert b"Ordinary lockpick test" in z.read("header")
    elif a.mode=="ordinary-reload":
        assert "ordinary reload cracked=1 chest_ui=2" in measured, f"Reload lost ordinary unlock: {out}"
    else:
        assert "reload cracked=1 chest_ui=2" in measured, "Reload lost unlock or chest access"
    assert "Internal Exception" not in measured, "Script exception during tested flow"
    assert "translation failure" not in measured, "Unmapped memory during tested flow"
finally:
    assert hashlib.sha256(save.read_bytes()).digest()==original, "Source save changed"
print(f"PASS {a.mode}: {out}")
