# chttp

A high-performance, multi-threaded HTTP/1.1 web server built as part of a [self-directed C curriculum](https://github.com/WillEdgington/c-curriculum).

---

## Build

Requires `gcc` and `make`. Once the repository is cloned from the remote, you can run these build commands:

```bash
make setup_deps # Clone the dependency repos
make            # Compiles the executable chttp binaries
```

**dev builds:**

```bash
make test       # Compiles and runs unit tests
make debug      # Builds with debug symbols and sanitisers
```

---

## Project Structure

```
include/chttp/    # Public interface definitions
├── config.h      # Custom TOML server configuration parser
├── err.h         # Custom server error page template parser
├── fs.h          # File system boundary definitions
├── handler.h     # Routing and state-machine loop
├── http.h        # HTTP parser and string processing
├── logger.h      # Thread-safe format logging (CLI or .log file)
├── server.h      # Network listener and socket setup
└── tpool.h       # Multi-thread worker pool abstraction
src/              # Core implementation (.c files)
tests/            # Per-module unit tests (functional verification)
templates/        # Default template assets (e.g., err.html.srv)
public/           # Default root directory for static serving assets
server.toml       # Default server configuration file
```

---

## How To Run

Once [compiled](#build), you can start the server engine by executing the compiled binary:

```bash
./chttp
```

By default, the executable initialises the network layer using the values defined within [`server.toml`](#configurations-servertoml) and exposes assets hosted inside the `./public` directory.

#### Core Routing Rules:

- **Static File Resolution:** The server operates natively on standard paths. Any file request pointing to a valid relative path within your public root (e.g., `/styles.css` or `/image/logo.png`) will be safely read from disk and streamed back to the client.
- **Directory Indexing:** Any request path ending with a trailing slash (`/`) or missing an explicit file suffix fall back to serving `/index.html` from that target directory. For example a request path of `/home` or `/home/`, the server will try respond with the file at the relative path `/home/index.html` from the public directory.
- **Method Constraints:** To remain unopinionated and free of complex external database/state dependencies, the server engine only fulfills `GET` requests. Any incoming mutating method requests from the client (such as `POST`, `PUT`, `DELETE`) are responded to by the server with a `405 Method Not Allowed` error.

### Configurations (`server.toml`)

The execution layer is fully configurable via a `server.toml` file at the root of the project. This lets you have control over threading, logging, connection rules and general server parameters without recompiling or bloating the run command.

|TOML Key|Header|Default|Operational Bound|Purpose|
|---|---|---|---|---|
|`port`|`[server]`|`9000`|Integer, `1` to `65535`|TCP port the server binds to.|
|`public_dir`|`[server]`|`./public`|Valid path relative to root|Directory where all static serving assets are read.|
|`listen_backlog`|`[server]`|`10`|Integer, `1` to `4096`|Maximum size of kernel-level queue holding established sockets awaiting acceptance.|
|`log_level`|`[server]`|`DEBUG`|`DEBUG`, `INFO`, `WARN`, `ERROR`|Server logging has an established hierarchy (`DEBUG` -> `INFO` -> `WARN` -> `ERROR`). (see [logging](#logging-cli-or-file-output))|
|`log_file`|`[server]`|Not declared|Valid file path relative to root|If `log_file` is given (recommend `.log` suffix), all logs write to that file. (see [logging](#logging-cli-or-file-output))|
|`thread_count`|`[server]`|`4`|Integer, > `0`|Total persistent worker threads|
|`worker_arena_size`|`[server]`|`8192` (8 KB)|Integer, > `0` (Recommend atleast >= 4096)|Slab size for each worker thread's internal arena allocator.|
|`default_keep_alive`|`[server]`|`false`|`true` (or `1`), `false` (or `0`)|Used when client omits explicit `Connection` header from request.|
|`connection_timeout`|`[server]`|`5` (seconds)|Integer, >= 0 (Recommend `0` to `60`)|Maximum duration a worker thread will block on waiting for subsequent request from `keep-alive` connection.|

#### Example `server.toml`:
```TOML
[server]
# Basic server configs
port = 9000
public_dir = ./public
listen_backlog = 10

# logger configs
log_level = DEBUG
# log_file = ./server.log

# Thread pool configs
thread_count = 4
worker_arena_size = 8192 # 8 KB per worker

# keep-alive configs
default_keep_alive = false
connection_timeout = 5
```

All configurations must be underneath the correct header (`[...]`) to be visible to the parser. This decision was made for the benefit of having an organised and scalable design. As you can see, Currently, everything is under the `[server]` header. This is the only header currently recognised.

Configurations are prioritised by "last-in", so if you have a duplicate key or header that is the same as one already parsed then that will overwrite the current configurations.

You can comment in `server.toml` by the use of the `#` or `;` characters prepended on the same line as the comment. These characters tell the parser to ignore what comes after it on that line.

### Error Handling (`templates/err.html.srv`)

When an HTTP error state is triggered (such as `404 Not Found` or `500 Internal Server Error`), the server avoids responding wiht hardcoded inline text. Instead, `err.c` parses a dynamic template file located at `templates/err.html.srv`.

The parsing engine scans the template file and substitutes runtime token namespaces on the fly:
- `{status_code}`: Replaced with the 3-digit numerical HTTP error status code (e.g. `404`).
- `{status_message}`: Replaced with the formal HTTP text description string (e.g. `Not Found`).

> **Note:** The `templates/` directory is isolated and inaccessible to external HTTP clients by default. If your custom error template references external design assets (like a dedicated CSS stylesheet, a favicon, or branding image), those assets **must reside insid the `public/` directory** (or the specified `public_dir` in `server.toml`). Ensure all resource links inside the template point to public paths, not relative template files.

#### Example `templates/err.html.srv`:
```HTML
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{status_code}</title>
</head>
<body>
  <h1>{status_code} {status_message}</h1>
</body>
</html>
```

If `templates/err.html.srv` cannot be resolved by the server `err.c` will fall back to a default error template in `err.c`.

### Logging (CLI or File Output)

The logging subsystem (`logger.c`) provides a thread-safe, synchronised logging pipeline protected by an internal global mutex lock. it dynamically routes data to either standard output (`stdout`) or appends directly to a dedicated file stream using the `"ae"` (append, close-on-exec) flag based on the `log-file` parameter inside `server.toml`.

The engine separates records into two distinct layout profiles:

#### 1. System & Diagnostic Logs (`chttp_log_write`):

Tracks server state changes, configuration loading parameters, and runtime warnings. These logs enforce the defined `log_level` hierarchy configuration (`DEBUG`, `INFO`, `WARN`, `ERROR`), completely dropping entries below the defined minimum threshold.
- **Format:** `YYYY-MM-DD HH:MM:SS [LEVEL] [source_file:line_number]: message`
- **Example:** `2026-06-01 12:44:13 [INFO] [src/server.c:159]: Thread pool engine spawned with 4 active workers.`

#### 2. HTTP Operational Traffic Logs (`chttp_log_http`):

Independently captures processing outputs using a standardised NCSA Common variant. This stream ignores the standard log levels to guarantee that every inbound transaction is reliably audited.

- **Format:** `client_ip - - [DD/Mmm/YYYY:HH:MM:SS +offset] "request_line" status_code response_size`
- **Example:** `127.0.0.1 - - [01/Jun/2026:12:50:49 +0100] "GET /index.html HTTP/1.1" 200 447`

Every log statement executes an explicit `fflush()` immediately before releasing the mutex to guarantee real-time data persistence and protect against trace buffer drops during unhandled failures.

---

## Project Architecture & Components

The core design of `chttp` focuses on maintaining absolute control over memory structures and thread interactions through clean decoupling.

### Multi-Threaded Engine & Queue Lifecycle (`server.c`/`tpool.c`)

The network backbone uses a fixed **Thread-per-Connection Pool** architecture to decouple client handling from socket listening:
- **The Main Thread:** Runs an infinite execution loop dedicated solely to accepting new inbound connections at the kernel level.
- **The Shared Queue:** Upon acceptance, the main thread pushes the client socket descriptor into a thread-safe FIFO task queue and signals the pool using a condition variable.
- **Worker Threads:** Persistent worker threads pull connections from the queue to manage the entire lifecycle of the request-response transaction, leaving the main thread free to handle subsequent handshakes immediately.

### Guarding Against Resource Starvation (`server.c`)

Because threads run synchronously, an idle client could easily block a worker thread indefinitely. To prevent this, `server.c` enforces socket-level timeouts. If a connection is marked as `keep-alive` but remains silent, the kernel forces the block to drop, allowing the worker thread to break out of its loop and return to the shared pool queue.

### Memory Isolation & Request Parsing (`http.c`)

The server minimises the tracking overhead of individual object lifecycles by isolating each thread's workspace using a **`clib` Arena Allocator**:
- **Arena Alignment:** Every worker thread executes its processing pipeline inside a pre-allocated memory chunk. Inbound request data and response headers are parsed linearly and built straight into this space.
- **Deterministic Cleanup:** After the serialised response string is successfully flushed down the client socket, the engine issues a single call to reset the thread's arena offset back to zero. This reclaims all request-scoped allocations instantly, achieving zero long-term heap fragmentation and bypassing individual heap destruction routines.

### Path Sandboxing & Routing (`fs.c` / `handler.c`)

The pipeline securely translates virtual request strings into local file resources while defending the host filesystem from malicious actions:
- **Directory Sandboxing:** The filesystem layer validates all requests against the absolute path of the designated public asset directory. Any request containing path traversal sequences designed to step outside this directory tree boundary is actively caught and blocked.
- **Method Routing:** To avoid heavy string-comparison logic during routing, the request dispatcher maps the request's method index straight to a constant array of function pointers. Unsupported request methods are automatically routed to a fallback handler that returns a standardised error profile.

---

## Author

Created by [**WillEdgington**](https://github.com/WillEdgington)

📧 [**willedge037@gmail.com**](mailto:willedge037@gmail.com) &nbsp;|&nbsp; 🔗 [**LinkedIn**](https://www.linkedin.com/in/williamedgington/)
