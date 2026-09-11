"""Reproduce Open Lock on a supplied pre-chest save; all writes use private copies."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import struct
import sys
import time
import zipfile

p = argparse.ArgumentParser()
for name in ["executable", "game", "save", "output"]:
    p.add_argument("--"+name, type=Path, required=True)
p.add_argument("--mode", choices=["cast","partial","spell-partial","ordinary","ordinary-reload","reload"], default="cast")
p.add_argument("--expect-hybrid", action="store_true", help="Require the hybrid achievement branch from an existing partial lock")
p.add_argument("--setup-chest", action="store_true", help="Privately position at Q101 chest and supply the Open Lock scroll/mana")
p.add_argument("--reject", choices=["address", "truncated", "fingerprint"], help="Verify rejection of damaged private compatibility data")
p.add_argument("--world-state-only", action="store_true", help="Test native world progress independently of the compatibility heap, in the private copy")
a = p.parse_args()
exe, game, save, out = (x.resolve() for x in [a.executable,a.game,a.save,a.output])
original = hashlib.sha256(save.read_bytes()).digest()
out.mkdir(parents=True,exist_ok=False)
shutil.copy2(save,out/"save_slot_1.sav")
if a.reject or a.world_state_only:
    with zipfile.ZipFile(save) as z:
        entries = {n:z.read(n) for n in z.namelist()}
    if a.world_state_only:
        assert not a.reject, "World-only and rejection checks are separate scenarios"
        del entries["game/compatibility"]
    else:
        data = bytearray(entries["game/compatibility"])
        assert struct.unpack_from("<I",data)[0]==2, "Rejection test requires a v2 lock save"
        if a.reject=="address":
            # v2 ends in (mobsiId,address,progress) records. Damage the last address.
            struct.pack_into("<I",data,len(data)-8,0xfffffff8)
        elif a.reject=="truncated":
            data = data[:-1]
        else:
            data[4] ^= 1
        entries["game/compatibility"] = data
    with zipfile.ZipFile(out/"save_slot_1.sav","w",zipfile.ZIP_DEFLATED) as z:
        for name,contents in entries.items():
            z.writestr(name,contents)
(out/"Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
env = {k:v for k,v in os.environ.items() if not k.startswith("OPENGOTHIC_")}
env.update(OPENGOTHIC_PROFILE="1",OPENGOTHIC_LOCK_PROBE=a.mode)
if a.setup_chest:
    env["OPENGOTHIC_LOCK_FIXTURE"]="1"
try:
    with (out/"terminal.log").open("w") as log:
        command = [str(exe),"-g",str(game),"-game:TheChroniclesOfMyrtana.ini","-window","-rt","0","-gi","0","-bl","0","-save","1"]
        record = {"command":command,"runner":[sys.executable]+sys.argv,"mode":a.mode,"setup_chest":a.setup_chest,"source":str(save),
                  "source_sha256":original.hex(),"executable_sha256":hashlib.sha256(exe.read_bytes()).hexdigest()}
        (out/"run.json").write_text(json.dumps(record,indent=2)+"\n")
        if a.reject:
            # A rejected load returns to the menu, so there is no profile exit.
            # Stop only after the explicit rejection, never while writing a save.
            process = subprocess.Popen(command,cwd=out,env=env,stdout=log,stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic()+180
                trace = ""
                while time.monotonic()<deadline:
                    trace = (out/"terminal.log").read_text(errors="replace")
                    if "loading error:" in trace or process.poll() is not None:
                        break
                    time.sleep(.25)
                expected = {"address":"Invalid compatibility lock binding", "truncated":"unable to read save-game file",
                            "fingerprint":"Incompatible script/compatibility snapshot"}[a.reject]
                assert "loading error: "+expected in trace, f"Missing rejection: {out}"
                assert "[LOCK_PROBE]" not in trace and not (out/"save_slot_2.sav").exists()
            finally:
                if process.poll() is None:
                    process.terminate()
                process.wait(timeout=20)
            record.update(game_exit_code=process.returncode,result="PASS reject-"+a.reject)
            (out/"run.json").write_text(json.dumps(record,indent=2)+"\n")
            print(f"PASS reject-{a.reject}: {out} (menu terminated after confirmed load rejection)")
            raise SystemExit(0)
        result = subprocess.run(command,cwd=out,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
        print(f"Game exit code: {result.returncode}")
        record["game_exit_code"] = result.returncode
        (out/"run.json").write_text(json.dumps(record,indent=2)+"\n")
        assert result.returncode==0, f"Game exited {result.returncode}: {out}"
    text = (out/"terminal.log").read_text(errors="replace")
    assert "[ARCHOLOS_PROFILE] frames=900" in text, "Incomplete run"
    measured = text.split("[LOCK_PROBE]",1)[1]
    if a.mode in ("cast", "reload"):
        assert "unarmed focus=Chest mob=Q101_CHEST_01" in measured, "Wrong reproduction position/chest"
    if a.mode=="cast":
        for expected in ["spell=103 focus=Chest mob=Q101_CHEST_01", "away_rejected=1", "no_mana_rejected=1", "unlocked_rejected=1", "target_changed_rejected=1", "cast input held", "complete cracked=1 scroll_used=1 mana_used=1", "chest_ui=2"]:
            assert expected in measured, f"Missing {expected}: {out}"
        with zipfile.ZipFile(out/"save_slot_2.sav") as z, zipfile.ZipFile(save) as source:
            assert z.testzip() is None
            assert b"Open Lock test" in z.read("header")
            assert z.read("game/quests")==source.read("game/quests"), "Unlock unexpectedly changed existing quests"
    elif a.mode=="ordinary":
        for expected in ["ordinary target=Q101_CHEST_01", "ordinary failed_without_break=1", "ordinary broken=1", "ordinary cracked=1 chest_ui=2",
                         "focus locked_unlocked=1 null=1 target_change=1 address_reuse=1 cleared=1",
                         "hook success=0 broken_open=0", "hook success=0 broken_open=1",
                         "hook success=1 broken_open=0", "hook success=1 broken_open=1"]:
            assert expected in measured, f"Missing {expected}: {out}"
        with zipfile.ZipFile(out/"save_slot_2.sav") as z:
            assert z.testzip() is None
            assert b"Ordinary lockpick test" in z.read("header")
    elif a.mode=="ordinary-reload":
        assert "ordinary reload cracked=1 chest_ui=2" in measured, f"Reload lost ordinary unlock: {out}"
        assert "restored ownership=1 stale_bytes=0" in measured, "Lost virtual lock ownership"
        with zipfile.ZipFile(out/"save_slot_2.sav") as z:
            assert z.testzip() is None
    elif a.mode=="partial":
        assert "partial progress=1 cracked=0" in measured, "Ordinary partial progress missing"
        assert "hook success=1 broken_open=0 before=0 partial=0" in measured, "Incorrect partial hook state"
        with zipfile.ZipFile(out/"save_slot_2.sav") as z:
            assert z.testzip() is None
            assert any(z.read(n)==struct.pack("<I",1) for n in z.namelist() if n.endswith("/lockpick-progress")), "World storage lost partial progress"
    elif a.mode=="spell-partial":
        assert "spell_partial progress=1 cracked=0 scroll_used=0" in measured, "Spell partial progress or cancellation failed"
        with zipfile.ZipFile(out/"save_slot_2.sav") as z:
            assert z.testzip() is None
    else:
        assert "reload cracked=1 chest_ui=2" in measured, "Reload lost unlock or chest access"
        with zipfile.ZipFile(out/"save_slot_2.sav") as z:
            assert z.testzip() is None
            assert struct.unpack_from("<I",z.read("game/compatibility"))[0]==2, "Save did not upgrade to v2"
    assert "Internal Exception" not in measured, "Script exception during tested flow"
    assert "translation failure" not in measured, "Unmapped memory during tested flow"
    if a.expect_hybrid:
        expected = "hybrid achievement=1" if a.mode=="cast" else "broken_open=1 before=2 partial=1"
        assert expected in measured, "Hybrid achievement branch not reached"
    elif a.mode=="ordinary":
        assert "broken_open=1 before=2 partial=0" in measured, "Fresh ordinary lock falsely marked hybrid"
    assert not (out/"save_slot_2.sav.tmp").exists(), "Save was not finalized"
finally:
    assert hashlib.sha256(save.read_bytes()).digest()==original, "Source save changed"
record["result"] = "PASS "+a.mode
(out/"run.json").write_text(json.dumps(record,indent=2)+"\n")
print(f"PASS {a.mode}: {out}")
