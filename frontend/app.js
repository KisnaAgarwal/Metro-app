const colors = {
  RED: '#ff5368', YELLOW: '#ffd84d', BLUE: '#50b8ff', VIOLET: '#c093ff',
  GREEN: '#5bdd93', PINK: '#ff7bc6', MAGENTA: '#ee73dd', GRAY: '#b8c2d2',
  'ORANGE/AIRPORT': '#ff9f59', AQUA: '#5ee5e8', RAPID: '#70dfb7',
};
const modeNames = { time: 'FASTEST TIME', distance: 'SHORTEST DISTANCE', transfers: 'FEWEST TRANSFERS' };
const $ = (id) => document.getElementById(id);
let stations = [];
let stationPlatforms = [];
let selectedMode = 'time';
let map;
let routeLayer;
let stationLayer;
let currentRouteBounds;
let toastTimer;

function lineNames(value) { return (value || '').split('|').filter(Boolean); }
function stationLabel(station) {
  const lines = lineNames(station.line).join(' / ');
  return `${station.name} — ${lines}`;
}
function showToast(message) {
  const toast = $('toast');
  toast.textContent = message;
  toast.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => toast.classList.remove('show'), 3200);
}
function setLoading(loading) {
  const button = $('find-route');
  button.disabled = loading;
  button.querySelector('span:first-child').textContent = loading ? 'Finding your route…' : 'Find my route';
}

async function api(path) {
  const response = await fetch(path);
  const body = await response.json();
  if (!response.ok) throw new Error(body.error || 'The route service could not complete the request.');
  return body;
}

function initStationOptions() {
  $('station-count').textContent = stations.length;
  const list = $('station-options');
  list.replaceChildren(...stations.map((station) => {
    const option = document.createElement('option');
    option.value = stationLabel(station);
    option.dataset.id = station.id;
    return option;
  }));
  const initial = stations.find((station) => station.name.toLowerCase() === 'rajiv chowk');
  const destination = stations.find((station) => station.name.toLowerCase() === 'aiims');
  if (initial) $('origin').value = stationLabel(initial);
  if (destination) $('destination').value = stationLabel(destination);
}

function selectedStation(inputId) {
  const value = $(inputId).value.trim();
  return stations.find((station) => stationLabel(station) === value);
}
function selectedFriendStation(input) {
  const value = input.value.trim();
  return stations.find((station) => stationLabel(station) === value);
}

function showPlanner(mode) {
  const meetup = mode === 'meetup';
  document.querySelectorAll('[data-single-trip]').forEach((element) => element.classList.toggle('hidden', meetup));
  $('meetup-panel').classList.toggle('hidden', !meetup);
  $('single-trip-tab').classList.toggle('active', !meetup);
  $('meetup-tab').classList.toggle('active', meetup);
  $('single-trip-tab').setAttribute('aria-selected', String(!meetup));
  $('meetup-tab').setAttribute('aria-selected', String(meetup));
  $('route-stations-panel').classList.toggle('hidden', meetup);
  if (meetup) resetDisplayedMap();
}

function resetDisplayedMap() {
  routeLayer?.clearLayers();
  stationLayer?.clearLayers();
  currentRouteBounds = null;
  $('map-empty').classList.remove('hidden');
  renderLegend([]);
}

function addFriendRow() {
  const rows = $('friend-stations');
  const count = rows.querySelectorAll('[data-friend-station]').length;
  if (count >= 8) {
    showToast('Meetups support up to 8 friends.');
    return;
  }
  const label = document.createElement('label');
  label.className = 'friend-station-row';
  const number = document.createElement('span');
  number.className = 'friend-number';
  number.textContent = String(count + 1);
  const input = document.createElement('input');
  input.dataset.friendStation = '';
  input.setAttribute('list', 'station-options');
  input.autocomplete = 'off';
  input.placeholder = `Friend ${count + 1} starting station`;
  const remove = document.createElement('button');
  remove.type = 'button';
  remove.className = 'remove-friend';
  remove.textContent = '×';
  remove.setAttribute('aria-label', `Remove friend ${count + 1}`);
  remove.addEventListener('click', () => { label.remove(); renumberFriends(); });
  label.append(number, input, remove);
  rows.append(label);
}

function renumberFriends() {
  $('friend-stations').querySelectorAll('.friend-station-row').forEach((row, index) => {
    row.querySelector('.friend-number').textContent = String(index + 1);
    row.querySelector('input').placeholder = `Friend ${index + 1} starting station`;
    const remove = row.querySelector('.remove-friend');
    if (remove) remove.setAttribute('aria-label', `Remove friend ${index + 1}`);
  });
  $('add-friend').classList.toggle('disabled', $('friend-stations').querySelectorAll('[data-friend-station]').length >= 8);
}

