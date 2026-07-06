#pragma once
#include "FloorPlan.h"
#include <vector>
#include <random>

class IsovistPlacer {
public:
    IsovistPlacer(const FloorPlan& floorPlan, int n = 400, int candidates = 7, unsigned int seed = 42);

    // Runs Mitchell's best-candidate algorithm and returns isovist centers.
    std::vector<Point> place();

    const std::vector<Point>& allIsovists() const { return m_allIsovists; }

private:
    const FloorPlan& m_floorPlan;
    int m_n;
    int m_numCandidates;
    std::mt19937 m_rng;

    std::vector<Point> m_allIsovists;

    Point randomValidPoint();
    double minDistToExisting(const Point& p) const;
};
