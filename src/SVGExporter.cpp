#include "SVGExporter.h"
#include "ColorMap.h"
#include "SVGLabel.h"
#include "../vendor/pugixml.hpp"
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <cmath>

void SVGExporter::exportSVG(const std::string& inputPath,
                             const std::string& outputPath,
                             const std::vector<Point>& isovistCenters,
                             double pointRadius,
                             const std::string& pointColor) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(inputPath.c_str());
    if (!result)
        throw std::runtime_error("SVGExporter: failed to reload SVG: " + std::string(result.description()));

    pugi::xml_node svgNode = doc.child("svg");
    if (!svgNode)
        throw std::runtime_error("SVGExporter: no <svg> root element.");

    // Append a group containing one circle per isovist center
    pugi::xml_node group = svgNode.append_child("g");
    group.append_attribute("id") = "isovist_centers";

    for (const auto& p : isovistCenters) {
        pugi::xml_node circle = group.append_child("circle");

        std::ostringstream cx, cy;
        cx << std::fixed << std::setprecision(3) << p.x;
        cy << std::fixed << std::setprecision(3) << p.y;

        circle.append_attribute("cx")   = cx.str().c_str();
        circle.append_attribute("cy")   = cy.str().c_str();
        circle.append_attribute("r")    = pointRadius;
        circle.append_attribute("fill") = pointColor.c_str();
    }

    addMeasureLabel(svgNode, "isovists", isovistCenters);

    if (!doc.save_file(outputPath.c_str()))
        throw std::runtime_error("SVGExporter: failed to write output file: " + outputPath);

    std::cout << "Exported " << isovistCenters.size()
              << " isovist centers to: " << outputPath << "\n";
}

void SVGExporter::exportExperiment(const std::string& inputPath,
                                    const std::string& outputPath,
                                    const std::vector<ColoredIsovist>& isovists,
                                    double fillOpacity,
                                    double dotRadius) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(inputPath.c_str());
    if (!result)
        throw std::runtime_error("SVGExporter::exportExperiment: " + std::string(result.description()));

    pugi::xml_node svgNode = doc.child("svg");
    if (!svgNode)
        throw std::runtime_error("SVGExporter::exportExperiment: no <svg> root.");

    auto fmtPt = [](double v) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(3) << v;
        return ss.str();
    };

    auto fmtOp = [](double v) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << v;
        return ss.str();
    };

    pugi::xml_node group = svgNode.append_child("g");
    group.append_attribute("id") = "isovist_experiment";

    for (const auto& iso : isovists) {
        const auto& verts = iso.polygon.vertices();
        if (verts.size() < 3) continue;

        // Build SVG polygon points string
        std::ostringstream pts;
        for (const auto& v : verts)
            pts << fmtPt(v.x) << "," << fmtPt(v.y) << " ";

        pugi::xml_node poly = group.append_child("polygon");
        poly.append_attribute("points")       = pts.str().c_str();
        poly.append_attribute("fill")         = iso.color.c_str();
        poly.append_attribute("fill-opacity") = fmtOp(fillOpacity).c_str();
        poly.append_attribute("stroke")       = iso.color.c_str();
        poly.append_attribute("stroke-width") = "0.5";

        // Dot at viewpoint
        pugi::xml_node dot = group.append_child("circle");
        dot.append_attribute("cx")   = fmtPt(iso.center.x).c_str();
        dot.append_attribute("cy")   = fmtPt(iso.center.y).c_str();
        dot.append_attribute("r")    = dotRadius;
        dot.append_attribute("fill") = iso.color.c_str();
    }

    {
        std::vector<Point> pts;
        for (const auto& iso : isovists) pts.push_back(iso.center);
        addMeasureLabel(svgNode, "experiment", pts);
    }

    if (!doc.save_file(outputPath.c_str()))
        throw std::runtime_error("SVGExporter::exportExperiment: failed to write: " + outputPath);

    std::cout << "Exported " << isovists.size()
              << " isovist polygons to: " << outputPath << "\n";
}

