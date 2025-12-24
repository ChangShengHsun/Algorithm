// router.cpp
#include "router.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>

namespace {

constexpr int OVERFLOW_WEIGHT = 100000;

int totalVertices(const Grid &grid) {
    return grid.numLayers() * grid.xSize() * grid.ySize();
}

char normalizeDir(char d) {
    return static_cast<char>(std::toupper(static_cast<unsigned char>(d)));
}

std::vector<Coord3D> reconstructPath(
    const Grid &grid,
    int sourceIdx,
    int targetIdx,
    const std::vector<int> &prev
) {
    if (sourceIdx < 0 || targetIdx < 0) return {};
    int cur = targetIdx;
    std::vector<Coord3D> path;
    while(cur != -1 && cur != sourceIdx){
        if(cur < 0 || cur >= static_cast<int>(prev.size())) return {};
        path.push_back(grid.fromIndex(cur));
        cur = prev[cur];
    }
     if(cur == -1)return {};
    path.push_back(grid.fromIndex(sourceIdx));
    std::reverse(path.begin(), path.end());
    return path;
}

std::vector<Coord3D> buildFallbackPath(const Grid &grid, const Net &net) {
    std::vector<Coord3D> path;
    path.push_back(grid.fromIndex(grid.gcellIndex(net.pin1.layer, net.pin1.col, net.pin1.row)));
    int horizontaldis = net.pin2.col - net.pin1.col;
    int verticaldis = net.pin2.row - net.pin1.row;
    if(grid.layerInfo(net.pin1.layer).direction == 'H'){
        int step = horizontaldis > 0 ? 1 : -1;
        for(int col = net.pin1.col; col != net.pin2.col; col += step){
            path.push_back(grid.fromIndex(grid.gcellIndex(net.pin1.layer, col + step, net.pin1.row)));
        }
        path.push_back(grid.fromIndex(grid.gcellIndex(net.pin2.layer, net.pin2.col, net.pin1.row)));
        step = verticaldis > 0 ? 1 : -1;
        for(int row = net.pin1.row; row != net.pin2.row; row += step){
            path.push_back(grid.fromIndex(grid.gcellIndex(net.pin2.layer, net.pin2.col, row + step)));
        }
    }
    else{
        int step = verticaldis > 0 ? 1 : -1;
        for(int row = net.pin1.row; row != net.pin2.row; row += step){
            path.push_back(grid.fromIndex(grid.gcellIndex(net.pin1.layer, net.pin1.col, row + step)));
        }
        path.push_back(grid.fromIndex(grid.gcellIndex(net.pin2.layer, net.pin1.col, net.pin2.row)));
        step = horizontaldis > 0 ? 1 : -1;
        for(int col = net.pin1.col; col != net.pin2.col; col += step){
            path.push_back(grid.fromIndex(grid.gcellIndex(net.pin2.layer, col + step, net.pin2.row)));
        }
    }
    return path;
}

void updateDemandAlongPath(
    Grid &grid,
    int netId,
    const std::vector<Coord3D> &coords
) {
    for (const Coord3D &c : coords) {
        grid.addDemandForNetGCell(netId, c.layer, c.col, c.row);
    }
}

} // namespace

Graph buildGraphFromGrid(const Grid &grid) {
    Graph g(totalVertices(grid));

    // iterate over every gcell and add edges to its right/down neighbors
    const int L = grid.numLayers();
    const int X = grid.xSize(); // number of columns
    const int Y = grid.ySize(); // number of rows

    for (int layer = 0; layer < L; ++layer) {
        char dir = grid.layerInfo(layer).direction;
        if (dir == 'H') {
            for (int row = 0; row < Y; ++row) {
                for (int col = 0; col < X - 1; ++col) {
                    int u = grid.gcellIndex(layer, col, row);
                    int v = grid.gcellIndex(layer, col + 1, row);
                    int cost = grid.horizontalDist(col); // W_j corresponds to distance between col and col+1
                    g.addEdge(u, v, cost);
                    g.addEdge(v, u, cost);
                }
            }
        }
        else {
            for (int row = 0; row < Y - 1; ++row) {
                for (int col = 0; col < X; ++col) {
                    int u = grid.gcellIndex(layer, col, row);
                    int v = grid.gcellIndex(layer, col, row+1);
                    int cost = grid.verticalDist(row); 
                    g.addEdge(u, v, cost);
                    g.addEdge(v, u, cost);
                }
            }
        }
        if (layer + 1 < L) {
            for (int row = 0; row < Y; ++row) {
                for (int col = 0; col < X; ++col) {
                    int u = grid.gcellIndex(layer, col, row);
                    int v = grid.gcellIndex(layer+1, col, row);
                    int cost = grid.wlViaCost();
                    g.addEdge(u, v, cost);
                    g.addEdge(v, u, cost);
                }
            }
        }
    }
    return g;
}

std::vector<int> computeVertexCost(const Grid &grid) {
    const int total = totalVertices(grid);
    std::vector<int> costs(total, 0);
    return costs;
}

RoutingResult runRouting(
    Grid &grid,
    const std::vector<Net> &nets
) {
    RoutingResult result;
    result.nets.reserve(nets.size());

    grid.resetDemand();
    Graph graph = buildGraphFromGrid(grid);

    for (size_t netIdx = 0; netIdx < nets.size(); ++netIdx) {
        const Net &net = nets[netIdx];
        RoutedNet routed;
        routed.name = net.name;

        int src = grid.gcellIndex(net.pin1.layer, net.pin1.col, net.pin1.row);
        int dst = grid.gcellIndex(net.pin2.layer, net.pin2.col, net.pin2.row);

        std::vector<int> predecessors;
        std::vector<int> costs = computeVertexCost(grid);

        std::vector<Coord3D> path;
        std::vector<int> dist = dijkstra(graph, src, &predecessors);
        if (dst >= 0 && dst < static_cast<int>(dist.size()) && dist[dst] < INF) {
            path = reconstructPath(grid, src, dst, predecessors);
        }
        if (path.empty()) {
            std::cerr << "Warning: using fallback routing for " << net.name << "\n";
            path = buildFallbackPath(grid, net);
        }

        for (size_t i = 1; i < path.size(); ++i) {
            routed.segments.push_back(Segment{path[i - 1], path[i]});
        }
        result.nets.push_back(routed);

        updateDemandAlongPath(grid, static_cast<int>(netIdx), path);
    }

    return result;
}

bool writeRouteFile(const std::string &filename, const RoutingResult &result) {
    std::ofstream fout(filename);
    if (!fout) return false;

    for (const RoutedNet &net : result.nets) {
        fout << net.name << "\n";
        fout << "(\n";
        for (const Segment &seg : net.segments) {
            fout << seg.from.layer << " " << seg.from.col << " " << seg.from.row << " "
                 << seg.to.layer << " " << seg.to.col << " " << seg.to.row << "\n";
        }
        fout << ")\n";
    }

    return true;
}
