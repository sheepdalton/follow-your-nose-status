#include "VisibilityGraph.h"
#include <stdexcept>
#include <iostream>
#include <queue>
#include <stack>
#include <unordered_set>
#include <cmath>
#include <limits>
#include <algorithm>
#include <map>
#include <random>

VisibilityGraph::VisibilityGraph(int n) : m_n(n), m_adj(n) {}

void VisibilityGraph::build(const std::vector<Polygon>& polygons,
                             const std::vector<Point>&  centers) {
    if (static_cast<int>(polygons.size()) != m_n ||
        static_cast<int>(centers.size())  != m_n)
        throw std::runtime_error("VisibilityGraph::build: size mismatch.");

    m_edgeCount = 0;
    for (auto& list : m_adj) list.clear();

    std::cout << "Building visibility graph (" << m_n << " nodes)...\n";

    for (int i = 0; i < m_n; ++i) {
        for (int j = i + 1; j < m_n; ++j) {
            // Edge exists if center j is visible from i (inside i's isovist polygon).
            // By symmetry of 2-D visibility this equals center i visible from j.
            if (polygons[i].containsPoint(centers[j])) {
                m_adj[i].push_back(j);
                m_adj[j].push_back(i);
                ++m_edgeCount;
            }
        }
        if ((i + 1) % 50 == 0)
            std::cout << "  " << (i+1) << " / " << m_n << " rows done\n";
    }

    std::cout << "Visibility graph: " << m_edgeCount << " edges\n";
}

int VisibilityGraph::degree(int node) const {
    return static_cast<int>(m_adj[node].size());
}

const std::vector<int>& VisibilityGraph::neighbours(int node) const {
    return m_adj[node];
}

// Brandes' betweenness centrality algorithm (Brandes 2001).
//
// For each source s, BFS builds:
//   d[v]    — hop distance from s
//   sigma[v] — number of shortest paths from s to v
//   pred[v]  — predecessors of v on shortest paths from s
//
// Back-propagation then distributes dependency scores delta[v] back
// through predecessors. Each node w ≠ s accumulates its share of flow
// from every (s,*) pair it sits on.
std::vector<double> VisibilityGraph::computeChoice() const {
    std::vector<double> choice(m_n, 0.0);

    std::vector<double>              sigma(m_n);
    std::vector<int>                 d(m_n);
    std::vector<double>              delta(m_n);
    std::vector<std::vector<int>>    pred(m_n);

    std::cout << "Computing choice (betweenness)...\n";

    for (int s = 0; s < m_n; ++s) {
        // --- initialise per-source structures ---
        for (int v = 0; v < m_n; ++v) {
            sigma[v] = 0.0;
            d[v]     = -1;
            delta[v] = 0.0;
            pred[v].clear();
        }
        sigma[s] = 1.0;
        d[s]     = 0;

        std::queue<int> Q;
        std::stack<int> S;
        Q.push(s);

        // --- BFS ---
        while (!Q.empty()) {
            int v = Q.front(); Q.pop();
            S.push(v);

            for (int w : m_adj[v]) {
                if (d[w] < 0) {           // first visit to w
                    d[w] = d[v] + 1;
                    Q.push(w);
                }
                if (d[w] == d[v] + 1) {   // w is on a shortest path via v
                    sigma[w] += sigma[v];
                    pred[w].push_back(v);
                }
            }
        }

        // --- back-propagation ---
        while (!S.empty()) {
            int w = S.top(); S.pop();
            for (int v : pred[w]) {
                delta[v] += (sigma[v] / sigma[w]) * (1.0 + delta[w]);
            }
            if (w != s)
                choice[w] += delta[w];
        }

        if ((s + 1) % 50 == 0)
            std::cout << "  " << (s+1) << " / " << m_n << " sources done\n";
    }

    std::cout << "Choice computation complete.\n";
    return choice;
}

