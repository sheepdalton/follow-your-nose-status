CXX     ?= c++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I src -I vendor

SRCS = src/main.cpp \
       src/Polygon.cpp \
       src/FloorPlan.cpp \
       src/SVGParser.cpp \
       src/IsovistPlacer.cpp \
       src/IsovistComputer.cpp \
       src/VisibilityGraph.cpp \
       src/SVGExporter.cpp \
       src/MetricsExporter.cpp \
       src/Gates.cpp \
       src/Network.cpp \
       vendor/pugixml.cpp

TARGET = isovist

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: clean
