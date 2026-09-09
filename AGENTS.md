# Local Archolos development

- Commit changes locally. Do not push, open pull requests, or submit anything upstream unless the user explicitly requests it.
- When giving launch instructions, provide one pasteable Terminal command, not the launcher script body:
  `rtk proxy /bin/zsh "/Users/lu2/Documents/Codex/2026-09-09/https-github-com-try-opengothic-issues/outputs/Play-Archolos.command"`
- Prefix shell commands with `rtk`; use `rtk proxy` for commands without a filter.
- Preserve the original game data, installers, and playtest save. Run benchmarks using independent save copies, without concurrent builds or other benchmark runs.
- Ship bars and the reproduced Willem dialogue-exit failure are fixed locally. The user confirmed recipe learning/document generation and XP notices. Journal scrolling required a second fix in the macOS modal event loop; use tests/run_archolos_modal.py, not the removed detached-dialog test. The same runner tests actual pause-menu saving and can re-save its own output to check reloads. Next gameplay priorities: normal opening-quest progression, actual cooking, and quest-triggered gate opening. The user has confirmed that loading a save restores quest progression and inventory.

- Read `ARCHOLOS_DEVELOPMENT.md` before continuing. Update it at verified milestones with cause, change, tests, limitations, and next steps; keep the user-facing status report in sync. Ship-bar, dialogue/XP-notification, recipe, and journal-scrolling regression checks are available under tests/.

- Cursor changes were reverted after the user reported broken mouse look. Leave cursor behavior alone until explicitly requested again.
