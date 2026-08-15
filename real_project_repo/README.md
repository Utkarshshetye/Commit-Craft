# Commit-Craft: Distributed Version Control System

A custom-built distributed version control system implemented in C,
featuring Git-compatible object storage, zero-copy file transfer via
sendfile(), and a multi-threaded server architecture.

## Architecture
- **Client.c**: Terminal interface, sends commands via IPC message queue
- **ProxyServer.c**: Middleware that handles local git operations (libgit2)
  and pushes files to Main_Server over TCP sockets
- **Main_Server.c**: Remote server that receives files, computes SHA-1 hashes,
  and stores them in .git/objects/ using Git's content-addressable storage

## Features
- Zero-copy network transfer using Linux sendfile() syscall
- SHA-1 content-addressable object storage
- Multi-threaded server with thread pool (configurable MAX_THREADS)
- Redis-backed command routing
- Supports files up to 1GB+ transfers
