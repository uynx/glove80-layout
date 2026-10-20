#!/usr/bin/env dash
# Optimize once per weight setting: the defaults, then each weight at 0.5x and 1.5x.
# Usage: tools/sweep.sh OUTDIR [extra kl opt args...]
set -eu
out=$1
shift
mkdir -p "$out"
run() {
    name=$1
    shift
    [ -s "$out/$name.txt" ] && return
    ./build/kl opt corpus/corpus.txt -r 12 -n 8000000 "$@" > "$out/$name.tmp" 2> "$out/$name.log"
    mv "$out/$name.tmp" "$out/$name.txt"
    echo "$name $(grep '^score' "$out/$name.txt")"
}
run base "$@"
for kv in effort=1 sfb=4 sfs=1.5 scissor=5 half_scissor=1.5 wide_jump=2 lsb=1 inroll=-0.6 outroll=-0.3 ring_pinky=0.5 roll3=-0.3 \
          redirect=1.5 bad_redirect=1.5 layer_switch=1.5 layer_boundary=0.75; do
    k=${kv%=*}
    v=${kv#*=}
    for f in 0.5 1.5; do
        run "$k-$f" "$@" -w "$k=$(awk "BEGIN { print $v * $f }")"
    done
done
