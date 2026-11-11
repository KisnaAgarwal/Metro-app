#!/usr/bin/env python3
"""Convert a DMRC GTFS ZIP snapshot to MetroMitra station/edge CSV files.

Uses only GTFS records in the archive. Edge distances use shape_dist_traveled
where available; missing segment distances use a Haversine straight-line estimate.
Travel minutes are median scheduled segment times across trips/directions.
"""
import argparse
import csv
import io
import math
import statistics
import zipfile
from collections import defaultdict
from pathlib import Path


def read_table(archive: zipfile.ZipFile, name: str):
    with archive.open(name) as raw:
        text = io.TextIOWrapper(raw, encoding="utf-8-sig", newline="")
        return list(csv.DictReader(text))


def minutes(value: str) -> int:
    hours, mins, seconds = value.split(":")
    return int(hours) * 60 + int(mins) + int(seconds) / 60


def haversine_km(a, b) -> float:
    lat1, lon1 = map(math.radians, a)
    lat2, lon2 = map(math.radians, b)
    dlat, dlon = lat2 - lat1, lon2 - lon1
    h = math.sin(dlat / 2) ** 2 + math.cos(lat1) * math.cos(lat2) * math.sin(dlon / 2) ** 2
    return 6371.0088 * 2 * math.asin(math.sqrt(h))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("gtfs_zip", type=Path)
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()

    with zipfile.ZipFile(args.gtfs_zip) as archive:
        stops = read_table(archive, "stops.txt")
        routes = read_table(archive, "routes.txt")
        trips = read_table(archive, "trips.txt")
        stop_times = read_table(archive, "stop_times.txt")

    route_line = {}
    for route in routes:
        # The feed uses a line prefix followed by route endpoints.
        route_line[route["route_id"]] = route["route_long_name"].split("_", 1)[0]
    trip_line = {trip["trip_id"]: route_line[trip["route_id"]] for trip in trips}

    station_by_id = {stop["stop_id"]: stop for stop in stops}
    lines_by_stop = defaultdict(set)
    observations = defaultdict(lambda: [[], []])
    by_trip = defaultdict(list)
    for row in stop_times:
        by_trip[row["trip_id"]].append(row)
    for trip_id, rows in by_trip.items():
        rows.sort(key=lambda row: int(row["stop_sequence"]))
        line = trip_line[trip_id]
        for row in rows:
            lines_by_stop[row["stop_id"]].add(line)
        for first, second in zip(rows, rows[1:]):
            a, b = first["stop_id"], second["stop_id"]
            if a == b or a not in station_by_id or b not in station_by_id:
                continue
            key = (min(a, b), max(a, b), line)
            elapsed = minutes(second["arrival_time"]) - minutes(first["departure_time"])
            if elapsed > 0:
                observations[key][1].append(elapsed)
            try:
                shape_delta = abs(float(second["shape_dist_traveled"]) - float(first["shape_dist_traveled"]))
                if shape_delta > 0:
                    observations[key][0].append(shape_delta / 1000.0)
            except (KeyError, ValueError):
                pass

    args.output_dir.mkdir(parents=True, exist_ok=True)
    station_path = args.output_dir / "stations.csv"
    connection_path = args.output_dir / "connections.csv"
    with station_path.open("w", encoding="utf-8", newline="") as out:
        writer = csv.writer(out)
        writer.writerow(["id", "physical_station_id", "name", "line", "latitude", "longitude", "interchange"])
        for stop in sorted(stops, key=lambda item: int(item["stop_id"])):
            sid = stop["stop_id"]
            line_names = sorted(lines_by_stop[sid])
            if not line_names:
                line_names = ["UNASSIGNED"]
            writer.writerow([f"dmrc_{sid}", sid, stop["stop_name"], "|".join(line_names),
                             stop["stop_lat"], stop["stop_lon"], str(len(line_names) > 1).lower()])

    missing_distance = 0
    with connection_path.open("w", encoding="utf-8", newline="") as out:
        writer = csv.writer(out)
        writer.writerow(["source_id", "destination_id", "distance_km", "travel_time_minutes", "line", "type"])
        for (a, b, line), (distances, times) in sorted(observations.items()):
            if not times:
                continue
            if distances:
                distance = statistics.median(distances)
            else:
                pa, pb = station_by_id[a], station_by_id[b]
                distance = haversine_km((float(pa["stop_lat"]), float(pa["stop_lon"])),
                                        (float(pb["stop_lat"]), float(pb["stop_lon"])))
                missing_distance += 1
            writer.writerow([f"dmrc_{a}", f"dmrc_{b}", f"{distance:.6f}",
                             f"{statistics.median(times):.3f}", line, "ride"])

    print(f"Wrote {len(stops)} GTFS stops and {len(observations)} unique line-specific segments.")
    print(f"Used straight-line distance for {missing_distance} segments missing shape distance.")


if __name__ == "__main__":
    main()
