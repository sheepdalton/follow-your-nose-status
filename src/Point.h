#pragma once
#include <cmath>

struct Point {
    double x, y;

    Point(double x = 0.0, double y = 0.0) : x(x), y(y) {}

    double distanceTo(const Point& other) const {
        double dx = x - other.x;
        double dy = y - other.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};
