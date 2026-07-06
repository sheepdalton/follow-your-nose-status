#pragma once
#include "FloorPlan.h"
#include "Polygon.h"
#include "VisibilityGraph.h"
#include <vector>
#include <string>

struct ColoredIsovist {
    Polygon  polygon;   // visibility polygon
    Point    center;    // viewpoint
    std::string color;  // SVG fill colour, e.g. "#e41a1c"
};

class SVGExporter {
public:
    // Copies the original SVG from inputPath, appends isovist center circles,
    // and writes the result to outputPath.
    void exportSVG(const std::string& inputPath,
                   const std::string& outputPath,
                   const std::vector<Point>& isovistCenters,
                   double pointRadius = 3.0,
                   const std::string& pointColor = "red");

    // Copies the original SVG, overlays filled+semi-transparent isovist polygons
    // (each in its own colour), plus a dot at each viewpoint.
    void exportExperiment(const std::string& inputPath,
                          const std::string& outputPath,
                          const std::vector<ColoredIsovist>& isovists,
                          double fillOpacity = 0.35,
                          double dotRadius   = 4.0);

    // Draws all centres as grey dots, then highlights one selected node in red
    // and draws lines to every neighbour in blue.
    void exportConnections(const std::string& inputPath,
                           const std::string& outputPath,
                           const std::vector<Point>& centers,
                           int selectedNode,
                           const std::vector<int>& neighbours,
                           double dotRadius = 3.0);

    // Draws every edge in the visibility graph as a thin grey line, then all centres.
    void exportGraph(const std::string& inputPath,
                     const std::string& outputPath,
                     const std::vector<Point>& centers,
                     const std::vector<std::vector<int>>& adj,
                     double dotRadius = 3.0);

    // Draws the Nose Integration single path: edges coloured by depth cost,
    // text label on each edge showing the depth value.
    void exportNosePath(const std::string& inputPath,
                        const std::string& outputPath,
                        const std::vector<Point>& centers,
                        const NoseResult& nose,
                        int origin, int dest,
                        double dotRadius = 3.0);

    // Draws all centres as grey dots, then the angular shortest path as blue lines,
    // intermediate nodes as blue dots, origin as green, destination as red.
    void exportAChoicePath(const std::string& inputPath,
                           const std::string& outputPath,
                           const std::vector<Point>& centers,
                           const std::vector<int>& path,
                           int origin, int dest,
                           double dotRadius = 3.0);

    // Computes dot radius as sqrt(0.5 * openArea / n).
    static double computeDotRadius(double openArea, int n);
};
