# OS‑Shell

A lightweight Unix‑like command‑line shell written in C. It provides interactive command execution, job control, user authentication with role‑based permissions, and a simple remote‑shell client (`rsh`).

## Features
- Built‑in commands: `cd`, `pwd`, `history`, `jobs`, `fg`, `exit`.
- Background job management (up to a configurable limit).
- Role‑based access control (admin / user / guest).
- Remote shell server (`--server`) and client (`rsh`).
- Simple history logging.

## Build
```bash
make            # compiles `oshell` with -Wall -Wextra -Iinclude -pthread
```

## Run
- Interactive shell: `./oshell`
- Server mode: `./oshell --server`
- Remote client: `./oshell rsh <host> <port> "<command>"`

## Usage Example
```bash
$ ./oshell
Login: user1
Password: 123
[user1@oshell> ] ls -l > out.txt
[user1@oshell> ] cat < out.txt
[user1@oshell> ] exit
```

## License
MIT – see LICENSE file.

