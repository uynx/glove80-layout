"""Build corpus/corpus.txt (training mix) and corpus/mine.txt (your own writing).

Usage: python3 corpus/build.py [total bytes, default 20000000]
Put your own essays (.pdf or .txt) in corpus/private/. Downloads are cached in corpus/raw/.
"""
import html
import http.client
import json
import random
import re
import subprocess
import sys
import time
import unicodedata
import urllib.error
import urllib.parse
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent
RAW = ROOT / "raw"
TOTAL = int(sys.argv[1]) if len(sys.argv) > 1 else 20_000_000
ROWS = "https://datasets-server.huggingface.co/rows"
GH_API = "https://api.github.com"

SUBREDDITS = [
    "relationship_advice", "tifu", "Showerthoughts", "socialskills", "Fitness",
    "bodyweightfitness", "gaming", "Games", "buildapc", "personalfinance",
    "changemyview", "explainlikeimfive", "todayilearned", "books", "space",
    "science", "technology", "programming", "LifeProTips", "travel",
]

CODE = {
    "c": (0.10, [
        ("redis/redis", "7.2.5", ["src"], ".c .h"),
        ("curl/curl", "curl-8_10_1", ["lib"], ".c .h"),
    ]),
    "cpp": (0.25, [
        ("abseil/abseil-cpp", "20240722.0", ["absl"], ".cc .h"),
        ("fmtlib/fmt", "11.0.2", ["include", "src"], ".cc .h"),
    ]),
    "python": (0.20, [
        ("python/cpython", "v3.12.7", ["Lib"], ".py"),
        ("django/django", "5.1.2", ["django"], ".py"),
    ]),
    "rust": (0.10, [
        ("BurntSushi/ripgrep", "14.1.1", ["crates"], ".rs"),
        ("tokio-rs/tokio", "tokio-1.40.0", ["tokio/src"], ".rs"),
    ]),
    "nix": (0.10, [("NixOS/nixpkgs", "24.05", ["lib", "nixos/modules"], ".nix")]),
    "js": (0.08, [
        ("expressjs/express", "4.21.0", ["lib"], ".js"),
        ("preactjs/preact", "10.24.2", ["src", "hooks/src", "compat/src"], ".js"),
    ]),
    "fish": (0.05, [
        ("fish-shell/fish-shell", "3.7.1", ["share/functions"], ".fish"),
        ("IlanCosman/tide", "v6.1.1", ["functions", "conf.d"], ".fish"),
    ]),
    "haskell": (0.04, [
        ("xmonad/xmonad", "v0.18.0", ["src"], ".hs"),
        ("xmonad/xmonad-contrib", "v0.18.1", ["XMonad"], ".hs"),
    ]),
    "ocaml": (0.04, [
        ("ocaml/dune", "3.16.1", ["src"], ".ml .mli"),
        ("janestreet/base", "v0.17.1", ["src"], ".ml .mli"),
    ]),
    "ruby": (0.04, [
        ("jekyll/jekyll", "v4.3.4", ["lib"], ".rb"),
        ("sinatra/sinatra", "v4.0.0", ["lib"], ".rb"),
    ]),
}

FOLD = str.maketrans({
    "‘": "'", "’": "'", "“": '"', "”": '"', "–": "-",
    "—": "-", "…": "...", " ": " ", "\t": " ",
})


def get(url, headers=None):
    req = urllib.request.Request(url, headers={"User-Agent": "glove80-layout", **(headers or {})})
    last = None
    for attempt in range(8):
        try:
            with urllib.request.urlopen(req, timeout=120) as r:
                return r.read()
        except urllib.error.HTTPError as e:
            if e.code not in (429, 500, 502, 503, 504):
                raise
            last = e.code
        except (http.client.HTTPException, OSError) as e:
            last = e
        time.sleep(min(60, 2 ** attempt * 2))
    raise RuntimeError(f"giving up on {url} ({last})")


def ascii_only(s):
    s = unicodedata.normalize("NFKD", s.translate(FOLD))
    return s.encode("ascii", "ignore").decode()


def tidy(s):
    s = re.sub(r" +", " ", ascii_only(s))
    s = re.sub(r" *\n *", "\n", s)
    return re.sub(r"\n{2,}", "\n", s).strip()


def unwrap(s):
    s = re.sub(r"[ \t]+\n", "\n", s)
    return re.sub(r"(?<!\n)\n(?!\n)", " ", s)


def clean_reddit(s):
    if s in ("[deleted]", "[removed]"):
        return ""
    return tidy(re.sub(r"https?://\S+", "", html.unescape(s)))


def clean_arxiv(s):
    s = re.sub(r"@x(math|cite)\d*", "", s.replace("\n", " "))
    s = re.sub(r" +", " ", s)
    s = re.sub(r"(\w) - (\w)", r"\1-\2", s)
    s = re.sub(r" ([.,;:!?)\]])", r"\1", s)
    s = re.sub(r"([(\[]) ", r"\1", s)
    s = re.sub(r" (n't|'s|'re|'ve|'ll|'d|'m)\b", r"\1", s)
    return tidy(s)


