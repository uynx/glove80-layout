import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from langmap import langmap

QWERTY = (ROOT / "layouts/qwerty.txt").read_text()


def place(moves):
    base, sym = QWERTY.split("sym:")
    return " ".join(moves.get(t, t) for t in base.split(" ")) + "sym:" + sym


def swap(a, b):
    return place({a: b, b: a})


def pairs(lm):
    out, chars, i = set(), [], 0
    while i < len(lm):
        if lm[i] == "\\":
            chars.append(lm[i + 1])
            i += 2
        elif lm[i] == ",":
            i += 1
        else:
            chars.append(lm[i])
            i += 1
        if len(chars) == 2:
            out.add("".join(chars))
            chars = []
    return out


assert langmap(QWERTY) == ""

colemak = pairs(langmap((ROOT / "layouts/colemak-dh.txt").read_text()))
assert {"fe", "FE", "pr", "PR"} <= colemak

assert pairs(langmap(swap(".", "v"))) == {".v", ">V", "v.", "V>"}
assert pairs(langmap(swap("s", ";"))) == {"s;", "S:", ";s", ":S"}
assert "s\\;" in langmap(swap("s", ";"))

# "?" sends Shift+/ already, so Shift adds nothing and the key keeps q, not Q.
assert pairs(langmap(place({"q": "?", "/": "q"}))) == {"?q", "q/", "Q?"}

# A character two keys can send goes to the one that reaches a letter.
text = swap("g", ",").replace("· z x", "< z x", 1)
p = pairs(langmap(text))
assert "<G" in p and "<`" not in p
print("ok")
