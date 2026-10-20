import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "corpus"))
from build import clean_arxiv, clean_code, clean_essay, clean_markdown, dedupe, drop_docs, drop_templates

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
assert clean_arxiv("the results ( see table [ tab : x ] ) agree. in fig . [ figure : a ] the rate rises. it is good.") == "it is good."

markdown = [
    ("see [the docs](https://x.com/a_(b)) now", "see the docs now"),
    ("source (https://x.com/page) here", "source here"),
    ("~~wrong~~ right", "wrong right"),
    ("that's ^(tiny text) and ^super", "that's tiny text and super"),
    ("word\\_with\\_escapes and \\*star\\*", "word_with_escapes and *star*"),
    ("**bold** and __under__ and snake_case", "bold and under and snake_case"),
    ("if a < b and c > d then", "if a < b and c > d then"),
    ("Check r/fitness and u/someone", "Check r/fitness and u/someone"),
    ("2^10 is 1024", "2^10 is 1024"),
]
for given, want in markdown:
    assert clean_markdown(given) == want, (given, clean_markdown(given))

essay = ("Vaccines work.\nThey are safe (Smith, 2014).\nReferences\nSmith, J. (2014). Vaccines. Journal, 3(2), 45-67.\n"
         "Doe, A. 2012. Web. https://www.nytimes.com/x")
assert clean_essay(essay) == "Vaccines work.\nThey are safe (Smith, 2014)."
assert clean_essay("Read it at www.unfpa.org/pubs today.\r\nOk.") == "Read it at today.\nOk."

assert clean_code("/*\nCopyright (c) 2012 Someone\nPermission is hereby granted, free of charge\n*/\nint x = 1;") == "int x = 1;"
assert clean_code("int y;\n0x30, 0x82, 0x02, 0x22, 0x30, 0x0d,\nflags = 0x1 | 0x2;") == "int y;\nflags = 0x1 | 0x2;"
print("ok")
