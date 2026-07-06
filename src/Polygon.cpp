#include "Polygon.h"
#include <cmath>
#include <algorithm>
#include <limits>

void Polygon::addVertex(const Point& p) {
    m_vertices.push_back(p);
}

// Shoelace formula — returns absolute area
double Polygon::area() const {
    int n = static_cast<int>(m_vertices.size());
    if (n < 3) return 0.0;
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        const Point& a = m_vertices[i];
        const Point& b = m_vertices[(i + 1) % n];
        sum += (a.x * b.y) - (b.x * a.y);
    }
    return std::abs(sum) * 0.5;
}

double Polygon::perimeter() const {
    size_t n = m_vertices.size();
    if (n < 2) return 0.0;
    double total = 0.0;
    for (size_t i = 0; i < n; ++i)
        total += m_vertices[i].distanceTo(m_vertices[(i + 1) % n]);
    return total;
}

// Ray-casting algorithm
bool Polygon::containsPoint(const Point& p) const {
    int n = static_cast<int>(m_vertices.size());
    if (n < 3) return false;
    bool inside = false;
    int j = n - 1;
    for (int i = 0; i < n; ++i) {
        const Point& pi = m_vertices[i];
        const Point& pj = m_vertices[j];
        if (((pi.y > p.y) != (pj.y > p.y)) &&
            (p.x < (pj.x - pi.x) * (p.y - pi.y) / (pj.y - pi.y) + pi.x)) {
            inside = !inside;
        }
        j = i;
    }
    return inside;
}

Polygon::BBox Polygon::boundingBox() const {
    BBox bbox{
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::lowest(),
        std::numeric_limits<double>::lowest()
    };
    for (const auto& v : m_vertices) {
        bbox.minX = std::min(bbox.minX, v.x);
        bbox.minY = std::min(bbox.minY, v.y);
        bbox.maxX = std::max(bbox.maxX, v.x);
        bbox.maxY = std::max(bbox.maxY, v.y);
    }
    return bbox;
}
