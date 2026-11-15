# Presenting the routing algorithm

## Your 30-second explanation

"I represent the metro as a graph. Stations are vertices and connections are edges. Each edge has a distance and travel time. I use Dijkstra's algorithm to find a cheap route. A min-heap gives me the lowest-cost station to explore next. Whenever I find a cheaper way to a station, I update its cost and remember the previous station. At the end, I follow those previous stations backward to recover the route."

## The tree part: a min-heap

The metro network is a graph. The priority queue uses a heap, which is a complete binary tree stored in an array.

You do not manually create tree nodes. C++ `priority_queue` manages the heap.

- A parent has a cost no greater than its children.
- The cheapest item is at the root, so `top()` reads it.
- `push()` adds a candidate and restores the heap order.
- `pop()` removes the cheapest candidate and restores the heap order.
- The heap is not a binary search tree. Its left and right branches are not sorted ranges.

`greater<Entry>` makes this a min-heap. Without it, the default priority queue gives the largest item first. An entry is a pair containing the cost and station ID; equal costs are ordered by ID.

## A tiny example

Connections: A to B costs 4, A to C costs 1, and C to B costs 2.

| Action | Best cost to B | Best cost to C |
| --- | --- | --- |
| Start at A | infinity | infinity |
| Explore A | 4 | 1 |
| Explore C next, because 1 is cheapest | 3 | 1 |
| Explore B | 3 | 1 |

B remembers C as its previous station. C remembers A. Read backward: B, C, A. Reverse it: A, C, B. Total cost: 3.

The heap may still contain the old entry for B with cost 4. The code ignores old entries whose cost is worse than the current best.

## Read Dijkstra.cpp in six steps

1. Set the source cost to zero.
2. Pop the cheapest candidate from the heap.
3. Look at its neighboring stations.
4. Update a neighbor only if the new route costs less.
5. Follow previous stations backward and reverse the result.
6. Sum the distance, time, and transfers along that route.

`unordered_map` is a lookup table keyed by station ID. Each station record stores its best cost, previous station, previous edge, and arrival line. The previous edge tells us which connection was chosen if two stations share more than one line.

## What was simplified

There is one record per station. The previous version kept separate records for arriving at the same station on different lines.

Distance mode minimizes edge distance. Time mode minimizes the CSV edge times, without adding extra dwell or interchange allowances. CSV times for explicit transfer or walking edges still count.

Fewest-transfers mode uses a cost of one when changing lines and zero otherwise. It is approximate: remembering only one arrival line can discard another arrival that would need fewer changes later. Equal-cost routes do not use time as a tie-breaker. Say this honestly if asked.

## Likely questions

**Why not BFS?** BFS minimizes the number of edges. Metro connections have different distances and times, so the route with the fewest stops is not necessarily fastest.

**Why a heap?** It retrieves the cheapest candidate efficiently. Push and pop cost logarithmic time; reading the top is constant time.

**Why no negative weights?** Dijkstra assumes nonnegative edge costs. Distances, times, and transfer counts meet that condition.

**Complexity?** With this implementation's duplicate heap entries, time is O((V+E) log(V+E)), and auxiliary space is O(V+E). V is stations and E is connections. These bounds describe distance/time Dijkstra; the transfer mode is a heuristic.

**What if there is no route?** The destination never gets a record, so the function returns an empty result.

**What if start and destination match?** The route contains that one station and has zero cost.

**Does meetup planning still work?** Yes. It calls this routing function repeatedly to compare meeting stations. It now uses the simpler time model too.
