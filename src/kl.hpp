#pragma once
#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>
#include <cstdint>
#include <string>
#include <vector>

namespace kl {

inline const std::string ALPHA =
    "abcdefghijklmnopqrstuvwxyz0123456789 \n!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

inline int char_index(char ch) {
    unsigned char c = ch;
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    auto i = ALPHA.find(char(c));
    return i == std::string::npos ? -1 : int(i);
}

struct Gram2 { uint8_t a, b; double f; };
struct Gram3 { uint8_t a, b, c; double f; };

struct Ngrams {
    std::vector<double> mono;
    std::vector<Gram2> bi, skip;
    std::vector<Gram3> tri;
};

inline Ngrams count(const std::string& text) {
    const int N = ALPHA.size();
    int idx[256];
    for (int c = 0; c < 256; c++) idx[c] = char_index(char(c));
    std::vector<double> c1(N), c2(N * N), cs(N * N), c3(N * N * N);
    double total = 0;
    int p1 = -1, p2 = -1;
    for (unsigned char ch : text) {
        int c = idx[ch];
        if (c >= 0) {
            c1[c]++, total++;
            if (p1 >= 0) c2[p1 * N + c]++;
            if (p2 >= 0) cs[p2 * N + c]++, c3[(p2 * N + p1) * N + c]++;
        }
        p2 = c >= 0 ? p1 : -1;
        p1 = c;
    }
    Ngrams n;
    if (total == 0) return n;
    for (int i = 0; i < N; i++) n.mono.push_back(c1[i] / total);
    for (int i = 0; i < N * N; i++) {
        if (c2[i]) n.bi.push_back({uint8_t(i / N), uint8_t(i % N), c2[i] / total});
        if (cs[i]) n.skip.push_back({uint8_t(i / N), uint8_t(i % N), cs[i] / total});
    }
    for (int i = 0; i < N * N * N; i++)
        if (c3[i]) n.tri.push_back({uint8_t(i / (N * N)), uint8_t(i / N % N), uint8_t(i % N), c3[i] / total});
    return n;
}

struct Layout {
    std::vector<int> pos;  // char -> slot, or -1
    std::vector<int> occ;  // slot -> char, or -1
};

// A slot is one key on one layer; several slots share a physical key.
struct Model {
    int nslots = 0, nphys = 0;
    std::vector<int> phys;
    std::vector<float> mono, bi;    // by slot
    std::vector<float> skip, tri;   // by physical key
    std::vector<uint8_t> allowed;   // [char * nslots + slot]
    std::vector<int> pinned;        // char -> slot, or -1
};

class Scorer {
public:
    const Model& m;
    Ngrams n;

    Scorer(const Model& model, Ngrams grams) : m(model), n(std::move(grams)), of(ALPHA.size()) {
        for (auto& g : n.bi) for (int c : distinct(g)) of[c].bi.push_back(g);
        for (auto& g : n.skip) for (int c : distinct(g)) of[c].skip.push_back(g);
        for (auto& g : n.tri) for (int c : distinct(g)) of[c].tri.push_back(g);
    }

    float bi(int a, int b) const { return m.bi[a * m.nslots + b]; }
    float skip(int a, int b) const { return m.skip[m.phys[a] * m.nphys + m.phys[b]]; }
    float tri(int a, int b, int c) const { return m.tri[(m.phys[a] * m.nphys + m.phys[b]) * m.nphys + m.phys[c]]; }

    double score(const std::vector<int>& p) const {
        double t = 0;
        for (size_t c = 0; c < n.mono.size(); c++)
            if (n.mono[c] > 0) t += n.mono[c] * m.mono[p[c]];
        for (auto& g : n.bi) t += g.f * bi(p[g.a], p[g.b]);
        for (auto& g : n.skip) t += g.f * skip(p[g.a], p[g.b]);
        for (auto& g : n.tri) t += g.f * tri(p[g.a], p[g.b], p[g.c]);
        return t;
    }

    // Swaps the contents of two slots and returns the change in score.
    double swap(Layout& L, int s1, int s2) const {
        int x = L.occ[s1], y = L.occ[s2];
        std::swap(L.occ[s1], L.occ[s2]);
        if (x >= 0) L.pos[x] = s2;
        if (y >= 0) L.pos[y] = s1;
        const auto& p = L.pos;
        auto old = [&](int c) { return c == x ? s1 : c == y ? s2 : p[c]; };
        double d = 0;
        for (int c : {x, y}) {
            if (c < 0) continue;
            int other = c == y ? x : -1;
            auto& l = of[c];
            d += n.mono[c] * (m.mono[p[c]] - m.mono[old(c)]);
            for (auto& g : l.bi)
                if (!has(g, other)) d += g.f * (bi(p[g.a], p[g.b]) - bi(old(g.a), old(g.b)));
            for (auto& g : l.skip)
                if (!has(g, other)) d += g.f * (skip(p[g.a], p[g.b]) - skip(old(g.a), old(g.b)));
            for (auto& g : l.tri)
                if (!has(g, other)) d += g.f * (tri(p[g.a], p[g.b], p[g.c]) - tri(old(g.a), old(g.b), old(g.c)));
        }
        return d;
    }

private:
    struct Lists {
        std::vector<Gram2> bi, skip;
        std::vector<Gram3> tri;
    };
    std::vector<Lists> of;  // n-grams containing each character

