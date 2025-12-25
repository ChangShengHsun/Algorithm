#include "graph.h"
#include "grid.h"
#include <queue>
#include <stdexcept>
#include <cmath>
#include <algorithm>

constexpr int OVERFLOW_WEIGHT = 1e5;
Graph::Graph() = default;
Graph::Graph(int numVertices) {resize(numVertices);}

void Graph::resize(int numVertices) {adj_.assign(numVertices, {});}

void Graph::addEdge(int u, int v, int baseCost) {
    Edge e1, e2;
    e1.to = v;
    e1.baseCost = baseCost;
    adj_[u].push_back(e1);
    e2.to = u;
    e2.baseCost = baseCost;
    adj_[v].push_back(e2);
}

using namespace std;
inline int getHeuristic(const Grid &grid, const int u, const int target) {
    Coord3D c1 = grid.fromIndex(u), c2 = grid.fromIndex(target);
    return abs(c1.col - c2.col) + abs(c1.row - c2.row);
}

inline int getNodeCost(const Grid &grid, const int v, const vector <int> &history) {
    int demand = grid.demandByIndex(v), capacity = grid.capacityByIndex(v), base = 1 + history[v];
    if (demand >= capacity)
        return base + OVERFLOW_WEIGHT + (1 << min(demand - capacity, 20)) * 50;
    return base;
}

void dijkstra(const Graph &g, const Grid &grid, const int source, const int target, const vector <int> &history, vector <int> *outPrev = nullptr) { // Heuristic A*
    const int n = g.numVertices();
    static vector <int> dist;
    dist.size() == n ? fill(dist.begin(), dist.end(), INF) : dist.assign(n, INF);
    if (outPrev)
        fill(outPrev->begin(), outPrev->end(), -1);
    priority_queue <pair <int, int>, vector< pair<int, int> >, greater< pair<int, int> > > pq;
    dist[source] = 0;
    pq.emplace(getHeuristic(grid, source, target), source);
    while (!pq.empty()) {
        auto cur = pq.top();
        pq.pop();
        if (cur.second == target)
            break;
        if (dist[cur.second] != INF && cur.first > dist[cur.second] + getHeuristic(grid, cur.second, target))
            continue;
        for (const auto &e:g.adj(cur.second))
            if (getNodeCost(grid, e.to, history) + e.baseCost + dist[cur.second] < dist[e.to]) {
                dist[e.to] = getNodeCost(grid, e.to, history) + e.baseCost + dist[cur.second];
                if (outPrev)
                    (*outPrev)[e.to] = cur.second;
                pq.emplace(dist[e.to] + getHeuristic(grid, e.to, target), e.to);
            }
    }
}
