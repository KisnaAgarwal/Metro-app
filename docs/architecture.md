# MetroMitra architecture

The browser sends requests to a local Node.js server, which runs a C++17 executable and returns its JSON output. The C++ program loads the CSV graph for each request. Leaflet draws selected routes on OpenStreetMap tiles.

The backend is organized by responsibility: Graph stores the network, CsvLoader reads the data, Dijkstra finds routes, Meetup handles meeting plans, JsonOutput formats shared JSON fields, and main dispatches commands. BFS and DFS are separate educational algorithms.

The frontend uses plain HTML, JavaScript, and one stylesheet. The server checks source and header timestamps and recompiles the executable when needed at startup.

See [the code walkthrough](code_walkthrough.md) for an explanation you can use when presenting the project, and [data sources](data_sources.md) for dataset limitations.
