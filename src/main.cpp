#include "kl.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>

using namespace kl;

struct Weights {
    double effort = 1, sfb = 4, sfs = 1.5, scissor = 2, lsb = 1;
    double inroll = -0.6, outroll = -0.3, roll3 = -0.3, redirect = 1.5, bad_redirect = 1.5;
    double layer_switch = 1, thumb_sfb = 2;

    std::map<std::string, double*> table() {
        return {{"effort", &effort}, {"sfb", &sfb}, {"sfs", &sfs}, {"scissor", &scissor},
                {"lsb", &lsb}, {"inroll", &inroll}, {"outroll", &outroll}, {"roll3", &roll3},
                {"redirect", &redirect}, {"bad_redirect", &bad_redirect},
                {"layer_switch", &layer_switch}, {"thumb_sfb", &thumb_sfb}};
    }
};

// Physical keys: 2 hands x 4 rows (number, top, home, bottom) x 6 columns
// (outer pinky .. inner index), then 2 x 6 thumb keys.
// Both thumbs hold the same symbol layer, so the free thumb holds it and no thumb conflict is modelled.
constexpr int NP = 60, LAYERS = 2;
const char* LAYER_NAME[LAYERS] = {"base", "sym"};
const double EFFORT[4][6] = {
    {6.0, 4.5, 3.5, 3.0, 3.5, 4.5},
    {4.0, 2.6, 1.8, 1.4, 1.6, 2.4},
    {2.8, 1.6, 1.2, 1.0, 1.0, 1.9},
    {4.0, 2.8, 2.0, 1.7, 1.5, 2.6},
};

int key(int hand, int row, int col) { return hand * 24 + row * 6 + col; }
int thumb_key(int hand, int t) { return 48 + hand * 6 + t; }
bool is_thumb(int p) { return p >= 48; }
int hand_of(int p) { return is_thumb(p) ? (p - 48) / 6 : p / 24; }
int row_of(int p) { return p % 24 / 6; }
int col_of(int p) { return p % 6; }
int finger(int p) { return is_thumb(p) ? 4 : col_of(p) <= 1 ? 0 : std::min(col_of(p) - 1, 3); }
double dist(int a, int b) { return std::hypot(row_of(a) - row_of(b), col_of(a) - col_of(b)); }

const int TAB = key(0, 1, 0), SPACE = thumb_key(1, 5), ENTER = thumb_key(1, 4);
// Where the digits sit on the symbol layer: "home" (12345 | 67890 on the home rows),
// "numpad-left" / "numpad-right" (789/456/123, 0 under the pinky), or "free" (optimized).
std::vector<std::pair<char, int>> digit_keys(const std::string& mode) {
    std::vector<std::pair<char, int>> out;
    if (mode == "home") {
        for (int i = 0; i < 5; i++) out.push_back({char('1' + i), key(0, 2, 1 + i)});
        for (int i = 0; i < 5; i++) out.push_back({"67890"[i], key(1, 2, 5 - i)});
    } else if (mode == "numpad-left" || mode == "numpad-right") {
        int hand = mode == "numpad-right";
        out.push_back({'0', key(hand, 2, 1)});
        const char* rows[] = {"789", "456", "123"};
        for (int r = 0; r < 3; r++)
            for (int i = 0; i < 3; i++) out.push_back({rows[r][i], key(hand, r + 1, hand ? 4 - i : 2 + i)});
    } else if (mode != "free") {
        throw std::runtime_error("unknown digit mode " + mode);
    }
    return out;
}

struct Geo {
    const Weights& w;

