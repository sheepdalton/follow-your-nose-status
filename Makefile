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

SIMPLIFY_SRCS = src/SimplifySVG.cpp vendor/pugixml.cpp
SIMPLIFY      = simplifySVG

all: $(TARGET) $(SIMPLIFY)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

$(SIMPLIFY): $(SIMPLIFY_SRCS)
	$(CXX) $(CXXFLAGS) -o $(SIMPLIFY) $(SIMPLIFY_SRCS)

clean:
	rm -f $(TARGET) $(SIMPLIFY)

.PHONY: all clean