function initModes() {
  document.querySelectorAll('.mode-option').forEach((button) => {
    button.addEventListener('click', () => {
      selectedMode = button.dataset.mode;
      document.querySelectorAll('.mode-option').forEach((item) => item.classList.toggle('active', item === button));
    });
  });
}

function initMap() {
  if (!window.L) {
    showToast('The map library did not load. Check your internet connection and refresh.');
    return;
  }
  map = L.map('map', { zoomControl: true, scrollWheelZoom: true, attributionControl: true }).setView([28.635, 77.22], 11);
  // Standard OpenStreetMap tiles provide a normal street map with no API key.
  // Metro lines are still drawn only for the selected route below.
  L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap contributors</a>',
    subdomains: 'abc',
    maxZoom: 19,
  }).addTo(map);
  stationLayer = L.layerGroup().addTo(map);
  routeLayer = L.layerGroup().addTo(map);
}

function renderLegend(lines) {
  const legend = $('line-legend');
  legend.replaceChildren(...lines.map((line) => {
    const item = document.createElement('span');
    item.className = 'legend-item';
    const swatch = document.createElement('i');
    swatch.className = 'legend-swatch';
    swatch.style.backgroundColor = colors[line] || '#77808b';
    item.append(swatch, document.createTextNode(line));
    return item;
  }));
}

function renderRoute(route, origin, destination) {
  const title = `${origin.name} to ${destination.name}`;
  $('route-heading').textContent = title;
  $('route-mode-tag').textContent = modeNames[selectedMode];
  $('metric-time').textContent = `${Math.round(route.estimatedTimeMinutes)} min`;
  $('metric-distance').textContent = `${route.distanceKm.toFixed(1)} km`;
  $('metric-transfers').textContent = route.transfers;
  $('line-summary').replaceChildren(...route.lines.map((line) => {
    const chip = document.createElement('span');
    chip.className = 'line-chip';
    chip.style.borderLeft = `3px solid ${colors[line] || '#77808b'}`;
    chip.textContent = line;
    return chip;
  }));
  const list = $('route-stops');
  list.replaceChildren(...route.stations.map((station, index) => {
    const item = document.createElement('li');
    item.className = 'route-stop-card';
    const dot = document.createElement('span');
    dot.className = 'stop-dot';
    dot.textContent = String(index + 1);
    const name = document.createElement('span');
    name.className = 'route-stop-name';
    name.textContent = station.name;
    item.append(dot, name);
    const incomingLine = route.segmentLines?.[index - 1];
    const outgoingLine = route.segmentLines?.[index];
    if (station.interchange && incomingLine && outgoingLine && incomingLine !== outgoingLine &&
        incomingLine !== 'TRANSFER' && outgoingLine !== 'TRANSFER' && incomingLine !== 'WALK' && outgoingLine !== 'WALK') {
      const instruction = document.createElement('small');
      instruction.className = 'transfer-instruction';
      const nextStation = route.stations[index + 1]?.name;
      const platformOptions = stationPlatforms.filter((row) => row.stationId === station.id && row.line === outgoingLine);
      if (platformOptions.length) {
        instruction.textContent = `Change to ${outgoingLine}${nextStation ? ` toward ${nextStation}` : ''} · DMRC platform ${platformOptions.map((row) => `${row.platform} toward ${row.direction}`).join(' / ')}`;
      } else {
        instruction.textContent = `Change to ${outgoingLine}${nextStation ? ` toward ${nextStation}` : ''} · platform number unavailable; follow ${outgoingLine} signs toward ${nextStation || 'your destination'}`;
      }
      item.append(instruction);
    }
    return item;
  }));
  $('stop-count').textContent = `${route.stations.length} stops`;
  $('route-results').classList.remove('hidden');

  routeLayer.clearLayers();
  stationLayer.clearLayers();
  $('map-empty').classList.add('hidden');
  renderLegend(route.lines);
  const points = route.stations.filter((s) => s.latitude != null && s.longitude != null)
    .map((station) => [Number(station.latitude), Number(station.longitude)]);
  if (points.length) {
    for (let i = 0; i < points.length - 1; i++) {
      const line = route.segmentLines?.[i] || route.lines?.[0] || 'BLUE';
      const segment = [points[i], points[i + 1]];
      L.polyline(segment, { color: '#07111c', weight: 10, opacity: .95, lineCap: 'round' }).addTo(routeLayer);
      L.polyline(segment, {
        color: colors[line] || '#f4f7fb', weight: 6.5, opacity: 1,
        lineCap: 'round', dashArray: line === 'TRANSFER' || line === 'WALK' ? '5 5' : null,
      }).addTo(routeLayer);
    }
    for (let i = 0; i < route.stations.length; i++) {
      const station = route.stations[i];
      if (station.latitude == null || station.longitude == null) continue;
      const isOrigin = i === 0;
      const isDestination = i === route.stations.length - 1;
      const adjacentLine = route.segmentLines?.[Math.min(i, route.segmentLines.length - 1)] || route.lines?.[0] || 'BLUE';
      const marker = L.circleMarker([Number(station.latitude), Number(station.longitude)], {
        radius: isOrigin || isDestination ? 7 : station.interchange ? 5.5 : 4.5,
        color: '#090d12', weight: isOrigin || isDestination ? 2.5 : 1.5,
        fillColor: isOrigin ? '#69e1a0' : isDestination ? '#ff8b72' : colors[adjacentLine] || '#e6edf5',
        fillOpacity: 1,
      }).addTo(stationLayer);
      const showName = isOrigin || isDestination || station.interchange || route.stations.length <= 18;
      if (showName) {
        const label = isOrigin || isDestination ? `${isOrigin ? 'START' : 'END'} · ${station.name}` : station.name;
        marker.bindTooltip(label, {
          permanent: true, direction: isOrigin ? 'bottom' : 'top', className: 'route-label', offset: [0, 4],
        });
      } else {
        marker.bindTooltip(station.name, { direction: 'top', className: 'route-label', offset: [0, -3] });
      }
    }
    currentRouteBounds = L.latLngBounds(points);
    map.fitBounds(currentRouteBounds.pad(.26), { maxZoom: 13 });
  }
}

