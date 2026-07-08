#pragma once
#include "Point.h"
#include "../vendor/pugixml.hpp"
#include <string>
#include <sstream>
#include <algorithm>
#include <vector>

// Places a measure-name label at the top-left, just above the content
// bounding box, clamped inside the SVG viewBox so it isn't clipped.
inline void addMeasureLabelAt(pugi::xml_node svgNode, const std::string& label,
                              double minX, double minY) {
    if (label.empty()) return;

    double x = minX;
    double y = minY - 8.0;   // just above the bounding box

    // Clamp inside the viewBox (if present) so the label stays visible.
    std::string vb = svgNode.attribute("viewBox").as_string();
    if (!vb.empty()) {
        std::replace(vb.begin(), vb.end(), ',', ' ');
        std::istringstream ss(vb);
        double vx = 0, vy = 0, vw = 0, vh = 0;
        if (ss >> vx >> vy >> vw >> vh) {
            x = std::max(x, vx + 2.0);
            y = std::max(y, vy + 16.0);
        }
    }

    std::ostringstream fx, fy;
    fx << x;
    fy << y;
    pugi::xml_node txt = svgNode.append_child("text");
    txt.append_attribute("x")           = fx.str().c_str();
    txt.append_attribute("y")           = fy.str().c_str();
    txt.append_attribute("font-size")   = "16";
    txt.append_attribute("font-family") = "Helvetica, Arial, sans-serif";
    txt.append_attribute("fill")        = "#000000";
    txt.text().set(label.c_str());
}

// Convenience: bounding box from a set of points.
inline void addMeasureLabel(pugi::xml_node svgNode, const std::string& label,
                            const std::vector<Point>& points) {
    if (points.empty()) {
        addMeasureLabelAt(svgNode, label, 10.0, 26.0);
        return;
    }
    double minX = points[0].x, minY = points[0].y;
    for (const auto& p : points) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
    }
    addMeasureLabelAt(svgNode, label, minX, minY);
}
