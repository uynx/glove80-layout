"""Score every layout from a sweep under every sweep setting and rank them by worst-case regret.

Usage: python3 tools/regret.py SWEEP_DIR [extra layout files...]
Regret of a layout under a setting is how far its score sits above the best layout for that setting.
"""
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

# Must match the defaults in src/main.cpp and the list in tools/sweep.sh.
DEFAULTS = {"effort": 1, "sfb": 4, "sfs": 1.5, "scissor": 2, "lsb": 1, "inroll": -0.6, "outroll": -0.3,
            "roll3": -0.3, "redirect": 1.5, "bad_redirect": 1.5, "layer_switch": 1.5, "layer_boundary": 0.75}


def worst_regret(scores):
    settings = next(iter(scores.values())).keys()
    best = {s: min(row[s] for row in scores.values()) for s in settings}
    return {name: max(row[s] / best[s] - 1 for s in settings) for name, row in scores.items()}


def setting_args(name):
    if name == "base":
        return []
    k, f = name.rsplit("-", 1)
    return ["-w", f"{k}={DEFAULTS[k] * float(f)}"]


def score(layout, args):
    out = subprocess.run(["./build/kl", "score", "corpus/corpus.txt", str(layout), *args],
                         capture_output=True, text=True, check=True).stdout
    return float(re.search(r"^score (\S+)", out, re.M).group(1))


def main():
    sweep = Path(sys.argv[1])
    settings = sorted(p.stem for p in sweep.glob("*.txt"))
    layouts = {p.stem: p for p in sweep.glob("*.txt")} | {Path(p).stem: Path(p) for p in sys.argv[2:]}
    jobs = [(l, s) for l in layouts for s in settings]
    with ThreadPoolExecutor() as ex:
        results = ex.map(lambda j: score(layouts[j[0]], setting_args(j[1])), jobs)
        scores = {}
        for (l, s), v in zip(jobs, results):
            scores.setdefault(l, {})[s] = v
    regret = worst_regret(scores)
    mean = {l: sum(scores[l][s] / min(scores[x][s] for x in scores) - 1 for s in settings) / len(settings)
            for l in scores}
    for l in sorted(regret, key=regret.get):
        print(f"{l:20} worst {100 * regret[l]:6.2f}%   mean {100 * mean[l]:6.2f}%")


if __name__ == "__main__":
    main()
