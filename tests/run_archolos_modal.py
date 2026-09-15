"""Exercise real modal menus with in-process Cocoa key events and copied saves."""
import argparse
import hashlib
import os
from pathlib import Path
import re
from archolos_test_data import copy_save
import subprocess
import struct
import zipfile

p = argparse.ArgumentParser()
for name in ["executable", "game", "save", "output"]:
    p.add_argument("--" + name, required=True, type=Path)
p.add_argument("--mode", choices=["journal", "save"], required=True)
a = p.parse_args()
exe, game, save, out = (x.resolve() for x in [a.executable, a.game, a.save, a.output])
original = hashlib.sha256(save.read_bytes()).digest()
out.mkdir(parents=True, exist_ok=False)
copy_save(save, out / "save_slot_1.sav")
(out / "Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
events = []
def key(t, code, ch, repeats=0):
    events.append((t, code, ch, 1))
    events.extend((t + 50 * (i+1), code, ch, 1) for i in range(repeats))
    events.append((t + 50 * repeats + 20, code, ch, 0))
if a.mode == "journal":
    key(500, 36, 13)
    key(1300, 36, 13)
    key(2200, 125, 0xf701, 15) # Held Down: last page.
    key(3500, 126, 0xf700, 15) # Held Up: first page.
    key(4800, 1, ord("s"), 15) # Held S: last page.
    key(6100, 13, ord("w"), 15) # Held W: first page.
    events.extend((7400 + i*30, -1, -1, 0) for i in range(20))
    key(8500, 53, 27)
    key(9000, 53, 27)
    key(9500, 53, 27)
else:
    key(100, 53, 27) # Actual Escape/pause-menu path.
    key(500, 36, 13) # Default selection in MENU_GAME is Save Game.
    key(900, 125, 0xf701) # Slot 2, leaving the source copy intact.
    key(1200, 36, 13)
    for i, (code, ch) in enumerate([(46,"M"),(31,"o"),(2,"d"),(0,"a"),(37,"l"),(17,"T"),(14,"e"),(1,"s"),(17,"t")]):
        key(2000+i*100,code,ord(ch))
    key(4000,36,13)
    key(6500,53,27)
(out / "events.txt").write_text("".join(" ".join(map(str,e))+"\n" for e in sorted(events)))
env = {k:v for k,v in os.environ.items() if not k.startswith("OPENGOTHIC_")}
env.update(OPENGOTHIC_PROFILE="1", OPENGOTHIC_UI_PROBE=a.mode, OPENGOTHIC_UI_EVENTS=str(out/"events.txt"))
try:
    with (out/"terminal.log").open("w") as log:
        result = subprocess.run([str(exe),"-g",str(game),"-game:TheChroniclesOfMyrtana.ini","-window","-rt","0","-gi","0","-aa","0","-bl","0","-save","1"],cwd=out,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
    assert result.returncode == 0, f"Game exited {result.returncode}: {out}"
    text = (out/"terminal.log").read_text(errors="replace")
    assert "[ARCHOLOS_PROFILE] frames=900" in text, "Incomplete modal test"
    if a.mode == "journal":
        text = text.split("[MODAL_PROBE] content opened",1)[1].split("[MODAL_PROBE] content closed",1)[0]
        # Require a visible redraw in every phase, not merely an initial top page.
        for code, down in [(125, True), (126, False), (1, True), (13, False), (-1, True)]:
            phase = text.split(f"code={code} down=1" if code>=0 else "code=-1 down=0",1)[1]
            next_phase = re.search(r"\[MODAL_PROBE\] input=\d+ code=(?!"+str(code)+r"(?: |$))",phase)
            if next_phase:
                phase = phase[:next_phase.start()]
            draws = [tuple(map(int,m)) for m in re.findall(r"\[MODAL_PROBE\] draw scroll=(\d+) lines=(\d+) visible=(\d+)",phase)]
            assert any((v<n and offset==n-v) if down else offset==0 for offset,n,v in draws), f"No visible {'last' if down else 'first'} page for code {code}"
    else:
        assert "menu opened=MENU_GAME" in text, "Pause menu did not use the in-game menu"
        assert "menu item=MENUITEM_MAIN_INGAME_SAVEGAME_SAVE" in text, "No Save Game entry in pause menu"
        assert "save name drawn=ModalTest" in text, "Typed save name was not visibly updated"
        assert "save name closed accepted=1 text=ModalTest" in text, "Save name was not accepted"
        saved = out/"save_slot_2.sav"
        assert saved.is_file(), "Save slot 2 was not created"
        with zipfile.ZipFile(saved) as z:
            assert z.testzip() is None
            assert b"ModalTest" in z.read("header"), "Saved header has the wrong name"
            assert z.read("game/quests") and z.read("game/daedalus"), "Save lacks quest/script state"
            with zipfile.ZipFile(save) as source:
                assert source.read("game/quests")==z.read("game/quests"), "Quest state changed across load/save"
                # Locate the serialized HERO NPC reference, then its inventory.
                tag = struct.pack("<IBI",7,1,4)+b"HERO"
                def inventory(archive):
                    state = archive.read("game/daedalus")
                    hero = struct.unpack_from("<I",state,state.index(tag)+len(tag))[0]
                    paths = [n for n in archive.namelist() if n.endswith(f"/npc/{hero}/inventory")]
                    assert len(paths)==1, "This ship-save check expects one active world"
                    return archive.read(paths[0])
                assert inventory(source)==inventory(z), "Player inventory changed across load/save"
finally:
    assert hashlib.sha256(save.read_bytes()).digest()==original, "Original save changed"
print(f"PASS {a.mode}: {out}")
