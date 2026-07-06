#pragma once
#include "IsovistRecord.h"
#include <vector>
#include <string>

class MetricsExporter {
public:
    void setFlip(bool flip) { m_flip = flip; }
    void setLog (bool log)  { m_log  = log;  }
    // Writes id,x,y,area,perimeter,degree CSV to outputPath.
    void exportCSV(const std::vector<IsovistRecord>& records,
                   const std::string& outputPath) const;

    // Exports the original SVG with isovist centers coloured by area.
    void exportAreaHeatmap(const std::string& inputSVGPath,
                           const std::string& outputPath,
                           const std::vector<IsovistRecord>& records,
                           double dotRadius = 5.0) const;

    // Exports the original SVG with isovist centers coloured by perimeter.
    void exportPerimeterHeatmap(const std::string& inputSVGPath,
                                const std::string& outputPath,
                                const std::vector<IsovistRecord>& records,
                                double dotRadius = 5.0) const;

    // Exports the original SVG with isovist centers coloured by visibility-graph degree.
    void exportDegreeHeatmap(const std::string& inputSVGPath,
                             const std::string& outputPath,
                             const std::vector<IsovistRecord>& records,
                             double dotRadius = 5.0) const;

    // Exports the original SVG with isovist centers coloured by choice (betweenness).
    void exportChoiceHeatmap(const std::string& inputSVGPath,
                             const std::string& outputPath,
                             const std::vector<IsovistRecord>& records,
                             double dotRadius = 5.0) const;

    // Exports the original SVG with isovist centers coloured by distributed choice.
    void exportDChoiceHeatmap(const std::string& inputSVGPath,
                              const std::string& outputPath,
                              const std::vector<IsovistRecord>& records,
                              double dotRadius = 5.0) const;

    // Exports the original SVG with isovist centers coloured by angular choice.
    void exportAChoiceHeatmap(const std::string& inputSVGPath,
                              const std::string& outputPath,
                              const std::vector<IsovistRecord>& records,
                              double dotRadius = 5.0) const;

private:
    enum class Metric { Area, Perimeter, Degree, Choice, DChoice, AChoice };
    bool m_flip = false;
    bool m_log  = false;

    void exportHeatmap(const std::string& inputSVGPath,
                       const std::string& outputPath,
                       const std::vector<IsovistRecord>& records,
                       Metric metric,
                       double dotRadius) const;
};