function renderMeetupMap(result) {
  routeLayer.clearLayers();
  stationLayer.clearLayers();
  $('map-empty').classList.add('hidden');
  const allLines = [...new Set(result.journeys.flatMap((journey) => journey.segmentLines).filter((line) => line !== 'TRANSFER' && line !== 'WALK'))];
  renderLegend(allLines);
  const allPoints = [];
  result.journeys.forEach((journey, journeyIndex) => {
    const coords = journey.stations.filter((station) => station.latitude != null && station.longitude != null)
      .map((station) => [Number(station.latitude), Number(station.longitude)]);
    allPoints.push(...coords);
    for (let i = 0; i + 1 < coords.length; i++) {
      const line = journey.segmentLines?.[i] || 'BLUE';
      const segment = [coords[i], coords[i + 1]];
      L.polyline(segment, { color: '#07111c', weight: 9, opacity: .9, lineCap: 'round' }).addTo(routeLayer);
      L.polyline(segment, { color: colors[line] || '#e6edf5', weight: 4.5, opacity: .8, lineCap: 'round' }).addTo(routeLayer);
    }
    journey.stations.forEach((station, index) => {
      if (station.latitude == null || station.longitude == null) return;
      const isStart = index === 0;
      const isMeetup = index === journey.stations.length - 1;
      const line = journey.segmentLines?.[Math.min(index, journey.segmentLines.length - 1)] || 'BLUE';
      L.circleMarker([Number(station.latitude), Number(station.longitude)], {
        radius: isStart ? 6 : isMeetup ? 8 : 4,
        color: '#101820', weight: 1.5,
        fillColor: isMeetup ? '#69e1a0' : isStart ? ['#69e1a0', '#ff8b72', '#c093ff', '#ffd84d', '#50b8ff', '#ff7bc6', '#5bdd93', '#ff9f59'][journeyIndex] : colors[line] || '#e6edf5',
        fillOpacity: 1,
      }).addTo(stationLayer);
    });
  });
  if (result.onwardRoute) {
    const onward = result.onwardRoute;
    const coords = onward.stations.filter((station) => station.latitude != null && station.longitude != null)
      .map((station) => [Number(station.latitude), Number(station.longitude)]);
    allPoints.push(...coords);
    for (let i = 0; i + 1 < coords.length; i++) {
      const line = onward.segmentLines?.[i] || 'BLUE';
      L.polyline([coords[i], coords[i + 1]], { color: '#07111c', weight: 10, opacity: .95, lineCap: 'round' }).addTo(routeLayer);
      L.polyline([coords[i], coords[i + 1]], { color: colors[line] || '#e6edf5', weight: 6, opacity: 1, lineCap: 'round' }).addTo(routeLayer);
    }
    onward.stations.slice(1).forEach((station, index, remaining) => {
      if (station.latitude == null || station.longitude == null) return;
      const isFinal = index === remaining.length - 1;
      L.circleMarker([Number(station.latitude), Number(station.longitude)], {
        radius: isFinal ? 8 : 4, color: '#101820', weight: 1.5,
        fillColor: isFinal ? '#ff8b72' : colors[onward.segmentLines?.[index] || 'BLUE'] || '#e6edf5', fillOpacity: 1,
      }).bindTooltip(isFinal ? `FINAL · ${station.name}` : station.name, {
        permanent: isFinal, direction: 'top', className: 'route-label', offset: [0, -4],
      }).addTo(stationLayer);
    });
  }
  if (result.meetup.latitude != null && result.meetup.longitude != null) {
    L.circleMarker([Number(result.meetup.latitude), Number(result.meetup.longitude)], {
      radius: 11, color: '#eafbf2', weight: 2.5, fillColor: '#32b477', fillOpacity: 1,
    }).bindTooltip(`MEET HERE · ${result.meetup.name}`, {
      permanent: true, direction: 'top', className: 'route-label meetup-map-label', offset: [0, -5],
    }).addTo(stationLayer);
  }
  if (allPoints.length) {
    currentRouteBounds = L.latLngBounds(allPoints);
    map.fitBounds(currentRouteBounds.pad(.22), { maxZoom: 13 });
  }
}

