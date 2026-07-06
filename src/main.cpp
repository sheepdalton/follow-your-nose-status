#include "SVGParser.h"
#include "IsovistPlacer.h"
#include "IsovistComputer.h"
#include "IsovistRecord.h"
#include "VisibilityGraph.h"
#include "SVGExporter.h"
#include "MetricsExporter.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <filesystem>
#include <memory>
#include <random>
#include <cmath>

namespace fs = std::filesystem;

static void printUsage(const std::string& prog) {
    std::cerr << "Usage: " << prog << " <input.svg> [options]\n"
              << "  --n <count>       isovist centers to place   (default: 400)\n"
              << "  --candidates <k>  candidates per step        (default: 7)\n"
              << "  --seed <s>        RNG seed                   (default: 42)\n"
              << "  --T               export CSV (id,x,y,area,perimeter,degree)\n"
              << "  --vis-area        export area heatmap SVG\n"
              << "  --vis-perim       export perimeter heatmap SVG\n"
              << "  --vis-degree      export visibility-graph degree heatmap SVG\n"
              << "  --vis-choice      export betweenness (choice) heatmap SVG\n"
              << "  --vis-d-choice    export distributed choice heatmap SVG\n"
              << "  --vis-a-choice    export angular shortest path SVG\n"
              << "  --OR <id>         origin node for a-choice  (default: 0)\n"
              << "  --DEST <id>       dest   node for a-choice  (default: n-1)\n"
              << "  --vis-graph        draw all visibility graph edges as grey lines\n"
              << "  --vis-nose-single  Nose Integration single path SVG\n"
              << "  --ODseed <O> <D>   origin and dest node IDs for nose (default: random)\n"
              << "  --SIZE <r>        override automatic dot radius\n"
              << "  --LOG             apply natural log to metric before colour mapping\n"
              << "  --FLIP            reverse colour spectrum (blue=low, red=high)\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) { printUsage(argv[0]); return 1; }

    std::string inputPath;
    int          n           = 400;
    int          candidates  = 7;
    unsigned int seed        = 42;
    bool         doCSV          = false;
    bool         doVisArea      = false;
    bool         doVisPerim     = false;
    bool         doVisDegree    = false;
    bool         doVisChoice    = false;
    bool         doVisDChoice   = false;
    bool         doVisAChoice   = false;
    bool         doVisConnects  = false;
    bool         doVisGraph     = false;
    bool         doVisNose      = false;
    int          noseOrigin     = -1;
    int          noseDest       = -1;
    int          aChoiceOrigin  = -1;
    int          aChoiceDest    = -1;
    bool         doFlip         = false;
    double       sizeOverride   = -1.0;
    bool         doLog          = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if      (arg == "--n"          && i+1<argc) { n          = std::stoi(argv[++i]); }
        else if (arg == "--candidates" && i+1<argc) { candidates = std::stoi(argv[++i]); }
        else if (arg == "--seed"       && i+1<argc) { seed = static_cast<unsigned>(std::stoul(argv[++i])); }
        else if (arg == "--T")                       { doCSV       = true; }
        else if (arg == "--vis-area")                { doVisArea   = true; }
        else if (arg == "--vis-perim")               { doVisPerim  = true; }
        else if (arg == "--vis-degree")              { doVisDegree   = true; }
        else if (arg == "--vis-choice")              { doVisChoice   = true; }
        else if (arg == "--vis-d-choice")            { doVisDChoice  = true; }
        else if (arg == "--vis-a-choice")            { doVisAChoice  = true; }
        else if (arg == "--OR"   && i+1<argc)        { aChoiceOrigin = std::stoi(argv[++i]); }
        else if (arg == "--DEST" && i+1<argc)        { aChoiceDest   = std::stoi(argv[++i]); }
        else if (arg == "--vis-connections")         { doVisConnects = true; }
        else if (arg == "--vis-graph")               { doVisGraph    = true; }
        else if (arg == "--vis-nose-single")         { doVisNose     = true; }
        else if (arg == "--ODseed" && i+2<argc)      { noseOrigin = std::stoi(argv[++i]);
                                                       noseDest   = std::stoi(argv[++i]); }
        else if (arg == "--FLIP")                    { doFlip        = true; }
        else if (arg == "--SIZE" && i+1<argc)        { sizeOverride  = std::stod(argv[++i]); }
        else if (arg == "--LOG")                     { doLog         = true; }
        else if (arg[0] != '-')                      { inputPath     = arg; }
        else { std::cerr << "Unknown argument: " << arg << "\n"; printUsage(argv[0]); return 1; }
    }

    if (inputPath.empty()) { printUsage(argv[0]); return 1; }

    fs::path inPath(inputPath);
    fs::path outDir = inPath.parent_path().parent_path() / "out";
    fs::create_directories(outDir);

    std::string stem = inPath.stem().string();
    std::string nStr = std::to_string(n);

    try {
        // ---- parse + place ----
        std::cout << "Parsing: " << inputPath << "\n";
        SVGParser parser;
        FloorPlan fp = parser.parse(inputPath);

        std::cout << "Placing " << n << " isovist centers "
                  << "(candidates=" << candidates << ", seed=" << seed << ")...\n";
        IsovistPlacer placer(fp, n, candidates, seed);
        std::vector<Point> centers = placer.place();

        double dotRadius = (sizeOverride > 0.0) ? sizeOverride
                                                 : SVGExporter::computeDotRadius(fp.openArea(), n);
        std::cout << "Open area: " << fp.openArea() << "  dot radius: " << dotRadius << "\n";

        SVGExporter svgExp;
        svgExp.exportSVG(inputPath, (outDir / (stem + ".svg")).string(), centers, dotRadius);

        // ---- compute all visibility polygons ----
        bool needMetrics = doCSV || doVisArea || doVisPerim || doVisDegree || doVisChoice || doVisDChoice || doVisAChoice || doVisConnects || doVisGraph || doVisNose;
        bool needGraph   = doVisDegree || doVisChoice || doVisDChoice || doVisAChoice || doVisConnects || doVisGraph || doVisNose;

        std::vector<IsovistRecord> records;
        std::vector<Polygon>       polygons; // kept only when graph is required

        if (needMetrics) {
            std::cout << "Computing " << n << " visibility polygons...\n";
            IsovistComputer computer;
            records.reserve(n);
            if (needGraph) polygons.reserve(n);

            for (int i = 0; i < n; ++i) {
                Polygon vis = computer.compute(centers[i], fp);
                records.push_back({i, centers[i], vis.area(), vis.perimeter(), 0});
                if (needGraph) polygons.push_back(vis);
                if ((i + 1) % 50 == 0)
                    std::cout << "  " << (i+1) << " / " << n << " done\n";
            }

            // ---- build visibility graph (if requested) ----
            std::unique_ptr<VisibilityGraph> graph;
            if (needGraph) {
                graph = std::make_unique<VisibilityGraph>(n);
                graph->build(polygons, centers);
                polygons.clear();
                polygons.shrink_to_fit();

                for (int i = 0; i < n; ++i)
                    records[i].degree = graph->degree(i);

                if (doVisChoice) {
                    std::vector<double> choice = graph->computeChoice();
                    for (int i = 0; i < n; ++i)
                        records[i].choice = choice[i];
                }
                if (doVisDChoice) {
                    std::vector<double> dc = graph->computeDChoice();
                    for (int i = 0; i < n; ++i)
                        records[i].dChoice = dc[i];
                }
                bool aChoiceFullAccum = doVisAChoice &&
                                        !(aChoiceOrigin >= 0 && aChoiceOrigin < n &&
                                          aChoiceDest   >= 0 && aChoiceDest   < n);
                if (aChoiceFullAccum) {
                    std::vector<double> ac = graph->computeAChoice(centers);
                    for (int i = 0; i < n; ++i)
                        records[i].aChoice = ac[i];
                }
            }

            MetricsExporter metricsExp;
            metricsExp.setFlip(doFlip);
            metricsExp.setLog(doLog);

            if (doCSV) {
                fs::path csvPath = outDir / (stem + "-" + nStr + ".csv");
                metricsExp.exportCSV(records, csvPath.string());
            }
            if (doVisArea) {
                fs::path p = outDir / (stem + "-area-" + nStr + ".svg");
                metricsExp.exportAreaHeatmap(inputPath, p.string(), records, dotRadius);
            }
            if (doVisPerim) {
                fs::path p = outDir / (stem + "-perim-" + nStr + ".svg");
                metricsExp.exportPerimeterHeatmap(inputPath, p.string(), records, dotRadius);
            }
            if (doVisDegree) {
                fs::path p = outDir / (stem + "-degree-" + nStr + ".svg");
                metricsExp.exportDegreeHeatmap(inputPath, p.string(), records, dotRadius);
            }
            if (doVisChoice) {
                fs::path p = outDir / (stem + "-choice-" + nStr + ".svg");
                metricsExp.exportChoiceHeatmap(inputPath, p.string(), records, dotRadius);
            }
            if (doVisDChoice) {
                fs::path p = outDir / (stem + "-d-choice-" + nStr + ".svg");
                metricsExp.exportDChoiceHeatmap(inputPath, p.string(), records, dotRadius);
            }
            if (doVisAChoice) {
                bool hasSinglePair = (aChoiceOrigin >= 0 && aChoiceOrigin < n &&
                                      aChoiceDest   >= 0 && aChoiceDest   < n);

                if (hasSinglePair) {
                    // ── Single path visualisation ─────────────────────────────
                    int ori = aChoiceOrigin;
                    int dst = aChoiceDest;
                    std::cout << "Computing A-choice path from node " << ori
                              << " to node " << dst << "...\n";

                    AChoiceResult ac = graph->computeAChoicePath(ori, dst, centers);

                    if (ac.path.empty()) {
                        std::cout << "A-choice: no path found.\n";
                    } else {
                        const double RAD2DEG = 180.0 / M_PI;
                        std::cout << "Path (" << ac.path.size() << " nodes, "
                                  << ac.path.size() - 1 << " steps):\n";
                        for (int i = 0; i < (int)ac.path.size(); ++i) {
                            int id = ac.path[i];
                            std::cout << "  [" << i << "] node " << id
                                      << "  (" << centers[id].x << ", " << centers[id].y << ")";
                            if (i == 0)
                                std::cout << "  <-- origin";
                            else if (i == (int)ac.path.size() - 1)
                                std::cout << "  <-- dest";
                            else {
                                double turnDeg  = ac.turnAngles[i-1] * RAD2DEG;
                                double angleDeg = 180.0 - turnDeg;
                                std::cout << "  turn=" << std::fixed << std::setprecision(1)
                                          << turnDeg << "deg"
                                          << "  angle(A→D,A→O)=" << angleDeg << "deg";
                            }
                            std::cout << "\n";
                        }
                        std::cout << "Total angular cost: "
                                  << std::fixed << std::setprecision(1)
                                  << ac.totalAngle * RAD2DEG << " deg\n";

                        fs::path p = outDir / (stem + "-a-choice-" + nStr + ".svg");
                        svgExp.exportAChoicePath(inputPath, p.string(), centers,
                                                 ac.path, ori, dst, dotRadius);
                    }
                } else {
                    // ── Full accumulation heatmap (values already in records) ──
                    fs::path p = outDir / (stem + "-a-choice-" + nStr + ".svg");
                    metricsExp.exportAChoiceHeatmap(inputPath, p.string(), records, dotRadius);
                }
            }
            if (doVisConnects) {
                std::mt19937 rng(seed);
                std::uniform_int_distribution<int> dist(0, n - 1);
                int picked = dist(rng);
                fs::path p = outDir / (stem + "-connections-" + nStr + ".svg");
                svgExp.exportConnections(inputPath, p.string(), centers,
                                         picked, graph->neighbours(picked), dotRadius);
            }
            if (doVisGraph) {
                fs::path p = outDir / (stem + "-graph-" + nStr + ".svg");
                svgExp.exportGraph(inputPath, p.string(), centers,
                                   graph->adjacency(), dotRadius);
            }
            if (doVisNose) {
                // Pick O and D: use --ODseed values or fall back to random
                int ori, dst;
                if (noseOrigin >= 0 && noseOrigin < n &&
                    noseDest   >= 0 && noseDest   < n) {
                    ori = noseOrigin;
                    dst = noseDest;
                } else {
                    std::mt19937 rng(seed);
                    std::uniform_int_distribution<int> dist(0, n - 1);
                    ori = dist(rng);
                    do { dst = dist(rng); } while (dst == ori);
                }

                std::cout << "Nose Integration: origin=" << ori
                          << "  dest=" << dst << "\n";

                NoseResult nose = graph->computeNosePath(ori, dst, centers);

                if (!nose.path.empty()) {
                    const double DEG = 180.0 / M_PI;
                    (void)DEG;
                    std::cout << "Path (" << nose.path.size() << " nodes, "
                              << nose.path.size()-1 << " steps):\n";
                    for (int i = 0; i < (int)nose.path.size(); ++i) {
                        int id = nose.path[i];
                        std::cout << "  [" << i << "] node " << id
                                  << "  (" << std::fixed << std::setprecision(1)
                                  << centers[id].x << ", " << centers[id].y << ")";
                        if (i == 0)
                            std::cout << "  <-- origin";
                        else {
                            double cost    = nose.edgeCosts[i-1];
                            double angleDeg = cost * 90.0;
                            std::cout << "  depth=" << std::setprecision(3) << cost
                                      << "  angle=" << std::setprecision(1) << angleDeg << "deg";
                            if (i == (int)nose.path.size()-1)
                                std::cout << "  <-- dest";
                        }
                        std::cout << "\n";
                    }
                    std::cout << "Total depth: " << std::setprecision(3)
                              << nose.totalDepth << "\n";

                    fs::path p = outDir / (stem + "-nose-" + nStr + ".svg");
                    svgExp.exportNosePath(inputPath, p.string(), centers,
                                          nose, ori, dst, dotRadius);
                }
            }
        }

        // ---- 7-isovist visual experiment (always) ----
        {
            const std::vector<std::string> palette = {
                "#e41a1c", "#377eb8", "#4daf4a", "#984ea3",
                "#ff7f00", "#a65628", "#f781bf"
            };
            const int numExp = static_cast<int>(palette.size());
            const int step   = static_cast<int>(centers.size()) / numExp;

            std::cout << "Computing " << numExp << " experiment visibility polygons...\n";
            IsovistComputer computer;
            std::vector<ColoredIsovist> expIsovists;
            for (int i = 0; i < numExp; ++i) {
                const Point& c = centers[static_cast<size_t>(i * step)];
                Polygon vis    = computer.compute(c, fp);
                expIsovists.push_back({std::move(vis), c, palette[i]});
            }
            svgExp.exportExperiment(inputPath,
                                    (outDir / (stem + "_experiment.svg")).string(),
                                    expIsovists);
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