// Distributed choice (d-choice).
//
// Identical to Brandes' choice up to and including the BFS and the recursive
// delta (dependency) computation.  The only difference is in the accumulation
// step: in standard choice, node v collects its own delta[v]; in d-choice,
// every neighbour of v collects delta[v] instead.  The intuition is that the
// flow passing through v is "visible" to — and therefore credited to — all
// nodes directly connected to v.
std::vector<double> VisibilityGraph::computeDChoice() const {
    std::vector<double> dchoice(m_n, 0.0);

    std::cout << "Computing d-choice...\n";

    for (int s = 0; s < m_n; ++s) {

        // ── Step 1: BFS from source s ────────────────────────────────────────
        // sigma[v] : number of shortest paths from s to v
        // dist[v]  : hop distance from s to v  (-1 = not yet reached)
        // succ[v]  : nodes one step further from s on a shortest path through v
        std::vector<double>           sigma(m_n, 0.0);
        std::vector<int>              dist(m_n, -1);
        std::vector<std::vector<int>> succ(m_n);

        sigma[s] = 1.0;
        dist[s]  = 0;

        std::queue<int> Q;
        Q.push(s);
        while (!Q.empty()) {
            int v = Q.front(); Q.pop();
            for (int w : m_adj[v]) {
                if (dist[w] < 0) {            // first time reaching w
                    dist[w] = dist[v] + 1;
                    Q.push(w);
                }
                if (dist[w] == dist[v] + 1) { // w is one hop further than v
                    sigma[w] += sigma[v];
                    succ[v].push_back(w);
                }
            }
        }

        // ── Step 2: recursive delta computation (Brandes dependency) ─────────
        // delta[v] is the total pair-dependency of v with respect to source s:
        //   delta[v] = sum over each successor w of (sigma[v]/sigma[w]) * (1 + delta[w])
        // Computed recursively with memoisation so each node is evaluated once.
        std::vector<double> delta(m_n, 0.0);
        std::vector<bool>   visited(m_n, false);

        std::function<double(int)> computeDelta = [&](int v) -> double {
            if (visited[v]) return delta[v];
            visited[v] = true;

            delta[v] = 0.0;
            for (int w : succ[v]) {
                delta[v] += (sigma[v] / sigma[w]) * (1.0 + computeDelta(w));
            }
            return delta[v];
        };

        for (int v = 0; v < m_n; ++v)
            if (v != s) computeDelta(v);

        // ── Step 3: accumulation — spread delta to all neighbours ─────────────
        // Standard choice: dchoice[v] += delta[v]   (v must lie on a shortest path)
        //
        // D-choice: for each node v that has a non-zero delta, every neighbour j
        // of v accumulates delta[v].  j does not need to lie on the shortest path;
        // it only needs to be directly connected (visible) to v.
        for (int v = 0; v < m_n; ++v) {
            if (v == s || dist[v] < 0) continue;

            for (int j : m_adj[v]) {
                if (j == s) continue;
                dchoice[j] += delta[v];
            }
        }

        if ((s + 1) % 50 == 0)
            std::cout << "  " << (s+1) << " / " << m_n << " sources done\n";
    }

    std::cout << "D-choice computation complete.\n";
    return dchoice;
}

