// simplifySVG — standalone cleaner for SVG floor plans.
//
// Reads an SVG, rewrites every <path> "d" attribute so it contains only
// straight-line commands (M, L, z).  Curve commands are handled two ways:
//   - a Bezier whose control points lie on the start→end chord (an
//     "accidental curve", common in CAD exports) becomes a single L;
//   - a genuinely curved Bezier is tessellated into --segments straight
//     lines so the drawn shape is preserved.
// Elliptical arcs are replaced by their chord (rare in floor plans).
// Everything else in the document (layers, styles, attributes) is untouched.
//
// Usage: simplifySVG <input.svg> [output.svg] [--segments <k>] [--tolerance <t>]
//        output defaults to <input dir>/<input stem>-simple.svg

#include "../vendor/pugixml.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <cmath>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

struct Pt { double x = 0.0, y = 0.0; };

struct Stats {
    int paths       = 0;   // paths rewritten
    int straightened = 0;  // curve commands collapsed to a single line
    int tessellated  = 0;  // curve commands expanded to line segments
    int arcs         = 0;  // arc commands replaced by their chord
};

// Perpendicular distance from p to the segment a→b (falls back to the
// distance to a when the chord is degenerate).
static double distToChord(const Pt& p, const Pt& a, const Pt& b) {
    double ex = b.x - a.x, ey = b.y - a.y;
    double len2 = ex*ex + ey*ey;
    if (len2 < 1e-18)
        return std::hypot(p.x - a.x, p.y - a.y);
    double t = ((p.x - a.x) * ex + (p.y - a.y) * ey) / len2;
    t = std::max(0.0, std::min(1.0, t));
    return std::hypot(p.x - (a.x + t * ex), p.y - (a.y + t * ey));
}

class PathSimplifier {
public:
    PathSimplifier(int segments, double tolerance)
        : m_segments(segments), m_tol(tolerance) {}

    std::string simplify(const std::string& d, Stats& stats) {
        m_d   = &d;
        m_i   = 0;
        m_out.str("");
        m_out << std::fixed << std::setprecision(3);

        Pt   cur, start;
        Pt   prevCubicCtrl2, prevQuadCtrl;
        char prevCmd = '\0';
        char cmd     = '\0';

        while (m_i < d.size()) {
            skip();
            if (m_i >= d.size()) break;

            if (std::isalpha(static_cast<unsigned char>(d[m_i])))
                cmd = d[m_i++];

            bool rel = std::islower(static_cast<unsigned char>(cmd));
            char up  = static_cast<char>(std::toupper(static_cast<unsigned char>(cmd)));

            if (up == 'M') {
                Pt p;
                if (!readPair(p)) break;
                if (rel) { p.x += cur.x; p.y += cur.y; }
                cur = start = p;
                emit('M', p);
                cmd = rel ? 'l' : 'L';   // implicit repetition after M is lineto

            } else if (up == 'L') {
                Pt p;
                if (!readPair(p)) break;
                if (rel) { p.x += cur.x; p.y += cur.y; }
                cur = p;
                emit('L', p);

            } else if (up == 'H') {
                double x;
                if (!readNum(x)) break;
                cur.x = rel ? cur.x + x : x;
                emit('L', cur);

            } else if (up == 'V') {
                double y;
                if (!readNum(y)) break;
                cur.y = rel ? cur.y + y : y;
                emit('L', cur);

            } else if (up == 'C' || up == 'S') {
                Pt c1, c2, e;
                if (up == 'C') {
                    if (!readPair(c1) || !readPair(c2) || !readPair(e)) break;
                    if (rel) { c1.x+=cur.x; c1.y+=cur.y; c2.x+=cur.x; c2.y+=cur.y;
                               e.x +=cur.x; e.y +=cur.y; }
                } else {
                    // S: first control point is the reflection of the previous
                    // cubic's second control point (or the current point).
                    if (!readPair(c2) || !readPair(e)) break;
                    if (rel) { c2.x+=cur.x; c2.y+=cur.y; e.x+=cur.x; e.y+=cur.y; }
                    char pu = static_cast<char>(std::toupper(static_cast<unsigned char>(prevCmd)));
                    if (pu == 'C' || pu == 'S')
                        c1 = { 2*cur.x - prevCubicCtrl2.x, 2*cur.y - prevCubicCtrl2.y };
                    else
                        c1 = cur;
                }
                emitCubic(cur, c1, c2, e, stats);
                prevCubicCtrl2 = c2;
                cur = e;

            } else if (up == 'Q' || up == 'T') {
                Pt q, e;
                if (up == 'Q') {
                    if (!readPair(q) || !readPair(e)) break;
                    if (rel) { q.x+=cur.x; q.y+=cur.y; e.x+=cur.x; e.y+=cur.y; }
                } else {
                    if (!readPair(e)) break;
                    if (rel) { e.x+=cur.x; e.y+=cur.y; }
                    char pu = static_cast<char>(std::toupper(static_cast<unsigned char>(prevCmd)));
                    if (pu == 'Q' || pu == 'T')
                        q = { 2*cur.x - prevQuadCtrl.x, 2*cur.y - prevQuadCtrl.y };
                    else
                        q = cur;
                }
                // Elevate the quadratic to a cubic and reuse the same logic.
                Pt c1 = { cur.x + 2.0/3.0*(q.x - cur.x), cur.y + 2.0/3.0*(q.y - cur.y) };
                Pt c2 = { e.x   + 2.0/3.0*(q.x - e.x),   e.y   + 2.0/3.0*(q.y - e.y) };
                emitCubic(cur, c1, c2, e, stats);
                prevQuadCtrl = q;
                cur = e;

            } else if (up == 'A') {
                // rx ry rot large-arc sweep x y — replaced by the chord.
                double rx, ry, rot, laf, sf;
                Pt e;
                if (!readNum(rx) || !readNum(ry) || !readNum(rot) ||
                    !readNum(laf) || !readNum(sf) || !readPair(e)) break;
                if (rel) { e.x += cur.x; e.y += cur.y; }
                cur = e;
                emit('L', e);
                ++stats.arcs;

            } else if (up == 'Z') {
                m_out << "z ";
                cur = start;

            } else {
                ++m_i;   // unknown command: skip a character, stay safe
            }

            prevCmd = cmd;
        }

        std::string s = m_out.str();
        if (!s.empty() && s.back() == ' ') s.pop_back();
        return s;
    }

private:
    void skip() {
        const std::string& d = *m_d;
        while (m_i < d.size() &&
               (d[m_i]==' ' || d[m_i]=='\t' || d[m_i]=='\n' || d[m_i]=='\r' || d[m_i]==','))
            ++m_i;
    }

