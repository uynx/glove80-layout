import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "corpus"))
from build import clean_arxiv, dedupe, drop_docs, drop_templates

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
macro = "Removed due to low effort post. See the posting guidelines in r/"
text = "\n".join([macro + "a", "short", macro + "b", "a real comment that is long enough to count here", macro + "c"])
assert drop_templates(text) == "short\na real comment that is long enough to count here"
assert drop_templates("\n".join([macro + "a", macro + "b"])) == f"{macro}a\n{macro}b"

assert clean_arxiv("the flux rises. see kolda, s. pokorski and n. polonsky, phys. rev. 80 (1998) 5263. it then falls.") == \
    "the flux rises. it then falls."
assert clean_arxiv("the coefficient of @xmath5 will shift. it then falls. as shown in @xcite, it rises.") == "it then falls."
print("ok")
