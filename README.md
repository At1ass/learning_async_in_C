# async

A simple educational async runtime implemented in C.

This project demonstrates how to build an asynchronous task executor from scratch using a worker thread pool and a future-based system. It is written entirely in C with POSIX threads and is intended **for educational purposes only**.

---

## Features

- Worker thread pool (autodetected based on CPU cores)
- Future-based asynchronous task API
- Callback support with `future_then()`
- Manual polling via `future_get()` or automatic via `async_run()`
- Event-loop-style task processing
- Static (`.a`) and shared (`.so`) library builds
- Clean, minimal API via `async.h`

---

## Project structure

```
async/
├── include/         # Public API header (async.h)
├── src/             # Library implementation
├── example/         # Example usage
├── build/           # Compiled objects (auto-created)
├── Makefile
```

---

## Build

To build the static and shared libraries along with the example:

```bash
make
```

To run the example:

```bash
LD_LIBRARY_PATH=. ./example/example
```

To clean the project:

```bash
make clean
```

---

## Install (optional)

```bash
sudo cp libasync.so /usr/local/lib/
sudo cp include/async.h /usr/local/include/
sudo ldconfig
```

---

## Disclaimer

This is an **educational project** to explore how asynchronous systems work in C.
It is not production-ready and lacks proper error handling, timeout support, and thread safety guarantees for concurrent usage of futures.  
Use it as a learning tool or starting point for deeper exploration.

---

## License

MIT