void SVGExporter::exportTopoDepth(const std::string& inputPath,
                                   const std::string& outputPath,
                                   const std::vector<Point>& centers,
                                   const std::vector<int>& topoDepths,
                                   const NoseResult& nose,
                                   int origin, int dest,
                                   double dotRadius) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(inputPath.c_str());
    if (!result)
        throw std::runtime_error("SVGExporter::exportTopoDepth: " + std::string(result.description()));

    pugi::xml_node svgNode = doc.child("svg");
    if (!svgNode)
        throw std::runtime_error("SVGExporter::exportTopoDepth: no <svg> root.");

    auto fmt = [](double v) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(3) << v;
        return ss.str();
    };
    auto fmt2 = [](double v) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << v;
        return ss.str();
    };

    int maxDepth = 0;
    for (int d : topoDepths)
        maxDepth = std::max(maxDepth, d);

    pugi::xml_node group = svgNode.append_child("g");
    group.append_attribute("id") = "topo_depth";

    // ── All centres coloured by topological depth ────────────────────────────
    for (size_t i = 0; i < centers.size(); ++i) {
        std::string color;
        double radius = dotRadius;

        if ((int)i == dest) {
            color  = "#ff00ff";          // magenta
            radius = dotRadius * 1.5;
        } else if ((int)i == origin) {
            color  = "#000000";          // black
            radius = dotRadius * 1.5;
        } else if (topoDepths[i] < 0) {
            color = "#888888";           // unreachable
        } else {
            color = valueToColor(static_cast<double>(topoDepths[i]),
                                 0.0, static_cast<double>(maxDepth));
        }

        pugi::xml_node c = group.append_child("circle");
        c.append_attribute("cx")   = fmt(centers[i].x).c_str();
        c.append_attribute("cy")   = fmt(centers[i].y).c_str();
        c.append_attribute("r")    = radius;
        c.append_attribute("fill") = color.c_str();
    }

    // ── Overlay the nose path ────────────────────────────────────────────────
    const auto& path = nose.path;

    // Dotted vectors from each path node (except dest) to the destination
    if (path.size() >= 2) {
        const Point& d = centers[path.back()];
        pugi::xml_node vecs = group.append_child("g");
        vecs.append_attribute("id")               = "dest_vectors";
        vecs.append_attribute("stroke")           = "#555555";
        vecs.append_attribute("stroke-width")     = "0.5";
        vecs.append_attribute("stroke-dasharray") = "2,3";
        vecs.append_attribute("opacity")          = "0.7";
        for (int i = 0; i + 1 < (int)path.size(); ++i) {
            const Point& a = centers[path[i]];
            pugi::xml_node ln = vecs.append_child("line");
            ln.append_attribute("x1") = fmt(a.x).c_str();
            ln.append_attribute("y1") = fmt(a.y).c_str();
            ln.append_attribute("x2") = fmt(d.x).c_str();
            ln.append_attribute("y2") = fmt(d.y).c_str();
        }
    }

    // Path edges with depth-cost labels
    for (int i = 0; i + 1 < (int)path.size(); ++i) {
        const Point& a = centers[path[i]];
        const Point& b = centers[path[i+1]];
        double cost    = nose.edgeCosts[i];

        pugi::xml_node ln = group.append_child("line");
        ln.append_attribute("x1")           = fmt(a.x).c_str();
        ln.append_attribute("y1")           = fmt(a.y).c_str();
        ln.append_attribute("x2")           = fmt(b.x).c_str();
        ln.append_attribute("y2")           = fmt(b.y).c_str();
        ln.append_attribute("stroke")       = "#000000";
        ln.append_attribute("stroke-width") = "2.0";

        double mx = (a.x + b.x) * 0.5;
        double my = (a.y + b.y) * 0.5;
        pugi::xml_node txt = group.append_child("text");
        txt.append_attribute("x")           = fmt(mx).c_str();
        txt.append_attribute("y")           = fmt(my).c_str();
        txt.append_attribute("font-size")   = "8";
        txt.append_attribute("fill")        = "#000000";
        txt.append_attribute("text-anchor") = "middle";
        txt.text().set(fmt2(cost).c_str());
    }

    addMeasureLabel(svgNode, "topo-depth", centers);

    std::cout << "Exported topo-depth debug (max depth " << maxDepth
              << ") to: " << outputPath << "\n";

    if (!doc.save_file(outputPath.c_str()))
        throw std::runtime_error("SVGExporter::exportTopoDepth: failed to write: " + outputPath);
}

