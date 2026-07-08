#pragma once
#include "Point.h"

struct IsovistRecord {
    int    id;
    Point  center;
    double area;
    double perimeter;
    int    degree  = 0;     // populated after graph build
    double choice  = 0.0;  // betweenness centrality; populated after choice computation
    double dChoice = 0.0;  // distributed choice; populated after d-choice computation
    double aChoice = 0.0;  // angular choice; populated after a-choice computation
    double polarStatusAngle   = 0.0; // sum of angle-only costs of polar routes here
    double polarStatusProduct = 0.0; // sum of angle*distance costs of polar routes here
    double topoStatus         = 0.0; // total depth: sum of BFS hops to all other nodes
};
