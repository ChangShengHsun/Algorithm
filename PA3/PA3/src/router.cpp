// router.cpp
#include "router.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iostream>

namespace {

constexpr int OVERFLOW_WEIGHT = 100000;

int totalVertices(const Grid &grid) {
    return grid.numLayers() * grid.xSize() * grid.ySize();
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
    Coord3D cur = net.pin1; 
    Coord3D target = net.pin2;
    path.push_back(cur);
    while (cur.col != target.col) {
        if (grid.layerInfo(cur.layer).direction == 'V') {
            cur.layer = !cur.layer;
            path.push_back(cur);
        }
        cur.col < target.col ? cur.col++ : cur.col--;
        path.push_back(cur);
    }
    while (cur.row != target.row) {
        if (grid.layerInfo(cur.layer).direction == 'H') {
            cur.layer = !cur.layer;
            path.push_back(cur);
        }
        cur.row < target.row ? cur.row++ : cur.row--;
        path.push_back(cur);
    }
    if (cur.layer != target.layer) {
        cur.layer = target.layer;
        path.push_back(cur);
    }
    return path;
}

void updateDemandAlongPath(Grid &grid, int netId, const std::vector<Coord3D> &coords) {
    for (const Coord3D &c : coords) {
        grid.addDemandForNetGCell(netId, c.layer, c.col, c.row);
    }
}
void removeDemandAlongPath(Grid &grid, int netId, const std::vector<Coord3D> &coords){
    for (const Coord3D &c : coords)
        grid.removeDemandForNetGCell(netId, c.layer, c.col, c.row);
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

std::vector<long long> computeVertexCost(const Grid &grid) {
    const int total = totalVertices(grid);
    std::vector<long long> costs(total, 0);
    for(int i = 0 ; i < total ; i++)
    {
        long long alpha = 1000;
        int demand = grid.demandByIndex(i);
        int capacity = grid.capacityByIndex(i);
        int overflow = std::min(20, std::max(0, demand - capacity));
        costs[i] = alpha * ((1 << overflow) - 1);
    }
    return costs;
}

RoutingResult runRouting(Grid &grid,const std::vector<Net> &nets) {
    auto startTime = std::chrono::steady_clock::now();

    RoutingResult result;
    RoutingResult result_back;
    result.nets.reserve(nets.size());
    result_back.nets.reserve(nets.size());
    std::vector<std::vector<Coord3D>> pathOfNet(nets.size());

    grid.resetDemand();
    Graph graph = buildGraphFromGrid(grid);

    const int totalV = totalVertices(grid);
    long long maxIterations = INF;
    std::vector<long long> history(totalV, 0);
    int historyInc = 1;     // 每次 overfull +1
    const long long beta = 500;        // history 懲罰尺度：你W/H在 5700/6000，beta建議先試 1000~6000
    int stagcnt = 0;
    long long lastOverflow = INF;

    for (int iter = 0; iter < maxIterations; ++iter) {
        if (iter == 0) {
            // baseline routing
            for (size_t netIdx = 0; netIdx < nets.size(); ++netIdx) {
                const Net &net = nets[netIdx];
                RoutedNet routed;
                routed.name = net.name;

                int src = grid.gcellIndex(net.pin1.layer, net.pin1.col, net.pin1.row);
                int dst = grid.gcellIndex(net.pin2.layer, net.pin2.col, net.pin2.row);

                std::vector<int> predecessors;
                std::vector<long long> costs = computeVertexCost(grid);

                std::vector<Coord3D> path;
                std::vector<long long> dist = astar(graph, grid, src, dst, costs, &predecessors);
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
                
                pathOfNet[netIdx] = path;
                updateDemandAlongPath(grid, static_cast<int>(netIdx), path);
            }

            long long totalOverflow = 0;
            for (int i = 0; i < totalV; ++i) {
                int of = grid.demandByIndex(i) - grid.capacityByIndex(i);
                if (of > 0) totalOverflow += of;
            }
            std::cerr << "[RRR] iter=" << iter << " totalOverflow=" << totalOverflow << "\n";
            result_back = result;
            lastOverflow = totalOverflow;
            if (totalOverflow == 0) break;
        }
        else {
            // 1) 計算 overflow，標記 overfull cells
            long long totalOverflow = 0;
            std::vector<int> isOverfull(totalV, 0);

            for (int i = 0; i < totalV; ++i) {
                int of = grid.demandByIndex(i) - grid.capacityByIndex(i);
                isOverfull[i] = of > 0 ? 1 : 0;
                totalOverflow += of > 0 ? of : 0;
            }

            // debug / early stop
            std::cerr << "[RRR] iter=" << iter << " totalOverflow=" << totalOverflow << "\n";
            if (totalOverflow == 0) break;

            // 2) 更新 history：只對 overfull 的 gcell 加重
            for (int i = 0; i < totalV; ++i) {
                if (isOverfull[i]) history[i] += historyInc;
            }

            // 3) 選出要 reroute 的 nets：只 reroute 路徑碰到 overfull cell 的 net
            std::vector<int> netsToReroute;
            netsToReroute.reserve(nets.size());

            for (int netId = 0; netId < (int)nets.size(); ++netId) {
                bool hit = false;
                for (const auto &c : pathOfNet[netId]) {
                    int idx = grid.gcellIndex(c.layer, c.col, c.row);
                    if (idx >= 0 && idx < totalV && isOverfull[idx]) {
                        hit = true;
                        break;
                    }
                }
                if (hit) netsToReroute.push_back(netId);
            }
            if(stagcnt >=200)
            {
                std::cerr << "[RRR] stagnation detected, rerouting all nets\n";
                netsToReroute.clear();
                for (int netId = 0; netId < (int)nets.size(); ++netId) {
                    netsToReroute.push_back(netId);
                }
                stagcnt = 0;
                lastOverflow = INF;
            }

            // 4) reroute 之前先算一次 costs（以目前 demand + history）
            //    注意：不要在 reroute 每條 net 都重算整張 costs，成本很高、也不一定更好
            // 5) 只 reroute 被選到的 nets（rip-up 後才重算 cost，讓 demand 變化被看到）
            for (int netId : netsToReroute) {
                const Net &net = nets[netId];
                // (a) rip-up：先把舊路徑從 demand 拿掉
                removeDemandAlongPath(grid, netId, pathOfNet[netId]);
                std::vector<long long> costs = computeVertexCost(grid);
                for (size_t i = 0; i < costs.size(); ++i) {
                    // history penalty：讓曾經塞爆的格子在未來更不想走
                    // 用 min 避免爆掉（可調）
                    costs[i] += beta * history[i];
                }

                int src = grid.gcellIndex(net.pin1.layer, net.pin1.col, net.pin1.row);
                int dst = grid.gcellIndex(net.pin2.layer, net.pin2.col, net.pin2.row);

                std::vector<int> predecessors;
                std::vector<long long> dist = astar(graph, grid, src, dst, costs, &predecessors);

                std::vector<Coord3D> newPath;
                if (dst >= 0 && dst < (int)dist.size() && dist[dst] < INF) {
                    newPath = reconstructPath(grid, src, dst, predecessors);
                }
                if (newPath.empty()) {
                    std::cerr << "Warning: using fallback routing for " << net.name << "\n";
                    newPath = buildFallbackPath(grid, net);
                }

                // (b) commit：更新 result & pathOfNet & demand
                RoutedNet routed;
                routed.name = net.name;
                routed.segments.clear();
                for (size_t i = 1; i < newPath.size(); ++i) {
                    routed.segments.push_back(Segment{newPath[i - 1], newPath[i]});
                }
                result.nets[netId] = routed;

                pathOfNet[netId] = newPath;
                updateDemandAlongPath(grid, netId, newPath);
            }
            if (totalOverflow >= lastOverflow) {
                stagcnt++;
            } else {
                stagcnt = 0;
            }
            if(totalOverflow <= lastOverflow)
            {
                result_back = result;
            }
            lastOverflow = std::min(totalOverflow, lastOverflow);
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime
        ).count();
        if (elapsed >= 300) {
            std::cerr << "[RRR] time limit reached (" << elapsed << "s) after iter=" << iter << "\n";
            break;
        }
    }
    return result_back;
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
