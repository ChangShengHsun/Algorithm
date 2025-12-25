// router.cpp
#include "router.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <numeric>
#include <vector>
#include <random>
#include <cmath>
#include <chrono>
#include <climits>

namespace {
    constexpr unsigned HISTORY_PUNISHMENT = 1000, TIME_LIMIT = 580, ITER_LIMIT = 100;
    int totalVertices(const Grid& grid) {return grid.numLayers() * grid.xSize() * grid.ySize();}
    char normalizeDir(char d) {return static_cast<char>(std::toupper(static_cast<unsigned char>(d)));}
    std::vector<Coord3D> reconstructPath(const Grid& grid, int sourceIdx, int targetIdx, const std::vector<int>& prev) {
        std::vector <Coord3D> path;
        int cur = targetIdx;
        while (cur != sourceIdx && cur != -1) {
            path.push_back(grid.fromIndex(cur));
            cur = prev[cur];
        }
        if (cur == sourceIdx) {
            path.push_back(grid.fromIndex(cur));
            std::reverse(path.begin(), path.end());
            return path;
        }
        else
            return {};
    }

    std::vector<Coord3D> buildFallbackPath(const Grid& grid, const Net& net) {
        Coord3D cur = net.pin1, tar = net.pin2;
        std::vector <Coord3D> path{cur};
        while (cur.col != tar.col) {
            if (grid.layerInfo(cur.layer).direction == 'V') {
                cur.layer = !cur.layer;
                path.push_back(cur);
            }
            cur.col < tar.col ? cur.col++ : cur.col--;
            path.push_back(cur);
        }
        while (cur.row != tar.row) {
            if (grid.layerInfo(cur.layer).direction == 'H') {
                cur.layer = !cur.layer;
                path.push_back(cur);
            }
            cur.row < tar.row ? cur.row++ : cur.row--;
            path.push_back(cur);
        }
        if (cur.layer != tar.layer) {
            cur.layer = tar.layer;
            path.push_back(cur);
        }
        return path;
    }
    void updateDemandAlongPath(Grid& grid, int netId, const std::vector<Coord3D>& coords) {
        for(const Coord3D &c:coords)
            grid.addDemandForNetGCell(netId, c.layer, c.col, c.row);
    }
    unsigned long long calculateTotalCost(const Grid &grid, const RoutingResult &res) {
        unsigned long long total_len = 0;
        int via_cost = grid.wlViaCost();
        for (const auto& net : res.nets)
            for (const auto& seg : net.segments)
                if (seg.from.layer != seg.to.layer)
                    total_len += via_cost;
                else {
                    if (seg.from.col != seg.to.col)
                        for(int k = std::min(seg.from.col, seg.to.col); k < std::max(seg.from.col, seg.to.col); k++)
                            total_len += grid.horizontalDist(k);
                    else if (seg.from.row != seg.to.row)
                        for(int k=std::min(seg.from.row, seg.to.row); k < std::max(seg.from.row, seg.to.row); k++)
                            total_len += grid.verticalDist(k);
                }
        return total_len;
    }
}

using namespace std;
using namespace std::chrono;
Graph buildGraphFromGrid(const Grid& grid) {
    Graph g(totalVertices(grid));
    LayerInfo layer0 = grid.layerInfo(0), layer1 = grid.layerInfo(1);
    if (layer0.direction == 'H')
        for (int x = 0; x < grid.xSize() - 1; x++)
            for (int y = 0; y < grid.ySize(); y++)
                g.addEdge(grid.gcellIndex(0, x, y), grid.gcellIndex(0, x + 1, y), grid.horizontalDist(x));
    if (layer0.direction == 'V')
        for (int y = 0; y < grid.ySize() - 1; y++)
            for (int x = 0; x < grid.xSize(); x++)
                g.addEdge(grid.gcellIndex(0, x, y), grid.gcellIndex(0, x, y + 1), grid.verticalDist(y));
    if (layer1.direction == 'H')
        for (int x = 0; x < grid.xSize() - 1; x++)
            for (int y = 0; y < grid.ySize(); y++)
                g.addEdge(grid.gcellIndex(1, x, y), grid.gcellIndex(1, x + 1, y), grid.horizontalDist(x));
    if (layer1.direction == 'V')
        for (int y = 0; y < grid.ySize() - 1; y++)
            for (int x = 0; x < grid.xSize(); x++)
                g.addEdge(grid.gcellIndex(1, x, y), grid.gcellIndex(1, x, y + 1), grid.verticalDist(y));
    for (int x = 0; x < grid.xSize(); x++)
        for (int y = 0; y < grid.ySize(); y++)
            g.addEdge(grid.gcellIndex(0, x, y), grid.gcellIndex(1, x, y), grid.wlViaCost());
    return g;
}

