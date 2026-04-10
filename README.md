# io_uring TCP Echo Server (Mojo + C)

> Minimal high-performance TCP server using io_uring with Mojo FFI.

- Linux `io_uring`
- C (liburing)
- Mojo (FFI)

## Features

- Async TCP server using `io_uring`
- Mojo ↔ C FFI integration
- Simple echo protocol
- Minimal architecture

## Build

```bash
pixi run build
```

## Run

```bash
./build/server 8080
```

## Test

```bash
nc 127.0.0.1 8080
```

Type something:

```text
hello
```

You will receive:

```text
hello
```

## Architecture

```
Mojo (entrypoint)
        ↓
FFI (external_call)
        ↓
C (liburing)
        ↓
Linux io_uring
```

## Why?

This project demonstrates:

- how to integrate Mojo with native C
- how to use io_uring for networking
- how to build high-performance systems with minimal layers

## Project Structure

```
src/
 ├── server.c      # io_uring TCP server (C)
 ├── server.mojo   # Mojo entrypoint
pixi.toml
README.md
```

## Next Steps

- multishot accept
- buffer pools
- partial write handling
- Mojo-side request processing

## LICENCE

**MIT
