# Archolos on macOS

A fork of [OpenGothic](https://github.com/Try/OpenGothic) working toward a complete,
natively playable **The Chronicles of Myrtana: Archolos** campaign on macOS.
The current target is an Apple M2 Mac, using Metal and Archolos 1.2.11 game data.

- [Roadmap and goal](https://github.com/Lu2cz/OpenGothic/issues/1)
- [Planning board](https://github.com/users/Lu2cz/projects/1)
- [Issues and verification](https://github.com/Lu2cz/OpenGothic/issues)
- [Persistent project instructions](AGENTS.md)
- [Build, local test setup and launch](docs/archolos/environment.md)
- [Initial LeGo/Ikarus coverage audit](docs/archolos/lego-ikarus-audit.md)
- [Historical fixes and evidence](ARCHOLOS_DEVELOPMENT.md)

## Campaign progress

**Furthest story sequence reached and tested: early Chapter 1, the forest
conversation with Jorn and Fabio about going to the village or staying in the cave.**
The opening ship quest, captain/departure sequence and arrival on the beach have
also been exercised. Story progression into Silbach and beyond is not yet verified.

Separately, a **synthetic Chapter 2 checkpoint** has been used to explore Archolos
city and its surroundings, including walking outside the city, NPC interactions,
music and performance. It skips ahead using the game's story helper; it does not
establish that Chapter 1 can be completed or that Chapter 2 quests work.

These are results from individual playtests and targeted scenarios across development
builds. **A continuous clean playthrough and campaign completion remain unverified.**
Progress as of 11 September 2026; new chapter evidence belongs in
[the campaign tracker](https://github.com/Lu2cz/OpenGothic/issues/14).

## What works in tested scenarios

- Playable movement and basic combat, with the original severe performance problem
  addressed by `-bl 0`. Controlled ship performance improved from about 12 to 60 FPS;
  limited city samples also reached about 60 FPS. This is not an everywhere guarantee.
- Polish dialogue audio, native Archolos music with day/night/combat changes, and
  menu music without the original Gothic soundtrack playing over it.
- Recipe reading, journal entries and scrolling, cooking, XP notices, and menu saving.
- Ship barriers, ordinary lockpicking and the Open Lock spell (including combined
  partial progress), stash discovery, corrected fresh container loot, and tested
  opening cutscene progression and speaker labels. See [lockpick verification](docs/archolos/lockpick-regression.md).
- Inventory/quest save restoration, plus Ikarus heap and delayed-callback persistence
  in the tested save/restart scenarios.
- Shared NPC queue waits and dialogue processing, with normal-duration captain/forest
  and pending-wait save/reload checks. See [coverage and remaining camera limitations](docs/archolos/ai-waittillend.md).
- City ↔ sewers transitions, rendered destination movement, inventory/quest and
  partial-lock continuity, with callback/reference restart checks. See
  [world-transition coverage and synthetic setup](docs/archolos/world-transitions.md).

The remaining work includes broader NPC/item focus access, additional world/campaign
coverage, cutscene camera framing, essential boss/buff UI and campaign
validation. See [the roadmap](https://github.com/Lu2cz/OpenGothic/issues/1) for priorities
and acceptance criteria. Full LeGo/Ikarus or Windows DLL compatibility is not claimed.

## Build and play

Use the [build, test and launch guide](docs/archolos/environment.md). The original
Archolos game files and voice pack must be supplied separately; this repository
contains engine code and tests, not game assets or personal saves.

Working branch: `archolos/performance-v092`, based on OpenGothic v0.92.
Baseline tag: `archolos-baseline-2026-09-11`. Modified dependencies are pinned to
[Tempest](https://github.com/Lu2cz/Tempest) and [ZenKit](https://github.com/Lu2cz/ZenKit)
forks so a recursive clone retrieves the required fixes. Updating to newer upstream
is [separate work](https://github.com/Lu2cz/OpenGothic/issues/13).

Existing saves may contain omissions from earlier builds. New persistence saves
retain the state currently present; they cannot reconstruct previously lost state.
Use private copies for testing and preserve story saves.

## Development

Work is organized as one substantial issue per task and isolated branch/worktree.
Read [AGENTS.md](AGENTS.md) before contributing. Issues hold current findings and
verification; the historical development log is archived.

Cursor changes, Steam/GOG/Discord integration and cosmetic menu fidelity are deferred.
Original Windows debug/renderer hooks are not required goals; gameplay UI remains in scope.

Built on OpenGothic and its dependencies. See [LICENSE](LICENSE) and the respective
dependency licenses. This is an independent compatibility fork, not an official
Archolos release.