// Angular shortest path via Dijkstra (A-choice).
//
// The cost of a step is the turn deviation at the intermediate node:
//   cost = pi - angle( vector(A→V),  vector(A→P) )
// where P is where we came from and V is where we are going.
// This equals 0 for perfectly straight travel and pi for a U-turn.
// We minimise the total cost, giving the straightest possible path.
//
// Note: because the step cost depends on the direction we arrived from,
// the full state is (current_node, previous_node).  We track this in the
// priority queue so the result is globally optimal.
AChoiceResult VisibilityGraph::computeAChoicePath(int origin, int dest,
                                                   const std::vector<Point>& centers) const {
    const double INF = std::numeric_limits<double>::infinity();

    // Helper: turn deviation at node A, arrived from P, going to V.
    // Returns 0 if P == -1 (we are at the origin, no incoming direction yet).
    auto turnCost = [&](int P, int A, int V) -> double {
        if (P < 0) return 0.0;
        double ax = centers[V].x - centers[A].x;   // A→V
        double ay = centers[V].y - centers[A].y;
        double bx = centers[P].x - centers[A].x;   // A→P  (reverse of incoming)
        double by = centers[P].y - centers[A].y;
        double magA = std::sqrt(ax*ax + ay*ay);
        double magB = std::sqrt(bx*bx + by*by);
        if (magA < 1e-12 || magB < 1e-12) return 0.0;
        double cosT = (ax*bx + ay*by) / (magA * magB);
        cosT = std::max(-1.0, std::min(1.0, cosT));
        // angle(A→V, A→P) is pi when going straight, 0 when U-turning.
        // Turn deviation = pi - that angle = 0 straight, pi U-turn.
        return M_PI - std::acos(cosT);
    };

    // State: (node, previous_node).  previous_node == -1 at the origin.
    // dist[node][prev+1] — we use a flat map: key = node * (m_n+1) + (prev+1)
    // For simplicity use a std::map keyed on pair<int,int>.
    using State = std::pair<int,int>;          // (current, previous)
    std::map<State, double>  dist;
    std::map<State, State>   came_from;

    // Min-heap: (cost, current_node, previous_node)
    using Entry = std::tuple<double,int,int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> PQ;

    State startState = {origin, -1};
    dist[startState] = 0.0;
    PQ.push({0.0, origin, -1});

    while (!PQ.empty()) {
        auto [d, u, p] = PQ.top(); PQ.pop();

        State cur = {u, p};
        // Skip if we already found a better route to this state
        if (dist.count(cur) && d > dist.at(cur)) continue;

        if (u == dest) break;   // reached destination

        for (int v : m_adj[u]) {
            if (v == p) continue;   // don't immediately backtrack

            double cost   = d + turnCost(p, u, v);
            State  next   = {v, u};

            if (!dist.count(next) || cost < dist.at(next)) {
                dist[next]      = cost;
                came_from[next] = cur;
                PQ.push({cost, v, u});
            }
        }
    }

    AChoiceResult result;
    result.totalAngle = INF;

    // Find the best state at dest (any incoming direction)
    State bestEnd = {dest, -2};
    for (auto& [state, d] : dist) {
        if (state.first == dest && d < result.totalAngle) {
            result.totalAngle = d;
            bestEnd = state;
        }
    }

    if (result.totalAngle == INF) {
        std::cout << "A-choice: no path found from " << origin << " to " << dest << "\n";
        return result;
    }

    // Reconstruct path by following came_from back to origin
    std::vector<int> reversed;
    State cur = bestEnd;
    while (true) {
        reversed.push_back(cur.first);
        if (cur.first == origin) break;
        cur = came_from.at(cur);
    }
    std::reverse(reversed.begin(), reversed.end());
    result.path = reversed;

    // Compute the turn angle at each intermediate node for reporting.
    // We report angle(A→next, A→prev) in radians — 0 = straight, pi = U-turn.
    for (int i = 1; i + 1 < (int)result.path.size(); ++i) {
        int P = result.path[i-1];
        int A = result.path[i];
        int V = result.path[i+1];
        result.turnAngles.push_back(turnCost(P, A, V));
    }

    return result;
}