void SVGExporter::exportNosePath(const std::string& inputPath,
                                  const std::string& outputPath,
                                  const std::vector<Point>& centers,
                                  const NoseResult& nose,
                                  int origin, int dest,
                                  double dotRadius,
                                  const std::string& title) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(inputPath.c_str());
    if (!result)
        throw std::runtime_error("SVGExporter::exportNosePath: " + std::string(result.description()));

    pugi::xml_node svgNode = doc.child("svg");
    if (!svgNode)
        throw std::runtime_error("SVGExporter::exportNosePath: no <svg> root.");

    auto fmt = [](double v) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(3) << v;
        return ss.str();
    };
    auto fmt2 = [](double v) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << v;
        return ss.str();
    };

    pugi::xml_node group = svgNode.append_child("g");
    group.append_attribute("id") = "nose_path";

    // All centres: small grey dots
    for (const auto& p : centers) {
        pugi::xml_node c = group.append_child("circle");
        c.append_attribute("cx")   = fmt(p.x).c_str();
        c.append_attribute("cy")   = fmt(p.y).c_str();
        c.append_attribute("r")    = dotRadius;
        c.append_attribute("fill") = "#aaaaaa";
    }

    // Thin dotted lines from each path node (except dest) to the destination:
    // the reference vector V that each edge's angle is measured against.
    const auto& path = nose.path;
    if (path.size() >= 2) {
        const Point& d = centers[path.back()];
        pugi::xml_node vecs = group.append_child("g");
        vecs.append_attribute("id")               = "dest_vectors";
        vecs.append_attribute("stroke")           = "#555555";
        vecs.append_attribute("stroke-width")     = "0.5";
        vecs.append_attribute("stroke-dasharray") = "2,3";
        vecs.append_attribute("opacity")          = "0.7";
        for (int i = 0; i + 1 < (int)path.size(); ++i) {
            const Point& a = centers[path[i]];
            pugi::xml_node ln = vecs.append_child("line");
            ln.append_attribute("x1") = fmt(a.x).c_str();
            ln.append_attribute("y1") = fmt(a.y).c_str();
            ln.append_attribute("x2") = fmt(d.x).c_str();
            ln.append_attribute("y2") = fmt(d.y).c_str();
        }
    }

    // Edges coloured by depth cost (0=blue, 1=green, 2=red) with text labels
    for (int i = 0; i + 1 < (int)path.size(); ++i) {
        const Point& a = centers[path[i]];
        const Point& b = centers[path[i+1]];
        double cost    = nose.edgeCosts[i];

        // Colour: interpolate blue→green→red over [0,2]
        double t = std::max(0.0, std::min(1.0, cost / 2.0));
        int r = static_cast<int>(255 * t);
        int g = static_cast<int>(255 * (1.0 - std::abs(2.0*t - 1.0)));
        int bl = static_cast<int>(255 * (1.0 - t));
        std::ostringstream colorSS;
        colorSS << "rgb(" << r << "," << g << "," << bl << ")";

        pugi::xml_node ln = group.append_child("line");
        ln.append_attribute("x1")           = fmt(a.x).c_str();
        ln.append_attribute("y1")           = fmt(a.y).c_str();
        ln.append_attribute("x2")           = fmt(b.x).c_str();
        ln.append_attribute("y2")           = fmt(b.y).c_str();
        ln.append_attribute("stroke")       = colorSS.str().c_str();
        ln.append_attribute("stroke-width") = "2.0";

        // Text label at edge midpoint
        double mx = (a.x + b.x) * 0.5;
        double my = (a.y + b.y) * 0.5;
        pugi::xml_node txt = group.append_child("text");
        txt.append_attribute("x")           = fmt(mx).c_str();
        txt.append_attribute("y")           = fmt(my).c_str();
        txt.append_attribute("font-size")   = "8";
        txt.append_attribute("fill")        = "#000000";
        txt.append_attribute("text-anchor") = "middle";
        txt.text().set(fmt2(cost).c_str());
    }

    // Intermediate nodes: white dot with black border
    for (int i = 1; i + 1 < (int)path.size(); ++i) {
        const Point& p = centers[path[i]];
        pugi::xml_node c = group.append_child("circle");
        c.append_attribute("cx")           = fmt(p.x).c_str();
        c.append_attribute("cy")           = fmt(p.y).c_str();
        c.append_attribute("r")            = dotRadius * 1.5;
        c.append_attribute("fill")         = "#ffffff";
        c.append_attribute("stroke")       = "#333333";
        c.append_attribute("stroke-width") = "0.8";
    }

    // Origin: green
    {
        const Point& p = centers[origin];
        pugi::xml_node c = group.append_child("circle");
        c.append_attribute("cx")   = fmt(p.x).c_str();
        c.append_attribute("cy")   = fmt(p.y).c_str();
        c.append_attribute("r")    = dotRadius * 2.0;
        c.append_attribute("fill") = "#33a02c";
    }

    // Destination: red
    {
        const Point& p = centers[dest];
        pugi::xml_node c = group.append_child("circle");
        c.append_attribute("cx")   = fmt(p.x).c_str();
        c.append_attribute("cy")   = fmt(p.y).c_str();
        c.append_attribute("r")    = dotRadius * 2.0;
        c.append_attribute("fill") = "#e31a1c";
    }

    addMeasureLabel(svgNode, title, centers);

    if (!doc.save_file(outputPath.c_str()))
        throw std::runtime_error("SVGExporter::exportNosePath: failed to write: " + outputPath);

    std::cout << "Exported Nose Integration path ("
              << (path.empty() ? 0 : (int)path.size()-1)
              << " steps) to: " << outputPath << "\n";
}