function renderMeetupResults(result) {
  const container = $('meetup-results');
  container.replaceChildren();
  const summary = document.createElement('div');
  summary.className = 'meetup-summary';
  const heading = document.createElement('p');
  heading.className = 'eyebrow';
  heading.textContent = 'BEST MEETUP STATION';
  const title = document.createElement('h3');
  title.textContent = result.meetup.name;
  if (result.finalDestination) {
    const destination = document.createElement('p');
    destination.className = 'meetup-final-destination';
    destination.textContent = `Meet here, then travel together to ${result.finalDestination.name}`;
    summary.append(heading, title, destination);
  } else {
    summary.append(heading, title);
  }
  const stats = document.createElement('p');
  stats.className = 'meetup-stats';
  stats.textContent = `${result.journeys.length} friends · ${Math.round(result.totalTimeMinutes)} combined time to meetup · longest ${Math.round(result.longestTripMinutes)} min`;
  summary.append(stats);
  container.append(summary);
  const journeys = document.createElement('div');
  journeys.className = 'meetup-journeys';
  result.journeys.forEach((journey, index) => {
    const card = document.createElement('details');
    card.className = 'meetup-journey';
    const summaryLine = document.createElement('summary');
    const from = document.createElement('strong');
    from.textContent = `Friend ${index + 1}: ${journey.from.name}`;
    const time = document.createElement('span');
    time.textContent = `${Math.round(journey.estimatedTimeMinutes)} min · ${journey.transfers} changes`;
    summaryLine.append(from, time);
    const lines = document.createElement('small');
    lines.textContent = journey.segmentLines.filter((line) => line !== 'TRANSFER' && line !== 'WALK').filter((line, i, arr) => i === 0 || arr[i - 1] !== line).join(' → ') || 'Direct walk';
    const stops = document.createElement('p');
    stops.className = 'meetup-stop-list';
    stops.textContent = journey.stations.map((station) => station.name).join(' → ');
    card.append(summaryLine, lines, stops);
    if (journey.joinAt) {
      const join = document.createElement('small');
      join.className = 'meetup-join-note';
      join.textContent = `Joins the group at ${journey.joinAt.name}`;
      card.append(join);
    }
    journeys.append(card);
  });
  container.append(journeys);
  if (result.stages?.length) {
    const schedule = document.createElement('ol');
    schedule.className = 'meetup-stages';
    result.stages.forEach((stage, index) => {
      const item = document.createElement('li');
      const title = document.createElement('strong');
      title.textContent = index === 0 ? `Meet first at ${stage.station.name}` : `Next, meet at ${stage.station.name}`;
      const friends = document.createElement('small');
      friends.textContent = index === 0
        ? `Friends ${stage.friends.join(', ')} start travelling together`
        : `Friend${stage.friends.length > 1 ? 's' : ''} ${stage.friends.join(', ')} join the group`;
      item.append(title, friends);
      schedule.append(item);
    });
    container.append(schedule);
  }
  if (result.onwardRoute && result.finalDestination) {
    const fullJourney = document.createElement('section');
    fullJourney.className = 'meetup-full-journey';
    const heading = document.createElement('strong');
    heading.textContent = `Full journey after the first meetup · ${Math.round(result.onwardRoute.estimatedTimeMinutes)} min`;
    const stops = document.createElement('p');
    stops.textContent = result.onwardRoute.stations.map((station) => station.name).join(' → ');
    fullJourney.append(heading, stops);
    container.append(fullJourney);
  }
  if (result.finalLeg) {
    const onward = document.createElement('p');
    onward.className = 'meetup-onward';
    onward.textContent = `After everyone has joined: ${result.finalLeg.stations.map((station) => station.name).join(' → ')} · ${Math.round(result.finalLeg.estimatedTimeMinutes)} min remaining to ${result.finalDestination.name}`;
    container.append(onward);
  } else if (result.onwardRoute) {
    const onward = document.createElement('p');
    onward.className = 'meetup-onward';
    onward.textContent = `Together: ${result.onwardRoute.stations.map((station) => station.name).join(' → ')} · ${Math.round(result.onwardRoute.estimatedTimeMinutes)} min`;
    container.append(onward);
  }
  container.classList.remove('hidden');
  renderMeetupMap(result);
}

