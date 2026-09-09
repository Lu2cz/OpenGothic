"""Check the normal-forward-input ship-stair probe; requires a disposable save run."""
import argparse
from pathlib import Path
import re

parser = argparse.ArgumentParser()
parser.add_argument("log", type=Path)
parser.add_argument("--state", choices=("closed", "open"), default="closed")
args = parser.parse_args()
log = args.log.read_text(errors="replace")
start = re.search(r"\[GATE_INPUT\] start=([-0-9.]+),([-0-9.]+),([-0-9.]+) floor=1", log)
assert start, "Missing valid stair starting point"
points = [(int(f), float(x), float(y), float(z)) for f, x, y, z in re.findall(
    r"\[GATE_INPUT\] frame=(\d+) pos=([-0-9.]+),([-0-9.]+),([-0-9.]+)", log)]
assert len(points) >= 6 and points[-1][0] >= 150, "Incomplete forward-input test"
finished = re.search(r"\[ARCHOLOS_PROFILE\] frames=(\d+)", log)
assert finished and int(finished[1]) >= 180, "Run did not finish"
assert points[-1][0] >= int(finished[1])-30, "Missing final movement samples"
state = re.search(r"\[GATE_STATE\] frame=(\d+) state=(\d+)", log)
if state:
    assert int(state[2]) == 0, "Gate was still moving when the test started"
    assert (int(state[1]) == 0) == (args.state == "closed"), "Incorrect gate state"
x0, y0, z0 = map(float, start.groups())
assert y0 < -1800, "Player did not start below the gate"
assert max(p[1] for p in points) - x0 > 150, "Forward input did not move the player"
if args.state == "closed":
    assert max(p[2] for p in points) < -1600, "Player passed through the closed gate"
else:
    assert any(p[2] > -1530 and p[1] - x0 > 600 for p in points), "Player never reached the deck through the opened gate"
print(f"PASS: {args.state} gate, final height {points[-1][2]:.2f}")
