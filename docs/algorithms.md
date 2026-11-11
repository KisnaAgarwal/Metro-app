# Algorithm plan

| Algorithm | Purpose | Time | Space |
|---|---|---:|---:|
| BFS | Minimum-edge paths / exploration | O(V+E) | O(V) |
| DFS | Traversal and connectivity | O(V+E) | O(V) |
| Dijkstra (binary heap) | Nonnegative distance or time costs | O((S+F) log S) | O(S+F) |
| Fewest transfers | Lexicographic or expanded-state shortest path | O((V+E) log V) | O(V+E) |
| A* | Destination-directed weighted routing | O((V+E) log V) typical; worst-case Dijkstra | O(V) |
| Floyd-Warshall | All-pairs costs | O(V^3) | O(V^2) |

For all route searches, parent state must include the platform vertex and route metrics need to be recomputed from the reconstructed path. A* requires validated coordinates and an admissible geographic lower bound; it is unavailable for this synthetic dataset.

The current BFS and DFS APIs return the traversal order of the connected component containing the supplied start vertex. They reject an unknown start ID. BFS explores in minimum-edge layers; neither traversal considers distance or time weights.

`shortestRoute` implements Dijkstra using a min-priority queue over `(station, incoming line)` states, with shortest-distance, fastest-time, and fewest-transfers objectives. Fewest-transfers minimizes line changes first and estimated travel time second. It reconstructs the path from parent links and totals both metrics. For time routing, continuing on the same line adds a configurable default dwell of 0.75 minutes (45 seconds) at each intermediate stop; it excludes the origin and destination. Changing lines at an implicit interchange adds 2 minutes for a two-line station or 3 minutes for a station serving three or more lines, replacing same-line dwell at that stop. Line count is used as a rough proxy for crowding because crowd observations are unavailable. Explicit transfer edges use their own travel-time weight. The algorithm reports route lines and interchange count. An unreachable destination returns an empty path. Ties select one valid optimal route; alternative-route enumeration is future work. Train waiting and actual transfer walking remain excluded.

For the expanded Dijkstra state graph, `S` is the number of reachable station/incoming-line states and `F` is the number of state transitions.
