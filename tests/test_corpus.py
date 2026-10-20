import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "corpus"))
from build import dedupe, drop_docs

cases = [
    (['def f():', '"""Doc.', 'more', '"""', 'return 1'], ['def f():', 'return 1']),
    (['x = """', 'text', '"""', 'y = 1'], ['x = """', 'text', '"""', 'y = 1']),
    (['"""One-line doc."""', 'z = 2'], ['z = 2']),
    (['/// doc', 'fn a() {}'], ['fn a() {}']),
    (['/**', '* doc', '*/', 'int x;'], ['int x;']),
    (['/* normal', '* c', '*/'], ['/* normal', '* c', '*/']),
    (['s = """a', '"""', '"""Doc after."""', 'k = 3'], ['s = """a', '"""', 'k = 3']),
    (["r'''raw doc", "'''", 'q = 4'], ['q = 4']),
    (['-- | haddock', 'f x = x', '(** odoc *)', 'let y = 1'], ['f x = x', 'let y = 1']),
]
for given, want in cases:
    assert drop_docs(given) == want, (given, drop_docs(given))

pasted = "using AnyInvType = typename TypeParam::AnyInvType;"
seen = set()
assert dedupe(f"x = 1\n{pasted}\n}}", seen) == f"x = 1\n{pasted}\n}}"
assert dedupe(f"{pasted}\n}}\nx = 1", seen) == "}\nx = 1"
print("ok")
