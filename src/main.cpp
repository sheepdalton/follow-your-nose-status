#include "SVGParser.h"
#include "Gates.h"
#include "Network.h"
#include "IsovistPlacer.h"
#include "IsovistComputer.h"
#include "IsovistRecord.h"
#include "VisibilityGraph.h"
#include "SVGExporter.h"
#include "MetricsExporter.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <filesystem>
#include <memory>
#include <random>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <limits>

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
              << "  --vis-polar-single polar (angle x distance) single path SVG\n"
              << "  --vis-metric-single metric (euclidean distance) single path SVG\n"
              << "  --polar-g <g>      polar angle exponent gamma (default: 1; 0=metric, >1=straighter)\n"
              << "  --vis-polar-status-angle    heatmap: sum of angle costs of all polar routes here\n"
              << "  --vis-polar-status-product  heatmap: sum of angle x distance costs of all polar routes here\n"
              << "  --vis-topo-status  heatmap: total topological depth (integration)\n"
              << "  --gates <file>     match gate-count CSV (label,x,y,count) to isovists;\n"
              << "                     writes <stem>-gate-counts.csv with all measures\n"
              << "  --MAX-GATE-RADIUS <r>  max gate-to-isovist match distance (default: 50)\n"
              << "  --HEAL             on a disconnected graph, add bridge points (kept only\n"
              << "                     if they join islands) until fully connected\n"
              << "  --NETWORK <file>          save final isovist centres (after any HEAL) as CSV\n"
              << "  --NETWORK-RESTORE <file>  skip placement, load centres from a saved network\n"
              << "  --ODseed <O> <D>   origin and dest node IDs for nose (default: random)\n"
              << "  --SIZE <r>        override dot radius (default: 3)\n"
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
    bool         doVisPolar     = false;
    bool         doVisMetric    = false;
    double       polarGamma     = 1.0;
    bool         doVisPolarStatusAngle   = false;
    bool         doVisPolarStatusProduct = false;
    bool         doVisTopoStatus         = false;
    std::string  gatesFile;
    double       maxGateRadius  = 50.0;
    bool         doHeal         = false;
    std::string  networkOut;
    std::string  networkIn;
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
        else if (arg == "--vis-polar-single")        { doVisPolar    = true; }
        else if (arg == "--vis-metric-single")       { doVisMetric   = true; }
        else if (arg == "--polar-g" && i+1<argc)     { polarGamma    = std::stod(argv[++i]); }
        else if (arg == "--vis-polar-status-angle")  { doVisPolarStatusAngle   = true; }
        else if (arg == "--vis-polar-status-product"){ doVisPolarStatusProduct = true; }
        else if (arg == "--vis-topo-status")         { doVisTopoStatus         = true; }
        else if (arg == "--gates" && i+1<argc)       { gatesFile     = argv[++i]; }
        else if (arg == "--MAX-GATE-RADIUS" && i+1<argc) { maxGateRadius = std::stod(argv[++i]); }
        else if (arg == "--HEAL")                    { doHeal        = true; }
        else if (arg == "--NETWORK" && i+1<argc)         { networkOut = argv[++i]; }
        else if (arg == "--NETWORK-RESTORE" && i+1<argc) { networkIn  = argv[++i]; }
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

        std::vector<Point> centers;
        if (!networkIn.empty()) {
            if (doHeal) {
                std::cerr << "ERROR: --HEAL cannot be combined with --NETWORK-RESTORE "
                          << "(healing would regenerate the restored centres).\n";
                return 1;
            }
            centers = Network::load(networkIn);
            n    = static_cast<int>(centers.size());
            nStr = std::to_string(n);
        } else {
            std::cout << "Placing " << n << " isovist centers "
                      << "(candidates=" << candidates << ", seed=" << seed << ")...\n";
            IsovistPlacer placer(fp, n, candidates, seed);
            centers = placer.place();
        }

        double dotRadius = (sizeOverride > 0.0) ? sizeOverride : 3.0;
        std::cout << "Open area: " << fp.openArea() << "  dot radius: " << dotRadius << "\n";

        SVGExporter svgExp;
        svgExp.exportSVG(inputPath, (outDir / (stem + ".svg")).string(), centers, dotRadius);

        // ---- compute all visibility polygons ----
        bool doGates       = !gatesFile.empty();
        bool doPolarStatus = doVisPolarStatusAngle || doVisPolarStatusProduct;
        bool needMetrics = doCSV || doVisArea || doVisPerim || doVisDegree || doVisChoice || doVisDChoice || doVisAChoice || doVisConnects || doVisGraph || doVisNose || doVisPolar || doVisMetric || doPolarStatus || doVisTopoStatus || doGates;
        bool needGraph   = doVisDegree || doVisChoice || doVisDChoice || doVisAChoice || doVisConnects || doVisGraph || doVisNose || doVisPolar || doVisMetric || doPolarStatus || doVisTopoStatus || doGates;

        std::vector<IsovistRecord> records;
        std::vector<Polygon>       polygons; // kept only when graph is required

        if (needMetrics) {
            IsovistComputer computer;
            std::unique_ptr<VisibilityGraph> graph;

            // Compute all polygons and (if needed) the visibility graph.
            // With --HEAL a disconnected graph is repaired in-place by adding
            // bridge points (kept only when they join islands); each round
            // re-checks connectivity until one component remains.
            const int MAX_HEAL_ROUNDS = 100;
            int healRound = 0;
            while (true) {
                std::cout << "Computing " << n << " visibility polygons...\n";
                records.clear();
                polygons.clear();
                records.reserve(n);
                if (needGraph) polygons.reserve(n);

                for (int i = 0; i < n; ++i) {
                    Polygon vis = computer.compute(centers[i], fp);
                    records.push_back({i, centers[i], vis.area(), vis.perimeter(), 0});
                    if (needGraph) polygons.push_back(vis);
                    if ((i + 1) % 50 == 0)
                        std::cout << "  " << (i+1) << " / " << n << " done\n";
                }

                if (!needGraph) break;

                graph = std::make_unique<VisibilityGraph>(n);
                graph->build(polygons, centers);

                std::vector<int> comps = graph->componentSizes();
                if (comps.size() == 1) {        // fully connected
                    polygons.clear();
                    polygons.shrink_to_fit();
                    break;
                }

                if (!doHeal) {
                    if (doVisGraph) {
                        // Export the component-coloured graph anyway — it is
                        // exactly the diagnostic needed for a fragmented map.
                        fs::path p = outDir / (stem + "-graph-" + nStr + ".svg");
                        svgExp.exportGraph(inputPath, p.string(), centers,
                                           graph->adjacency(), dotRadius);
                    }
                    std::cerr << "\nERROR: the visibility graph is NOT fully connected.\n"
                              << "  " << comps.size() << " components; largest has "
                              << comps[0] << " of " << n << " nodes.\n"
                              << "  Graph measures would be meaningless across components.\n"
                              << "  Increase the resolution (--n), change --seed, or use --HEAL.\n";
                    return 1;
                }

                if (++healRound > MAX_HEAL_ROUNDS) {
                    std::cerr << "\nERROR: --HEAL gave up after " << MAX_HEAL_ROUNDS
                              << " rounds (still " << comps.size()
                              << " islands at n=" << n << ").\n"
                              << "  The open space itself may be disconnected.\n";
                    return 1;
                }

                // Bridge-hunting heal: sample candidate points and keep one
                // only if it sees nodes of two or more different islands (it
                // merges them).  Candidates are drawn from a Gaussian centred
                // on the SMALLEST remaining island — most of the map can never
                // yield a bridge, so aiming at the offending cluster is far
                // more efficient than sampling uniformly.  The spread widens
                // adaptively if the tight search stalls, and falls back to a
                // full-uniform search as a last resort, so it is never worse
                // than the old uniform hunt.  Only accepted bridges are kept,
                // so the node count grows minimally.
                std::cout << "HEAL: " << comps.size()
                          << " islands -> hunting bridge points (targeted)...\n";

                std::vector<int> labels = graph->componentLabels();
                int islandCount = static_cast<int>(comps.size());

                Polygon::BBox bbox = fp.boundingBox();
                double maxDim   = std::max(bbox.maxX - bbox.minX,
                                           bbox.maxY - bbox.minY);
                double sigmaBase = 0.08 * maxDim;   // tight: hug the target island
                double sigmaCap  = 0.20 * maxDim;   // broadest before uniform fallback

                std::mt19937 rng(seed + 1000003u * static_cast<unsigned>(healRound));
                std::uniform_real_distribution<double> rx(bbox.minX, bbox.maxX);
                std::uniform_real_distribution<double> ry(bbox.minY, bbox.maxY);

                // Centre the Gaussian on a random real node of the smallest
                // remaining island.  A real node is guaranteed to sit in valid
                // open space, unlike a centroid that may fall inside a wall.
                auto pickTargetCentre = [&](Point& out) {
                    std::unordered_map<int,int> count;
                    for (int l : labels) ++count[l];
                    int bestLabel = labels[0];
                    int bestCount = std::numeric_limits<int>::max();
                    for (const auto& kv : count)
                        if (kv.second < bestCount) { bestCount = kv.second; bestLabel = kv.first; }
                    std::vector<int> members;
                    for (int i = 0; i < (int)labels.size(); ++i)
                        if (labels[i] == bestLabel) members.push_back(i);
                    std::uniform_int_distribution<int> pick(0, (int)members.size() - 1);
                    out = centers[members[pick(rng)]];
                };

                Point targetCentre;
                pickTargetCentre(targetCentre);

                const int MAX_BRIDGE_TRIES = 20000;  // consecutive fails -> give up
                const int WIDEN_AFTER      = 400;    // fails at one spread before widening
                double sigma       = sigmaBase;
                int    fails       = 0;   // consecutive (governs give-up)
                int    localFails  = 0;   // consecutive at current spread (governs widening)
                int    accepted    = 0;
                bool   uniformMode = false;

                while (islandCount > 1 && fails < MAX_BRIDGE_TRIES) {
                    Point cand;
                    if (uniformMode) {
                        cand = Point(rx(rng), ry(rng));
                    } else {
                        std::normal_distribution<double> gx(targetCentre.x, sigma);
                        std::normal_distribution<double> gy(targetCentre.y, sigma);
                        cand = Point(gx(rng), gy(rng));
                    }

                    // Which islands does this candidate see?  (Visibility is
                    // symmetric: cand sees node j iff cand is inside j's polygon.)
                    std::vector<int> seen;
                    if (fp.isValidPoint(cand)) {
                        for (size_t j = 0; j < polygons.size(); ++j) {
                            if (polygons[j].containsPoint(cand)) {
                                int c = labels[j];
                                if (std::find(seen.begin(), seen.end(), c) == seen.end())
                                    seen.push_back(c);
                            }
                        }
                    }

                    if (seen.size() < 2) {
                        ++fails;
                        if (++localFails >= WIDEN_AFTER) {
                            localFails = 0;
                            if (!uniformMode && sigma < sigmaCap) {
                                sigma = std::min(sigma * 1.6, sigmaCap);
                                pickTargetCentre(targetCentre);  // try a different node
                            } else if (!uniformMode) {
                                uniformMode = true;              // last resort: global search
                            }
                        }
                        continue;
                    }

                    // Accept: merge every island it sees into one, and give
                    // the bridge its own polygon so later bridges can chain.
                    int keep = seen[0];
                    for (int& l : labels)
                        if (std::find(seen.begin()+1, seen.end(), l) != seen.end())
                            l = keep;
                    islandCount -= static_cast<int>(seen.size()) - 1;

                    centers.push_back(cand);
                    polygons.push_back(computer.compute(cand, fp));
                    labels.push_back(keep);
                    ++accepted;

                    // Restart the hunt tightly on the next smallest island.
                    fails = 0; localFails = 0;
                    sigma = sigmaBase; uniformMode = false;
                    if (islandCount > 1) pickTargetCentre(targetCentre);
                }

                if (islandCount > 1) {
                    if (doVisGraph) {
                        // Rebuild including the accepted bridges so the
                        // coloured graph shows the truly unbridgeable islands.
                        VisibilityGraph gFinal(static_cast<int>(centers.size()));
                        gFinal.build(polygons, centers);
                        fs::path p = outDir / (stem + "-graph-"
                                     + std::to_string(centers.size()) + ".svg");
                        svgExp.exportGraph(inputPath, p.string(), centers,
                                           gFinal.adjacency(), dotRadius);
                    }
                    std::cerr << "\nERROR: HEAL gave up: no bridge candidate found in "
                              << MAX_BRIDGE_TRIES << " consecutive tries; "
                              << islandCount << " islands remain (after "
                              << accepted << " bridges).\n"
                              << "  The remaining islands are probably sealed spaces "
                              << "(e.g. courtyards) that no point can bridge.\n"
                              << "  Use --vis-graph without --HEAL to see them.\n";
                    return 1;
                }

                n    = static_cast<int>(centers.size());
                nStr = std::to_string(n);
                std::cout << "HEAL: connected with " << accepted
                          << " bridge points (n = " << n << "); verifying...\n";
                svgExp.exportSVG(inputPath, (outDir / (stem + ".svg")).string(),
                                 centers, dotRadius);
            }

            // ---- graph-derived measures ----
            if (needGraph) {
                for (int i = 0; i < n; ++i)
                    records[i].degree = graph->degree(i);

                if (doVisChoice || doGates) {
                    std::vector<double> choice = graph->computeChoice();
                    for (int i = 0; i < n; ++i)
                        records[i].choice = choice[i];
                }
                if (doVisDChoice || doGates) {
                    std::vector<double> dc = graph->computeDChoice();
                    for (int i = 0; i < n; ++i)
                        records[i].dChoice = dc[i];
                }
                bool aChoiceFullAccum = doVisAChoice &&
                                        !(aChoiceOrigin >= 0 && aChoiceOrigin < n &&
                                          aChoiceDest   >= 0 && aChoiceDest   < n);
                if (aChoiceFullAccum || doGates) {
                    std::vector<double> ac = graph->computeAChoice(centers);
                    for (int i = 0; i < n; ++i)
                        records[i].aChoice = ac[i];
                }
                if (doPolarStatus || doGates) {
                    std::vector<double> psa, psp;
                    graph->computePolarStatus(centers, psa, psp, polarGamma);
                    for (int i = 0; i < n; ++i) {
                        records[i].polarStatusAngle   = psa[i];
                        records[i].polarStatusProduct = psp[i];
                    }
                }
                if (doVisTopoStatus || doGates || doCSV) {
                    std::vector<double> ts = graph->computeTopoStatus();
                    for (int i = 0; i < n; ++i)
                        records[i].topoStatus = ts[i];
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
            if (doVisPolarStatusAngle) {
                fs::path p = outDir / (stem + "-polar-status-angle-" + nStr + ".svg");
                metricsExp.exportPolarStatusAngleHeatmap(inputPath, p.string(), records, dotRadius);
            }
            if (doVisPolarStatusProduct) {
                fs::path p = outDir / (stem + "-polar-status-product-" + nStr + ".svg");
                metricsExp.exportPolarStatusProductHeatmap(inputPath, p.string(), records, dotRadius);
            }
            if (doVisTopoStatus) {
                fs::path p = outDir / (stem + "-topo-status-" + nStr + ".svg");
                metricsExp.exportTopoStatusHeatmap(inputPath, p.string(), records, dotRadius);
            }
            if (doGates) {
                std::vector<Gate> gates = GateFile::load(gatesFile);
                std::cout << "Loaded " << gates.size() << " gates from: " << gatesFile << "\n";

                // Match each gate to the nearest isovist centre within radius
                std::vector<int> matched(gates.size(), -1);
                int unmatched = 0;
                for (size_t g = 0; g < gates.size(); ++g) {
                    Point gp(gates[g].x, gates[g].y);
                    double best = maxGateRadius;
                    for (int i = 0; i < n; ++i) {
                        double d = centers[i].distanceTo(gp);
                        if (d <= best) { best = d; matched[g] = i; }
                    }
                    if (matched[g] < 0) {
                        ++unmatched;
                        std::cerr << "WARNING: gate '" << gates[g].label
                                  << "' has no isovist within " << maxGateRadius
                                  << " units\n";
                    }
                }
                if (unmatched)
                    std::cerr << unmatched << " of " << gates.size()
                              << " gates unmatched\n";

                fs::path csvP = outDir / (stem + "-gate-counts.csv");
                metricsExp.exportGateCSV(gates, matched, records, csvP.string());

                fs::path svgP = outDir / (stem + "-gates-" + nStr + ".svg");
                svgExp.exportGates(inputPath, svgP.string(), centers,
                                   gates, matched, dotRadius);
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
                                  << centers[id].x << ", " << centers[id].y << ")"
                                  << "  topo=" << nose.topoDepths[i];
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

                    // Debug: all nodes coloured by topological depth from dest
                    fs::path pt = outDir / (stem + "-nose-topo-" + nStr + ".svg");
                    svgExp.exportTopoDepth(inputPath, pt.string(), centers,
                                           graph->computeTopoDepths(dst),
                                           nose, ori, dst, dotRadius);
                }
            }
            if (doVisPolar) {
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

                std::cout << "Polar path: origin=" << ori
                          << "  dest=" << dst << "\n";

                NoseResult polar = graph->computePolarPath(ori, dst, centers, polarGamma);

                if (!polar.path.empty()) {
                    std::cout << "Path (" << polar.path.size() << " nodes, "
                              << polar.path.size()-1 << " steps):\n";
                    for (int i = 0; i < (int)polar.path.size(); ++i) {
                        int id = polar.path[i];
                        std::cout << "  [" << i << "] node " << id
                                  << "  (" << std::fixed << std::setprecision(1)
                                  << centers[id].x << ", " << centers[id].y << ")"
                                  << "  topo=" << polar.topoDepths[i];
                        if (i == 0)
                            std::cout << "  <-- origin";
                        else {
                            // True turn angle at node i-1: between (A→dest) and (A→next),
                            // computed geometrically so it is correct for any gamma.
                            const Point& A = centers[polar.path[i-1]];
                            const Point& B = centers[id];
                            const Point& D = centers[dst];
                            double dxv = D.x - A.x, dyv = D.y - A.y;
                            double exv = B.x - A.x, eyv = B.y - A.y;
                            double magD = std::sqrt(dxv*dxv + dyv*dyv);
                            double magE = std::sqrt(exv*exv + eyv*eyv);
                            double angleDeg = 0.0;
                            if (magD > 1e-12 && magE > 1e-12) {
                                double c = (dxv*exv + dyv*eyv) / (magD*magE);
                                c = std::max(-1.0, std::min(1.0, c));
                                angleDeg = std::acos(c) * 180.0 / M_PI;
                            }
                            std::cout << "  cost=" << std::setprecision(3) << polar.edgeCosts[i-1]
                                      << "  dist=" << std::setprecision(1) << magE
                                      << "  angle=" << angleDeg << "deg";
                            if (i == (int)polar.path.size()-1)
                                std::cout << "  <-- dest";
                        }
                        std::cout << "\n";
                    }
                    std::cout << "Total polar cost: " << std::setprecision(3)
                              << polar.totalDepth << "\n";

                    std::ostringstream lbl, gtag;
                    lbl  << "polar (g=" << polarGamma << ")";
                    gtag << "-g" << polarGamma;
                    fs::path p = outDir / (stem + "-polar" + gtag.str() + "-" + nStr + ".svg");
                    svgExp.exportNosePath(inputPath, p.string(), centers,
                                          polar, ori, dst, dotRadius, lbl.str());
                }
            }
            if (doVisMetric) {
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

                std::cout << "Metric path: origin=" << ori
                          << "  dest=" << dst << "\n";

                NoseResult metric = graph->computeMetricPath(ori, dst, centers);

                if (!metric.path.empty()) {
                    std::cout << "Path (" << metric.path.size() << " nodes, "
                              << metric.path.size()-1 << " steps):\n";
                    for (int i = 0; i < (int)metric.path.size(); ++i) {
                        int id = metric.path[i];
                        std::cout << "  [" << i << "] node " << id
                                  << "  (" << std::fixed << std::setprecision(1)
                                  << centers[id].x << ", " << centers[id].y << ")"
                                  << "  topo=" << metric.topoDepths[i];
                        if (i == 0)
                            std::cout << "  <-- origin";
                        else {
                            double len = metric.edgeCosts[i-1];
                            std::cout << "  dist=" << std::setprecision(1) << len;
                            if (i == (int)metric.path.size()-1)
                                std::cout << "  <-- dest";
                        }
                        std::cout << "\n";
                    }
                    std::cout << "Total metric length: " << std::setprecision(1)
                              << metric.totalDepth << "\n";

                    fs::path p = outDir / (stem + "-metric-" + nStr + ".svg");
                    svgExp.exportNosePath(inputPath, p.string(), centers,
                                          metric, ori, dst, dotRadius, "metric");
                }
            }
        }

        // ---- save network (post-HEAL centres) ----
        if (!networkOut.empty())
            Network::save(centers, networkOut);

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
