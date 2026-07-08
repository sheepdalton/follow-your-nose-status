#pragma once
#include "Point.h"
#include "Polygon.h"
#include <vector>

// Result of a Nose Integration single-path query.
struct NoseResult {
    std::vector<int>    path;       // node indices from origin to dest
    std::vector<double> edgeCosts;  // cost of each step = angle(node→D, edge) / 90°
    std::vector<int>    topoDepths; // BFS hop-depth to dest of each path node
    double              totalDepth; // sum of edge costs
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

    // Sizes of the connected components, largest first.
    // A fully connected graph returns a single entry equal to nodeCount().
    std::vector<int> componentSizes() const;

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

    // BFS hop-depth of every node from dest (-1 = unreachable).
    std::vector<int> computeTopoDepths(int dest) const;

    // Topological status (space syntax total depth / integration):
    // for each node, the sum of BFS hop-distances to every other node.
    // Low = topologically integrated, high = segregated.
    std::vector<double> computeTopoStatus() const;

    // Polar path: like the nose path but the cost of edge A→B is
    //   (angle(A→D, A→B) / 90°) × euclidean_distance(A, B)
    // so the best candidate is both in the right direction AND close.
    // No topological depth constraint — pure Dijkstra over all neighbours.
    NoseResult computePolarPath(int origin, int dest,
                                const std::vector<Point>& centers) const;

    // Polar centrality status: for every destination D, sum over all
    // origins O of (a) the cumulative angle and (b) the cumulative
    // angle×distance product along the polar-optimal route O→D.
    // Results are indexed by destination node.
    void computePolarStatus(const std::vector<Point>& centers,
                            std::vector<double>& statusAngle,
                            std::vector<double>& statusProduct) const;

    // Full A-choice accumulation: runs angular Dijkstra from every source,
    // traces back the least-turn path to every destination, and accumulates
    // 1.0 on each intermediate node.  Produces a heatmap metric analogous
    // to betweenness but using angular cost rather than hop count.
    std::vector<double> computeAChoice(const std::vector<Point>& centers) const;

private:
    int m_n;
    int m_edgeCount = 0;
    std::vector<std::vector<int>> m_adj;
};
