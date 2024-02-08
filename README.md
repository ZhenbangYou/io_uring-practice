# `io_uring`-practice
I enjoy performance engineering.

After playing with compute-bound and memory-bound programs for years, I wanted to have hands-on experiences I/O-bound programs.

After playing with modern asynchonous programming framework (e.g., Rust `tokio`, Kotlin coroutines, Python `asyncio`), I wanted to dive into lower level.

After playing with `epoll` and non-blocking sockets (thanks to John, I used these to build a networking module for Raft), I felt very strongly that I was desired to touch something more modern and more performant.

Thus, here it goes: my journey with `io_uring`.

## Usage
```
g++ -o {name} {name}.cpp -luring -std=c++20
```
`clang++` also works.

## Requirement
- C++ 11 or later
- liburing
- Linux at least 5.1 or later (not sure about the earliest version that is supported)
  - My machine is Linux 5.15 with liburing 2.1 (installed by `apt`)
  - Be careful about compatibility when installing `liburing` from source (e.g., I tried to install `liburing` 2.2 and later on Ubuntu 22.04 LTS, and got runtime error)
  - In Ubuntu 22.04 LTS, `liburing` can be installed via `apt` (`sudo apt install liburing-dev`), but this doesn't apply to 20.04 LTS.
