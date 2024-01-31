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
- C++20 or later
- liburing
- Linux at least 5.1 or later (not sure about the earliest version that is supported)
