# Archolos world-transition regression

The private regression checks the real loaded level triggers, rather than calling the world-change helper directly:

- `CHANGELEVEL_ARCHOLOS_2_SEWERS` → `FP_M6_2_SEWERS`
- `CHANGELEVEL_SEWERS_2_M6` → `FP_SEWERS_2_M6`

Run it against a disposable copy of an Archolos save and output directory:

```sh
python3 tests/run_archolos_world_transition.py \
  --executable build/opengothic/Gothic2Notr \
  --game /path/to/archolos-game \
  --save /path/to/private/save_slot_2.sav \
  --output /path/to/private/output
```

The three stages are Mainland → Sewer, Sewer → Mainland, and a fresh-process Mainland reload. Each requires a completed save and verifies destination rendering/input movement, player inventory, quest archive, lock progress, live/deleted script references, stale focus clearing, a pending callback, and spaced recurring callbacks. `run.json` records `RUNNING`, `PASS`, `FAIL`, or `TIMEOUT` per stage.

The fix invokes LeGo's level-change begin/end hooks around a successful native transition. Without that scope, LeGo treats destination initialization as a normal load and attempts persistent-memory unarchive during Sewer startup.
