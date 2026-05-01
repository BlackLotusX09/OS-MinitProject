# OS-Shell — Project Report & Test Cases

**Course:** Operating Systems Mini-Project  
**Language:** C (C11)  
**Platform:** macOS / Linux  
**Build System:** GNU Make / GCC  

---

## 1. Project Overview

OS-Shell is a custom Unix-like command-line shell written in C. It demonstrates core operating-system concepts including process management, signal handling, inter-process communication (IPC), file locking, job control, and network programming.

The shell supports two modes:

| Mode | Command | Description |
|------|---------|-------------|
| Interactive | `./oshell` | Full shell REPL with login, built-ins, job control |
| Server | `./oshell --server` | TCP server on port 9090 accepting remote commands |

---

## 2. Architecture

```
OS-Shell/
├── include/
│   ├── shell.h          # Core types, globals, function prototypes
│   ├── client.h         # RSH client header
│   └── server.h         # TCP server header
├── src/
│   ├── main.c           # Entry point, shell REPL loop
│   ├── shell.c          # Core shell logic
│   ├── client.c         # Remote shell (rsh) client
│   └── server.c         # TCP server for remote connections
├── data/
│   ├── users.txt        # User credentials (username:password:role)
│   └── history.log      # Command history (auto-generated)
├── Makefile
└── README.md
```

---

## 3. Features Implemented

### 3.1 User Authentication
- Three-attempt login with username/password
- Credentials loaded from `data/users.txt` (format: `username:password:role`)
- Roles: **admin**, **user**, **guest**
- After 3 failures the shell exits

### 3.2 Role-Based Access Control (RBAC)

| Command | Admin | User | Guest |
|---------|-------|------|-------|
| ls, pwd, echo, help, exit | ✅ | ✅ | ✅ |
| All other commands | ✅ | ✅ | ❌ |
| kill | ✅ | ❌ | ❌ |

### 3.3 Built-in Commands

| Command | Description |
|---------|-------------|
| `cd <dir>` | Change working directory |
| `pwd` | Print working directory |
| `history` | Show last 20 commands |
| `jobs` | List background jobs |
| `fg <N>` | Bring job N to foreground |
| `exit` | Exit the shell |
| `rsh <host> <port> <cmd>` | Run command on remote server |

### 3.4 External Commands & Pipes
- Any non-built-in command is `fork`+`execvp`'d
- Pipes supported: `cmd1 | cmd2 | cmd3`
- Background execution with `&`: `sleep 30 &`

### 3.5 Job Control
- Background jobs tracked in `jobs[]` array (mutex-protected)
- Semaphore (`dispatch_semaphore_t`) limits concurrent background jobs to **5**
- `fg <N>` resumes a stopped/background job in the foreground
- `Ctrl+Z` stops foreground process, adds to jobs list, reprints prompt
- `Ctrl+C` kills foreground process (or just reprints prompt if none running)

### 3.6 Signal Handling

| Signal | Behaviour |
|--------|-----------|
| SIGINT (Ctrl+C) | Kills foreground child; reprints prompt if none running |
| SIGTSTP (Ctrl+Z) | Stops foreground child; adds to jobs; reprints prompt |
| SIGCHLD | Async reap of zombie children (signal-safe, no mutex) |

### 3.7 Command History
- Every non-`history` command appended to `data/history.log`
- File-level read/write locking via `fcntl` (F_RDLCK / F_WRLCK)
- `history` shows last 20 entries

### 3.8 Remote Shell (RSH)
- Client connects to the server via TCP
- Auth phase: sends `current_user:current_password`
- Command sent after auth; server executes via `popen` and streams result back
- Server forks per-client; supports multiple simultaneous clients

---

## 4. Key OS Concepts Demonstrated

| Concept | Where Used |
|---------|-----------|
| `fork` / `execvp` | External command execution |
| `waitpid` with `WUNTRACED` | Detect stopped (Ctrl+Z) vs exited foreground job |
| `setpgid` | Each child gets its own process group |
| POSIX signals + `sigaction` | SIGINT, SIGTSTP, SIGCHLD |
| `pthread_mutex_t` | Protect `jobs[]` array from race conditions |
| `dispatch_semaphore_t` (GCD) | Limit max background jobs to 5 |
| `fcntl` file locking | Safe concurrent history writes |
| TCP sockets | Server/client remote shell |
| `popen` | Server-side command execution |
| File permissions / RBAC | Role-based command filtering |

---

## 5. Build & Run

```bash
# Build
gcc -Iinclude src/main.c src/shell.c src/server.c src/client.c -o oshell

# Run interactively
./oshell

# Run server (in a separate terminal)
./oshell --server
```

---

## 6. Test Cases

### TC-01: Valid Login — Admin
**Steps:**
1. Run `./oshell`
2. Enter `Username: admin`
3. Enter `Password: admin`

**Expected:** `Login successful as admin` — prompt shows `admin@oshell>`

---

### TC-02: Valid Login — User
**Steps:**
1. Run `./oshell`
2. Enter `Username: user1`, `Password: 123`

**Expected:** `Login successful as user1` — prompt shows `user1@oshell>`

---

### TC-03: Valid Login — Guest
**Steps:**
1. Run `./oshell`
2. Enter `Username: guest`, `Password: guest`

**Expected:** `Login successful as guest` — prompt shows `guest@oshell>`

---

### TC-04: Invalid Login (3 Attempts)
**Steps:**
1. Run `./oshell`
2. Enter wrong credentials 3 times

**Expected:** `Too many failed attempts` — shell exits

---

### TC-05: Built-in `pwd`
**Steps (any role):**
```
admin@oshell> pwd
```
**Expected:** Prints current working directory, e.g. `/Users/jaswanth/Desktop/OS-SHELL`