// Nose Integration single path.
//
// Cost of edge A→B = angle(A→D, A→B) / 90°
//   0   = edge points directly at destination  (free)
//   1   = edge is perpendicular to destination (one unit of depth)
//   2   = edge points directly away            (two units of depth)
//
// Because cost depends only on (A, B, fixed_destination) — not on the
// incoming direction — standard Dijkstra gives the globally optimal path.
NoseResult VisibilityGraph::computeNosePath(int origin, int dest,
                                             const std::vector<Point>& centers) const {
    const double INF = std::numeric_limits<double>::infinity();

    // Cost of moving from node A to neighbour B, given fixed destination dest.
    auto edgeCost = [&](int A, int B) -> double {
        double dx = centers[dest].x - centers[A].x;  // A→D
        double dy = centers[dest].y - centers[A].y;
        double ex = centers[B].x   - centers[A].x;   // A→B
        double ey = centers[B].y   - centers[A].y;
        double magD = std::sqrt(dx*dx + dy*dy);
        double magE = std::sqrt(ex*ex + ey*ey);
        if (magD < 1e-12 || magE < 1e-12) return 0.0;
        double cosT = (dx*ex + dy*ey) / (magD * magE);
        cosT = std::max(-1.0, std::min(1.0, cosT));
        return std::acos(cosT) * (180.0 / M_PI) / 90.0;  // degrees / 90
    };

    std::vector<double> dist(m_n, INF);
    std::vector<int>    prev(m_n, -1);

    dist[origin] = 0.0;

    using Entry = std::pair<double,int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> PQ;
    PQ.push({0.0, origin});

    while (!PQ.empty()) {
        auto [d, u] = PQ.top(); PQ.pop();
        if (d > dist[u]) continue;
        if (u == dest) break;

        for (int v : m_adj[u]) {
            double cost = d + edgeCost(u, v);
            if (cost < dist[v]) {
                dist[v] = cost;
                prev[v] = u;
                PQ.push({cost, v});
            }
        }
    }

    NoseResult result;
    result.totalDepth = dist[dest];

    if (dist[dest] == INF) {
        std::cout << "Nose: no path from " << origin << " to " << dest << "\n";
        return result;
    }

    // Reconstruct path
    for (int v = dest; v != -1; v = prev[v])
        result.path.push_back(v);
    std::reverse(result.path.begin(), result.path.end());

    // Compute and store each edge cost
    for (int i = 0; i + 1 < (int)result.path.size(); ++i)
        result.edgeCosts.push_back(edgeCost(result.path[i], result.path[i+1]));

    // Assert: the final edge (last node → dest) must have cost 0,
    // because edgeCost(B, D) = angle(B→D, B→D) = 0.
    if (!result.edgeCosts.empty()) {
        double lastCost = result.edgeCosts.back();
        if (lastCost > 1e-6)
            std::cerr << "Nose assert FAILED: last edge cost = " << lastCost
                      << " (expected 0)\n";
        else
            std::cout << "Nose assert OK: last edge cost = " << lastCost << "\n";
    }

    return result;
}

// One noisy greedy walk.
//
// A sequential greedy navigation, not a global Dijkstra: the walker only
// ever knows an imperfect (noisy) direction to the destination, re-sampled
// at every intermediate node.  This simulates poor direction finding.
// The walk ends when the true destination is directly visible (final step
// always correct) or after a safety cap of 4*N steps (walk failed).
NoseResult VisibilityGraph::walkOnce(int origin, int dest,
                                      double sigmaX, double sigmaY,
                                      std::mt19937& rng,
                                      const std::vector<Point>& centers,
                                      std::vector<Point>* samples,
                                      double* maxSpread) const {
    std::normal_distribution<double> noiseX(0.0, sigmaX);
    std::normal_distribution<double> noiseY(0.0, sigmaY);

    // Angular cost (units of 90 deg) between vector A→target and edge A→B.
    auto edgeCost = [&](int A, int B, const Point& target) -> double {
        double dx = target.x - centers[A].x;
        double dy = target.y - centers[A].y;
        double ex = centers[B].x - centers[A].x;
        double ey = centers[B].y - centers[A].y;
        double magD = std::sqrt(dx*dx + dy*dy);
        double magE = std::sqrt(ex*ex + ey*ey);
        if (magD < 1e-12 || magE < 1e-12) return 0.0;
        double cosT = (dx*ex + dy*ey) / (magD * magE);
        cosT = std::max(-1.0, std::min(1.0, cosT));
        return std::acos(cosT) * (180.0 / M_PI) / 90.0;
    };

    auto seesDest = [&](int node) {
        const auto& nb = m_adj[node];
        return std::find(nb.begin(), nb.end(), dest) != nb.end();
    };

    const int MAX_STEPS = 4 * m_n;

    NoseResult walk;
    walk.totalDepth = 0.0;
    int cur = origin;
    walk.path.push_back(cur);

    for (int step = 0; step < MAX_STEPS && cur != dest; ++step) {
        if (seesDest(cur)) {
            // Final step: destination visible, taken exactly (cost 0).
            double c = edgeCost(cur, dest, centers[dest]);
            walk.edgeCosts.push_back(c);
            walk.totalDepth += c;
            cur = dest;
            walk.path.push_back(cur);
            break;
        }

        // Perceived destination = true centre + Gaussian error.
        Point perceived(centers[dest].x + noiseX(rng),
                        centers[dest].y + noiseY(rng));
        if (samples) samples->push_back(perceived);
        if (maxSpread) {
            double sx = perceived.x - centers[dest].x;
            double sy = perceived.y - centers[dest].y;
            *maxSpread = std::max(*maxSpread, std::sqrt(sx*sx + sy*sy));
        }

        // Greedy: edge deviating least from the perceived direction.
        int    best     = -1;
        double bestCost = std::numeric_limits<double>::infinity();
        for (int nb : m_adj[cur]) {
            double c = edgeCost(cur, nb, perceived);
            if (c < bestCost) { bestCost = c; best = nb; }
        }
        if (best < 0) break;   // isolated node — give up

        walk.edgeCosts.push_back(bestCost);
        walk.totalDepth += bestCost;
        cur = best;
        walk.path.push_back(cur);
    }

    return walk;
}

