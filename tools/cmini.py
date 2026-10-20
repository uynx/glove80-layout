"""Convert cmini layout JSON (github.com/Apsu/cmini, layouts/*.json) to a base layer holding only its letters.

Usage: python3 tools/cmini.py OUTDIR FILE.json...
A letter on a thumb is left out, so `kl opt -f` places it on the base layer.
"""
import json
import sys
from pathlib import Path


def convert(d):
    grid = [["·"] * 12 for _ in range(4)]
    grid[1][0], grid[2][0] = "⇥", "⎋"
    for ch, k in d["keys"].items():
        if len(ch) != 1 or not ch.isalpha() or k["row"] not in (0, 1, 2) or k["finger"] in ("LT", "RT", "TB"):
            continue
        # Column c prints at c + 1 because our right half prints inner to outer, as cmini's does.
        grid[k["row"] + 1][k["col"] + 1] = ch.lower()
    rows = [" ".join(r[:6]) + "   " + " ".join(r[6:]) for r in grid]
    return "base:\n" + "\n".join(rows) + "\n"


def main():
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    for f in sys.argv[2:]:
        (out / (Path(f).stem + ".txt")).write_text(convert(json.load(open(f))))


if __name__ == "__main__":
    main()