---

### TC-06: Built-in `cd`
**Steps:**
```
admin@oshell> cd /tmp
admin@oshell> pwd
```
**Expected:** Second command prints `/tmp`

---

### TC-07: Built-in `history`
**Steps:**
```
admin@oshell> ls
admin@oshell> pwd
admin@oshell> history
```
**Expected:** Lists recent commands (up to 20), ending with `ls` and `pwd`

---

### TC-08: External Command — `ls`
**Steps:**
```
admin@oshell> ls
```
**Expected:** Lists files in the current directory

---

### TC-09: Pipe — `ls | wc -l`
**Steps:**
```
admin@oshell> ls | wc -l
```
**Expected:** Prints the count of files/directories in the current directory

---

### TC-10: Background Job — `sleep 10 &`
**Steps:**
```
admin@oshell> sleep 10 &
```
**Expected:** Prints `[1] <pid>` immediately; shell returns to prompt; `jobs` shows `[1] Running sleep [pid]`

---

### TC-11: `jobs` — List Background Jobs
**Steps:**
```
admin@oshell> sleep 30 &
admin@oshell> sleep 40 &
admin@oshell> jobs
```
**Expected:** Lists both running jobs with their IDs and PIDs

---

### TC-12: Foreground with `fg`
**Steps:**
```
admin@oshell> sleep 30 &
admin@oshell> fg 1
```
**Expected:** Shell blocks waiting for `sleep 30` to finish (or until Ctrl+C/Z)

---

### TC-13: Semaphore — Max 5 Background Jobs
**Steps:**
```
admin@oshell> sleep 60 &   # repeat 5 times
admin@oshell> sleep 60 &   # 6th attempt
```
**Expected:** The 6th `sleep 60 &` blocks until one of the earlier jobs finishes

---

### TC-14: Ctrl+C at Prompt (No Foreground Child)
**Steps:**
1. At `admin@oshell>` with no running foreground process, press `Ctrl+C`

**Expected:** Shell prints a newline and reprints the prompt — shell does **not** exit

---

### TC-15: Ctrl+C Kills Foreground Process
**Steps:**
```
admin@oshell> sleep 100
```
Then press `Ctrl+C`

**Expected:** `sleep 100` is terminated; shell returns to `admin@oshell>`

---

### TC-16: Ctrl+Z Stops Foreground Process
**Steps:**
```
admin@oshell> sleep 100
```
Then press `Ctrl+Z`

**Expected:** Output like `[1]+ Stopped   sleep` is shown; shell returns to prompt; `jobs` lists job as stopped

---

### TC-17: Ctrl+Z at Prompt (No Foreground Child)
**Steps:**
1. At `admin@oshell>` press `Ctrl+Z`

**Expected:** Newline printed; prompt reprinted — shell does **not** suspend

---

### TC-18: RBAC — Guest Cannot Run `cat`
**Steps:**
1. Login as guest
2. Run `cat README.md`

**Expected:** `Permission denied`

---

### TC-19: RBAC — Guest Can Run `ls` and `pwd`
**Steps:**
1. Login as guest
2. Run `ls`, then `pwd`

**Expected:** Both commands succeed normally

---

### TC-20: RBAC — User Cannot Run `kill`
**Steps:**
1. Login as user1
2. Run `kill 1234`

**Expected:** `Permission denied`

---

### TC-21: Remote Shell — `rsh` Basic Command
**Pre-condition:** Server running (`./oshell --server` in another terminal)

**Steps:**
```
admin@oshell> rsh 127.0.0.1 9090 ls
```
**Expected:** Output of `ls` on the server's working directory is printed

---

### TC-22: Remote Shell — Auth Uses Logged-in User
**Steps:**
1. Login as admin
2. `rsh 127.0.0.1 9090 pwd`

**Expected:** Auth succeeds using `admin:admin`; `pwd` output is returned

---

### TC-23: Remote Shell — Guest Permission Denied on Server
**Steps:**
1. Login as guest locally
2. `rsh 127.0.0.1 9090 cat /etc/hosts`

**Expected:** Server replies `Permission denied` (role enforced server-side)

---

### TC-24: History Log Persistence
**Steps:**
1. Run a few commands
2. Exit and restart `./oshell`
3. Run `history`

**Expected:** Commands from the previous session are still shown

---

### TC-25: `exit` — Graceful Shell Exit
**Steps:**
```
admin@oshell> exit
```
**Expected:** Shell exits cleanly, returning to the terminal prompt

---

## 7. Known Limitations

1. **I/O Redirection** (`>`, `<`) is not implemented.
2. **`kill` built-in** is not implemented (only blocked at the RBAC layer).
3. The server uses `popen` which spawns a `/bin/sh` subprocess — not the oshell itself.
4. History is plain-text; not bounded (grows unbounded over time).
5. `fg` does not restore the terminal foreground process group properly on macOS.

---

## 8. Files Modified / Created During the Project

| File | Status | Notes |
|------|--------|-------|
| `src/main.c` | Modified | Added semaphore init, `getcwd`, signal setup |
| `src/shell.c` | Modified | All core logic: login, RBAC, job control, signals |
| `src/client.c` | Modified | Uses `current_user`/`current_password` for auth |
| `src/server.c` | Existing | TCP server, per-client fork, role-based command filtering |
| `include/shell.h` | Modified | Added externs, `dispatch_semaphore_t`, `MAX_BG_JOBS` |
| `data/users.txt` | Fixed | Changed role `users` → `user` for correct RBAC mapping |
| `src/Makefile` | Replaced | Fixed missing-separator error; proper build rules |
| `README.md` | Updated | Full project description |

---

*Report generated: April 2026*
