# MetroMitra

An educational C++ graph project for Delhi Metro journey planning. The default CSV dataset is a 2023 DMRC GTFS snapshot with 262 stops. A separate three-node synthetic fixture remains available for tests.

## Browser app

Node.js and g++ are required. From the project root, start the web app:

```powershell
node frontend/server.mjs
```

Open `http://127.0.0.1:4173`. The server compiles the C++ route engine on startup and sends route requests to it. It serves the browser UI and local JSON API. Leaflet loads from a public CDN and displays OpenStreetMap tiles (no API key required), with the selected MetroMitra route drawn on top.

The browser planner supports single trips and friend meetups for 2–8 starting stations. Without a shared destination, meetup planning selects one station with the lowest combined estimated travel time. With a shared destination, it can create staged meetups: an initial group meets first, other friends join at later stations along the group's route, and the planner shows the remaining trip after everyone has joined. Estimates use the same 2023 network and timing model as single-trip routing.

Platform directions are stored separately in `backend/data/station_platforms.csv`. They are supplementary current DMRC data and are not part of the 2023 GTFS snapshot. The CSV includes its DMRC station-page source and retrieval date for each record. Coverage is partial; when a platform is not listed for a transfer, the app directs riders to follow the line signs.

## Project status

1. Core station/edge graph and synthetic fixture (implemented).
2. Validated CSV loading and BFS/DFS algorithms (implemented).
3. Dijkstra distance/time and route reconstruction (implemented); fare estimates remain future work.
4. 2023 GTFS station and connection import (implemented; see source and limitations).
5. Transfer-aware routing, single-trip browser planner, route map, and friend meetup planner (implemented).
6. Platform-number guidance from verified DMRC station pages (partial coverage; additional verified rows can be added to the platform CSV).
7. Fare estimates, sequential-pickup group planning, and live service data remain future work.

See `docs/` for the architecture, complexity plan, data provenance plan, and group-planner design considerations.
