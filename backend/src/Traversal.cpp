#include "Traversal.h"

#include <queue>
#include <stdexcept>
#include <unordered_set>

using namespace std;

namespace metromitra {

vector<string> breadthFirstTraversal(const Graph& graph,
                                               const string& startId) {
    if (!graph.findStation(startId)) throw invalid_argument("Unknown start station: " + startId);
    vector<string> order;
    queue<string> pending;
    unordered_set<string> visited{startId};
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

vector<string> depthFirstTraversal(const Graph& graph,
                                             const string& startId) {
    if (!graph.findStation(startId)) throw invalid_argument("Unknown start station: " + startId);
    vector<string> order;
    vector<string> pending{startId};
    unordered_set<string> visited;
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
