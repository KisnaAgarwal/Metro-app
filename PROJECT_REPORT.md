# MetroMitra: Delhi Metro Route Planning System

## 2. Table of Contents

1. [Title Page](#metromitra-delhi-metro-route-planning-system)
2. [Table of Contents](#2-table-of-contents)
3. [Summary](#3-summary)
4. [Introduction](#4-introduction)
5. [System Requirements](#5-system-requirements)
6. [Design and Implementation](#6-design-and-implementation)
   - [6.1 Architecture & Class Diagram](#61-architecture--class-diagram)
   - [6.2 Data Structures Used](#62-data-structures-used)
   - [6.3 Algorithms & Complexities](#63-algorithms--complexities)
   - [6.4 Key Code Snippets](#64-key-code-snippets)
   - [6.5 System Outputs](#65-system-outputs)
7. [Conclusion](#7-conclusion)
8. [References](#8-references)

---

## 3. Summary

### Brief Overview of the Project
MetroMitra is a route planning application for the Delhi Metro. It models 262 stations and their track connections as a weighted, undirected graph in C++. Given any two stations, it computes the optimal route using Dijkstra's algorithm backed by a binary min-heap priority queue. Routes can be calculated in a command-line interface or viewed on an interactive web map.

### Objectives
- Model the real-world Delhi Metro network as an Adjacency List graph.
- Implement Dijkstra’s shortest-path algorithm using a min-heap priority queue.
- Optimize routes by travel time, physical distance, or number of line transfers.
- Display the result with exact station sequences, transfer instructions, and map polylines.

---

## 4. Introduction

### Background and Context
The Delhi Metro network spans over 390 km with multiple intersecting lines (Red, Yellow, Blue, Violet, etc.). Commuters frequently need to choose paths through major transfer hubs (such as Rajiv Chowk and Kashmere Gate) where lines cross.

### Problem Statement
Finding an optimal route between two metro stations cannot be solved by simply counting stops (BFS). Segments vary in physical distance and travel time, and transferring between lines incurs walking and waiting overhead. A weighted shortest-path algorithm is required to compute routes based on specific user priorities: fastest time, shortest distance, or fewest transfers.

### Project Objectives
1. Parse and validate CSV data containing 262 stations and their connections.
2. Build an in-memory graph representation with bidirectional edges.
3. Compute shortest paths using Dijkstra's algorithm in $O((V+E)\log V)$ time.
4. Reconstruct the exact path using predecessor pointers.
5. Provide both a terminal interface and a web UI with map visualization.

### Scope of the Project
- **Included:** 262 active stations, bidirectional track connections, platform transfer directions, terminal CLI, and web interface.
- **Excluded:** Live train tracking, dynamic timetable schedules, and fare calculation.

---

## 5. System Requirements

### Functional Requirements
- **Data Ingestion:** Load and validate `stations.csv` (names, lines, coordinates) and `connections.csv` (source, destination, distance, travel time, line).
- **Route Calculation:**
  - `Fastest Time`: Minimize cumulative travel minutes.
  - `Shortest Distance`: Minimize cumulative track kilometers.
  - `Fewest Transfers`: Minimize line changes.
- **Route Reconstruction:** Return ordered station names, distance, estimated time, and transfer counts.
- **Transfer Guidance:** Identify interchange stations and indicate the outgoing platform/direction.
- **Interfaces:**
  - CLI: Direct terminal query (`--route FROM TO MODE`).
  - Web UI: Autocomplete station search, summary metrics, and Leaflet map rendering.

### Technical Specifications
- **Language:** C++17 (compiled with `-O2 -Wall -Wextra`).
- **Server:** Node.js HTTP server (`frontend/server.mjs`).
- **Frontend:** Vanilla HTML5, CSS3, JavaScript, Leaflet.js 1.9.4.

---

## 6. Design and Implementation

### 6.1 Architecture & Class Diagram

```mermaid
flowchart LR
    Browser["Web Client (Leaflet.js)"] -->|HTTP API| Server["Node.js Server"]
    Server -->|Process Execution| Engine["C++ Engine (metromitra.exe)"]
    Engine -->|Reads| CSVs["stations.csv / connections.csv"]
    Engine -->|JSON stdout| Server
    Server -->|JSON Response| Browser
```

```mermaid
classDiagram
    class Station {
        +string id
        +string name
        +string line
        +optional~double~ latitude
        +optional~double~ longitude
        +bool interchange
    }

    class Edge {
        +string destinationId
        +double distanceKm
        +double travelTimeMinutes
        +string line
        +EdgeType type
    }

    class Graph {
        -unordered_map~string, Station~ stations_
        -unordered_map~string, vector~Edge~~ adjacency_
        +bool addStation(Station station)
        +bool addConnection(string from, string to, double dist, double time, string line, EdgeType type)
        +const Station* findStation(string id) const
        +const vector~Edge~& neighbors(string id) const
        +size_t stationCount() const
    }

    class RouteResult {
        +vector~string~ stationIds
        +vector~string~ linesUsed
        +double totalDistanceKm
        +double estimatedTravelTimeMinutes
        +size_t transferCount
    }

    Graph "1" *-- "many" Station : stores
    Graph "1" *-- "many" Edge : adjacency list
```

---

### 6.2 Data Structures Used

| Data Structure | C++ STL Type | Purpose in MetroMitra | Why Chosen |
| :--- | :--- | :--- | :--- |
| **Graph (Adjacency List)** | `unordered_map<string, vector<Edge>>` | Stores vertices and outgoing edges for each station. | Metro network is sparse ($V=262, E \approx 300$). Saves memory ($O(V+E)$ vs $O(V^2)$ matrix) and enables $O(\text{deg}(v))$ neighbor traversal. |
| **Priority Queue (Min-Heap)** | `priority_queue<Entry, vector<Entry>, greater<Entry>>` | Selects the unvisited station with the lowest tentative cost during Dijkstra. | Complete binary heap stored in array. $O(1)$ read of minimum, $O(\log V)$ insertion and extraction. Avoids $O(V)$ linear scan. |
| **Hash Map** | `unordered_map<string, Station>` and `unordered_map<string, StationRecord>` | Stores station details by ID and keeps tentative Dijkstra state. | $O(1)$ average lookup and update time for station records by alphanumeric ID. |
| **Dynamic Array** | `vector<string>`, `vector<Edge>` | Stores neighbor lists, path sequences, and lines used. | Cache-friendly contiguous memory storage with $O(1)$ amortized append. |

---

### 6.3 Algorithms & Complexities

#### Dijkstra's Algorithm
Dijkstra's algorithm finds the shortest path from a starting station to all other reachable stations on a graph with non-negative edge weights.

```
Algorithm Dijkstra(Graph, source, destination, objective):
1. best[source].cost = 0; all other costs = infinity
2. MinHeap.push((0, source))
3. While MinHeap is not empty:
     (cost, u) = MinHeap.pop()
     If cost > best[u].cost: continue    // skip stale heap entries
     If u == destination: break          // target reached

     For each edge (u, v) in Graph.neighbors(u):
         edgeWeight = getWeight(edge, objective)
         newCost = cost + edgeWeight
         If newCost < best[v].cost:
             best[v].cost = newCost
             best[v].previous = u
             best[v].previousEdge = edge
             MinHeap.push((newCost, v))
4. Reconstruct path by following best[v].previous from destination back to source
5. Reverse path to get source -> destination sequence
```

#### Objective Weighting
- **Time Mode:** $\text{weight} = \text{edge.travelTimeMinutes}$
- **Distance Mode:** $\text{weight} = \text{edge.distanceKm}$
- **Fewest Transfers:** $\text{weight} = 1 \text{ if line changes, else } 0$

#### Path Reconstruction
Maintains a `previousStation` link for each relaxed vertex. Once the destination is reached, the path is reconstructed by following predecessor pointers backward ($O(L)$ time, where $L$ is path length) and reversing the list.

#### Complexity Analysis

| Step | Time Complexity | Space Complexity | Notes |
| :--- | :--- | :--- | :--- |
| **Graph Loading** | $O(V + E)$ | $O(V + E)$ | Inserts 262 stations and connections into adjacency list. |
| **Heap Push / Pop** | $O(\log V)$ | $O(1)$ | Bubble-up / bubble-down in binary heap. |
| **Dijkstra Search** | $O((V + E) \log V)$ | $O(V + E)$ | Each vertex is popped once; each edge relaxes into heap at most once. |
| **Path Backtracking** | $O(L) \subseteq O(V)$ | $O(L)$ | Traversing predecessor pointers back to source. |
| **Total Query** | **$O((V + E) \log V)$** | **$O(V + E)$** | Executes in $< 2\text{ ms}$ for $V=262, E \approx 300$. |

---

### 6.4 Key Code Snippets

#### 1. Graph Class & Adjacency List (`Graph.h` / `Graph.cpp`)
```cpp
bool Graph::addConnection(const string& fromId, const string& toId,
                          double distanceKm, double travelTimeMinutes,
                          const string& line, EdgeType type) {
    if (fromId == toId || !stations_.count(fromId) || !stations_.count(toId)) {
        return false;
    }
    // Bidirectional edges for undirected metro tracks
    adjacency_[fromId].push_back({toId, distanceKm, travelTimeMinutes, line, type});
    adjacency_[toId].push_back({fromId, distanceKm, travelTimeMinutes, line, type});
    return true;
}
```

#### 2. Min-Heap Dijkstra Implementation (`Dijkstra.cpp`)
```cpp
RouteResult shortestRoute(const Graph& graph, const string& sourceId,
                          const string& destinationId, RouteObjective objective) {
    unordered_map<string, StationRecord> best;
    best[sourceId].cost = 0;

    // Min-heap storing (cost, stationId)
    using Entry = pair<double, string>;
    priority_queue<Entry, vector<Entry>, greater<Entry>> pending;
    pending.push({0, sourceId});

    while (!pending.empty()) {
        const auto [cost, currentId] = pending.top();
        pending.pop();

        if (cost > best.at(currentId).cost) continue;
        if (currentId == destinationId) break;

        for (const auto& edge : graph.neighbors(currentId)) {
            double edgeCost = (objective == RouteObjective::Distance)
                                  ? edge.distanceKm
                                  : edge.travelTimeMinutes;

            const double newCost = cost + edgeCost;
            auto& next = best[edge.destinationId];

            if (newCost < next.cost) {
                next.cost = newCost;
                next.previousStation = currentId;
                next.previousEdge = edge;
                pending.push({newCost, edge.destinationId});
            }
        }
    }
    // Backtracking path reconstruction follows...
}
```

---

### 6.5 System Outputs

#### Terminal Output
```bash
$ ./metromitra.exe --route dmrc_1 dmrc_50 time

Route (fastest time):
  Dilshad Garden [dmrc_1] ->
  Jhilmil [dmrc_2] ->
  Mansrover park [dmrc_3] ->
  Shahdara [dmrc_4] ->
  Welcome [dmrc_5] ->
  Seelam Pur [dmrc_6] ->
  Shastri Park [dmrc_7] ->
  Kashmere Gate [dmrc_8] ->
  Chandni Chowk [dmrc_47] ->
  Chawri Bazar [dmrc_48] ->
  New Delhi [dmrc_49] ->
  Rajiv Chowk [dmrc_50]
Distance: 13.6088 km
Estimated edge travel time: 24.885 min
Line changes: 1
Lines: RED -> YELLOW
```

#### Web Application Output
- **Input Panel:** Start and destination station search with datalist auto-completion and optimization mode selector.
- **Results Card:** Displays total travel time, physical distance, number of transfers, and ordered stop cards.
- **Transfer Instructions:** Highlights line change points (e.g., *"Change to Yellow line toward Samaypur Badli · Platform 2"*).
- **Map View:** Leaflet.js map drawing color-coded polyline tracks (Red line, Yellow line) and station markers on OpenStreetMap tiles.

---

## 7. Conclusion

### Summary & Achievement of Objectives
- Modeled the entire 262-station Delhi Metro network using an Adjacency List graph.
- Implemented Dijkstra's algorithm with a binary min-heap, achieving $O((V+E)\log V)$ time complexity and sub-2ms query times.
- Supported multi-criteria route optimization: travel time, physical track distance, and fewest line changes.
- Built both a CLI tool and an interactive web interface with map visualization.

### Future Work
- **Fare Matrix:** Compute ticket prices based on official DMRC distance slabs.
- **A\* Search:** Implement Euclidean coordinate heuristics to prune exploration for even faster lookups.
- **Live Disruption Data:** Dynamically update edge weights based on service advisories or track delays.

---

## 8. References

1. Cormen, T. H., Leiserson, C. E., Rivest, R. L., & Stein, C. (2009). *Introduction to Algorithms* (3rd ed.). MIT Press. (Chapter 22: Graph Algorithms, Chapter 24: Single-Source Shortest Paths).
2. Delhi Metro Rail Corporation (DMRC). (2023). *GTFS Data Snapshot & Station Information*. https://www.delhimetrorail.com
3. ISO/IEC 14882:2017. *Programming Languages — C++ Standard Library (Containers & Algorithms)*.
4. Leaflet.js Documentation. *Mobile-Friendly Interactive Maps*. https://leafletjs.com
