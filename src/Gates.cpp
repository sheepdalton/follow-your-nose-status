#include "Gates.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>

namespace {

// Trim whitespace and CR from both ends.
std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool parseDouble(const std::string& s, double& out) {
    try {
        size_t pos = 0;
        out = std::stod(s, &pos);
        return pos == s.size();
    } catch (...) {
        return false;
    }
}

} // namespace

std::vector<Gate> GateFile::load(const std::string& path) {
    std::ifstream in(path);
    if (!in)
        throw std::runtime_error("GateFile: cannot open: " + path);

    std::vector<Gate> gates;
    std::string line;
    int lineNo = 0;

    while (std::getline(in, line)) {
        ++lineNo;
        line = trim(line);
        if (line.empty()) continue;

        std::vector<std::string> fields;
        std::istringstream ss(line);
        std::string field;
        while (std::getline(ss, field, ','))
            fields.push_back(trim(field));

        if (fields.size() < 4) {
            std::cerr << "GateFile: line " << lineNo
                      << " has fewer than 4 fields — skipped\n";
            continue;
        }

        Gate g;
        g.label = fields[0];
        if (!parseDouble(fields[1], g.x) ||
            !parseDouble(fields[2], g.y) ||
            !parseDouble(fields[3], g.count)) {
            if (lineNo == 1) continue;   // header row
            std::cerr << "GateFile: line " << lineNo
                      << " has non-numeric X/Y/count — skipped\n";
            continue;
        }
        gates.push_back(std::move(g));
    }

    return gates;
}
