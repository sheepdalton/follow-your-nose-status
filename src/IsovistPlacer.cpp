#include "IsovistPlacer.h"
#include <limits>
#include <stdexcept>
#include <iostream>

IsovistPlacer::IsovistPlacer(const FloorPlan& floorPlan, int n, int candidates, unsigned int seed)
    : m_floorPlan(floorPlan), m_n(n), m_numCandidates(candidates), m_rng(seed) {}

Point IsovistPlacer::randomValidPoint() {
    Polygon::BBox bbox = m_floorPlan.boundingBox();
    std::uniform_real_distribution<double> distX(bbox.minX, bbox.maxX);
    std::uniform_real_distribution<double> distY(bbox.minY, bbox.maxY);

    const int maxAttempts = 100000;
    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        Point p(distX(m_rng), distY(m_rng));
        if (m_floorPlan.isValidPoint(p))
            return p;
    }
    throw std::runtime_error("Could not find a valid point after many attempts. Check floor plan geometry.");
}

double IsovistPlacer::minDistToExisting(const Point& p) const {
    if (m_allIsovists.empty())
        return std::numeric_limits<double>::max();

    double minDist = std::numeric_limits<double>::max();
    for (const auto& existing : m_allIsovists) {
        double d = p.distanceTo(existing);
        if (d < minDist) minDist = d;
    }
    return minDist;
}

std::vector<Point> IsovistPlacer::place() {
    m_allIsovists.clear();
    m_allIsovists.reserve(m_n);

    for (int i = 0; i < m_n; ++i) {
        Point bestCandidate;
        double bestDist = -1.0;

        for (int c = 0; c < m_numCandidates; ++c) {
            Point candidate = randomValidPoint();
            double dist = minDistToExisting(candidate);
            if (dist > bestDist) {
                bestDist = dist;
                bestCandidate = candidate;
            }
        }

        m_allIsovists.push_back(bestCandidate);

        if ((i + 1) % 50 == 0)
            std::cout << "Placed " << (i + 1) << " / " << m_n << " isovist centers\n";
    }

    return m_allIsovists;
}
