#pragma once
#include "Polygon.h"
#include <vector>
#include <string>

class FloorPlan {
public:
    Polygon outerBoundary;
    std::vector<Polygon> obstacles;

    // SVG document metadata preserved for export
    std::string svgWidth;
    std::string svgHeight;
    std::string svgViewBox;

    bool isValidPoint(const Point& p) const;
    Polygon::BBox boundingBox() const;
    double openArea() const;
};
