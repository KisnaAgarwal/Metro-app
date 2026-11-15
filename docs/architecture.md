# MetroMitra architecture (initial slice)

The core is a frontend-independent C++17 library. `Graph` owns station records and an adjacency list. For the 2023 GTFS import, each GTFS stop is a vertex and can list multiple serving lines; consecutive-stop edges retain the line used. This lets an interchange remain a single connected physical station in the source model. The graph also supports explicit transfer and walk edges for later datasets with sourced platform-transfer information. Coordinates are taken from GTFS.

The sample fixture is synthetic and remains available as `stations_sample.csv` and `connections_sample.csv`. The default data files contain the imported GTFS snapshot, with its date and limitations documented in `data_sources.md`.

The current browser milestone adds a small Node.js local web server in `frontend/server.mjs`. It serves the browser interface and invokes the C++ executable for station/network JSON and route calculations. This adapter keeps every graph algorithm in C++; it is a local development bridge before the planned Crow API. Leaflet renders a dark schematic map directly from station coordinates and line connections without a basemap. The graph library does not depend on the web server, map, or a routing service.

Planned modules still include the group planner, fare policy, Crow API, and a packaged React/TypeScript client. The present browser interface is a dependency-light JavaScript MVP because npm is not installed in the development environment.