    static bool has(const Gram2& g, int c) { return g.a == c || g.b == c; }
    static bool has(const Gram3& g, int c) { return g.a == c || g.b == c || g.c == c; }
    static std::vector<int> distinct(const Gram2& g) { return g.a == g.b ? std::vector<int>{g.a} : std::vector<int>{g.a, g.b}; }
    static std::vector<int> distinct(const Gram3& g) {
        std::vector<int> v{g.a};
        if (g.b != g.a) v.push_back(g.b);
        if (g.c != g.a && g.c != g.b) v.push_back(g.c);
        return v;
    }
};

// Keeps the most frequent trigrams that together make up `coverage` of all trigram occurrences.
inline void prune_trigrams(Ngrams& n, double coverage) {
    std::sort(n.tri.begin(), n.tri.end(), [](auto& a, auto& b) { return a.f > b.f; });
    double total = 0, kept = 0;
    for (auto& g : n.tri) total += g.f;
    size_t k = 0;
    while (k < n.tri.size() && kept < coverage * total * (1 - 1e-12)) kept += n.tri[k++].f;
    n.tri.resize(k);
}

inline bool placeable(const Model& m, int c) {
    if (m.pinned[c] >= 0) return true;
    for (int s = 0; s < m.nslots; s++)
        if (m.allowed[c * m.nslots + s]) return true;
    return false;
}

inline std::vector<int> movable_slots(const Model& m) {
    std::vector<int> out;
    for (int s = 0; s < m.nslots; s++) {
        bool pinned = false, open = false;
        for (size_t c = 0; c < ALPHA.size(); c++) {
            pinned |= m.pinned[c] == s;
            open |= m.pinned[c] < 0 && m.allowed[c * m.nslots + s];
        }
        if (open && !pinned) out.push_back(s);
    }
    return out;
}

inline bool valid_swap(const Model& m, const Layout& L, int s1, int s2) {
    int x = L.occ[s1], y = L.occ[s2];
    if (s1 == s2 || (x < 0 && y < 0)) return false;
    return (x < 0 || m.allowed[x * m.nslots + s2]) && (y < 0 || m.allowed[y * m.nslots + s1]);
}

template <class Rng>
Layout random_layout(const Model& m, Rng& rng) {
    const int N = ALPHA.size();
    Layout L{std::vector<int>(N, -1), std::vector<int>(m.nslots, -1)};
    std::vector<std::pair<int, int>> order;  // (allowed slot count, char): most constrained first
    for (int c = 0; c < N; c++) {
        if (m.pinned[c] >= 0) L.pos[c] = m.pinned[c], L.occ[m.pinned[c]] = c;
        else if (placeable(m, c)) {
            int k = 0;
            for (int s = 0; s < m.nslots; s++) k += m.allowed[c * m.nslots + s];
            order.push_back({k, c});
        }
    }
    std::shuffle(order.begin(), order.end(), rng);
    std::stable_sort(order.begin(), order.end(), [](auto& a, auto& b) { return a.first < b.first; });
    for (auto [k, c] : order) {
        std::vector<int> open;
        for (int s = 0; s < m.nslots; s++)
            if (L.occ[s] < 0 && m.allowed[c * m.nslots + s]) open.push_back(s);
        if (open.empty()) throw std::runtime_error("no free slot for '" + std::string(1, ALPHA[c]) + "'");
        int s = open[rng() % open.size()];
        L.pos[c] = s, L.occ[s] = c;
    }
    return L;
}

struct Result {
    Layout layout;
    double score;
};

template <class Rng>
Result anneal(const Scorer& sc, Rng& rng, long iters) {
    const Model& m = sc.m;
    const auto slots = movable_slots(m);
    Layout L = random_layout(m, rng);
    int a, b;
    auto propose = [&] {
        do a = slots[rng() % slots.size()], b = slots[rng() % slots.size()];
        while (!valid_swap(m, L, a, b));
    };
    double uphill = 0;
    int n_up = 0;
    for (int i = 0; i < 1000; i++) {
        propose();
        double d = sc.swap(L, a, b);
        sc.swap(L, a, b);
        if (d > 0) uphill += d, n_up++;
    }
    double t = n_up ? uphill / n_up : 1;
    const double cool = std::pow(1e-4, 1.0 / iters);
    double cur = sc.score(L.pos), best_score = cur;
    Layout best = L;
    std::uniform_real_distribution<double> u(0, 1);
    for (long i = 0; i < iters; i++, t *= cool) {
        propose();
        double d = sc.swap(L, a, b);
        if (d <= 0 || u(rng) < std::exp(-d / t)) {
            cur += d;
            if (cur < best_score) best = L, best_score = cur;
        } else {
            sc.swap(L, a, b);
        }
    }
    return {best, sc.score(best.pos)};
}

}