async function findMeetup() {
  const inputs = [...$('friend-stations').querySelectorAll('[data-friend-station]')];
  const origins = inputs.map(selectedFriendStation);
  if (origins.length < 2 || origins.some((station) => !station)) {
    showToast('Choose a station for every friend from the suggestions.');
    return;
  }
  const destinationText = $('meetup-destination').value.trim();
  const destination = destinationText ? selectedStation('meetup-destination') : null;
  if (destinationText && !destination) {
    showToast('Choose the shared destination from the station suggestions.');
    return;
  }
  const button = $('find-meetup');
  button.disabled = true;
  button.querySelector('span:first-child').textContent = 'Finding the best meetup…';
  try {
    const query = new URLSearchParams();
    origins.forEach((station) => query.append('from', station.id));
    if (destination) query.set('to', destination.id);
    const result = await api(`/api/meetup?${query}`);
    if (!result.ok) {
      showToast('No station can be reached by everyone in this network snapshot.');
      return;
    }
    renderMeetupResults(result);
  } catch (error) {
    showToast(error.message);
  } finally {
    button.disabled = false;
    button.querySelector('span:first-child').textContent = 'Find meetup station';
  }
}

async function findRoute() {
  const origin = selectedStation('origin');
  const destination = selectedStation('destination');
  if (!origin || !destination) {
    showToast('Choose a starting and ending station from the suggestions.');
    return;
  }
  setLoading(true);
  try {
    const query = new URLSearchParams({ from: origin.id, to: destination.id, mode: selectedMode });
    const route = await api(`/api/route?${query}`);
    if (!route.ok) {
      $('route-results').classList.add('hidden');
      routeLayer?.clearLayers();
      stationLayer?.clearLayers();
      currentRouteBounds = null;
      $('map-empty').classList.remove('hidden');
      renderLegend([]);
      showToast('No route is present between these stations in this network snapshot.');
      return;
    }
    renderRoute(route, origin, destination);
  } catch (error) {
    showToast(error.message);
  } finally {
    setLoading(false);
  }
}

function start() {
  initModes();
  $('find-route').addEventListener('click', findRoute);
  $('single-trip-tab').addEventListener('click', () => showPlanner('single'));
  $('meetup-tab').addEventListener('click', () => showPlanner('meetup'));
  $('add-friend').addEventListener('click', addFriendRow);
  $('find-meetup').addEventListener('click', findMeetup);
  $('swap-stations').addEventListener('click', () => {
    const origin = $('origin').value;
    $('origin').value = $('destination').value;
    $('destination').value = origin;
  });
  $('reset-map').addEventListener('click', () => {
    if (map && currentRouteBounds?.isValid()) map.fitBounds(currentRouteBounds.pad(.26), { maxZoom: 13 });
    else if (map) map.setView([28.635, 77.22], 11);
  });
  Promise.all([api('/api/stations'), api('/api/platforms').catch(() => [])]).then(([stationData, platformData]) => {
    stations = stationData;
    stationPlatforms = platformData;
    initStationOptions();
    initMap();
  }).catch((error) => {
    showToast(error.message);
    $('station-count').textContent = '—';
  });
}

window.addEventListener('DOMContentLoaded', start);
