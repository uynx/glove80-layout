#undef NDEBUG
#include "../src/kl.hpp"
#include <cassert>
#include <cstdio>

using namespace kl;

static bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

static double find2(const std::vector<Gram2>& v, char a, char b) {
    for (auto& g : v)
        if (g.a == char_index(a) && g.b == char_index(b)) return g.f;
    return 0;
}

static double find3(const std::vector<Gram3>& v, char a, char b, char c) {
    for (auto& g : v)
        if (g.a == char_index(a) && g.b == char_index(b) && g.c == char_index(c)) return g.f;
    return 0;
}

static void counts_tiny_text() {
    Ngrams n = count("Ab a");
    assert(near(n.mono[char_index('a')], 0.5));
    assert(near(n.mono[char_index('b')], 0.25));
    assert(near(find2(n.bi, 'a', 'b'), 0.25));
    assert(near(find2(n.bi, 'b', ' '), 0.25));
    assert(near(find2(n.bi, ' ', 'a'), 0.25));
    assert(n.bi.size() == 3);
    assert(near(find2(n.skip, 'a', ' '), 0.25));
    assert(near(find2(n.skip, 'b', 'a'), 0.25));
    assert(near(find3(n.tri, 'a', 'b', ' '), 0.25));
    assert(near(find3(n.tri, 'b', ' ', 'a'), 0.25));
}

static void unknown_byte_breaks_context() {
    Ngrams n = count("ab\x01" "ab");
    assert(n.bi.size() == 1);
    assert(near(find2(n.bi, 'a', 'b'), 0.5));
    assert(n.skip.empty() && n.tri.empty());
}

static Model toy(int n) {
    Model m;
    m.nslots = m.nphys = n;
    for (int s = 0; s < n; s++) m.phys.push_back(s);
    m.mono.assign(n, 0);
    m.bi.assign(n * n, 0);
    m.skip.assign(n * n, 0);
    m.tri.assign(n * n * n, 0);
    m.allowed.assign(ALPHA.size() * n, 0);
    m.pinned.assign(ALPHA.size(), -1);
    return m;
}

static void toy_score_matches_hand_computation() {
    Model m = toy(4);
    m.mono = {1, 2, 3, 4};
    m.bi[0 * 4 + 1] = 10;
    Scorer s(m, count("ab"));
    std::vector<int> pos(ALPHA.size(), -1);
    pos[char_index('a')] = 0, pos[char_index('b')] = 1;
    // mono 0.5*1 + 0.5*2, bigram ab 0.5*10
    assert(near(s.score(pos), 6.5));

    Model m3 = toy(4);
    m3.bi[0 * 4 + 1] = 1, m3.bi[1 * 4 + 2] = 2, m3.skip[0 * 4 + 2] = 3, m3.tri[(0 * 4 + 1) * 4 + 2] = 6;
    Scorer s3(m3, count("abc"));
    pos[char_index('c')] = 2;
    // each n-gram occurs once in 3 characters: (1 + 2 + 3 + 6) / 3
    assert(near(s3.score(pos), 4));
}

static Model random_toy(int n, const std::string& text, std::mt19937_64& rng) {
    std::uniform_real_distribution<float> u(-1, 3);
    Model m = toy(n);
    for (auto* t : {&m.mono, &m.bi, &m.skip, &m.tri})
        for (auto& v : *t) v = u(rng);
    for (char c : text)
        for (int s = 0; s < n; s++) m.allowed[char_index(c) * n + s] = 1;
    return m;
}

static void swap_delta_matches_full_rescore() {
    std::mt19937_64 rng(1);
    const std::string text = "the quick brown fox jumps over the lazy dog, then again!";
    Model m = random_toy(40, text, rng);
    for (char c = 'a'; c <= 'z'; c++)
        for (int s = 30; s < 40; s++) m.allowed[char_index(c) * 40 + s] = 0;
    for (int s = 0; s < 40; s++) m.allowed[char_index(' ') * 40 + s] = 0;
    m.pinned[char_index(' ')] = 39;
    Scorer sc(m, count(text));
    Layout L = random_layout(m, rng);
    auto slots = movable_slots(m);
    double running = sc.score(L.pos);
    int done = 0;
    while (done < 100000) {
        int s1 = slots[rng() % slots.size()], s2 = slots[rng() % slots.size()];
        if (!valid_swap(m, L, s1, s2)) continue;
        double d = sc.swap(L, s1, s2);
        if (rng() % 2) running += d;
        else sc.swap(L, s1, s2);
        done++;
    }
    assert(L.pos[char_index(' ')] == 39);
    for (char c = 'a'; c <= 'z'; c++)
        if (L.pos[char_index(c)] >= 0) assert(L.pos[char_index(c)] < 30);
    assert(std::fabs(running - sc.score(L.pos)) < 1e-6);
}

static void anneal_finds_brute_force_optimum() {
    std::mt19937_64 rng(7);
    const std::string text = "abacabadabacabaeefgghhiijjjbcadhefgijabjihgfedcba";
    Model m = random_toy(10, text, rng);
    Scorer sc(m, count(text));
    std::vector<int> slots{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, pos(ALPHA.size(), -1);
    double best = 1e18;
    do {
        for (int k = 0; k < 10; k++) pos[char_index('a' + k)] = slots[k];
        best = std::min(best, sc.score(pos));
    } while (std::next_permutation(slots.begin(), slots.end()));
    Result r = anneal(sc, rng, 400000);
    assert(near(r.score, best));
}

int main() {
    counts_tiny_text();
    unknown_byte_breaks_context();
    toy_score_matches_hand_computation();
    swap_delta_matches_full_rescore();
    anneal_finds_brute_force_optimum();
    std::puts("ok");
}
