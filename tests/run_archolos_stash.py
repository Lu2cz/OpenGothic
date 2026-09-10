"""Fresh-game loot and real stash reveal/open checks using private run directories."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import zipfile

p=argparse.ArgumentParser()
for name in ["executable","game","output"]:
    p.add_argument("--"+name,type=Path,required=True)
p.add_argument("--save",type=Path)
p.add_argument("--repair-file",type=Path)
p.add_argument("--mode",choices=["fresh","stash","return","repair"],required=True)
a=p.parse_args()
exe,game,out=(x.resolve() for x in [a.executable,a.game,a.output])
save=a.save.resolve() if a.save else None
assert (save is None)==(a.mode=="fresh")
original=hashlib.sha256(save.read_bytes()).digest() if save else None
out.mkdir(parents=True,exist_ok=False)
args=["-nomenu"]
if save:
    shutil.copy2(save,out/"save_slot_1.sav")
    args=["-save","1"]
(out/"Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
env={k:v for k,v in os.environ.items() if not k.startswith("OPENGOTHIC_")}
env.update(OPENGOTHIC_PROFILE="1",OPENGOTHIC_STASH_PROBE="1")
if a.repair_file:
    env["OPENGOTHIC_STASH_REPAIR_FILE"]=str(a.repair_file.resolve())
if a.mode=="repair":
    env["OPENGOTHIC_STASH_KEEP_POSITION"]="1"
if a.mode=="return":
    env["OPENGOTHIC_STASH_RETURN"]="1"
try:
    with (out/"terminal.log").open("w") as log:
        result=subprocess.run([str(exe),"-g",str(game),"-game:TheChroniclesOfMyrtana.ini","-window","-rt","0","-gi","0","-bl","0"]+args,cwd=out,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
    assert result.returncode==0,f"Game exited {result.returncode}: {out}"
    text=(out/"terminal.log").read_text(errors="replace")
    assert "[ARCHOLOS_PROFILE] frames=900" in text,"Incomplete run"
    measured=text.split("[STASH_PROBE] begin",1)[1]
    if a.mode=="fresh":
        chest=measured.split("mob=Q101_WOODCHEST_01 ",1)[1].split("[STASH_PROBE] mob=",1)[0]
        for name,count in [("ITMW_1H_BAU_MACE",4),("ITMW_1H_BAU_KNIFE",1),("ITLSTORCH",2),("ITMI_GOLD",11)]:
            assert f"item={name} count={count}" in chest,f"Missing {name}: {out}"
        assert "item=ITMIS_Q101_VRAZKACHEST count=1" in measured,"Stash box missing"
    if a.mode in ("stash","return"):
        for expected in ["end flag=2","focus=Examine the boards","chest_ui=2"]:
            assert expected in measured,f"Missing {expected}: {out}"
        y=float(re.search(r"\[STASH_PROBE\] boards y=([-\d.]+)",measured)[1])
        assert y>-2000,"Boards still hidden under floor"
    if a.mode=="return":
        for expected in ["box_taken=1","box_after_return=0","followup_created=1"]:
            assert expected in measured,f"Missing {expected}: {out}"
    for bad in ["Internal Exception","translation failure","not implemented call [MOB_CREATEITEMS]"]:
        assert bad not in measured,f"{bad}: {out}"
    with zipfile.ZipFile(out/"save_slot_2.sav") as z:
        assert z.testzip() is None

finally:
    if save:
        assert hashlib.sha256(save.read_bytes()).digest()==original,"Source save changed"
print(f"PASS {a.mode}: {out}")
