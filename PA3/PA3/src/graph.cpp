// graph.cpp
#include "graph.h"
#include "grid.h"
#include <algorithm>
#include <functional>
#include <queue>
#include <stdexcept>
#include <utility>

namespace {

long long manhattanDistance(const Grid &grid, int fromIdx, int toIdx) {
    Coord3D from = grid.fromIndex(fromIdx);
    Coord3D to = grid.fromIndex(toIdx);

    long long cost = 0;
    int minCol = std::min(from.col, to.col);
    int maxCol = std::max(from.col, to.col);
    for (int c = minCol; c < maxCol; ++c) {
        cost += grid.horizontalDist(c);
    }

    int minRow = std::min(from.row, to.row);
    int maxRow = std::max(from.row, to.row);
    for (int r = minRow; r < maxRow; ++r) {
        cost += grid.verticalDist(r);
    }

    if (from.layer != to.layer) {
        cost += grid.wlViaCost();
    }

    return cost;
}

} // namespace

Graph::Graph() = default;

Graph::Graph(int numVertices) {
    resize(numVertices);
}

void Graph::resize(int numVertices) {
    adj_.assign(numVertices, {});
}

void Graph::addEdge(int u, int v, long long baseCost) {
    // TODO: add edge (u, v) between GCell u and v with baseCosts to the graph
    adj_[u].push_back({v, baseCost});
}

std::vector<long long> dijkstra(
    const Graph &g,
    int source,
    std::vector<long long>&vertex_cost,
    std::vector<int> *outPrev
) {
    const int n = g.numVertices();
    std::vector<long long> dist(n, INF);
    std::vector<int> prev(n, -1);

    using Node = std::pair<long long, int>; // (dist, vertex)
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;

    dist[source] = 0;
    pq.push({0, source});

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        if (d != dist[u]) continue;

        for (const Edge &e : g.adj(u)) {
            int v = e.to;
            long long nd = d + e.baseCost + vertex_cost[v]; // Updated cost calculation
            if (nd < dist[v]) {
                dist[v] = nd;
                prev[v] = u;
                pq.push({nd, v});
            }
        }
    }

    if (outPrev) *outPrev = std::move(prev);
    return dist;
}

std::vector<long long> astar(
    const Graph &g,
    const Grid &grid,
    int source,
    int target,
    std::vector<long long> &vertex_cost,
    std::vector<int> *outPrev
) {
    const int n = g.numVertices();
    std::vector<long long> gScore(n, INF);
    std::vector<int> prev(n, -1);
    std::vector<long long> bestF(n, INF);

    using Node = std::pair<long long, int>; // (fScore, vertex)
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;

    auto heuristic = [&](int idx) {
        return manhattanDistance(grid, idx, target);
    };

    gScore[source] = 0;
    bestF[source] = heuristic(source);
    pq.push({bestF[source], source});

    while (!pq.empty()) {
        auto [fScore, u] = pq.top();
        pq.pop();
        if (fScore != bestF[u]) continue;
        if (u == target) break;
        if (gScore[u] == INF) continue;

        for (const Edge &e : g.adj(u)) {
            int v = e.to;
            long long tentativeG = gScore[u] + e.baseCost + vertex_cost[v];
            if (tentativeG < gScore[v]) {
                gScore[v] = tentativeG;
                prev[v] = u;
                long long newF = tentativeG + heuristic(v);
                bestF[v] = newF;
                pq.push({newF, v});
            }
        }
    }

    if (outPrev) *outPrev = std::move(prev);
    return gScore;
}
