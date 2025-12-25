// graph.h
#ifndef GRAPH_H
#define GRAPH_H

#include <limits>
#include <vector>

class Grid;

struct Edge {
    int to;
    int baseCost;
};

class Graph {
  public:
    Graph();
    explicit Graph(int numVertices);

    void resize(int numVertices);
    int numVertices() const {
        return static_cast<int>(adj_.size());
    }

    void addEdge(int u, int v, int baseCost);

    const std::vector<Edge>& adj(int u) const {
        return adj_[u];
    }

  private:
    std::vector<std::vector<Edge>> adj_;
};


void dijkstra(const Graph &g, const Grid &grid, const int source, const int target, const std::vector <int> &history, std::vector <int> *outPrev);

const int INF = std::numeric_limits<int>::max() / 4;

#endif  // GRAPH_H