    bool sfb(int a, int b) const { return a != b && !is_thumb(a) && !is_thumb(b) && hand_of(a) == hand_of(b) && finger(a) == finger(b); }
    bool same_hand_pair(int a, int b) const {
        return !is_thumb(a) && !is_thumb(b) && hand_of(a) == hand_of(b) && finger(a) != finger(b);
    }
    bool scissor(int a, int b) const { return same_hand_pair(a, b) && std::abs(finger(a) - finger(b)) == 1 && std::abs(row_of(a) - row_of(b)) >= 2; }
    bool lsb(int a, int b) const {
        auto wide = [](int p) { return col_of(p) == 0 || col_of(p) == 5; };
        return same_hand_pair(a, b) && std::abs(finger(a) - finger(b)) == 1 && (wide(a) || wide(b));
    }
    // 1 roll in one direction, 2 redirect, 3 redirect without index, 0 neither
    int trigram(int a, int b, int c) const {
        if (!same_hand_pair(a, b) || !same_hand_pair(b, c) || hand_of(a) != hand_of(c)) return 0;
        bool same_dir = (finger(b) > finger(a)) == (finger(c) > finger(b));
        if (same_dir) return 1;
        return finger(a) == 3 || finger(b) == 3 || finger(c) == 3 ? 2 : 3;
    }

    double rel(int a, int b) const {
        if (a == b || hand_of(a) != hand_of(b)) return 0;
        if (is_thumb(a) && is_thumb(b)) return w.thumb_sfb;
        if (is_thumb(a) || is_thumb(b)) return 0;
        if (finger(a) == finger(b)) return w.sfb * dist(a, b);
        double c = finger(b) > finger(a) ? w.inroll : w.outroll;
        if (scissor(a, b)) c += w.scissor;
        if (lsb(a, b)) c += w.lsb;
        return c;
    }

    double bigram(int la, int a, int lb, int b) const { return rel(a, b) + (la != lb ? w.layer_switch : 0); }
};

Model build_model(const Weights& w, const std::string& digits, bool digits_on_base) {
    Geo g{w};
    const int N = ALPHA.size(), S = LAYERS * NP;
    Model m;
    m.nslots = S, m.nphys = NP;
    for (int s = 0; s < S; s++) m.phys.push_back(s % NP);
    m.mono.assign(S, 0);
    m.bi.assign(S * S, 0);
    m.skip.assign(NP * NP, 0);
    m.tri.assign(NP * NP * NP, 0);
    m.allowed.assign(N * S, 0);
    m.pinned.assign(N, -1);

    for (int s = 0; s < S; s++) {
        int l = s / NP, p = s % NP;
        if (!is_thumb(p)) m.mono[s] = w.effort * EFFORT[row_of(p)][col_of(p)];
        for (int t = 0; t < S; t++) m.bi[s * S + t] = g.bigram(l, p, t / NP, t % NP);
    }
    for (int a = 0; a < NP; a++)
        for (int b = 0; b < NP; b++) {
            if (g.sfb(a, b)) m.skip[a * NP + b] = w.sfs * dist(a, b);
            for (int c = 0; c < NP; c++) {
                int k = g.trigram(a, b, c);
                m.tri[(a * NP + b) * NP + c] = k == 1 ? w.roll3 : k == 2 ? w.redirect : k == 3 ? w.redirect + w.bad_redirect : 0;
            }
        }

    m.pinned[char_index(' ')] = SPACE;
    m.pinned[char_index('\n')] = ENTER;
    for (auto [d, p] : digit_keys(digits)) m.pinned[char_index(d)] = NP + p;
    for (int c = 0; c < N; c++) {
        if (m.pinned[c] >= 0) continue;
        bool letter = ALPHA[c] >= 'a' && ALPHA[c] <= 'z', digit = ALPHA[c] >= '0' && ALPHA[c] <= '9';
        for (int s = 0; s < S; s++) {
            int l = s / NP, p = s % NP;
            bool taken = std::find(m.pinned.begin(), m.pinned.end(), s) != m.pinned.end();
            m.allowed[c * S + s] = !is_thumb(p) && !(l == 0 && p == TAB) && !(letter && l > 0) &&
                                   !(digit && l == 0 && !digits_on_base) && !taken;
        }
    }
    return m;
}

std::string token(const Layout& L, int s) {
    if (s % NP == TAB && s < NP) return "⇥";
    return L.occ[s] < 0 ? "·" : std::string(1, ALPHA[L.occ[s]]);
}

