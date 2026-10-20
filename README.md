# glove80-layout

This is a keyboard layout optimizer I'm writing for my Glove80. It searches for a letter layer and a symbol layer together, scoring each candidate on a corpus that is about 80% English and 20% code, and it runs simulated annealing on as many cores as I give it with a different seed per restart.

## The layout

There are only two layers. The base layer holds the letters and the most common punctuation, and the symbol layer holds everything else, including the digits, which the optimizer places freely instead of forcing them into a numpad. Both thumb layer keys open the symbol layer, and Space and Enter stay transparent on it, so typing something like `= 1` doesn't require letting go of the layer key in the middle. Vim stays positional because the firmware sends the QWERTY key whenever Ctrl or Meta is held, and `tools/langmap.py` prints a Neovim langmap that does the same thing for normal mode.

## The corpus

The English part is 50% conversational (Reddit comments from 20 subreddits and typed chat from Blended Skill Talk), 30% essay writing (student essays, Paul Graham's essays and my own writing) and 20% technical writing (arXiv papers). The code part is C, C++, Python, Rust, Nix, JavaScript, fish, Haskell, OCaml, Ruby and a little LaTeX, pulled from well known repos at pinned tags with indentation stripped, since the editor types that and I don't.

Scraped text is full of things nobody actually typed, so `corpus/build.py` cleans every source before anything gets counted. Markdown and links come out of the Reddit comments, reference lists come out of the essays, and an arXiv sentence that lost its math or a figure reference gets dropped entirely. In the code, API documentation is dropped and a line pasted across files only counts once. I started with movie subtitles for the conversational part, but that dataset turned out to have most of its "br" pairs stripped out ("break" showed up 5 times against 232 copies of "eak"), so I replaced it with chat. `tests/test_corpus.py` gives every cleaner an input it has to reject, because one of them was silently doing nothing until a test like this caught it.

## How it scores a layout

The score adds up key effort, same finger bigrams (scaled by how far the finger travels) and skipgrams, scissors, wide jumps, lateral stretches and redirects, and subtracts a bonus for inward and outward rolls, which is smaller for ring to pinky rolls. A roll only counts when both keys are on the same layer, since the thumb press in between breaks it. Switching layers costs extra, but less next to Space or Enter, where it falls at a word boundary. Every weight can be changed from the command line with `-w name=value`, and the defaults are at the top of `src/main.cpp`.

To pick the weights, `tools/sweep.sh` optimizes once with the defaults and once with each weight at half and one and a half times its value, and `tools/regret.py` scores every resulting layout under all 31 settings. The layout from the default weights came within 0.79% of the best layout under every setting, which shows that the result doesn't depend much on the exact weights I chose.

The optimizer only rescores the n-grams that contain the two keys being swapped, which gets about half a million swaps per second on eight threads of my i7-8700K. A test checks that this running score matches a full rescore after 100,000 random swaps.

## Running it

```sh
direnv allow                      # or: devenv shell
make test
python3 corpus/build.py           # downloads sources into corpus/raw, writes corpus/corpus.txt
make build/kl
./build/kl opt corpus/corpus.txt -r 64 -n 16000000 > layouts/best.txt
./build/kl score corpus/corpus.txt layouts/qwerty.txt
python3 tools/zmk.py layouts/best.txt > firmware/glove80.keymap
python3 tools/langmap.py layouts/best.txt   # one line for init.lua
```

Pushing a change under `firmware/` makes GitHub Actions build `glove80.uf2`, which flashes onto both halves.

Put your own writing (PDF or text) in `corpus/private/` before building the corpus. It gets mixed in at its natural size and is also written to `corpus/mine.txt` so a layout can be scored on it alone. Both folders are ignored by git.
