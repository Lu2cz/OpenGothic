# Kurt fishing pose

Issue [#37](https://github.com/Lu2cz/OpenGothic/issues/37) is duplicate overlay
ownership. Installed Archolos 1.2.11 queues `KURT_Q108_FISHING_APPLYMDS` on both
actors; each callback applies `HumanS_Fishing_Dialogue.MDS` to both actors.
The ending invokes the removal callback once. Previously, applying appended a
duplicate and removing erased only one entry. The remaining overlay replaced
`S_RUN` and persisted in saves, even with no rod, interaction or pending AI.

Reapplication now replaces the existing entry, refreshing its priority and timed
expiry. Removal also clears duplicates from older snapshots. A bounded load
recovery removes only this overlay from Marvin and Kurt when `MIS_Q108 == 2`,
the scene's final callback counter is 4, the hero queue is empty and `ITAR_ROD`
is absent. No quest values, inventory, other overlays or animation layers are
reset. The native save and compatibility snapshot formats are unchanged.

## Private inputs and reproduction

Paths below are relative to the coordination workspace in [environment.md](environment.md).

- Affected player checkpoint:
  `work/silbach-scroll-fishing-20260925-0gb3tgv4/named-9.sav`, SHA-256
  `c28d7d07354c52bd780f893f0351bf9ce54bf5263bd7b8ee4ae181f7bc4169bd`.
- Before fishing: `work/issue37-prefishing-explore-b/before-kurt.sav`, SHA-256
  `94ff10b3dce3cfe773d6324654b480dc2af1cd123c539d167cdc0ff9d2b25d62`.
  Derived from the earlier map checkpoint through native Lokvar/Riordian
  dialogues, the script-awarded special herb and two existing world violets.
  Diagnostic repositioning shortened travel; no quest flags or items were granted.
- Native failing scene: `work/issue37-prefishing-explore-b/after-fishing-broken.sav`,
  SHA-256 `8734f1561350405da97c6eeb4ec36d71291f5b7b3d9dea1bd2872ba251fe5879`.
  Adjacent traces show two applications per actor and removal with two entries.
  Removing the final copy restores the pose while preserving the torch.

`tests/run_archolos_fishing.py --help` describes the runner. Supply explicit
`--executable`, `--game`, `--save` and a fresh `--output` directory. Modes:

- `scene`: native important dialogue, guiding and fishing choices, two attribute
  rewards, five fish, rod cleanup, camera/control return, movement and save.
  The harness follows Kurt by repositioning only before dialogue; it chooses
  Don't talk, Strength, Dexterity and the return-to-Jorn option through the UI.
- `reload`: settled affected or post-scene save, clear overlays on both actors,
  no rod, normal control and another save. Chain its output into a fresh process.
- `overlays`: real native solver/assets; repeated application/removal, timed
  renewal and replacement of a timed overlay by a permanent one.
- `scene-broken` / `reload-broken`: expect retained overlays on an unfixed build.

The overlay regression fails before the fix with `Duplicate overlay survived
removal`. Raw private evidence remains local. These checks do not establish a
continuous campaign playthrough. The earlier writing-table blocker and clock
failure were resolved separately in #36; missing-item feedback is #39.