// Retry limit: a walk that fails to arrive is discarded and re-run with
// fresh randomness.  This cap only exists so an unreachable destination
// cannot hang the program.
static const int K_WALK_MAX_RETRIES = 100;

KWalksResult VisibilityGraph::computeKWalks(int origin, int dest, int K,
                                             double sigmaX, double sigmaY, unsigned seed,
                                             const std::vector<Point>& centers) const {
    KWalksResult result;
    std::mt19937 rng(seed);

    for (int k = 0; k < K; ++k) {
        NoseResult walk;
        for (int attempt = 0; attempt < K_WALK_MAX_RETRIES; ++attempt) {
            walk = walkOnce(origin, dest, sigmaX, sigmaY, rng, centers,
                            &result.samples, &result.maxSpread);
            if (!walk.path.empty() && walk.path.back() == dest) break;
            // failed — reset and start again
        }
        result.walks.push_back(std::move(walk));
    }

    return result;
}

// K-path depth: sum of angular depth over K completed walks, for every
// origin node, toward one common destination.
std::vector<double> VisibilityGraph::computeKPathDepth(int dest, int K,
                                                        double sigmaX, double sigmaY,
                                                        unsigned seed,
                                                        const std::vector<Point>& centers) const {
    std::vector<double> depth(m_n, 0.0);
    int failedWalks = 0;

    std::cout << "Computing k-path depth (dest=" << dest << ", K=" << K << ")...\n";

    for (int o = 0; o < m_n; ++o) {
        if (o == dest) continue;

        // Per-origin seed so results don't depend on iteration order.
        std::mt19937 rng(seed + static_cast<unsigned>(o));

        double sum = 0.0;
        for (int k = 0; k < K; ++k) {
            NoseResult walk;
            bool ok = false;
            for (int attempt = 0; attempt < K_WALK_MAX_RETRIES; ++attempt) {
                walk = walkOnce(o, dest, sigmaX, sigmaY, rng, centers,
                                nullptr, nullptr);
                if (!walk.path.empty() && walk.path.back() == dest) { ok = true; break; }
            }
            if (!ok) ++failedWalks;
            sum += walk.totalDepth;
        }
        depth[o] = sum;

        if ((o + 1) % 50 == 0)
            std::cout << "  " << (o+1) << " / " << m_n << " origins done\n";
    }

    if (failedWalks > 0)
        std::cerr << "WARNING: " << failedWalks << " walks never reached the "
                  << "destination even after " << K_WALK_MAX_RETRIES << " retries\n";

    std::cout << "K-path depth complete.\n";
    return depth;
}

