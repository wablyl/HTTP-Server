# Multi-Threaded HTTP Server

A concurrent HTTP server in C, built for a systems programming course. Handles GET and PUT requests across a pool of worker threads, with per-URI reader-writer locking to guarantee a correct, linearizable ordering of concurrent requests — the server behaves as if it were single-threaded from an outside observer's point of view, while actually processing multiple clients in parallel.

## Architecture

- **Thread-pool design**: a single dispatcher thread accepts incoming connections and pushes them onto a thread-safe queue; a configurable number of worker threads pop connections off the queue and handle them independently.
- **Per-URI reader-writer locking**: GET requests take a read lock, PUT requests take a write lock, both scoped to the specific URI being accessed — so unrelated requests never block each other, and conflicting requests to the same resource are correctly serialized.
- **Atomic audit log**: every processed request is logged to stderr in a fixed format (`<Method>,<URI>,<Status>,<RequestID>`), reflecting the true linearized order the server committed to.

## What's Implemented

- `GET` — serves file contents, returns 404 for missing files, 403 for permission errors
- `PUT` — creates new files (201) or overwrites existing ones (200), streams the request body directly to disk
- Concurrency built from primitives implemented from scratch in an earlier assignment: a thread-safe bounded queue (`queue.c`) and a reader-writer lock supporting configurable reader/writer/n-way fairness policies (`rwlock.c`)

## Testing

Verified against the course's official concurrency test suite (request interleaving + audit log linearization + response consistency checks): **9 of 10 test scenarios pass**, including stress tests with dozens of concurrent conflicting and non-conflicting requests.

The one remaining failing test simulates a client that deliberately splits a large request body across two sends with a pause in between. Debugging that test surfaced a genuine "short read" issue — the helper library function responsible for streaming request bodies to disk can return fewer bytes than requested in a single call, and a naive read-until-EOF retry loop doesn't correctly handle its actual return-value contract under that specific pattern. That's an open item worth revisiting with clearer documentation on the helper library's internals.

## Build

```bash
make
./httpserver [-t threads] <port>
```

Requires a POSIX environment with `pthreads`.

## Try It Yourself

```bash
git clone https://github.com/wablyl/http-server.git
cd http-server
make
./httpserver -t 4 8080
```

In a second terminal:

```bash
# GET an existing file
echo "Hello world" > hello.txt
curl http://localhost:8080/hello.txt
# → 200 OK, returns "Hello world"

# PUT a new file
curl -X PUT http://localhost:8080/newfile.txt -d "Created via PUT"
# → 201 Created

# GET a file that doesn't exist
curl http://localhost:8080/missing.txt
# → 404 Not Found
```

Every request is logged to the server's terminal in the format `<method>,<uri>,<status>,<request-id>`, e.g.:

```
GET,/hello.txt,200,0
PUT,/newfile.txt,201,0
GET,/missing.txt,404,0
```

