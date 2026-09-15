"""Exercise Archolos's installed boss UI script through a private save."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
from archolos_test_data import copy_save
import struct
import subprocess
import time
import zipfile

p = argparse.ArgumentParser()
for name in ("executable", "game", "save", "output"):
    p.add_argument("--" + name, required=True, type=Path)
p.add_argument("--mode", choices=("seed", "reload", "event", "event-seed", "event-reload", "event-geometry", "focus-seed", "focus-reload", "legacy-reload", "legacy-seed"), required=True)
p.add_argument("--reject-view", action="store_true", help="Reject a private v4 snapshot with a view pointer into its allocation")
p.add_argument("--size-1024", action="store_true", help="Resize the native macOS content surface to 1024x768")
p.add_argument("--interface-scale", type=float, help="Use a private SystemPack interface multiplier")
p.add_argument("--consumers", action="store_true", help="Inspect status/crafting scale consumers after boss cleanup")
a = p.parse_args()
exe, game, save, out = (getattr(a, name).resolve() for name in ("executable", "game", "save", "output"))
original = hashlib.sha256(save.read_bytes()).digest()
source_hash = original.hex()
executable_hash = hashlib.sha256(exe.read_bytes()).hexdigest()
out.mkdir(parents=True, exist_ok=False)
if a.interface_scale is not None:
    assert 0 < a.interface_scale <= 3
    private_game = out / "game"
    private_game.mkdir()
    for path in game.iterdir():
        if path.name.lower() != "system":
            (private_game / path.name).symlink_to(path, target_is_directory=path.is_dir())
        else:
            target = private_game / path.name
            target.mkdir()
            for item in path.iterdir():
                if item.name.lower() == "systempack.ini":
                    import configparser
                    config = configparser.ConfigParser(strict=False)
                    config.read(item)
                    if not config.has_section("INTERFACE"):
                        config.add_section("INTERFACE")
                    config.set("INTERFACE", "Scale", str(a.interface_scale))
                    with (target / item.name).open("w") as ini:
                        config.write(ini)
                else:
                    (target / item.name).symlink_to(item, target_is_directory=item.is_dir())
    game = private_game
copy_save(save, out / "save_slot_1.sav")
if a.reject_view:
    with zipfile.ZipFile(save) as archive:
        entries = {name: archive.read(name) for name in archive.namelist()}
    data = bytearray(entries["game/compatibility"])
    assert data[:4] == b"\x04\0\0\0", "View rejection requires a v4 boss UI save"
    texture = b"BOSSBAR_BG.TGA"
    at = data.index(struct.pack("<I", len(texture)) + texture)
    pointer = struct.unpack_from("<I", data, at - 4)[0]
    struct.pack_into("<I", data, at - 4, pointer + 4)
    entries["game/compatibility"] = data
    with zipfile.ZipFile(out / "save_slot_1.sav", "w", zipfile.ZIP_DEFLATED) as archive:
        for name, contents in entries.items():
            archive.writestr(name, contents)
(out / "Gothic.ini").write_text("[INTERNAL]\nvidResIndex=0\n")
env = {key: value for key, value in os.environ.items() if not key.startswith("OPENGOTHIC_")}
env.update(OPENGOTHIC_PROFILE="1", OPENGOTHIC_BOSS_UI_PROBE=a.mode)
if a.size_1024:
    env["OPENGOTHIC_BOSS_UI_1024"] = "1"
if a.consumers:
    env["OPENGOTHIC_BOSS_UI_CONSUMERS"] = "1"
if a.mode.startswith(("event", "legacy-")) or a.mode == "reload":
    env["OPENGOTHIC_BOSS_UI_CAPTURE"] = "1"
try:
    with (out / "terminal.log").open("w") as log:
        if a.reject_view:
            process = subprocess.Popen([str(exe), "-g", str(game), "-game:TheChroniclesOfMyrtana.ini",
                "-window", "-rt", "0", "-gi", "0", "-aa", "0", "-bl", "0", "-save", "1"],
                cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 180
                while time.monotonic() < deadline and process.poll() is None:
                    if "loading error:" in (out / "terminal.log").read_text(errors="replace"):
                        break
                    time.sleep(.25)
                trace = (out / "terminal.log").read_text(errors="replace")
                assert "loading error: Invalid compatibility view" in trace, trace[-3000:]
                assert not (out / "save_slot_2.sav").exists()
            finally:
                if process.poll() is None:
                    process.terminate()
                process.wait(timeout=20)
            print(f"PASS reject-view: {out}")
            raise SystemExit(0)
        result = subprocess.run([str(exe), "-g", str(game), "-game:TheChroniclesOfMyrtana.ini",
            "-window", "-rt", "0", "-gi", "0", "-aa", "0", "-bl", "0", "-save", "1"],
            cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=180)
    assert result.returncode == 0, f"Game exited {result.returncode}"
    trace = (out / "terminal.log").read_text(errors="replace")
    if a.mode.startswith("legacy-"):
        with zipfile.ZipFile(save) as archive:
            version = struct.unpack("<I", archive.read("game/compatibility")[:4])[0]
            assert version in (1, 2) and archive.testzip() is None
        assert f"[LEGACY_UI] version={version} views=0 fonts=0" in trace
        assert "[BOSS_UI] reload active=1" in trace
        assert "[LEGACY_UI] retained active=1 bar_valid=1 title_valid=1" in trace
        initial = trace.split("[LEGACY_UI] retained", 1)[0]
        assert "[BOSS_UI] draw texture=BOSSBAR" not in initial
        assert "[BOSS_UI] text=Marvin rect=592,76,95,32" in initial
        if a.mode == "legacy-seed":
            assert "[LEGACY_UI] cleanup active=0" in trace
            assert "[LEGACY_UI] fresh bar_valid=1 destructor_arg=0" in trace
            assert "Internal Exception" not in trace
            fresh = trace.split("[LEGACY_UI] fresh active=1", 1)[1]
            assert "[BOSS_UI] draw texture=BOSSBAR_BG.TGA rect=160,-36,960,119" in fresh
            assert "[BOSS_UI] draw texture=BOSSBAR.TGA rect=200,14,439,19" in fresh
            assert "[BOSS_UI] text=Marvin rect=592,76,95,32" in fresh
            assert "[LEGACY_UI] fresh save requested" in fresh
            with zipfile.ZipFile(out / "save_slot_2.sav") as archive:
                assert archive.testzip() is None and archive.read("game/compatibility")[:4] == b"\x04\0\0\0"
        else:
            assert "[BOSS_UI] draw texture=BOSSBAR" not in trace
            assert not (out / "save_slot_2.sav").exists()
        assert all((out / f"boss-ui-{phase}.png").is_file() for phase in ("full", "half", "cleanup"))
        (out / "manifest.json").write_text(json.dumps({"executable": executable_hash,
            "input_save": source_hash, "mode": a.mode, "version": version,
            "boundary": "Retained script handles, missing historical native view/font metadata; not recovered UI",
            "output_saves": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                             for p in out.glob("save_slot_*.sav")},
            "output_images": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in out.glob("boss-ui-*.png")}}, indent=2) + "\n")
        print(f"PASS legacy state boundary (not visual recovery): {out}")
        raise SystemExit(0)
    if a.mode == "event-geometry":
        assert "[BOSS_UI] event start active=1 state=2 hp=1500" in trace
        assert "[BOSS_UI] event health active=1 hp=750" in trace
        assert "[BOSS_UI] event finish active=1 state=3 dead=1" in trace
        assert "[BOSS_UI] event cleanup active=0" in trace
        assert "[BOSS_UI] XP delta=50" in trace and (out / "boss-ui-xp.png").is_file()
        phases = ("boss-full", "other", "none", "boss-half", "resized", "resized-other", "resized-none", "menu", "resumed", "window-restored", "cleanup")
        assert all((out / f"boss-ui-{phase}.png").is_file() for phase in phases)
        captures = {name: [int(w), int(h), int(y)] for name, w, h, y in re.findall(
            r"\[BOSS_UI\] capture=(\S+) viewport=(\d+),(\d+) focus_y=(-?\d+)", trace)}
        assert all(phase in captures for phase in phases)
        if a.size_1024:
            assert captures["boss-full"][:2] == [1024, 768]
        assert captures["resized"][:2] != captures["boss-full"][:2], "Native resize did not occur"
        assert captures["window-restored"][:2] == captures["boss-full"][:2], "Window size not restored"
        backgrounds = {}
        for phase in phases[:-1]:
            before = trace.split(f"[BOSS_UI] capture={phase} ", 1)[0]
            frame = before.rsplit("[BOSS_UI] draw texture=BOSSBAR_BG.TGA", 1)[1]
            bx, by, bw, bh = map(int, re.search(r"rect=(-?\d+),(-?\d+),(\d+),(\d+)", frame).groups())
            backgrounds[phase] = (bx, by, bw, bh)
            assert bx >= -2 and bx+bw <= captures[phase][0]+2, f"Boss bar clipped horizontally at {phase}"
            assert abs(2*bx+bw-captures[phase][0]) <= 4, f"Boss bar not centered at {phase}"
            title = re.findall(r"\[BOSS_UI\] text=Armored razor rect=(-?\d+),(-?\d+),(\d+),(\d+)", frame)
            assert len(title) == 1, f"Missing or duplicate boss title before {phase}: {title}"
            x, y, width, height = map(int, title[0])
            assert abs(2*x + width - captures[phase][0]) <= 4, f"Boss title not centered at {phase}: {title[-1]}"
            if phase in ("other", "resized-other"):
                fx, fy, fw, fh = map(int, re.findall(r"\[BOSS_UI\] native bar=focus rect=(-?\d+),(-?\d+),(\d+),(\d+)", before)[-1])
                assert fy+fh <= y or y+height <= fy, f"Focus bar overlaps boss title at {phase}"
                fill = re.search(r"draw texture=BOSSBAR.TGA rect=(-?\d+),(-?\d+),(\d+),(\d+)", frame)
                _, bar_y, _, bar_h = map(int, fill.groups())
                _, fill_y, _, fill_h = map(int, re.findall(r"native fill=focus rect=(-?\d+),(-?\d+),(\d+),(\d+)", before)[-1])
                assert fill_y+fill_h <= bar_y or bar_y+bar_h <= fill_y, f"Focus fill overlaps boss fill at {phase}"
        assert all(abs(a-b) <= 4 for a, b in zip(backgrounds["boss-full"], backgrounds["window-restored"])), "Boss bar bounds not restored"
        cleanup = trace.split("[BOSS_UI] event cleanup active=0", 1)[1]
        assert "[BOSS_UI] draw texture=BOSSBAR" not in cleanup and "[BOSS_UI] text=Armored razor" not in cleanup
        assert captures["boss-full"][2] >= captures["boss-full"][1]
        assert 0 < captures["other"][2] < captures["other"][1]
        if a.consumers:
            assert "[UI_CONSUMER] native_status bar=0" in trace, "Recheck status activation boundary"
            assert "[UI_CONSUMER] explicit_status bar=" in trace and "[UI_CONSUMER] crafting open=1" in trace
            assert all((out / f"boss-ui-{phase}.png").is_file() for phase in
                       ("status-native", "status-explicit", "crafting", "consumers-cleanup"))
            exp = re.search(r"draw texture=BAR_EXPSTATUS_BACK.TGA rect=(-?\d+),(-?\d+),(\d+),(\d+)", trace)
            assert exp
            x, y, width, height = map(int, exp.groups())
            viewport = captures["window-restored"][:2]
            assert 0 <= x and x+width <= viewport[0] and 0 <= y and y+height <= viewport[1]
            crafting = re.search(r"view texture=DLG_CHOICE.TGA virtual_rect=(-?\d+),(-?\d+),(\d+),(\d+)", trace)
            assert crafting
            x, y, width, height = map(int, crafting.groups())
            assert 0 <= x and x+width <= 8192 and 0 <= y and y+height <= 8192
            assert "draw texture=DLG_CHOICE.TGA" not in trace, "Recheck unimplemented Render-list boundary"
        assert captures["cleanup"][2] * captures["other"][1] < captures["other"][2] * captures["cleanup"][1]
        opened = re.search(r"geometry menu_open=1 tick=(\d+)", trace)
        closed = re.search(r"geometry menu_close tick=(\d+)", trace)
        assert opened and closed and 0 <= int(closed[1]) - int(opened[1]) <= 50
        assert not (out / "save_slot_2.sav").exists()
        (out / "manifest.json").write_text(json.dumps({"executable": executable_hash,
            "input_save": source_hash, "mode": a.mode, "captures": captures,
            "interface_multiplier": a.interface_scale or 1, "native_1024": a.size_1024,
            "consumers": a.consumers,
            "output_images": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in out.glob("boss-ui-*.png")}}, indent=2) + "\n")
        print(f"PASS geometry event/focus/pause lifecycle (visual layout requires review): {out}")
        raise SystemExit(0)
    if a.mode.startswith("focus-"):
        marker = ("targets=1 null=1 retained_handle=1 removed=1 reuse=1" if a.mode == "focus-seed"
                  else "restart bindings=1 tombstones=1 removed=1")
        assert f"[NPC_FOCUS] {marker}" in trace, trace[-3000:]
        if a.mode == "focus-seed":
            assert "[VIEW_REUSE] installed_delete=1 unregistered=1 raw_address_reuse=1 constructor_calls=0" in trace
            assert "[VIEW_REUSE] destructor_unregistered=1 release_keeps_allocation=1 owner_free=1 raw_reuse=1" in trace
            assert "[DYNAMIC_CALL] integer=1 zero=1 reference=1 instance_to_int_zero=1" in trace
            with zipfile.ZipFile(out / "save_slot_2.sav") as archive:
                assert archive.testzip() is None and archive.read("game/compatibility")[:4] == b"\x04\0\0\0"
        else:
            assert not (out / "save_slot_2.sav").exists()
        (out / "manifest.json").write_text(json.dumps({"executable": executable_hash,
            "input_save": source_hash, "mode": a.mode,
            "output_saves": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                             for p in out.glob("save_slot_*.sav")}}, indent=2) + "\n")
        print(f"PASS {a.mode}: {out}")
        raise SystemExit(0)
    started = {"seed": "synthetic start", "reload": "reload", "event": "event start",
               "event-seed": "event start", "event-reload": "event reload"}[a.mode]
    assert f"[BOSS_UI] {started} active=1" in trace, trace[-3000:]
    background = "[BOSS_UI] draw texture=BOSSBAR_BG.TGA rect=160,-36,960,119"
    full = "[BOSS_UI] draw texture=BOSSBAR.TGA rect=200,14,878,19"
    half = "[BOSS_UI] draw texture=BOSSBAR.TGA rect=200,14,439,19"
    assert background in trace
    if a.mode == "seed":
        assert "[BOSS_UI] synthetic health active=1" in trace
        before, after = trace.split("[BOSS_UI] synthetic health active=1", 1)
        assert full in before and half in after
        assert "[BOSS_UI] synthetic save requested" in trace and (out / "save_slot_2.sav").is_file()
        with zipfile.ZipFile(out / "save_slot_2.sav") as archive:
            assert archive.testzip() is None and archive.read("game/compatibility")[:4] == b"\x04\0\0\0"
    elif a.mode == "reload":
        assert half in trace
        assert "[BOSS_UI] synthetic finish active=0" in trace
        assert trace.count("[BOSS_UI] view removed=") >= 2
        assert "[BOSS_UI] draw texture=" not in trace.split("[BOSS_UI] synthetic finish active=0", 1)[1]
    else:
        if a.mode == "event-reload":
            assert "[BOSS_UI] event reload active=1 state=2 hp=750" in trace
            assert half in trace and "[BOSS_UI] event health" not in trace
        else:
            before, after = trace.split("[BOSS_UI] event health active=1", 1)
            assert full in before and half in after
            assert trace.count("[BOSS_UI] event health active=1") == 1
        if a.mode == "event-seed":
            assert "[BOSS_UI] event save requested" in trace
            assert "[BOSS_UI] event finish" not in trace
            with zipfile.ZipFile(out / "save_slot_2.sav") as archive:
                assert archive.testzip() is None and archive.read("game/compatibility")[:4] == b"\x04\0\0\0"
        else:
            marker = "[BOSS_UI] event finish active=1 state=3 dead=1"
            assert marker in trace
            cleanup = trace.split(marker, 1)[1]
            assert cleanup.count("[BOSS_UI] view removed=") >= 2
            assert "[BOSS_UI] event cleanup active=0" in cleanup
            assert "[BOSS_UI] draw texture=" not in cleanup.split("[BOSS_UI] view removed=", 2)[2]
        phases = ("restored" if a.mode == "event-reload" else "full", "half",
                  "active" if a.mode == "event-seed" else "cleanup")
        assert all((out / f"boss-ui-{phase}.png").is_file() for phase in phases)
finally:
    assert hashlib.sha256(save.read_bytes()).digest() == original, "Source save changed"
output_hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                 for p in (*out.glob("save_slot_*.sav"), *out.glob("boss-ui-*.png"))}
(out / "manifest.json").write_text(json.dumps({"executable": executable_hash,
    "input_save": source_hash, "output_saves": output_hashes}, indent=2) + "\n")
print(f"PASS synthetic {a.mode}: {out}")
