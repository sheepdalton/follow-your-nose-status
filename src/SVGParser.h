#pragma once
#include "FloorPlan.h"
#include <string>
#include <vector>

class SVGParser {
public:
    FloorPlan parse(const std::string& filepath);

private:
    // Parses an SVG path d attribute into a Polygon.
    // Only handles M, L, z/Z commands (no curves).
    Polygon parsePath(const std::string& d);

    // Collects all <path> elements recursively from an XML node.
    void collectPaths(void* node, std::vector<Polygon>& out);
};
