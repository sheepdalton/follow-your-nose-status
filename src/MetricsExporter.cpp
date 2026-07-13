#include "MetricsExporter.h"
#include "ColorMap.h"
#include "SVGLabel.h"
#include "../vendor/pugixml.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <limits>
#include <cmath>

// ---- CSV ----

void MetricsExporter::exportCSV(const std::vector<IsovistRecord>& records,
                                  const std::string& outputPath) const {
    std::ofstream out(outputPath);
    if (!out)
        throw std::runtime_error("MetricsExporter: cannot open for writing: " + outputPath);

    out << "id,x,y,area,perimeter,degree,choice,d_choice,a_choice,"
           "polar_status_angle,polar_status_product,topo_status,prospect_status\n";
    out << std::fixed << std::setprecision(4);
    for (const auto& r : records)
        out << r.id << "," << r.center.x << "," << r.center.y
            << "," << r.area << "," << r.perimeter
            << "," << r.degree << "," << r.choice
            << "," << r.dChoice << "," << r.aChoice
            << "," << r.polarStatusAngle << "," << r.polarStatusProduct
            << "," << r.topoStatus << "," << r.prospectStatus << "\n";

    std::cout << "Exported CSV (" << records.size() << " rows) to: " << outputPath << "\n";
}

void MetricsExporter::exportGateCSV(const std::vector<Gate>& gates,
                                     const std::vector<int>& matched,
                                     const std::vector<IsovistRecord>& records,
                                     const std::string& outputPath) const {
    std::ofstream out(outputPath);
    if (!out)
        throw std::runtime_error("MetricsExporter: cannot open for writing: " + outputPath);

    out << "label,x,y,count,log_count,isovist_x,isovist_y,isovist_id,"
           "area,perimeter,degree,choice,d_choice,a_choice,"
           "polar_status_angle,polar_status_product,topo_status,prospect_status\n";
    out << std::fixed << std::setprecision(4);

    for (size_t g = 0; g < gates.size(); ++g) {
        const Gate& gate = gates[g];
        out << gate.label << "," << gate.x << "," << gate.y
            << "," << gate.count << ",";
        if (gate.count > 0.0) out << std::log(gate.count);
        out << ",";

        int idx = matched[g];
        if (idx >= 0) {
            const IsovistRecord& r = records[idx];
            out << r.center.x << "," << r.center.y << "," << r.id
                << "," << r.area << "," << r.perimeter
                << "," << r.degree << "," << r.choice
                << "," << r.dChoice << "," << r.aChoice
                << "," << r.polarStatusAngle << "," << r.polarStatusProduct
                << "," << r.topoStatus << "," << r.prospectStatus;
        } else {
            out << ",,,,,,,,,,,,";   // no isovist within range
        }
        out << "\n";
    }

    std::cout << "Exported gate counts (" << gates.size()
              << " rows) to: " << outputPath << "\n";
}

// ---- Heatmap dispatchers ----

void MetricsExporter::exportAreaHeatmap(const std::string& inputSVGPath,
                                         const std::string& outputPath,
                                         const std::vector<IsovistRecord>& records,
                                         double dotRadius) const {
    exportHeatmap(inputSVGPath, outputPath, records, Metric::Area, dotRadius);
}

void MetricsExporter::exportPerimeterHeatmap(const std::string& inputSVGPath,
                                              const std::string& outputPath,
                                              const std::vector<IsovistRecord>& records,
                                              double dotRadius) const {
    exportHeatmap(inputSVGPath, outputPath, records, Metric::Perimeter, dotRadius);
}

void MetricsExporter::exportDegreeHeatmap(const std::string& inputSVGPath,
                                           const std::string& outputPath,
                                           const std::vector<IsovistRecord>& records,
                                           double dotRadius) const {
    exportHeatmap(inputSVGPath, outputPath, records, Metric::Degree, dotRadius);
}

void MetricsExporter::exportChoiceHeatmap(const std::string& inputSVGPath,
                                           const std::string& outputPath,
                                           const std::vector<IsovistRecord>& records,
                                           double dotRadius) const {
    exportHeatmap(inputSVGPath, outputPath, records, Metric::Choice, dotRadius);
}

void MetricsExporter::exportDChoiceHeatmap(const std::string& inputSVGPath,
                                            const std::string& outputPath,
                                            const std::vector<IsovistRecord>& records,
                                            double dotRadius) const {
    exportHeatmap(inputSVGPath, outputPath, records, Metric::DChoice, dotRadius);
}

void MetricsExporter::exportAChoiceHeatmap(const std::string& inputSVGPath,
                                            const std::string& outputPath,
                                            const std::vector<IsovistRecord>& records,
                                            double dotRadius) const {
    exportHeatmap(inputSVGPath, outputPath, records, Metric::AChoice, dotRadius);
}

void MetricsExporter::exportPolarStatusAngleHeatmap(const std::string& inputSVGPath,
                                                     const std::string& outputPath,
                                                     const std::vector<IsovistRecord>& records,
                                                     double dotRadius) const {
    exportHeatmap(inputSVGPath, outputPath, records, Metric::PolarStatusAngle, dotRadius);
}

void MetricsExporter::exportPolarStatusProductHeatmap(const std::string& inputSVGPath,
                                                       const std::string& outputPath,
                                                       const std::vector<IsovistRecord>& records,
                                                       double dotRadius) const {
    exportHeatmap(inputSVGPath, outputPath, records, Metric::PolarStatusProduct, dotRadius);
}

