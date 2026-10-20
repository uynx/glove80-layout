import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from zmk import keymap


def layer(text, name):
    body = re.search(name + r" \{.*?bindings = <(.*?)>;", text, re.S).group(1)
    return re.findall(r"&\w+(?: [A-Z0-9_()]+)*", body)


qwerty = keymap((ROOT / "layouts/qwerty.txt").read_text())
colemak = keymap((ROOT / "layouts/colemak-dh.txt").read_text())

for text in (qwerty, colemak):
    for name in ("default_layer", "sym_layer", "magic_layer", "factory_test_layer"):
        assert len(layer(text, name)) == 80, name

base = layer(qwerty, "default_layer")
# row 3 starts at index 22: TAB Q W E R T | Y U I O P BSLH
assert base[22:28] == ["&kp TAB", "&kp Q", "&kp W", "&kp E", "&kp R", "&kp T"]
assert base[28] == "&kp Y" and base[34] == "&kp ESC"
# upper thumbs then lower thumbs
assert base[52:58] == ["&kp LSHFT", "&kp LCTRL", "&mo SYM", "&mo SYM", "&kp RCTRL", "&kp RSHFT"]
assert base[69:75] == ["&kp BSPC", "&kp DEL", "&kp LGUI", "&kp RGUI", "&kp RET", "&kp SPACE"]

cbase = layer(colemak, "default_layer")
# Colemak F sits on QWERTY E, so with Ctrl or Meta held it sends E
assert cbase[25] == "&pos_25"
assert re.search(r"pos_25: pos_25 \{.*?bindings = <&kp F>, <&kp E>;", colemak, re.S)
assert cbase[23] == "&kp Q"

# Shift only capitalizes: a key whose keycode Shift would change ignores Shift instead.
assert base[61] == "&sh_DOT" and base[23] == "&kp Q"
assert re.search(r"sh_DOT: sh_DOT \{.*?bindings = <&kp DOT>, <&kp DOT>;\s*mods = <\(MOD_LSFT\|MOD_RSFT\)>;", qwerty, re.S)
assert "sh_QMARK" not in qwerty

swapped = keymap((ROOT / "layouts/qwerty.txt").read_text().replace(". /", "v /").replace("c v b", "c . b"))
assert re.search(r"pos_50: pos_50 \{.*?bindings = <&sh_DOT>, <&kp V>;", swapped, re.S)

sym = layer(qwerty, "sym_layer")
assert sym[10] == "&kp PLUS" and sym[30] == "&sh_LBKT"
assert sym[52] == "&trans" and sym[0] == "&kp C_BRI_DN"
print("ok")
