# Recreate the Archolos workspace

Recorded **15 October 2026**, before temporarily removing the local project.
This is a reconstruction guide, not a backup. Nothing was deleted or moved when
this guide was written. Start here rather than replaying old investigation commands.

## 1. Before removing local files

Source, fixes, runners and project instructions are published in GitHub. **Personal
saves, the 28 regression checkpoints, exact signed apps and private diagnostics are
not. Deleting their only copy loses them; a Git clone cannot restore them.**

Keep a private backup on an external drive or other storage that will survive the
removal. Preserve the following:

| Material | Why |
|---|---|
| `work/playable` in its entirety | Campaign saves, settings and any local achievement/progress files. |
| Inputs and sidecars in [retained-checkpoints.md](retained-checkpoints.md) | One input per scenario; approximately 0.694 GB. Several cannot be regenerated exactly from a new game. |
| `work/ArcholosFast.app`, `work/ArcholosProfile.app` | Exact working/diagnostic executables; rebuilding need not reproduce their binary hashes. |
| `work/issue37-deployment-20260926-v1/rollback` | The single full rollback, including compatible saves/configuration. |
| Investigation exceptions in the retained-checkpoint inventory | Exact comparison apps, loading-stall sample and unresolved intake evidence. |
| `outputs` and workspace `AGENTS.md` | Private provenance, installation/retention manifests, unique historical probe patches and launcher. |
| `work/archolos-game`, **or** matching offline game/voice installers and all companion `.bin` files | Exact 1.2.11 data may not remain downloadable indefinitely. Game data is approximately 7.18 GB. |

The simplest backup is a verified copy of the entire coordination workspace to
external storage. Source checkouts under `/Users/lu2/projects` are reconstructible
only after checking for unpublished changes in the root repository and submodules.
At recording time the working source was clean apart from generated
`build-archolos.log` and `tests/__pycache__/`; its HEAD was published as
`b2c4204001695751b4206ede36578877efa067b0`.

Alternatively, this archive captures the private workspace while excluding the
re-downloadable game, reference source, Python packages and source-checkout links.
**It does not back up game data or installers; preserve one of those separately.**
Set these variables in the Terminal session used for the commands below. Replace
the backup location with a real external destination; it must not be inside a
directory you intend to remove. Commands assume `rtk` is available.

```sh
ARCHOLOS_WORKSPACE="/Users/lu2/Documents/Codex/2026-09-09/https-github-com-try-opengothic-issues"
ARCHOLOS_SOURCE="/Users/lu2/projects/OpenGothic-v092"
ARCHOLOS_BACKUP="/Volumes/YOUR-BACKUP-DRIVE/Archolos"

rtk proxy mkdir -p "$ARCHOLOS_BACKUP"
rtk proxy tar -czf "$ARCHOLOS_BACKUP/archolos-private.tgz" \
  --exclude='work/archolos-game' \
  --exclude='work/tcom-decompilation' \
  --exclude='work/python-deps' \
  --exclude='work/OpenGothic' \
  --exclude='work/OpenGothic-v092' \
  -C "$ARCHOLOS_WORKSPACE" work outputs AGENTS.md
rtk proxy shasum -a 256 "$ARCHOLOS_BACKUP/archolos-private.tgz"
rtk proxy tar -tzf "$ARCHOLOS_BACKUP/archolos-private.tgz"
```

Use a new archive filename if a backup already exists. Save the checksum alongside
the archive. Before deleting originals, test extraction into an empty directory
on the external storage and compare the restored saves/checkpoints and executables
against the originals with SHA-256. Listing an archive alone does not verify its
contents. Keep backups private; do not commit saves, installers, assets or raw logs.

## 2. Versions and sources to restore

