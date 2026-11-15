#include "Traversal.h"

#include <queue>
#include <stdexcept>
#include <unordered_set>

namespace metromitra {

std::vector<std::string> breadthFirstTraversal(const Graph& graph,
                                               const std::string& startId) {
    if (!graph.findStation(startId)) throw std::invalid_argument("Unknown start station: " + startId);
    std::vector<std::string> order;
    std::queue<std::string> pending;
    std::unordered_set<std::string> visited{startId};
    pending.push(startId);
    while (!pending.empty()) {
        auto current = pending.front();
        pending.pop();
        order.push_back(current);
        for (const auto& edge : graph.neighbors(current)) {
            if (visited.insert(edge.destinationId).second) pending.push(edge.destinationId);
        }
    }
    return order;
}

std::vector<std::string> depthFirstTraversal(const Graph& graph,
                                             const std::string& startId) {
    if (!graph.findStation(startId)) throw std::invalid_argument("Unknown start station: " + startId);
    std::vector<std::string> order;
    std::vector<std::string> pending{startId};
    std::unordered_set<std::string> visited;
    while (!pending.empty()) {
        auto current = pending.back();
        pending.pop_back();
        if (!visited.insert(current).second) continue;
        order.push_back(current);
        const auto& edges = graph.neighbors(current);
        for (auto it = edges.rbegin(); it != edges.rend(); ++it) {
            if (visited.count(it->destinationId) == 0) pending.push_back(it->destinationId);
        }
    }
    return order;
}

} // namespace metromitra
