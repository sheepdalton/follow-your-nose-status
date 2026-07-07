#pragma once
#include "Point.h"
#include "Polygon.h"
#include <vector>
#include <random>

// Result of a Nose Integration single-path query.
struct NoseResult {
    std::vector<int>    path;       // node indices from origin to dest
    std::vector<double> edgeCosts;  // cost of each step = angle(node→D, edge) / 90°
    double              totalDepth; // sum of edge costs
};

// Result of K noisy walks (k-ways experiment).
struct KWalksResult {
    std::vector<NoseResult> walks;       // one walk per repetition
    std::vector<Point>      samples;     // every perceived destination sampled
    double                  maxSpread = 0.0; // max distance of any sample from true dest
};

// Result of the angular (A-choice) shortest path query.
struct AChoiceResult {
    std::vector<int>    path;        // node indices from origin to dest
    std::vector<double> turnAngles;  // turn deviation (radians) at each intermediate node
    double              totalAngle;  // sum of turn deviations (radians)
};

// Undirected visibility graph over N isovist nodes.
// Edge (i,j) exists if center[j] lies inside polygon[i].
// Symmetry of visibility means we only test one direction per pair.
class VisibilityGraph {
public:
    explicit VisibilityGraph(int n);

    // Build edges from pre-computed polygons and their viewpoint centers.
    void build(const std::vector<Polygon>& polygons,
               const std::vector<Point>&  centers);

    int degree(int node) const;
    const std::vector<int>& neighbours(int node) const;

    int nodeCount()    const { return m_n; }
    int edgeCount()    const { return m_edgeCount; }
    const std::vector<std::vector<int>>& adjacency() const { return m_adj; }

    // Betweenness centrality via Brandes' algorithm.
    // For every ordered pair (s,t), each intermediate node on a shortest path
    // accumulates 1/k where k is the number of equal-length shortest paths.
    std::vector<double> computeChoice() const;

    // Distributed choice: for every ordered pair (A,B), each node J that is a
    // common neighbour of both A and B (J ≠ A, J ≠ B) accumulates 1.0.
    std::vector<double> computeDChoice() const;

    // Angular shortest path (A-choice): Dijkstra minimising total turn deviation.
    // At each intermediate node A (arrived from P, going to V) the cost is
    //   pi - angle(A→V, A→P)   which is 0 for straight ahead, pi for a U-turn.
    // centers must be the same vector used to build the graph.
    AChoiceResult computeAChoicePath(int origin, int dest,
                                     const std::vector<Point>& centers) const;

    // Nose Integration single path: Dijkstra where the cost of edge A→B is
    //   angle(A→D, A→B) / 90°  (0=straight toward D, 1=perpendicular, 2=away).
    // V (direction to D) is recomputed fresh at each node, not carried from O.
    NoseResult computeNosePath(int origin, int dest,
                               const std::vector<Point>& centers) const;

    // K-ways: simulate K greedy "follow your nose" walks from origin to dest.
    // At each intermediate node the perceived destination is re-sampled from a
    // 2D Gaussian centred on the true destination (sigmaX, sigmaY); the walker
    // takes the edge that deviates least from the perceived direction.
    // When the true destination is directly visible the final step is exact.
    KWalksResult computeKWalks(int origin, int dest, int K,
                               double sigmaX, double sigmaY, unsigned seed,
                               const std::vector<Point>& centers) const;

    // K-path depth: for a fixed destination, every other node acts as an
    // origin; its value is the sum of total angular depth over K completed
    // noisy walks to the destination (failed walks are reset and re-run).
    std::vector<double> computeKPathDepth(int dest, int K,
                                          double sigmaX, double sigmaY,
                                          unsigned seed,
                                          const std::vector<Point>& centers) const;

    // Full A-choice accumulation: runs angular Dijkstra from every source,
    // traces back the least-turn path to every destination, and accumulates
    // 1.0 on each intermediate node.  Produces a heatmap metric analogous
    // to betweenness but using angular cost rather than hop count.
    std::vector<double> computeAChoice(const std::vector<Point>& centers) const;

private:
    int m_n;
    int m_edgeCount = 0;
    std::vector<std::vector<int>> m_adj;

    // One noisy greedy walk from origin toward dest.  Every sampled perceived
    // destination is appended to *samples (if non-null) and *maxSpread updated.
    // Failure to arrive is detectable by path.back() != dest.
    NoseResult walkOnce(int origin, int dest,
                        double sigmaX, double sigmaY,
                        std::mt19937& rng,
                        const std::vector<Point>& centers,
                        std::vector<Point>* samples,
                        double* maxSpread) const;
};