The source snapshot is tagged
[`archolos-workspace-2026-10-15`](https://github.com/Lu2cz/OpenGothic/tree/archolos-workspace-2026-10-15).
It includes this guide; its engine code is unchanged from the published revision
above. The working branch remains `archolos/performance-v092`, based on OpenGothic
v0.92. Do not replace it with upstream master or the original v0.92 release binary:
neither contains this fork's complete set of fixes.

| Component | Recorded version / revision |
|---|---|
| Installed game | **Archolos 1.2.11**, GOG payload, English scripts/text. GOG game ID `1565201708`, recorded build ID `58625296474579151`. |
| Dubbing | Separate **1.2.11 Polish Voice-Over Pack**; six `KM_Speech1PL.mod` through `KM_Speech6PL.mod` archives. |
| Script runtime | Installed scripts detect Ikarus v10202 and LeGo 2.7.0; these are included in the game data, not separate packages to install. |
| Engine Tempest submodule | `Lu2cz/Tempest`, `fb9fa22d2e66fd350ca93cbef4a9cfca05ed5669`. |
| Engine ZenKit submodule | `Lu2cz/ZenKit`, `6fa71bfbd8f3c349be59bbc485f3e7bac7fa3961`. |
| Reference decompilation | [`lmichaelis/tcom-decompilation`](https://github.com/lmichaelis/tcom-decompilation), `41bc827a429adbc7a167e08910e9374a455017b0`, **script version 1.2.7**. |
| Optional Python inspection packages | `zenkit==1.3.0.4`, `pefile==2024.8.26`; Python 3.13.2 was used. This Python ZenKit package is separate from the engine's C++ submodule. |

The decompilation is a reading/reference aid, contains errors and is not intended
to compile. **Do not replace the installed 1.2.11 scripts with its 1.2.7 source.**
Confirm behavior against the installed bytecode when the two differ.

## 3. Restore source and private workspace

Use the original paths above to keep historical tools/path references usable.
These commands are for missing/new directories, not an existing live checkout.
One standalone engine clone is enough; it does not depend on the former separate
`/Users/lu2/projects/OpenGothic` repository or its shared worktree metadata.

```sh
rtk proxy mkdir -p /Users/lu2/projects "$ARCHOLOS_WORKSPACE/work" "$ARCHOLOS_WORKSPACE/outputs"
rtk proxy git clone --recurse-submodules --branch archolos-workspace-2026-10-15 \
  https://github.com/Lu2cz/OpenGothic.git "$ARCHOLOS_SOURCE"
rtk proxy git -C "$ARCHOLOS_SOURCE" switch -c archolos/performance-v092
rtk proxy git -C "$ARCHOLOS_SOURCE" branch --set-upstream-to=origin/archolos/performance-v092
rtk proxy git -C "$ARCHOLOS_SOURCE" remote add upstream https://github.com/Try/OpenGothic.git
rtk proxy git -C "$ARCHOLOS_SOURCE" submodule status --recursive
```

The recursive clone fetches all pinned dependencies, including their nested
submodules. Do not use `git submodule update --remote`. Do not pull a newer branch
revision before recovering and checking the recorded setup; upstream migration is
[issue #13](https://github.com/Lu2cz/OpenGothic/issues/13).

Restore the private archive only into the otherwise fresh workspace, then recreate
the legacy source alias. If no archive exists, create `work/playable` and a workspace
`AGENTS.md` pointing to the restored source's `AGENTS.md`; campaign/checkpoint data
will have to be rebuilt through play or documented instrumented setup.

```sh
rtk proxy tar -xzf "$ARCHOLOS_BACKUP/archolos-private.tgz" -C "$ARCHOLOS_WORKSPACE"
rtk proxy ln -s "$ARCHOLOS_SOURCE" "$ARCHOLOS_WORKSPACE/work/OpenGothic-v092"
```

Do not restore old `.git` worktree pointer files or old CMake caches into this clone.
If using different paths, adjust the launcher and new test commands; historical
absolute paths in provenance are records, not instructions to overwrite files.

## 4. Restore the game and Polish voices

Prefer copying a verified backup of `work/archolos-game` to the same location.
Otherwise obtain the GOG offline base installer and the matching
[Polish Voice-Over Pack](https://www.gog.com/en/game/the_chronicles_of_myrtana_archolos_polish_voiceover_pack).
Keep each `.exe` beside all of its companion `.bin` parts. Only pass the `.exe` to
`innoextract`; it reads the parts automatically. Original installer filenames and
installer checksums were not retained. The original filename misleadingly mentioned
Ukrainian; the extracted metadata and scripts were English. Verify the payload,
rather than relying on the filename or assuming the latest release is identical.

Install Homebrew if absent using its official instructions. With Apple's Command
Line Tools available, install the required tools:

```sh
rtk proxy brew install cmake glslang innoextract python@3.13
rtk proxy xcrun --show-sdk-version
```

If the SDK check fails because the Command Line Tools are missing, run
`rtk proxy xcode-select --install`, complete the system dialog, then retry.
Full Xcode and Windows/Wine are not needed for this extraction/native build.
Observed tools were CMake 4.4.3, glslang 16.5.0, innoextract 1.9, macOS SDK 15.5,
on an Apple M2 running macOS 26.5.2. A different toolchain can produce different
binary hashes. Allow roughly 15–20 GB temporarily for extraction, copies and builds.

For installer recovery, extract into separate fresh staging directories. Replace
the two installer paths below with the files you actually downloaded.

```sh
rtk proxy innoextract --extract --exclude-temp --language en-US \
  --output-dir "$ARCHOLOS_WORKSPACE/work/installer-base" "/path/to/base-installer.exe"
rtk proxy innoextract --extract --exclude-temp \
  --output-dir "$ARCHOLOS_WORKSPACE/work/installer-voices" "/path/to/polish-voice-installer.exe"
rtk proxy ditto "$ARCHOLOS_WORKSPACE/work/installer-base/Data" "$ARCHOLOS_WORKSPACE/work/archolos-game/Data"
rtk proxy ditto "$ARCHOLOS_WORKSPACE/work/installer-base/system" "$ARCHOLOS_WORKSPACE/work/archolos-game/system"
rtk proxy ditto "$ARCHOLOS_WORKSPACE/work/installer-base/_work" "$ARCHOLOS_WORKSPACE/work/archolos-game/_work"
rtk proxy cp "$ARCHOLOS_WORKSPACE/work/installer-base/vdfs.cfg" "$ARCHOLOS_WORKSPACE/work/archolos-game/vdfs.cfg"
rtk proxy cp "$ARCHOLOS_WORKSPACE/work/installer-base/goggame-1565201708.info" "$ARCHOLOS_WORKSPACE/work/archolos-game/goggame-1565201708.info"
rtk proxy python3 - "$ARCHOLOS_WORKSPACE" <<'PY'
from pathlib import Path
import shutil, sys
work = Path(sys.argv[1]) / "work"
for i in range(1, 7):
    name = f"KM_Speech{i}PL.mod"
    matches = list((work / "installer-voices").rglob(name))
    assert len(matches) == 1, (name, matches)
    shutil.copy2(matches[0], work / "archolos-game/Data" / name)
PY
```

The known installer places `Data`, `system` and `_work` at its extraction root.
If another installer layout differs, locate those directories before copying.
Windows redistributable installers, GOG clients and Windows DLL integrations do
not need to be installed. Keep the supplied DLLs/EXE as data/reference material.

Create the physical script directory even though the compiled script is archived.
Without it OpenGothic's initial path validation failed in the original setup.

```sh
rtk proxy mkdir -p "$ARCHOLOS_WORKSPACE/work/archolos-game/_work/Data/Scripts/_compiled"
```

Verify the recorded assets with [workspace-assets.sha256](workspace-assets.sha256).
It lists filenames and SHA-256 only, no proprietary content. Finder `.DS_Store`
files and the empty compiled-script directory are intentionally excluded.

```sh
rtk proxy /bin/zsh -c 'cd "$1" && rtk proxy shasum -a 256 -c "$2"' _ \
  "$ARCHOLOS_WORKSPACE/work/archolos-game" \
  "$ARCHOLOS_SOURCE/docs/archolos/workspace-assets.sha256"
```

In particular, `Data/KM_ScriptsEN.mod` must hash to
`cb7025bed2f298c537f9dd3c7aff2b70f3e0169f2c01591d05738261523797b1`.
Do not silently mix a newer script archive with old compatibility snapshots.
The small shipped configuration is `[SETTINGS] World=ARCHOLOS_MAINLAND.ZEN` in
`system/TheChroniclesOfMyrtana.ini`; SystemPack's recorded debug values are
`OptimizeVB=0` and `SteamOverlayFix=0`. Retain the original INIs rather than
inventing a new Gothic installation configuration.

After all fingerprints match, the two installer staging directories can be removed;
keep the original offline installers on backup storage. If the hashes differ,
investigate the version/language before loading private campaign saves.

## 5. Restore reference scripts and inspection tools

These are optional for playing; they are useful for continuing diagnosis.

```sh
rtk proxy git clone https://github.com/lmichaelis/tcom-decompilation.git \
  "$ARCHOLOS_WORKSPACE/work/tcom-decompilation"
rtk proxy git -C "$ARCHOLOS_WORKSPACE/work/tcom-decompilation" checkout \
  41bc827a429adbc7a167e08910e9374a455017b0
rtk proxy python3.13 -m pip install --target "$ARCHOLOS_WORKSPACE/work/python-deps" \
  zenkit==1.3.0.4 pefile==2024.8.26
rtk proxy env PYTHONPATH="$ARCHOLOS_WORKSPACE/work/python-deps" python3.13 -c \
  'import zenkit, pefile; print(zenkit.__file__); print(pefile.__version__)'
```

This installs into the project, not the global Python environment. Use native
ARM64 Python on this Mac. The recorded ZenKit wheel's file named
`zenkit/native/osx-x64.dylib` is actually ARM64 despite its name; it matched the
package's original RECORD hash and imported successfully. Do not replace it with
the engine's ZenKit library: the Python binding has its own C API/ABI.

## 6. Restore or rebuild the apps

If the backed-up apps were restored, use those for the exact previous binaries.
The signed playable executable's recorded SHA-256 is
`e7a1752aec3984a4168ae72e864a0c9b9963057e1f001455ade10e0cdd566731`.
Its runtime came from `f395d778ac91e40991b5f62f322920ec0544c781`, merged as
`bab501a881952de87d39695185e0e11e56c9a4b6`. The restoration tag includes the same
runtime plus later documentation. The backed-up Profile app is an older diagnostic
baseline; do not assume its hash/runtime equals Fast.

To rebuild, configure a **fresh** build directory. The policy compatibility flag
is needed by older dependencies with modern CMake; the recorded cache used 3.5.

```sh
rtk proxy cmake -S "$ARCHOLOS_SOURCE" -B "$ARCHOLOS_SOURCE/build" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DTEMPEST_BUILD_METAL=ON -DTEMPEST_BUILD_VULKAN=OFF
rtk proxy cmake --build "$ARCHOLOS_SOURCE/build" --target Gothic2Notr --parallel 4
```

The output is `build/opengothic/Gothic2Notr`. Build output is not an automatically
validated replacement for a backed-up working app. Rebuilds/signing can change
whole-file hashes; record their new provenance instead of claiming byte identity.

If **neither** Fast nor Profile has been restored, this constructs both bundles
from that build. It refuses existing destinations. These two newly built apps use
the same runtime, unlike the historical Fast/Profile pair.

```sh
rtk proxy python3.13 - "$ARCHOLOS_SOURCE" "$ARCHOLOS_WORKSPACE" <<'PY'
from pathlib import Path
import plistlib, shutil, subprocess, sys
source, workspace = map(Path, sys.argv[1:])
binary = source / "build/opengothic/Gothic2Notr"
assert binary.is_file(), binary
apps = [(workspace / f"work/Archolos{name}.app", name.lower())
        for name in ("Fast", "Profile")]
assert all(not app.exists() for app, _ in apps), "Preserve existing apps"
for app, identity in apps:
    macos = app / "Contents/MacOS"
    macos.mkdir(parents=True)
    shutil.copy2(binary, macos / "Gothic2Notr")
    info = dict(CFBundleDisplayName="Archolos", CFBundleExecutable="Gothic2Notr",
                CFBundleIdentifier=f"org.opengothic.archolos.{identity}",
                CFBundleName="Archolos", CFBundlePackageType="APPL",
                CFBundleShortVersionString="0.92", NSHighResolutionCapable=True)
    (app / "Contents/Info.plist").write_bytes(plistlib.dumps(info))
    subprocess.run(["codesign", "--force", "--deep", "--sign", "-", str(app)], check=True)
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(app)], check=True)
PY
```

## 7. Launcher, checks and resuming work

Restoring the private archive restores `outputs/Play-Archolos.command`. With the
original paths, the normal launch command remains:

```sh
rtk proxy /bin/zsh "$ARCHOLOS_WORKSPACE/outputs/Play-Archolos.command"
```

If no launcher was backed up, create it as follows. This portable replacement
derives the workspace from its own location, clears diagnostic environment variables,
opens the menu and permits ordinary saving/loading. It refuses to overwrite a file.

```sh
rtk proxy python3.13 - "$ARCHOLOS_WORKSPACE" <<'PY'
from pathlib import Path
import sys
workspace = Path(sys.argv[1])
(workspace / "work/playable").mkdir(parents=True, exist_ok=True)
launcher = workspace / "outputs/Play-Archolos.command"
launcher.parent.mkdir(parents=True, exist_ok=True)
assert not launcher.exists(), "Keep the restored launcher"
launcher.write_text('''#!/bin/zsh
rtk proxy python3 - "$0" <<'PYTHON'
import os, sys
from pathlib import Path
workspace = Path(sys.argv[1]).resolve().parents[1]
for key in list(os.environ):
    if key.startswith("OPENGOTHIC_"):
        os.environ.pop(key)
os.chdir(workspace / "work/playable")
exe = str(workspace / "work/ArcholosFast.app/Contents/MacOS/Gothic2Notr")
os.execv(exe, [exe, "-g", str(workspace / "work/archolos-game"),
              "-game:TheChroniclesOfMyrtana.ini", "-window",
              "-rt", "0", "-gi", "0", "-bl", "0"])
PYTHON
''')
launcher.chmod(0o755)
PY
```

Keep **`-bl 0`**: this selects the non-bindless path that removed the original
approximately 12 FPS slowdown on the M2. The launcher intentionally has no `-save`
argument. Saves/settings follow the working directory, so tests must never use
`work/playable` as their output. Fullscreen can be selected after launch.

Before resuming the campaign:

1. Check the engine/submodule pins and all asset fingerprints; verify app signatures.
2. Check restored private save/checkpoint hashes against the backup and run ZIP CRC
   checks. Keep the paired rollback intact. Public inventory alone cannot recreate
   the private inputs or state lost in earlier saves.
3. Test a **private copy** with the relevant `tests/run_archolos_*.py` runner and a
   fresh `--output`; consult its `--help` and the linked scenario guide. Recheck
   the original source hash after testing. A successful build/import is not a
   gameplay or save-compatibility pass.
4. Check menu/audio, load a copied save, movement, normal saving and process restart.
   Do not overwrite the only campaign save to test a rebuilt engine.
5. Read [AGENTS.md](../../AGENTS.md), [the roadmap](https://github.com/Lu2cz/OpenGothic/issues/1),
   and the assigned issue. Follow the [storage policy](storage-retention.md): one
   useful input per scenario, one full rollback, no accumulation of completed runs.

No new campaign progress is established by restoring this workspace. The README
and issue tracker retain the last verified progress and remaining work.

## Verification of this guide

Checked the published source/reference revisions, local package metadata and native
architecture, existing launcher/configuration and installer listing. Asset hashes
were recorded from the installed data. Fresh CMake configuration and a fresh
temporary installation/import of the pinned Python packages were checked on this Mac.
The actual offline installers are no longer present for a new extraction test.
The documented bundle/launcher creation was checked in temporary directories.
The archive exclusions were checked with dummy files, not a real backup transfer.
No full rebuild, game run, backup transfer or removal of project data was performed.
