# How to explain MetroMitra

MetroMitra models metro stations as a graph and uses Dijkstra's algorithm to plan journeys.

## Follow one request

1. `frontend/index.html` contains the two panels: the journey form and map.
2. `frontend/app.js` reads the selected stations and sends an HTTP request such as `/api/route?from=dmrc_1&to=dmrc_50&mode=time`.
3. `frontend/server.mjs` serves the webpage and runs the C++ program with those arguments.
4. `backend/src/main.cpp` loads the CSV graph and selects the requested command.
5. `backend/src/Dijkstra.cpp` calculates the route. The program prints JSON, which the server returns to the browser.
6. The browser displays the route details and draws the route using Leaflet.

The graph is loaded for each command. There is no database or persistent backend process; Node starts the executable for each API request.

## Files worth presenting

| File | Responsibility |
| --- | --- |
| `backend/include/Station.h` | Station fields and connection types |
| `backend/src/Graph.cpp` | Store stations and adjacency lists; add both directions of each connection |
| `backend/src/CsvLoader.cpp` | Read and validate station and connection CSV files |
| `backend/src/Dijkstra.cpp` | Find a route and reconstruct its stations and lines |
| `backend/src/Meetup.cpp` | Choose where friends meet and write the meetup response |
| `backend/src/JsonOutput.cpp` | Escape text and write station JSON |
| `backend/src/main.cpp` | Command dispatch and single-route output |
| `backend/src/Traversal.cpp` | BFS and DFS learning examples; not used by the browser planner |
| `frontend/server.mjs` | Compile when needed, serve files, and bridge HTTP to C++ |
| `frontend/app.js` | Inputs, API calls, results, and map drawing |
| `frontend/styles.css` | All interface styling, including mobile layouts |

## Explain the graph

A station is a vertex. A connection is an edge with distance, time, line, and type. The adjacency list stores each station's outgoing connections. The CSV loader adds connections in both directions.

## Explain Dijkstra

A min-heap chooses the cheapest candidate station. A lookup table stores one best cost and previous connection per station. Cheaper routes update that table and enter the heap. Following previous stations backward reconstructs the route.

Distance and time modes minimize the corresponding CSV edge weights. No extra dwell or interchange allowances are added. Fewest transfers is approximate because only one arrival line is retained per station; there is no time tie-breaker.

See [the presentation guide](presentation_guide.md) for a small worked example and an explanation of the heap.

## Explain meetups

Without a destination, try each station, calculate every friend's fastest route there, and choose the smallest combined time. Break ties using the longest individual journey.

With a destination, consider shared stations on friends' routes to that destination. Build a shared onward route and let the remaining friends join at stations along it. Compare the candidates by combined time to their joining stations. This is a staged-meeting heuristic, not a proof of the globally best group itinerary.

## Data and supporting files

`backend/data/stations.csv` and `connections.csv` are the runtime network. The platform CSV supplies optional transfer guidance. Sample CSVs support small demonstrations; the source GTFS files and import script document how the network was produced.

BFS, DFS, source data, and sample fixtures are retained for learning and reproducibility. They are separate from the browser request flow.

## Run

With Node.js and g++ installed, run `node frontend/server.mjs` from the project folder. Open http://127.0.0.1:4173. Restart the server after changing C++ or server code so it can rebuild the executable. HTML, CSS, and browser JavaScript changes need a page refresh.