// Full A-choice accumulation.
//
// For every ordered pair (s, t) we find the angular shortest path (minimum
// total turn deviation) and add 1.0 to every intermediate node on that path.
// The state in the Dijkstra is (current_node, previous_node) so that the
// turn cost at each step can be computed correctly.
// State is encoded as a single int: node * (m_n+1) + (prev+1), where prev=-1
// at the source.
std::vector<double> VisibilityGraph::computeAChoice(const std::vector<Point>& centers) const {
    std::vector<double> achoice(m_n, 0.0);

    const double INF = std::numeric_limits<double>::infinity();

    // Turn deviation at node A, arrived from P, going to V (P==-1 at source).
    auto turnCost = [&](int P, int A, int V) -> double {
        if (P < 0) return 0.0;
        double ax = centers[V].x - centers[A].x;
        double ay = centers[V].y - centers[A].y;
        double bx = centers[P].x - centers[A].x;
        double by = centers[P].y - centers[A].y;
        double magA = std::sqrt(ax*ax + ay*ay);
        double magB = std::sqrt(bx*bx + by*by);
        if (magA < 1e-12 || magB < 1e-12) return 0.0;
        double cosT = (ax*bx + ay*by) / (magA * magB);
        cosT = std::max(-1.0, std::min(1.0, cosT));
        return M_PI - std::acos(cosT);
    };

    // Encode / decode state (node, prev) as a single int.
    auto encodeState = [&](int node, int prev) { return node * (m_n + 1) + (prev + 1); };
    auto nodeOf      = [&](int k)               { return k / (m_n + 1); };
    auto prevOf      = [&](int k)               { return k % (m_n + 1) - 1; };

    std::cout << "Computing A-choice (all pairs)...\n";

    for (int s = 0; s < m_n; ++s) {

        // ── Angular Dijkstra from source s ───────────────────────────────────
        std::unordered_map<int,double> dist;
        std::unordered_map<int,int>    came_from;

        // For each node: the state key that achieved the minimum cost.
        std::vector<double> bestDist(m_n, INF);
        std::vector<int>    bestKey(m_n, -1);

        int startKey = encodeState(s, -1);
        dist[startKey]   = 0.0;
        bestDist[s]      = 0.0;
        bestKey[s]       = startKey;

        using Entry = std::pair<double,int>;
        std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> PQ;
        PQ.push({0.0, startKey});

        while (!PQ.empty()) {
            auto [d, k] = PQ.top(); PQ.pop();

            if (d > dist[k]) continue;

            int u = nodeOf(k);
            int p = prevOf(k);

            for (int v : m_adj[u]) {
                if (v == p) continue;

                double cost    = d + turnCost(p, u, v);
                int    nextKey = encodeState(v, u);

                auto it = dist.find(nextKey);
                if (it == dist.end() || cost < it->second) {
                    dist[nextKey]      = cost;
                    came_from[nextKey] = k;
                    PQ.push({cost, nextKey});

                    if (cost < bestDist[v]) {
                        bestDist[v] = cost;
                        bestKey[v]  = nextKey;
                    }
                }
            }
        }

        // ── Trace back to every destination, accumulate intermediate nodes ──
        for (int t = 0; t < m_n; ++t) {
            if (t == s || bestKey[t] < 0 || bestDist[t] == INF) continue;

            // Walk came_from from t back to s.
            // Each key decoded gives a node; collect nodes between t and s.
            // Sequence of keys: bestKey[t] → ... → startKey
            // Corresponding nodes: t, node_{n-1}, ..., node_1   (s not included)
            // Intermediate nodes to accumulate: node_{n-1}, ..., node_1
            // (i.e. everything except t and s)
            int cur = bestKey[t];
            bool first = true;
            while (cur != startKey) {
                int node = nodeOf(cur);
                if (!first) {          // skip t itself (first node decoded)
                    // Distributed: spread to all neighbours of this intermediate node,
                    // not just the node itself — same principle as d-choice.
                    for (int nb : m_adj[node]) {
                        achoice[nb] += 1.0;
                    }
                }
                first = false;

                auto cf = came_from.find(cur);
                if (cf == came_from.end()) break;
                cur = cf->second;
            }
        }

        if ((s + 1) % 50 == 0)
            std::cout << "  " << (s+1) << " / " << m_n << " sources done\n";
    }

    std::cout << "A-choice computation complete.\n";
    return achoice;
}
