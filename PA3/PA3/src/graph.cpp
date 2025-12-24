// graph.cpp
#include "graph.h"
#include <functional>
#include <queue>
#include <stdexcept>
#include <utility>

Graph::Graph() = default;

Graph::Graph(int numVertices) {
    resize(numVertices);
}

void Graph::resize(int numVertices) {
    adj_.assign(numVertices, {});
}

void Graph::addEdge(int u, int v, int baseCost) {
    // TODO: add edge (u, v) between GCell u and v with baseCosts to the graph
    adj_[u].push_back({v, baseCost});
}

std::vector<int> dijkstra(
    const Graph &g,
    int source,
    std::vector<int> *outPrev
) {
    const int n = g.numVertices();
    std::vector<int> dist(n, INF);
    std::vector<int> prev(n, -1);

    using Node = std::pair<int, int>; // (dist, vertex)
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;

    dist[source] = 0;
    pq.push({0, source});

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        if (d != dist[u]) continue;

        for (const Edge &e : g.adj(u)) {
            int v = e.to;
            int nd = d + e.baseCost;
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