def hf(dataset, config, split, column, clean, limit, rng, page=100):
    base = f"{ROWS}?dataset={urllib.parse.quote(dataset)}&config={config}&split={split}&length={page}"
    total = json.loads(get(base.replace(f"length={page}", "length=1") + "&offset=0"))["num_rows_total"]
    def fetch(offset):
        try:
            return json.loads(get(f"{base}&offset={offset}"))["rows"]
        except (RuntimeError, urllib.error.HTTPError, ValueError):
            return []

    out, size, seen, dry = [], 0, set(), 0
    with ThreadPoolExecutor(8) as ex:
        while size < limit:
            offsets = [rng.randrange(max(1, total - page)) for _ in range(8)]
            batches = list(ex.map(fetch, offsets))
            dry = 0 if any(batches) else dry + 1
            if dry == 3:
                raise RuntimeError(f"{dataset}/{split}: 24 pages in a row failed")
            for rows in batches:
                for r in rows:
                    if r["row_idx"] in seen:
                        continue
                    seen.add(r["row_idx"])
                    d = clean(r["row"][column] or "")
                    if d:
                        out.append(d)
                        size += len(d) + 1
    return out


def cached(name, limit, make):
    path = RAW / f"{name}.txt"
    if path.exists() and path.stat().st_size >= limit * 0.95:
        return path.read_text()[:limit]
    print(f"fetching {name} ({limit / 1e6:.1f} MB)", flush=True)
    text = "\n".join(make(limit))[:limit]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    return text


def reddit(limit, rng):
    out = []
    for sub in SUBREDDITS:
        out.append(cached(f"reddit/{sub}", limit // len(SUBREDDITS) + 1,
                          lambda n, sub=sub: hf("HuggingFaceGECLM/REDDIT_comments", "default", sub,
                                                "body", clean_reddit, n, rng)))
    return out


def gh_auth():
    try:
        token = subprocess.run(["gh", "auth", "token"], capture_output=True, text=True).stdout.strip()
    except FileNotFoundError:
        return {}
    return {"Authorization": f"Bearer {token}"} if token else {}


def gh_files(repo, tag, paths, exts, auth):
    files = []
    for path in paths:
        parent, _, name = path.rpartition("/")
        listing = json.loads(get(f"{GH_API}/repos/{repo}/contents/{parent}?ref={tag}", auth))
        sha = next(e["sha"] for e in listing if e["name"] == name)
        tree = json.loads(get(f"{GH_API}/repos/{repo}/git/trees/{sha}?recursive=1", auth))
        files += [f"{path}/{e['path']}" for e in tree["tree"]
                  if e["type"] == "blob" and Path(e["path"]).suffix in exts.split() and e["size"] < 200_000]
    return files


COMMENT = re.compile(r"^(//|/\*|\*|#(\s|!|$)|--|\(\*|\{-|;)")


def clean_code(s):
    lines = [l.strip() for l in ascii_only(s).splitlines()]
    lines = [l for l in lines if l and len(l) < 300]
    i = 0
    while i < len(lines) and COMMENT.match(lines[i]):
        i += 1
    return "\n".join(lines[i:])


def code(lang, limit, rng, auth):
    out = []
    repos = CODE[lang][1]
    with ThreadPoolExecutor(16) as ex:
        for repo, tag, paths, exts in repos:
            files = gh_files(repo, tag, paths, exts, auth)
            rng.shuffle(files)
            size, want = 0, limit // len(repos)
            for i in range(0, len(files), 16):
                batch = files[i:i + 16]
                urls = [f"https://raw.githubusercontent.com/{repo}/{tag}/{f}" for f in batch]
                for body in ex.map(get, urls):
                    d = clean_code(body.decode(errors="ignore"))
                    out.append(d)
                    size += len(d) + 1
                if size >= want:
                    break
    return out


def mine():
    priv = ROOT / "private"
    texts = []
    for p in sorted(priv.glob("*")) if priv.exists() else []:
        if p.suffix == ".pdf":
            texts.append(subprocess.run(["pdftotext", "-layout", str(p), "-"], check=True,
                                        capture_output=True, text=True).stdout)
        elif p.suffix == ".txt":
            texts.append(p.read_text())
    return [tidy(unwrap(t)) for t in texts]


def main():
    rng = random.Random(0)
    english = [
        ("reddit", 0.40 * 0.6, reddit),
        ("subtitles", 0.40 * 0.4, lambda n, r: hf("dim/opensubtitles_clean_v1", "default", "train",
                                                  "text", tidy, n, r, page=20)),
        ("ivypanda", 0.24 * 0.9, lambda n, r: hf("qwedsacf/ivypanda-essays", "default", "train",
                                                 "TEXT", tidy, n, r, page=20)),
        ("paulgraham", 0.24 * 0.1, lambda n, r: hf("baber/paul_graham_essays", "default", "train",
                                                   "text", lambda s: tidy(unwrap(s)), n, r, page=10)),
        ("arxiv", 0.16, lambda n, r: hf("ccdv/arxiv-summarization", "document", "train",
                                        "article", clean_arxiv, n, r, page=5)),
    ]
    parts = [cached(name, int(TOTAL * share), lambda n, fn=fn: fn(n, rng)) for name, share, fn in english]
    auth = gh_auth()
    for lang, (share, _) in CODE.items():
        parts.append(cached(f"code_{lang}", int(TOTAL * 0.20 * share),
                            lambda n, lang=lang: code(lang, n, rng, auth)))
    own = "\n".join(mine())
    parts.append(own)
    (ROOT / "mine.txt").write_text(own)
    (ROOT / "corpus.txt").write_text("\n".join(parts))
    print(f"corpus.txt {sum(map(len, parts)) / 1e6:.1f} MB, mine.txt {len(own) / 1e3:.0f} KB")


if __name__ == "__main__":
    main()
