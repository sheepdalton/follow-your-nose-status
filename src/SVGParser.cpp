#include "SVGParser.h"
#include "../vendor/pugixml.hpp"
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <cctype>

Polygon SVGParser::parsePath(const std::string& d) {
    Polygon poly;
    size_t i = 0;
    const size_t len = d.size();

    // Skip whitespace and commas (SVG path separators)
    auto skip = [&]() {
        while (i < len && (d[i] == ' ' || d[i] == '\t' || d[i] == '\n' || d[i] == '\r' || d[i] == ','))
            ++i;
    };

    // Read one floating-point number (handles scientific notation)
    auto readNum = [&](double& val) -> bool {
        skip();
        if (i >= len) return false;
        size_t start = i;
        if (d[i] == '-' || d[i] == '+') ++i;
        while (i < len && (std::isdigit(static_cast<unsigned char>(d[i])) || d[i] == '.')) ++i;
        if (i < len && (d[i] == 'e' || d[i] == 'E')) {
            ++i;
            if (i < len && (d[i] == '+' || d[i] == '-')) ++i;
            while (i < len && std::isdigit(static_cast<unsigned char>(d[i]))) ++i;
        }
        if (i == start) return false;
        val = std::stod(d.substr(start, i - start));
        return true;
    };

    auto readPair = [&](double& x, double& y) -> bool {
        return readNum(x) && readNum(y);
    };

    // Tessellate a cubic Bezier from (cx,cy)→(x3,y3) with given control points.
    // Samples at steps equally-spaced t values and adds each as a vertex.
    auto tessellateCubic = [&](double cx, double cy,
                                double x1, double y1,
                                double x2, double y2,
                                double x3, double y3,
                                int steps) {
        for (int s = 1; s <= steps; ++s) {
            double t  = static_cast<double>(s) / steps;
            double u  = 1.0 - t;
            double bx = u*u*u*cx + 3*u*u*t*x1 + 3*u*t*t*x2 + t*t*t*x3;
            double by = u*u*u*cy + 3*u*u*t*y1 + 3*u*t*t*y2 + t*t*t*y3;
            poly.addVertex(Point(bx, by));
        }
    };

    double cx = 0.0, cy = 0.0;   // current pen position
    char   cmd = '\0';
    bool   done = false;

    while (i < len && !done) {
        skip();
        if (i >= len) break;

        // If next char is a command letter, consume it
        if (std::isalpha(static_cast<unsigned char>(d[i]))) {
            cmd = d[i++];
        }

        // Implicit repetition: re-use current cmd with its current absoluteness
        if (cmd == 'M' || cmd == 'm') {
            double x, y;
            if (!readPair(x, y)) break;
            if (cmd == 'm') { x += cx; y += cy; }
            cx = x; cy = y;
            poly.addVertex(Point(cx, cy));
            // Subsequent coordinate pairs after M are treated as implicit L/l
            cmd = (cmd == 'M') ? 'L' : 'l';

        } else if (cmd == 'L' || cmd == 'l') {
            double x, y;
            if (!readPair(x, y)) break;
            if (cmd == 'l') { x += cx; y += cy; }
            cx = x; cy = y;
            poly.addVertex(Point(cx, cy));

        } else if (cmd == 'H' || cmd == 'h') {
            double x;
            if (!readNum(x)) break;
            if (cmd == 'h') x += cx;
            cx = x;
            poly.addVertex(Point(cx, cy));

        } else if (cmd == 'V' || cmd == 'v') {
            double y;
            if (!readNum(y)) break;
            if (cmd == 'v') y += cy;
            cy = y;
            poly.addVertex(Point(cx, cy));

        } else if (cmd == 'C' || cmd == 'c') {
            double x1, y1, x2, y2, x3, y3;
            if (!readPair(x1, y1) || !readPair(x2, y2) || !readPair(x3, y3)) break;
            if (cmd == 'c') { x1+=cx; y1+=cy; x2+=cx; y2+=cy; x3+=cx; y3+=cy; }
            tessellateCubic(cx, cy, x1, y1, x2, y2, x3, y3, 8);
            cx = x3; cy = y3;

        } else if (cmd == 'S' || cmd == 's') {
            // Smooth cubic: omitted first control point is reflection of previous C's P2
            // For simplicity treat as C with first control = current point
            double x2, y2, x3, y3;
            if (!readPair(x2, y2) || !readPair(x3, y3)) break;
            if (cmd == 's') { x2+=cx; y2+=cy; x3+=cx; y3+=cy; }
            tessellateCubic(cx, cy, cx, cy, x2, y2, x3, y3, 8);
            cx = x3; cy = y3;

        } else if (cmd == 'Q' || cmd == 'q') {
            double x1, y1, x2, y2;
            if (!readPair(x1, y1) || !readPair(x2, y2)) break;
            if (cmd == 'q') { x1+=cx; y1+=cy; x2+=cx; y2+=cy; }
            // Elevate quadratic to cubic
            double cx1 = cx  + 2.0/3.0*(x1-cx);
            double cy1 = cy  + 2.0/3.0*(y1-cy);
            double cx2 = x2  + 2.0/3.0*(x1-x2);
            double cy2 = y2  + 2.0/3.0*(y1-y2);
            tessellateCubic(cx, cy, cx1, cy1, cx2, cy2, x2, y2, 8);
            cx = x2; cy = y2;

        } else if (cmd == 'T' || cmd == 't') {
            double x, y;
            if (!readPair(x, y)) break;
            if (cmd == 't') { x+=cx; y+=cy; }
            poly.addVertex(Point(x, y));
            cx = x; cy = y;

        } else if (cmd == 'A' || cmd == 'a') {
            // Arc: rx ry x-rotation large-arc-flag sweep-flag x y
            // Skip arc parameters; just use the endpoint
            double rx, ry, rot, laf, sf, x, y;
            if (!readNum(rx) || !readNum(ry) || !readNum(rot) ||
                !readNum(laf) || !readNum(sf) || !readPair(x, y)) break;
            if (cmd == 'a') { x+=cx; y+=cy; }
            poly.addVertex(Point(x, y));
            cx = x; cy = y;

        } else if (cmd == 'z' || cmd == 'Z') {
            done = true;

        } else {
            // Unknown command: skip one character to avoid infinite loop
            ++i;
        }
    }

    // Remove closing vertex if it duplicates the first
    const auto& verts = poly.vertices();
    if (verts.size() > 1 && verts.front() == verts.back()) {
        Polygon trimmed;
        for (size_t j = 0; j < verts.size() - 1; ++j)
            trimmed.addVertex(verts[j]);
        return trimmed;
    }

    return poly;
}

