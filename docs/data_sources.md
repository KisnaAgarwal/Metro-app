# Delhi Metro data sourcing plan

# Data provenance and limits

## Imported snapshot

The default `stations.csv` and `connections.csv` were generated from `backend/data/source/DMRC_GTFS_2023.zip` using `backend/tools/import_gtfs.py`. It contains 262 stops and 36 directional route records, covering 11 line labels: RED, YELLOW, BLUE, VIOLET, GREEN, PINK, MAGENTA, GRAY, ORANGE/AIRPORT, AQUA, and RAPID. Aqua and Rapid lines are part of the feed, so this snapshot includes NCR rail services beyond the Delhi-only corridors.

The official [Delhi OTD DMRC Static Data page](https://otd.delhi.gov.in/data/staticDMRC/) lists 262 stops and says its source data was last updated 2023-08-10. The checked-in archive is a public DMRC GTFS snapshot mirrored in the [adot-7/metroshell release](https://github.com/adot-7/metroshell/releases/tag/v1.0.0); its calendar runs through 2025-12-31. The archive has no `feed_info.txt` publication timestamp, and its observed trip/stop-time counts differ from the portal's summary. Treat it as a 2023-era snapshot based on that source lineage, not a byte-for-byte verified copy of the OTD download. Newer stations opened after that snapshot are intentionally absent.

Station names, stop IDs, coordinates, and ordered trips come from GTFS. Each physical GTFS stop is one vertex, with a pipe-separated set of route line labels. An interchange is marked when the stop appears on more than one line. This keeps cross-line connectivity at a shared GTFS stop without inventing transfer connectors. The feed does not provide platform walking links or a transfer-time penalty, so reported travel times omit platform walking/waiting.

Connections are derived from adjacent stop-time pairs in each trip. Segment distance is the median `shape_dist_traveled` delta in kilometres; for the one segment with no usable shape distance, the importer uses Haversine straight-line distance from the GTFS coordinates. Segment time is the median scheduled departure-to-next-arrival interval across trips and directions. Time routing adds a user-selected 45-second dwell assumption at intermediate same-line stops. At an interchange, it uses a 2-minute allowance for two served lines and 3 minutes for three or more lines, instead of same-line dwell. Line count is a rough crowding proxy, not measured crowd data or walking time. The origin and destination add neither dwell nor a line-change penalty. Treat segment times as schedule estimates, not live travel times. Train waiting and actual platform walking remain excluded.

The feed does not encode every physical walking connection. The generated graph has two connected components (241 stops and 21 stops, the latter the Aqua Line). The data has no documented connecting transfer edge between those components, so no such edge is fabricated. A journey crossing components returns unreachable.

The OTD source page requires basic user details and terms acceptance for direct downloads. This project uses the publicly mirrored archive above and records its uncertainty. For redistribution beyond this educational project, review the applicable [Delhi OTD terms](https://otd.delhi.gov.in/terms) and acknowledge DMRC/Delhi OTD.

The separate three-stop fixture (`stations_sample.csv` and `connections_sample.csv`) is synthetic and is used only for unit tests. The graph schema also accepts explicit transfer/walk edges when trustworthy per-platform data becomes available. OpenStreetMap requires its own attribution and ODbL handling; no OSM data is included in this snapshot.
