#pragma once
#include "Point.h"
#include <vector>

class Polygon {
public:
    struct BBox {
        double minX, minY, maxX, maxY;
    };

    void addVertex(const Point& p);
    double area() const;
    double perimeter() const;
    bool containsPoint(const Point& p) const;
    BBox boundingBox() const;

    const std::vector<Point>& vertices() const { return m_vertices; }
    bool empty() const { return m_vertices.empty(); }
    size_t size() const { return m_vertices.size(); }

private:
    std::vector<Point> m_vertices;
};
