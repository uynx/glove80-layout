"""Print a Neovim langmap that makes every base-layer key act as its QWERTY key in normal mode.

Usage: python3 tools/langmap.py LAYOUT
Prints a Lua line for init.lua. Shift only capitalizes on this board, so a QWERTY capital that sits
under a symbol key has no key.
"""
import sys

from zmk import FACTORY, KC, parse

SHIFT = dict(zip("abcdefghijklmnopqrstuvwxyz1234567890-=[]\\;',./`",
                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()_+{}|:\"<>?~"))
CHAR = {kc: c for c, kc in KC.items() if len(c) == 1 and c.isascii()}


def langmap(text):
    base = parse(text)["base"]
    pairs = []
    for r in range(4):
        for c, ours in enumerate(base[r]):
            qwerty = CHAR.get(FACTORY[r][c])
            if ours not in CHAR.values() or qwerty is None:
                continue
            pairs.append((ours, qwerty))
            if ours.isalpha():
                pairs.append((ours.upper(), SHIFT.get(qwerty, qwerty)))
    esc = lambda ch: "\\" + ch if ch in ';,"|\\' else ch
    return ",".join(esc(f) + esc(t) for f, t in pairs if f != t)


if __name__ == "__main__":
    print(f"vim.opt.langmap = [[{langmap(open(sys.argv[1]).read())}]]")
