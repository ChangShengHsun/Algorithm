// graph.h
#ifndef GRAPH_H
#define GRAPH_H

#include <vector>
#include <limits>
#include <climits>

class Grid;

// Simple directed edge for adjacency list
struct Edge {
    int to;          // destination vertex id
    long long baseCost;    // physical length (W_j, H_i, or via)
    // students can ignore or extend this structure as needed
};

class Graph {
public:
    Graph();
    explicit Graph(int numVertices);

    void resize(int numVertices);
    int numVertices() const { return static_cast<int>(adj_.size()); }

    void addEdge(int u, int v, long long baseCost);

    const std::vector<Edge>& adj(int u) const { return adj_[u]; }

private:
    std::vector<std::vector<Edge>> adj_;
};

/// A minimal Dijkstra interface that students can call or modify.
/// They can also write their own version if they prefer.
std::vector<long long> dijkstra(
    const Graph &g,
    int source,
    std::vector<long long>&vertex_cost,
    std::vector<int> *outPrev = nullptr   // optional predecessor tree
);

std::vector<long long> astar(
    const Graph &g,
    const Grid &grid,
    int source,
    int target,
    std::vector<long long>&vertex_cost,
    std::vector<int> *outPrev = nullptr
);

const long long INF = LLONG_MAX >> 4;

#endif // GRAPH_H
