#pragma once
#include <string>
#include <vector>

// One observation gate: a labelled point with a pedestrian count.
struct Gate {
    std::string label;
    double x     = 0.0;
    double y     = 0.0;
    double count = 0.0;
};

class GateFile {
public:
    // Loads a CSV of Label,X,Y,Count rows.  A header row (non-numeric
    // X field) is skipped automatically.  Throws on unreadable file.
    static std::vector<Gate> load(const std::string& path);
};