void SVGExporter::exportGraph(const std::string& inputPath,
                               const std::string& outputPath,
                               const std::vector<Point>& centers,
                               const std::vector<std::vector<int>>& adj,
                               double dotRadius) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(inputPath.c_str());
    if (!result)
        throw std::runtime_error("SVGExporter::exportGraph: " + std::string(result.description()));

    pugi::xml_node svgNode = doc.child("svg");
    if (!svgNode)
        throw std::runtime_error("SVGExporter::exportGraph: no <svg> root.");

    auto fmt = [](double v) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(3) << v;
        return ss.str();
    };

    pugi::xml_node group = svgNode.append_child("g");
    group.append_attribute("id") = "visibility_graph";

    // All edges as thin grey lines (draw each undirected edge once: i < j)
    pugi::xml_node lines = group.append_child("g");
    lines.append_attribute("stroke")       = "#888888";
    lines.append_attribute("stroke-width") = "0.3";
    lines.append_attribute("opacity")      = "0.4";
    for (int i = 0; i < (int)adj.size(); ++i) {
        for (int j : adj[i]) {
            if (j <= i) continue;
            pugi::xml_node ln = lines.append_child("line");
            ln.append_attribute("x1") = fmt(centers[i].x).c_str();
            ln.append_attribute("y1") = fmt(centers[i].y).c_str();
            ln.append_attribute("x2") = fmt(centers[j].x).c_str();
            ln.append_attribute("y2") = fmt(centers[j].y).c_str();
        }
    }

    // Centres on top
    pugi::xml_node dots = group.append_child("g");
    dots.append_attribute("fill") = "#333333";
    for (const auto& p : centers) {
        pugi::xml_node c = dots.append_child("circle");
        c.append_attribute("cx") = fmt(p.x).c_str();
        c.append_attribute("cy") = fmt(p.y).c_str();
        c.append_attribute("r")  = dotRadius;
    }

    addMeasureLabel(svgNode, "visibility graph", centers);

    if (!doc.save_file(outputPath.c_str()))
        throw std::runtime_error("SVGExporter::exportGraph: failed to write: " + outputPath);

    int edgeCount = 0;
    for (const auto& nbrs : adj) edgeCount += static_cast<int>(nbrs.size());
    edgeCount /= 2;
    std::cout << "Exported visibility graph (" << edgeCount << " edges) to: " << outputPath << "\n";
}

