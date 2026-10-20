"""Print a Neovim langmap that makes every base-layer key act as its QWERTY key in normal mode.

Usage: python3 tools/langmap.py LAYOUT
Prints a Lua line for init.lua. Shift on a key whose keycode already includes Shift (? ! _) changes
nothing, so the QWERTY capital under it cannot be reached.
"""
import sys

from zmk import FACTORY, KC, parse

SHIFT = dict(zip("abcdefghijklmnopqrstuvwxyz1234567890-=[]\\;',./`",
                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()_+{}|:\"<>?~"))
CHAR = {kc: c for c, kc in KC.items() if len(c) == 1 and c.isascii()}


def langmap(text):
    base = parse(text)["base"]
    chosen = {}
    for r in range(4):
        for c, ours in enumerate(base[r]):
            qwerty = CHAR.get(FACTORY[r][c])
            if ours not in CHAR.values() or qwerty is None:
                continue
            for frm, to in ((ours, qwerty), (SHIFT.get(ours, ours), SHIFT.get(qwerty, qwerty))):
                old = chosen.get(frm)
                if old is None or (not old.isalpha() and to.isalpha()):
                    chosen[frm] = to
    esc = lambda ch: "\\" + ch if ch in ';,"|\\' else ch
    return ",".join(esc(f) + esc(t) for f, t in chosen.items() if f != t)


if __name__ == "__main__":
    print(f"vim.opt.langmap = [[{langmap(open(sys.argv[1]).read())}]]")
