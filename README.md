
# Multithreaded TCP Server in C++

A concurrent TCP server built from scratch in C++ using POSIX sockets, a fixed-size thread pool, a bounded task queue, and synchronization primitives.

## Features

- TCP client-server communication using POSIX sockets.
- Fixed-size thread pool with 3 worker threads.
- Thread-safe task queue using `mutex`.
- `condition_variable` for worker coordination.
- Atomic or synchronized client ID assignment.
- Bounded queue with a capacity of 5 pending clients.
- Overload handling with a server-busy response.
- Newline-delimited TCP message framing.
- Graceful shutdown on Ctrl+C.
- Multiple concurrent client connections.

## Architecture

1. The main server thread listens for incoming TCP connections.
2. Each accepted client is assigned a unique ID.
3. The client socket is added to a bounded task queue.
4. Worker threads retrieve client tasks from the queue.
5. Workers handle client communication independently.
6. If the queue is full, the server rejects the new client.
7. On shutdown, the server closes queued connections, interrupts active connections, and joins worker threads.

## Tech Stack

- C++
- POSIX Sockets
- TCP/IP
- STL Threads
- Mutex
- Condition Variables
- Queue
- Python (load testing)

## Project Structure

Multith_tcpServer/
├── src/
│   ├── server.cpp
│   └── client.cpp
├── load_test.py
└── README.md

## Build and Run

Compile the server:

```bash
g++ src/server.cpp -o server -pthread
```

Compile the client:

```bash
g++ src/client.cpp -o client -pthread
```

Start the server:

```bash
./server
```

Open another terminal and run:

```bash
./client
```

## Load Testing

Run the Python load tester:

```bash
python3 load_test.py
```

The benchmark simulates different numbers of concurrent clients, with each client sending 20 messages.

### Preliminary Benchmark Results

| Concurrent Clients | Successful Responses | Failed |
|---:|---:|---:|
| 1 | 20 | 0 |
| 3 | 60 | 0 |
| 5 | 100 | 0 |
| 8 | 160 | 0 |
| 10 | 160 | 2 |

Maximum observed throughput in this test: approximately 15,268 messages/second.

**Note:** These are preliminary localhost measurements. Results depend on the machine, scheduling, and test conditions.

## Current Limitations

- Worker threads block while waiting for client messages.
- The server supports a limited number of active client handlers.
- Newline framing does not support unescaped newlines inside messages.
- No authentication or encryption.
- No persistent application-level message storage.

## Future Improvements

- Non-blocking sockets and I/O multiplexing using `epoll`.
- Better connection management and timeouts.
- More robust error handling.
- Performance benchmarking under higher loads.
- Logging and monitoring.