void print_layout(const Layout& L, std::ostream& out) {
    for (int l = 0; l < LAYERS; l++) {
        out << LAYER_NAME[l] << ":\n";
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 6; c++) out << token(L, l * NP + key(0, r, c)) << ' ';
            out << ' ';
            for (int c = 5; c >= 0; c--) out << ' ' << token(L, l * NP + key(1, r, c));
            out << '\n';
        }
    }
}

Layout parse_layout(const Model& m, std::istream& in) {
    Layout L{std::vector<int>(ALPHA.size(), -1), std::vector<int>(m.nslots, -1)};
    for (size_t c = 0; c < ALPHA.size(); c++)
        if (m.pinned[c] >= 0) L.pos[c] = m.pinned[c], L.occ[m.pinned[c]] = c;
    std::string line;
    int l = -1, r = 0;
    while (std::getline(in, line)) {
        for (int k = 0; k < LAYERS; k++)
            if (line == std::string(LAYER_NAME[k]) + ":") l = k, r = 0;
        if (l < 0 || line.empty() || line.back() == ':' || r >= 4) continue;
        std::istringstream ss(line);
        std::string tok;
        for (int i = 0; ss >> tok && i < 12; i++) {
            int s = l * NP + (i < 6 ? key(0, r, i) : key(1, r, 11 - i));
            if (tok.size() != 1) continue;
            int c = char_index(tok[0]);
            if (c < 0 || m.pinned[c] >= 0) continue;
            if (!m.allowed[c * m.nslots + s] || L.occ[s] >= 0) throw std::runtime_error("bad position for '" + tok + "'");
            L.pos[c] = s, L.occ[s] = c;
        }
        r++;
    }
    for (size_t c = 0; c < ALPHA.size(); c++)
        if (L.pos[c] < 0 && placeable(m, c)) throw std::runtime_error("layout is missing '" + std::string(1, ALPHA[c]) + "'");
    return L;
}

void print_stats(const Scorer& sc, const Weights& w, const Layout& L) {
    Geo g{w};
    const auto& n = sc.n;
    auto P = [&](int c) { return L.pos[c] % NP; };
    auto Lr = [&](int c) { return L.pos[c] / NP; };
    double effort = 0, left = 0, keys = 0, fingers[10] = {};
    for (size_t c = 0; c < n.mono.size(); c++) {
        if (n.mono[c] == 0 || is_thumb(P(c))) continue;
        effort += n.mono[c] * EFFORT[row_of(P(c))][col_of(P(c))];
        keys += n.mono[c];
        if (hand_of(P(c)) == 0) left += n.mono[c];
        fingers[hand_of(P(c)) * 5 + finger(P(c))] += n.mono[c];
    }
    double sfb = 0, in = 0, out = 0, sc_ = 0, lsb = 0, alt = 0, presses = 0;
    for (auto& b : n.bi) {
        int a = P(b.a), c = P(b.b);
        if (g.sfb(a, c)) sfb += b.f;
        if (g.same_hand_pair(a, c)) (finger(c) > finger(a) ? in : out) += b.f;
        if (g.scissor(a, c)) sc_ += b.f;
        if (g.lsb(a, c)) lsb += b.f;
        if (!is_thumb(a) && !is_thumb(c) && hand_of(a) != hand_of(c)) alt += b.f;
        if (Lr(b.a) == 0 && Lr(b.b) == 1) presses += b.f;
    }
    double sfs = 0, roll3 = 0, red = 0, bad = 0;
    for (auto& s : n.skip)
        if (g.sfb(P(s.a), P(s.b))) sfs += s.f;
    for (auto& t : n.tri) {
        int k = g.trigram(P(t.a), P(t.b), P(t.c));
        if (k == 1) roll3 += t.f;
        if (k == 2) red += t.f;
        if (k == 3) bad += t.f;
    }
    auto pct = [](double x) { return 100 * x; };
    std::printf("score %.5f\n", sc.score(L.pos));
    std::printf("effort/char %.3f   left hand %.1f%%\n", effort / keys, pct(left / keys));
    std::printf("sfb %.2f%%  sfs %.2f%%  scissor %.2f%%  lsb %.2f%%\n", pct(sfb), pct(sfs), pct(sc_), pct(lsb));
    std::printf("inroll %.2f%%  outroll %.2f%%  alternate %.2f%%\n", pct(in), pct(out), pct(alt));
    std::printf("roll3 %.2f%%  redirect %.2f%%  bad redirect %.2f%%\n", pct(roll3), pct(red), pct(bad));
    std::printf("layer key presses %.2f per 100 chars\n", pct(presses));
    std::printf("finger load (L pinky..index, R index..pinky):");
    for (int f : {0, 1, 2, 3, 8, 7, 6, 5}) std::printf(" %.1f", pct(fingers[f] / keys));
    std::printf("\n");
}

