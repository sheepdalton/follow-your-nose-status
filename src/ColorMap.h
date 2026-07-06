#pragma once
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>

// Converts HSB (hue 0-360, sat 0-1, brightness 0-1) to a "#rrggbb" hex string.
inline std::string hsbToHex(double hue, double sat, double brightness) {
    // Clamp
    hue        = std::fmod(hue, 360.0);
    if (hue < 0) hue += 360.0;
    sat        = std::max(0.0, std::min(1.0, sat));
    brightness = std::max(0.0, std::min(1.0, brightness));

    double C = brightness * sat;
    double X = C * (1.0 - std::abs(std::fmod(hue / 60.0, 2.0) - 1.0));
    double m = brightness - C;

    double r = 0, g = 0, b = 0;
    if      (hue <  60) { r = C; g = X; b = 0; }
    else if (hue < 120) { r = X; g = C; b = 0; }
    else if (hue < 180) { r = 0; g = C; b = X; }
    else if (hue < 240) { r = 0; g = X; b = C; }
    else if (hue < 300) { r = X; g = 0; b = C; }
    else                { r = C; g = 0; b = X; }

    int ri = static_cast<int>(std::round((r + m) * 255));
    int gi = static_cast<int>(std::round((g + m) * 255));
    int bi = static_cast<int>(std::round((b + m) * 255));

    std::ostringstream ss;
    ss << "#" << std::hex << std::setfill('0')
       << std::setw(2) << ri
       << std::setw(2) << gi
       << std::setw(2) << bi;
    return ss.str();
}

// Maps a value to a hue in [0°, 240°] (red→blue), or [240°, 0°] if flip=true (blue→red).
inline std::string valueToColor(double value, double minVal, double maxVal, bool flip = false) {
    double t = (maxVal > minVal) ? (value - minVal) / (maxVal - minVal) : 0.0;
    t = std::max(0.0, std::min(1.0, t));
    if (flip) t = 1.0 - t;
    double hue = t * 240.0;
    return hsbToHex(hue, 1.0, 0.9);
}