std::vector<int> computeVertexCost(const Grid& grid) {return {};}

RoutingResult runRouting(Grid &grid, const vector <Net> &nets) {
    RoutingResult ret, cur;
    cur.nets.resize(nets.size());
    grid.resetDemand();
    Graph graph = buildGraphFromGrid(grid);
    vector <int> idx(nets.size());
    iota(idx.begin(), idx.end(), 0);
    auto cmp = [&](const int a, const int b) {
        const Net &n1 = nets[a], &n2 = nets[b];
        return abs(n1.pin1.col - n1.pin2.col) + abs(n1.pin1.row - n1.pin2.row) > abs(n2.pin1.col - n2.pin2.col) + abs(n2.pin1.row - n2.pin2.row);
    };
    sort(idx.begin(), idx.end(), cmp);
    vector < vector<Coord3D> > netPaths(nets.size());
    int cells = totalVertices(grid), min_overflow = INF;
    unsigned long long min_cost = LLONG_MAX >> 4;
    vector <int> predecessors(cells), history(cells, 0);
    static random_device rd;
    static mt19937 mt(rd());
    auto start_time = steady_clock::now();
    for (unsigned it = 1, cnt = 0; cnt < ITER_LIMIT && duration_cast<seconds>(steady_clock::now() - start_time).count() / double(it) * (it + 1) <= TIME_LIMIT; it++) {
        if (it > 1)
            if (min_overflow > 0) {
                shuffle(idx.begin(), idx.end(), mt);
                vector <int> net_overflow_scores(nets.size(), 0);
                for (int i = 0; i < nets.size(); i++)
                    if (!netPaths[i].empty())
                        for (const auto& p : netPaths[i])
                            if (grid.demand(p.layer, p.col, p.row) > grid.capacity(p.layer, p.col, p.row))
                                net_overflow_scores[i]++;
                stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
                    if (net_overflow_scores[a] == net_overflow_scores[b])
                        return cmp(a, b);
                    return net_overflow_scores[a] > net_overflow_scores[b];
                });
            }
            else
                sort(idx.begin(), idx.end(), cmp);
        for(int i = 0; i < nets.size(); i++) {
            int netIdx = idx[i];
            const Net& net = nets[netIdx];
            if (!netPaths[netIdx].empty()) {
                for (const auto &c:netPaths[netIdx])
                    grid.removeDemandForNetGCell(netIdx, c.layer, c.col, c.row);
                netPaths[netIdx].clear();
            }
            int src = grid.gcellIndex(net.pin1.layer, net.pin1.col, net.pin1.row), dst = grid.gcellIndex(net.pin2.layer, net.pin2.col, net.pin2.row);
            dijkstra(graph, grid, src, dst, history, &predecessors);
            vector <Coord3D> path = reconstructPath(grid, src, dst, predecessors);
            if (path.empty())
                path = buildFallbackPath(grid, net);
            updateDemandAlongPath(grid, netIdx, path);
            netPaths[netIdx] = path;
            RoutedNet routed;
            routed.name = net.name;
            for (int j = 1; j < path.size(); j++)
                routed.segments.push_back(Segment{path[j - 1], path[j]});
            cur.nets[netIdx] = routed;
        }
        int cur_overflow = 0;
        unsigned long long cur_cost = 0;
        for (int i = 0, x = HISTORY_PUNISHMENT * it * (min_overflow != 0); i < cells; i++)
            if (grid.demandByIndex(i) > grid.capacityByIndex(i)) {
                history[i] += x;
                cur_overflow += grid.demandByIndex(i) - grid.capacityByIndex(i);
            }
        bool flag = cur_overflow < min_overflow;
        if (cur_overflow == min_overflow || flag) {
            cur_cost = calculateTotalCost(grid, cur);
            if (cur_cost < min_cost)
                flag = true;
        }
        if (flag) {
            min_overflow = cur_overflow;
            min_cost = cur_cost;
            ret = cur;
            cnt = 0;
            if (!min_overflow)
                for (int &h:history)
                    h >>= 1;
        }
        else {
            cnt++;
            if (!min_overflow)
                for (int &h:history)
                    h *= 0.9;
        }
    }
    return ret;
}

bool writeRouteFile(const std::string& filename, const RoutingResult& result) {
    std::ofstream fout(filename);
    if(!fout)
        return false;
    for(const RoutedNet& net: result.nets) {
        fout << net.name << "\n";
        fout << "(\n";
        for(const Segment& seg: net.segments) {
            fout << seg.from.layer << " " << seg.from.col << " " << seg.from.row << " " << seg.to.layer << " " << seg.to.col << " " << seg.to.row
                 << "\n";
        }
        fout << ")\n";
    }

    return true;
}