void SVGExporter::exportAChoicePath(const std::string& inputPath,
                                     const std::string& outputPath,
                                     const std::vector<Point>& centers,
                                     const std::vector<int>& path,
                                     int origin, int dest,
                                     double dotRadius) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(inputPath.c_str());
    if (!result)
        throw std::runtime_error("SVGExporter::exportAChoicePath: " + std::string(result.description()));

    pugi::xml_node svgNode = doc.child("svg");
    if (!svgNode)
        throw std::runtime_error("SVGExporter::exportAChoicePath: no <svg> root.");

    auto fmt = [](double v) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(3) << v;
        return ss.str();
    };

    pugi::xml_node group = svgNode.append_child("g");
    group.append_attribute("id") = "a_choice_path";

    // All centres: small grey dots
    for (size_t i = 0; i < centers.size(); ++i) {
        pugi::xml_node c = group.append_child("circle");
        c.append_attribute("cx")   = fmt(centers[i].x).c_str();
        c.append_attribute("cy")   = fmt(centers[i].y).c_str();
        c.append_attribute("r")    = dotRadius;
        c.append_attribute("fill") = "#aaaaaa";
    }

    // Path lines
    if (path.size() >= 2) {
        pugi::xml_node lines = group.append_child("g");
        lines.append_attribute("stroke")       = "#1f78b4";
        lines.append_attribute("stroke-width") = "1.5";
        lines.append_attribute("opacity")      = "0.8";
        for (size_t i = 0; i + 1 < path.size(); ++i) {
            const Point& a = centers[path[i]];
            const Point& b = centers[path[i+1]];
            pugi::xml_node ln = lines.append_child("line");
            ln.append_attribute("x1") = fmt(a.x).c_str();
            ln.append_attribute("y1") = fmt(a.y).c_str();
            ln.append_attribute("x2") = fmt(b.x).c_str();
            ln.append_attribute("y2") = fmt(b.y).c_str();
        }
    }

    // Intermediate path nodes: blue dots
    for (size_t i = 1; i + 1 < path.size(); ++i) {
        const Point& p = centers[path[i]];
        pugi::xml_node c = group.append_child("circle");
        c.append_attribute("cx")   = fmt(p.x).c_str();
        c.append_attribute("cy")   = fmt(p.y).c_str();
        c.append_attribute("r")    = dotRadius * 1.4;
        c.append_attribute("fill") = "#1f78b4";
    }

    // Origin: green dot
    {
        const Point& p = centers[origin];
        pugi::xml_node c = group.append_child("circle");
        c.append_attribute("cx")   = fmt(p.x).c_str();
        c.append_attribute("cy")   = fmt(p.y).c_str();
        c.append_attribute("r")    = dotRadius * 2.0;
        c.append_attribute("fill") = "#33a02c";
    }

    // Destination: red dot
    {
        const Point& p = centers[dest];
        pugi::xml_node c = group.append_child("circle");
        c.append_attribute("cx")   = fmt(p.x).c_str();
        c.append_attribute("cy")   = fmt(p.y).c_str();
        c.append_attribute("r")    = dotRadius * 2.0;
        c.append_attribute("fill") = "#e31a1c";
    }

    addMeasureLabel(svgNode, "a-choice path", centers);

    if (!doc.save_file(outputPath.c_str()))
        throw std::runtime_error("SVGExporter::exportAChoicePath: failed to write: " + outputPath);

    std::cout << "Exported A-choice path (" << (path.empty() ? 0 : path.size()-1)
              << " steps) to: " << outputPath << "\n";
}

void SVGExporter::exportGates(const std::string& inputPath,
                               const std::string& outputPath,
                               const std::vector<Point>& centers,
                               const std::vector<Gate>& gates,
                               const std::vector<int>& matched,
                               double dotRadius) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(inputPath.c_str());
    if (!result)
        throw std::runtime_error("SVGExporter::exportGates: " + std::string(result.description()));

    pugi::xml_node svgNode = doc.child("svg");
    if (!svgNode)
        throw std::runtime_error("SVGExporter::exportGates: no <svg> root.");

    auto fmt = [](double v) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(3) << v;
        return ss.str();
    };

    pugi::xml_node group = svgNode.append_child("g");
    group.append_attribute("id") = "gates";

    // All isovist centres as small grey dots
    for (const auto& p : centers) {
        pugi::xml_node c = group.append_child("circle");
        c.append_attribute("cx")   = fmt(p.x).c_str();
        c.append_attribute("cy")   = fmt(p.y).c_str();
        c.append_attribute("r")    = dotRadius;
        c.append_attribute("fill") = "#cccccc";
    }

    for (size_t g = 0; g < gates.size(); ++g) {
        const Gate& gate = gates[g];
        bool ok = matched[g] >= 0;

        // Line from gate to matched isovist centre
        if (ok) {
            const Point& c = centers[matched[g]];
            pugi::xml_node ln = group.append_child("line");
            ln.append_attribute("x1")           = fmt(gate.x).c_str();
            ln.append_attribute("y1")           = fmt(gate.y).c_str();
            ln.append_attribute("x2")           = fmt(c.x).c_str();
            ln.append_attribute("y2")           = fmt(c.y).c_str();
            ln.append_attribute("stroke")       = "#1f78b4";
            ln.append_attribute("stroke-width") = "0.8";

            // Highlight the matched centre
            pugi::xml_node mc = group.append_child("circle");
            mc.append_attribute("cx")   = fmt(c.x).c_str();
            mc.append_attribute("cy")   = fmt(c.y).c_str();
            mc.append_attribute("r")    = dotRadius * 1.3;
            mc.append_attribute("fill") = "#1f78b4";
        }

        // Gate square: blue outline if matched, red if not
        double s = dotRadius * 2.0;
        pugi::xml_node sq = group.append_child("rect");
        sq.append_attribute("x")            = fmt(gate.x - s/2).c_str();
        sq.append_attribute("y")            = fmt(gate.y - s/2).c_str();
        sq.append_attribute("width")        = fmt(s).c_str();
        sq.append_attribute("height")       = fmt(s).c_str();
        sq.append_attribute("fill")         = "none";
        sq.append_attribute("stroke")       = ok ? "#1f78b4" : "#e31a1c";
        sq.append_attribute("stroke-width") = "1.2";

        // Label above the square
        pugi::xml_node txt = group.append_child("text");
        txt.append_attribute("x")           = fmt(gate.x).c_str();
        txt.append_attribute("y")           = fmt(gate.y - s).c_str();
        txt.append_attribute("font-size")   = "7";
        txt.append_attribute("fill")        = ok ? "#000000" : "#e31a1c";
        txt.append_attribute("text-anchor") = "middle";
        txt.text().set(gate.label.c_str());
    }

    addMeasureLabel(svgNode, "gates", centers);

    if (!doc.save_file(outputPath.c_str()))
        throw std::runtime_error("SVGExporter::exportGates: failed to write: " + outputPath);

    std::cout << "Exported " << gates.size() << " gates to: " << outputPath << "\n";
}

