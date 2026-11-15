import { createServer } from 'node:http';
import { existsSync, readFileSync, statSync } from 'node:fs';
import { extname, resolve, sep } from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const projectRoot = resolve(fileURLToPath(new URL('..', import.meta.url)));
const frontendRoot = resolve(projectRoot, 'frontend');
const executable = resolve(projectRoot, 'metromitra.exe');
const port = Number(process.env.PORT || 4173);
const sources = [
  'backend/src/Graph.cpp',
  'backend/src/CsvLoader.cpp',
  'backend/src/Traversal.cpp',
  'backend/src/Dijkstra.cpp',
  'backend/src/main.cpp',
  'backend/include/Station.h',
  'backend/include/Graph.h',
  'backend/include/CsvLoader.h',
  'backend/include/Traversal.h',
  'backend/include/Dijkstra.h',
];

function ensureBackend() {
  const stale = !existsSync(executable) || sources.some((file) =>
    statSync(resolve(projectRoot, file)).mtimeMs > (existsSync(executable) ? statSync(executable).mtimeMs : 0));
  if (!stale) return;
  console.log('Building the C++ route engine...');
  const args = ['-std=c++17', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Ibackend/include', ...sources, '-o', 'metromitra.exe'];
  const build = spawnSync('g++', args, { cwd: projectRoot, encoding: 'utf8' });
  if (build.status !== 0) throw new Error(build.stderr || build.error?.message || 'g++ build failed');
  console.log('C++ route engine built.');
}

function backendJson(args) {
  const result = spawnSync(executable, args, { cwd: projectRoot, encoding: 'utf8', maxBuffer: 8 * 1024 * 1024 });
  if (result.status !== 0) throw new Error(result.stderr?.trim() || 'Route engine failed');
  return JSON.parse(result.stdout);
}

function sendJson(response, status, body) {
  response.writeHead(status, { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store' });
  response.end(JSON.stringify(body));
}

const types = { '.html': 'text/html; charset=utf-8', '.css': 'text/css; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.svg': 'image/svg+xml' };

ensureBackend();
const server = createServer((request, response) => {
  const url = new URL(request.url || '/', `http://${request.headers.host || 'localhost'}`);
  try {
    if (request.method === 'GET' && url.pathname === '/api/stations') {
      return sendJson(response, 200, backendJson(['--json-stations']));
    }
    if (request.method === 'GET' && url.pathname === '/api/network') {
      return sendJson(response, 200, backendJson(['--json-network']));
    }
    if (request.method === 'GET' && url.pathname === '/api/platforms') {
      const rows = readFileSync(resolve(projectRoot, 'backend/data/station_platforms.csv'), 'utf8')
        .trim().split(/\r?\n/).slice(1).filter(Boolean).map((row) => {
          const [stationId, stationName, line, platform, direction, source, checkedAt] = row.split(',');
          return { stationId, stationName, line, platform: Number(platform), direction, source, checkedAt };
        });
      return sendJson(response, 200, rows);
    }
    if (request.method === 'GET' && url.pathname === '/api/route') {
      const from = url.searchParams.get('from');
      const to = url.searchParams.get('to');
      const mode = url.searchParams.get('mode') || 'time';
      if (!from || !to || !['time', 'distance', 'transfers'].includes(mode)) {
        return sendJson(response, 400, { error: 'Choose a valid origin, destination, and route mode.' });
      }
      return sendJson(response, 200, backendJson(['--json-route', from, to, mode]));
    }
    if (request.method === 'GET' && url.pathname === '/api/meetup') {
      const origins = url.searchParams.getAll('from');
      const destination = url.searchParams.get('to') || '';
      if (origins.length < 2 || origins.length > 8 || origins.some((id) => !id)) {
        return sendJson(response, 400, { error: 'Choose between 2 and 8 valid friend starting stations.' });
      }
      return sendJson(response, 200, backendJson(destination
        ? ['--json-meetup', origins.join(','), destination]
        : ['--json-meetup', origins.join(',')]));
    }

    const pathname = decodeURIComponent(url.pathname === '/' ? '/index.html' : url.pathname);
    const target = resolve(frontendRoot, `.${pathname}`);
    if (target !== frontendRoot && !target.startsWith(frontendRoot + sep)) {
      response.writeHead(403); response.end('Forbidden'); return;
    }
    if (!existsSync(target) || !statSync(target).isFile()) {
      response.writeHead(404); response.end('Not found'); return;
    }
    response.writeHead(200, { 'Content-Type': types[extname(target)] || 'application/octet-stream' });
    response.end(readFileSync(target));
  } catch (error) {
    sendJson(response, 500, { error: error.message || 'Unexpected server error' });
  }
});

server.listen(port, '127.0.0.1', () => {
  console.log(`MetroMitra is running at http://127.0.0.1:${port}`);
  console.log('Press Ctrl+C to stop the local server.');
});
