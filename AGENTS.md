# Local Archolos development

- Commit changes locally. Do not push, open pull requests, or submit anything upstream unless the user explicitly requests it.
- When giving launch instructions, provide one pasteable Terminal command, not the launcher script body:
  `rtk proxy /bin/zsh "/Users/lu2/Documents/Codex/2026-09-09/https-github-com-try-opengothic-issues/outputs/Play-Archolos.command"`
- Prefix shell commands with `rtk`; use `rtk proxy` for commands without a filter.
- Preserve the original game data, installers, and playtest save. Run benchmarks using independent save copies, without concurrent builds or other benchmark runs.
- Ship bars and the reproduced Willem dialogue-exit failure are fixed locally. Next gameplay priorities: learned-recipe journal generation, journal scrolling, and normal story progression. The user has confirmed that loading a save restores quest progression and inventory.

- Read `ARCHOLOS_DEVELOPMENT.md` before continuing. Update it at verified milestones with cause, change, tests, limitations, and next steps; keep the user-facing status report in sync. Ship-bar and dialogue/XP-notification regression checks are available under tests/.

- Cursor changes were reverted after the user reported broken mouse look. Leave cursor behavior alone until explicitly requested again.