void SVGParser::collectPaths(void* nodePtr, std::vector<Polygon>& out) {
    pugi::xml_node* node = static_cast<pugi::xml_node*>(nodePtr);
    for (auto child : node->children()) {
        std::string name = child.name();
        if (name == "path") {
            std::string d = child.attribute("d").as_string();
            if (!d.empty()) {
                Polygon p = parsePath(d);
                if (p.size() >= 3)
                    out.push_back(std::move(p));
            }
        }
        pugi::xml_node childCopy = child;
        collectPaths(static_cast<void*>(&childCopy), out);
    }
}

FloorPlan SVGParser::parse(const std::string& filepath) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filepath.c_str());
    if (!result)
        throw std::runtime_error("Failed to parse SVG: " + std::string(result.description()));

    pugi::xml_node svgNode = doc.child("svg");
    if (!svgNode)
        throw std::runtime_error("No <svg> root element found in: " + filepath);

    FloorPlan fp;
    fp.svgWidth   = svgNode.attribute("width").as_string();
    fp.svgHeight  = svgNode.attribute("height").as_string();
    fp.svgViewBox = svgNode.attribute("viewBox").as_string();

    std::vector<Polygon> allPolygons;
    collectPaths(static_cast<void*>(&svgNode), allPolygons);

    if (allPolygons.empty())
        throw std::runtime_error("No usable paths found in SVG.");

    // The polygon with the largest area is the outer boundary; all others are obstacles.
    size_t outerIdx = 0;
    double maxArea = 0.0;
    for (size_t i = 0; i < allPolygons.size(); ++i) {
        double a = allPolygons[i].area();
        if (a > maxArea) { maxArea = a; outerIdx = i; }
    }

    fp.outerBoundary = std::move(allPolygons[outerIdx]);
    for (size_t i = 0; i < allPolygons.size(); ++i) {
        if (i != outerIdx)
            fp.obstacles.push_back(std::move(allPolygons[i]));
    }

    std::cout << "Parsed " << fp.obstacles.size() << " obstacles. "
              << "Outer boundary area: " << maxArea << "\n";

    return fp;
}
