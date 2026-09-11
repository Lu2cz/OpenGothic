# Archolos world-transition regression

The private regression checks the real loaded level triggers, rather than calling the world-change helper directly:

- `CHANGELEVEL_ARCHOLOS_2_SEWERS` → `FP_M6_2_SEWERS`
- `CHANGELEVEL_SEWERS_2_M6` → `FP_SEWERS_2_M6`

Run it against a disposable copy of an Archolos save and output directory:

```sh
rtk python3 tests/run_archolos_world_transition.py \
  --executable build/opengothic/Gothic2Notr \
  --game /path/to/archolos-game \
  --save /path/to/private/save_slot_2.sav \
  --output /path/to/private/output
```

The three stages are Mainland → Sewer, Sewer → Mainland, and a fresh-process Mainland reload. The two transition stages require a completed save and verify destination rendering/input movement, player inventory, quest archive, lock progress, stale focus clearing, a pending callback, and spaced recurring callbacks. The fresh-process stage rechecks Q101's partial lock progress, the old hero and two distinct source-inventory addresses as tombstones, and the fresh hero/two inventory bindings as readable and writable before removing the test callback. The old hero address is deliberately a tombstone: `HeroStorage` transfers gameplay state, not script-reference identity. `run.json` records `RUNNING`, `PASS`, `FAIL`, or `TIMEOUT` per stage.

Latest private evidence was built from candidate tree `archolos/issue-4-world-transitions` (base commit `ce02bf5564a664d8e97f440ef9ef75595e72e7c4`), using `build/opengothic/Gothic2Notr` SHA-256 `5601c9b8f351a00a48c7da4652d695926d61a1296be81c5688b51f9a5cbba1ce`, private dependency `archolos-game`, and source save SHA-256 `5574d61f7aa31390b1db05d4b9f24cfa417ef5739ed9e548185dac9e17ac22ed`. The complete three-stage manifest is `/Users/lu2/Documents/Codex/2026-09-09/https-github-com-try-opengothic-issues/work/issue4-world-transition-restart-bindings-21/run.json`.

The fix invokes LeGo's level-change begin/end hooks around a successful native transition. Without that scope, LeGo treats destination initialization as a normal load and attempts persistent-memory unarchive during Sewer startup.
