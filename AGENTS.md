# Local Archolos development

- Commit changes locally. Do not push, open pull requests, or submit anything upstream unless the user explicitly requests it.
- When giving launch instructions, provide one pasteable Terminal command, not the launcher script body:
  `rtk proxy /bin/zsh "/Users/lu2/Documents/Codex/2026-09-09/https-github-com-try-opengothic-issues/outputs/Play-Archolos.command"`
- Prefix shell commands with `rtk`; use `rtk proxy` for commands without a filter.
- Preserve the original game data, installers, and playtest save. Run benchmarks using independent save copies, without concurrent builds or other benchmark runs.