double SVGExporter::computeDotRadius(double openArea, int n) {
    return std::sqrt(0.5 * openArea / static_cast<double>(n));
}

void SVGExporter::exportConnections(const std::string& inputPath,
                                     const std::string& outputPath,
                                     const std::vector<Point>& centers,
                                     int selectedNode,
                                     const std::vector<int>& neighbours,
                                     double dotRadius) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(inputPath.c_str());
    if (!result)
        throw std::runtime_error("SVGExporter::exportConnections: " + std::string(result.description()));

    pugi::xml_node svgNode = doc.child("svg");
    if (!svgNode)
        throw std::runtime_error("SVGExporter::exportConnections: no <svg> root.");

    auto fmt = [](double v) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(3) << v;
        return ss.str();
    };

    pugi::xml_node group = svgNode.append_child("g");
    group.append_attribute("id") = "connections";

    const Point& src = centers[selectedNode];

    // Grey dots for all nodes
    pugi::xml_node gDots = group.append_child("g");
    gDots.append_attribute("id") = "all_centers";
    for (const auto& p : centers) {
        pugi::xml_node c = gDots.append_child("circle");
        c.append_attribute("cx")   = fmt(p.x).c_str();
        c.append_attribute("cy")   = fmt(p.y).c_str();
        c.append_attribute("r")    = dotRadius;
        c.append_attribute("fill") = "#aaaaaa";
    }

    // Blue lines to each neighbour
    pugi::xml_node gLines = group.append_child("g");
    gLines.append_attribute("id")           = "connection_lines";
    gLines.append_attribute("stroke")       = "#1f78b4";
    gLines.append_attribute("stroke-width") = "0.8";
    gLines.append_attribute("opacity")      = "0.6";
    for (int nb : neighbours) {
        const Point& dst = centers[nb];
        pugi::xml_node line = gLines.append_child("line");
        line.append_attribute("x1") = fmt(src.x).c_str();
        line.append_attribute("y1") = fmt(src.y).c_str();
        line.append_attribute("x2") = fmt(dst.x).c_str();
        line.append_attribute("y2") = fmt(dst.y).c_str();
    }

    // Blue dots on neighbours
    pugi::xml_node gNb = group.append_child("g");
    gNb.append_attribute("id") = "neighbour_centers";
    for (int nb : neighbours) {
        pugi::xml_node c = gNb.append_child("circle");
        c.append_attribute("cx")   = fmt(centers[nb].x).c_str();
        c.append_attribute("cy")   = fmt(centers[nb].y).c_str();
        c.append_attribute("r")    = dotRadius * 1.4;
        c.append_attribute("fill") = "#1f78b4";
    }

    // Red dot for selected node (drawn last so it's on top)
    pugi::xml_node sel = group.append_child("circle");
    sel.append_attribute("cx")   = fmt(src.x).c_str();
    sel.append_attribute("cy")   = fmt(src.y).c_str();
    sel.append_attribute("r")    = dotRadius * 2.0;
    sel.append_attribute("fill") = "#e31a1c";

    addMeasureLabel(svgNode, "connections", centers);

    if (!doc.save_file(outputPath.c_str()))
        throw std::runtime_error("SVGExporter::exportConnections: failed to write: " + outputPath);

    std::cout << "Exported connections for node " << selectedNode
              << " (" << neighbours.size() << " neighbours) to: " << outputPath << "\n";
}
