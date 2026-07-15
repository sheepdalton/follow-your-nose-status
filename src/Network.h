#pragma once
#include "Point.h"
#include <string>
#include <vector>

// Save/restore of a placed isovist-centre network, so an expensive
// (e.g. healed) placement can be reused across runs.
class Network {
public:
    // Writes id,x,y CSV.  Throws on unwritable file.
    static void save(const std::vector<Point>& centers, const std::string& path);

    // Reads centres back (header row skipped).  Throws on unreadable file.
    static std::vector<Point> load(const std::string& path);
};