void MetricsExporter::exportProspectStatusHeatmap(const std::string& inputSVGPath,
                                                   const std::string& outputPath,
                                                   const std::vector<IsovistRecord>& records,
                                                   double dotRadius) const {
    exportHeatmap(inputSVGPath, outputPath, records, Metric::ProspectStatus, dotRadius);
}

void MetricsExporter::exportTopoStatusHeatmap(const std::string& inputSVGPath,
                                               const std::string& outputPath,
                                               const std::vector<IsovistRecord>& records,
                                               double dotRadius) const {
    exportHeatmap(inputSVGPath, outputPath, records, Metric::TopoStatus, dotRadius);
}

// ---- Shared heatmap implementation ----

void MetricsExporter::exportHeatmap(const std::string& inputSVGPath,
                                     const std::string& outputPath,
                                     const std::vector<IsovistRecord>& records,
                                     Metric metric,
                                     double dotRadius) const {
    if (records.empty()) return;

    auto getValue = [&](const IsovistRecord& r) -> double {
        switch (metric) {
            case Metric::Area:      return r.area;
            case Metric::Perimeter: return r.perimeter;
            case Metric::Degree:    return static_cast<double>(r.degree);
            case Metric::Choice:    return r.choice;
            case Metric::DChoice:   return r.dChoice;
            case Metric::AChoice:   return r.aChoice;
            case Metric::PolarStatusAngle:   return r.polarStatusAngle;
            case Metric::PolarStatusProduct: return r.polarStatusProduct;
            case Metric::TopoStatus:         return r.topoStatus;
            case Metric::ProspectStatus:     return r.prospectStatus;
        }
        return 0.0;
    };

    // Optionally apply natural log to compress the scale.
    // log(v+1) keeps zero values at zero and avoids log(0).
    auto display = [&](double v) {
        return m_log ? std::log(v + 1.0) : v;
    };

    double minVal = std::numeric_limits<double>::max();
    double maxVal = std::numeric_limits<double>::lowest();
    for (const auto& r : records) {
        double v = display(getValue(r));
        minVal = std::min(minVal, v);
        maxVal = std::max(maxVal, v);
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(inputSVGPath.c_str());
    if (!result)
        throw std::runtime_error("MetricsExporter: failed to load SVG: " +
                                 std::string(result.description()));

    pugi::xml_node svgNode = doc.child("svg");
    if (!svgNode)
        throw std::runtime_error("MetricsExporter: no <svg> root.");

    std::string groupId;
    switch (metric) {
        case Metric::Area:      groupId = "heatmap_area";      break;
        case Metric::Perimeter: groupId = "heatmap_perimeter"; break;
        case Metric::Degree:    groupId = "heatmap_degree";    break;
        case Metric::Choice:    groupId = "heatmap_choice";    break;
        case Metric::DChoice:   groupId = "heatmap_d_choice";  break;
        case Metric::AChoice:   groupId = "heatmap_a_choice";  break;
        case Metric::PolarStatusAngle:   groupId = "heatmap_polar_status_angle";   break;
        case Metric::PolarStatusProduct: groupId = "heatmap_polar_status_product"; break;
        case Metric::TopoStatus:         groupId = "heatmap_topo_status";          break;
        case Metric::ProspectStatus:     groupId = "heatmap_prospect_status";      break;
    }

    std::string metricName;
    switch (metric) {
        case Metric::Area:      metricName = "area";      break;
        case Metric::Perimeter: metricName = "perimeter"; break;
        case Metric::Degree:    metricName = "degree";    break;
        case Metric::Choice:    metricName = "choice";    break;
        case Metric::DChoice:   metricName = "d-choice";  break;
        case Metric::AChoice:   metricName = "a-choice";  break;
        case Metric::PolarStatusAngle:   metricName = "polar-status-angle";   break;
        case Metric::PolarStatusProduct: metricName = "polar-status-product"; break;
        case Metric::TopoStatus:         metricName = "topo-status";          break;
        case Metric::ProspectStatus:     metricName = "prospect-status";      break;
    }

    // Measure label just above the content bounding box
    {
        double minX = records[0].center.x, minY = records[0].center.y;
        for (const auto& r : records) {
            minX = std::min(minX, r.center.x);
            minY = std::min(minY, r.center.y);
        }
        std::string label = metricName;
        if (m_log)  label += " (log)";
        if (m_flip) label += " (flipped)";
        addMeasureLabelAt(svgNode, label, minX, minY);
    }

    pugi::xml_node group = svgNode.append_child("g");
    group.append_attribute("id") = groupId.c_str();

    auto fmt = [](double v) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(3) << v;
        return ss.str();
    };

    for (const auto& r : records) {
        std::string color = valueToColor(display(getValue(r)), minVal, maxVal, m_flip);

        pugi::xml_node circle = group.append_child("circle");
        circle.append_attribute("cx")   = fmt(r.center.x).c_str();
        circle.append_attribute("cy")   = fmt(r.center.y).c_str();
        circle.append_attribute("r")    = dotRadius;
        circle.append_attribute("fill") = color.c_str();
    }

    if (!doc.save_file(outputPath.c_str()))
        throw std::runtime_error("MetricsExporter: failed to write: " + outputPath);

    std::cout << "Exported " << metricName << " heatmap to: " << outputPath
              << "  (range " << fmt(minVal) << " – " << fmt(maxVal) << ")\n";
}
