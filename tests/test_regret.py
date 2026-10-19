import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
from regret import worst_regret

# scores[layout][setting]; setting bests are 1.0 (A) and 2.0 (B)
scores = {"a": {"A": 1.0, "B": 2.4}, "b": {"A": 1.1, "B": 2.0}, "c": {"A": 1.05, "B": 2.1}}
r = worst_regret(scores)
assert abs(r["a"] - 0.20) < 1e-9
assert abs(r["b"] - 0.10) < 1e-9
assert abs(r["c"] - 0.05) < 1e-9
assert min(r, key=r.get) == "c"
print("ok")
