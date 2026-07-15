#include "Network.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <iostream>

void Network::save(const std::vector<Point>& centers, const std::string& path) {
    std::ofstream out(path);
    if (!out)
        throw std::runtime_error("Network: cannot open for writing: " + path);

    out << "id,x,y\n";
    out << std::fixed << std::setprecision(6);
    for (size_t i = 0; i < centers.size(); ++i)
        out << i << "," << centers[i].x << "," << centers[i].y << "\n";

    std::cout << "Saved network (" << centers.size()
              << " centers) to: " << path << "\n";
}

std::vector<Point> Network::load(const std::string& path) {
    std::ifstream in(path);
    if (!in)
        throw std::runtime_error("Network: cannot open: " + path);

    std::vector<Point> centers;
    std::string line;
    int lineNo = 0;

    while (std::getline(in, line)) {
        ++lineNo;
        if (line.empty()) continue;

        // id,x,y — take the last two fields so a bare x,y file works too
        std::vector<std::string> fields;
        std::istringstream ss(line);
        std::string field;
        while (std::getline(ss, field, ','))
            fields.push_back(field);
        if (fields.size() < 2) continue;

        try {
            double x = std::stod(fields[fields.size() - 2]);
            double y = std::stod(fields[fields.size() - 1]);
            centers.push_back(Point(x, y));
        } catch (...) {
            if (lineNo != 1)   // line 1 is the header
                std::cerr << "Network: skipping malformed line " << lineNo << "\n";
        }
    }

    if (centers.empty())
        throw std::runtime_error("Network: no coordinates found in: " + path);

    std::cout << "Restored network (" << centers.size()
              << " centers) from: " << path << "\n";
    return centers;
}