Result optimize(const Scorer& sc, int threads, int restarts, long iters, uint64_t seed) {
    std::atomic<int> next{0};
    std::mutex mu;
    Result best{{}, 1e300};
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; t++)
        pool.emplace_back([&] {
            for (int k; (k = next++) < restarts;) {
                std::mt19937_64 rng(seed + k);
                Result r = anneal(sc, rng, iters);
                std::lock_guard<std::mutex> lock(mu);
                bool better = r.score < best.score;
                if (better) best = r;
                std::fprintf(stderr, "restart %3d  %.5f%s\n", k, r.score, better ? "  best" : "");
            }
        });
    for (auto& th : pool) th.join();
    return best;
}

std::string slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot read " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int usage() {
    std::fprintf(stderr,
                 "usage: kl opt CORPUS [-t threads] [-r restarts] [-n iters] [-s seed] [-c trigram coverage]\n"
                 "              [-d free|home|numpad-left|numpad-right] [-w name=value]...\n"
                 "       kl score CORPUS LAYOUT [-w name=value]...\n");
    return 2;
}

int main(int argc, char** argv) try {
    if (argc < 3) return usage();
    std::string cmd = argv[1];
    std::vector<std::string> pos;
    int threads = std::thread::hardware_concurrency(), restarts = 0;
    double coverage = 0.98;
    std::string digits = "free";
    long iters = 2'000'000;
    uint64_t seed = 1;
    Weights w;
    auto weights = w.table();
    for (int i = 2; i < argc; i++) {
        std::string a = argv[i];
        auto val = [&] { if (i + 1 >= argc) throw std::runtime_error("missing value for " + a); return std::string(argv[++i]); };
        if (a == "-t") threads = std::stoi(val());
        else if (a == "-r") restarts = std::stoi(val());
        else if (a == "-n") iters = std::stol(val());
        else if (a == "-s") seed = std::stoull(val());
        else if (a == "-c") coverage = std::stod(val());
        else if (a == "-d") digits = val();
        else if (a == "-w") {
            auto kv = val();
            auto eq = kv.find('=');
            if (eq == std::string::npos || !weights.count(kv.substr(0, eq))) throw std::runtime_error("unknown weight " + kv);
            *weights[kv.substr(0, eq)] = std::stod(kv.substr(eq + 1));
        } else pos.push_back(a);
    }
    if (restarts == 0) restarts = threads;
    bool scoring = cmd == "score";
    Model m = build_model(w, scoring ? "free" : digits, scoring);
    Ngrams full = count(slurp(pos.at(0))), grams = full;
    prune_trigrams(grams, coverage);
    Scorer sc(m, std::move(grams)), exact(m, std::move(full));
    if (cmd == "opt" && pos.size() == 1) {
        auto t0 = std::chrono::steady_clock::now();
        Result r = optimize(sc, threads, restarts, iters, seed);
        double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::fprintf(stderr, "%.1f s, %.2fM swaps/s\n", secs, restarts * double(iters) / secs / 1e6);
        print_layout(r.layout, std::cout);
        print_stats(exact, w, r.layout);
    } else if (cmd == "score" && pos.size() == 2) {
        std::ifstream f(pos[1]);
        if (!f) throw std::runtime_error("cannot read " + pos[1]);
        Layout L = parse_layout(m, f);
        print_layout(L, std::cout);
        print_stats(exact, w, L);
    } else {
        return usage();
    }
} catch (const std::exception& e) {
    std::fprintf(stderr, "kl: %s\n", e.what());
    return 1;
}
