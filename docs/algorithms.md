# Routing algorithms

The route finder uses min-heap Dijkstra with one best record per station. Distance mode minimizes CSV edge distances. Time mode minimizes CSV edge times without extra dwell or line-change allowances. Explicit transfer and walking edge times remain included.

The heap stores (cost, station ID) pairs. An improved route adds a new entry; outdated entries are skipped when popped. Previous stations and edges reconstruct the selected path.

Fewest-transfers mode is approximate because line-change cost depends on the arrival line and only one arrival is retained per station. Equal-cost candidates keep the first discovered path; there is no time tie-breaker.

For distance/time searches, time is O((V+E) log(V+E)) and auxiliary space is O(V+E), accounting for duplicate heap entries. The network uses nonnegative weights.

See [the presentation guide](presentation_guide.md) for a worked example, a heap explanation, and questions to prepare for.