    bool readNum(double& val) {
        skip();
        const std::string& d = *m_d;
        if (m_i >= d.size()) return false;
        size_t s = m_i;
        if (d[m_i]=='-' || d[m_i]=='+') ++m_i;
        while (m_i < d.size() &&
               (std::isdigit(static_cast<unsigned char>(d[m_i])) || d[m_i]=='.')) ++m_i;
        if (m_i < d.size() && (d[m_i]=='e' || d[m_i]=='E')) {
            ++m_i;
            if (m_i < d.size() && (d[m_i]=='+' || d[m_i]=='-')) ++m_i;
            while (m_i < d.size() && std::isdigit(static_cast<unsigned char>(d[m_i]))) ++m_i;
        }
        if (m_i == s) return false;
        val = std::stod(d.substr(s, m_i - s));
        return true;
    }

    bool readPair(Pt& p) { return readNum(p.x) && readNum(p.y); }

    void emit(char c, const Pt& p) {
        m_out << c << p.x << "," << p.y << " ";
    }

    // A cubic whose control points sit on the start→end chord draws a straight
    // line; replace it with one L.  Anything else keeps its shape via
    // tessellation into m_segments straight pieces.
    void emitCubic(const Pt& p0, const Pt& c1, const Pt& c2, const Pt& e, Stats& stats) {
        if (distToChord(c1, p0, e) <= m_tol && distToChord(c2, p0, e) <= m_tol) {
            emit('L', e);
            ++stats.straightened;
            return;
        }
        for (int s = 1; s <= m_segments; ++s) {
            double t = static_cast<double>(s) / m_segments;
            double u = 1.0 - t;
            Pt p { u*u*u*p0.x + 3*u*u*t*c1.x + 3*u*t*t*c2.x + t*t*t*e.x,
                   u*u*u*p0.y + 3*u*u*t*c1.y + 3*u*t*t*c2.y + t*t*t*e.y };
            emit('L', p);
        }
        ++stats.tessellated;
    }

    const std::string* m_d = nullptr;
    size_t             m_i = 0;
    std::ostringstream m_out;
    int                m_segments;
    double             m_tol;
};

static void walkPaths(pugi::xml_node node, PathSimplifier& simp, Stats& stats) {
    for (pugi::xml_node child : node.children()) {
        if (std::string(child.name()) == "path") {
            pugi::xml_attribute dAttr = child.attribute("d");
            if (dAttr && *dAttr.as_string()) {
                std::string nd = simp.simplify(dAttr.as_string(), stats);
                dAttr.set_value(nd.c_str());
                ++stats.paths;
            }
        }
        walkPaths(child, simp, stats);
    }
}

int main(int argc, char* argv[]) {
    std::string inPath, outPath;
    int    segments  = 8;
    double tolerance = 0.05;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if      (arg == "--segments"  && i+1 < argc) segments  = std::stoi(argv[++i]);
        else if (arg == "--tolerance" && i+1 < argc) tolerance = std::stod(argv[++i]);
        else if (arg[0] != '-' && inPath.empty())    inPath  = arg;
        else if (arg[0] != '-' && outPath.empty())   outPath = arg;
        else {
            std::cerr << "Unknown argument: " << arg << "\n";
            return 1;
        }
    }

    if (inPath.empty()) {
        std::cerr << "Usage: simplifySVG <input.svg> [output.svg]"
                  << " [--segments <k>] [--tolerance <t>]\n"
                  << "  --segments <k>   lines per genuine curve   (default: 8)\n"
                  << "  --tolerance <t>  max control-point distance from chord\n"
                  << "                   to call a curve straight  (default: 0.05)\n";
        return 1;
    }

    if (outPath.empty()) {
        fs::path p(inPath);
        outPath = (p.parent_path() / (p.stem().string() + "-simple.svg")).string();
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(inPath.c_str());
    if (!result) {
        std::cerr << "Failed to parse " << inPath << ": " << result.description() << "\n";
        return 1;
    }

    pugi::xml_node svgNode = doc.child("svg");
    if (!svgNode) {
        std::cerr << "No <svg> root element in " << inPath << "\n";
        return 1;
    }

    PathSimplifier simp(segments, tolerance);
    Stats stats;
    walkPaths(svgNode, simp, stats);

    if (!doc.save_file(outPath.c_str())) {
        std::cerr << "Failed to write " << outPath << "\n";
        return 1;
    }

    std::cout << "Rewrote " << stats.paths << " paths -> " << outPath << "\n"
              << "  curves collapsed to straight lines: " << stats.straightened << "\n"
              << "  curves tessellated (" << segments << " segments each): "
              << stats.tessellated << "\n"
              << "  arcs replaced by chords: " << stats.arcs << "\n";
    return 0;
}
