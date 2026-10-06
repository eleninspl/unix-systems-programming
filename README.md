# UNIX Systems Programming in C

Four small C programs for Linux that work directly with the kernel's process, signal, pipe and socket interfaces. Each one builds on the previous: creating a process, supervising a group of processes with signals, distributing work to them over pipes, and finally talking to a remote server over TCP.

These are my solutions to the lab assignments of **Operating Systems** (Λειτουργικά Συστήματα), a 6th-semester course at the School of Electrical and Computer Engineering, National Technical University of Athens (ECE NTUA), academic year 2023–24 (Flow Y, Prof. Panayiotis Tsanakas).

| Lab | Program | Topic | Main system calls |
|-----|---------|-------|-------------------|
| [1](#lab-1-processes-and-file-io) | `lab1/lab1.c` | Process creation and file I/O | `fork`, `wait`, `open`, `write`, `stat` |
| [2](#lab-2-signals-the-gates-are-open) | `lab2/parent.c`, `lab2/child.c` | Signals and process supervision | `sigaction`, `kill`, `alarm`, `waitpid`, `execv` |
| [3](#lab-3-pipes-and-io-multiplexing) | `lab3/lab3.c` | Inter-process communication | `pipe`, `poll`, `read`, `write` |
| [4](#lab-4-tcp-client) | `lab4/lab4.c` | Network programming | `socket`, `connect`, `select`, `gethostbyname` |

## Getting started

### Prerequisites

- Linux (the assignments target Linux; other POSIX systems mostly work, see [Known limitations](#known-limitations))
- `gcc` and `make`

```bash
git clone https://github.com/eleninspl/unix-systems-programming.git
cd unix-systems-programming
```

Each lab is built and run from its own folder, as shown below.

## Lab 1: Processes and file I/O

**Task.** The program takes one argument, a file name. The parent process forks a child, and each process writes one line to that file with its own PID and its parent's PID. The file must be written with the `write` system call.

- If the file already exists, print an error and exit with code 1.
- With no arguments or more than one, print a usage message and exit with code 1.
- With `--help`, print the usage message and exit with code 0.

**How it works.** The program checks that the file does not exist with `stat`, then calls `fork`. Both processes open the file with `O_APPEND`. The parent waits for the child to finish before writing its own line, so the child's line always comes first.

```bash
cd lab1
gcc -Wall lab1.c
./a.out output.txt
cat output.txt
```

```
[CHILD] getpid()=78206, getppid()=78204
[PARENT] getpid()=78204, getppid()=78129
```

## Lab 2: Signals ("The gates are open")

**Task.** The parent takes a string of `t` and `f` characters, for example `tfttt`. Each character is a gate: `t` means open and `f` means closed. The parent creates one child process per gate, and the child runs a separate program loaded with `execv`. The user controls the processes by sending signals from another terminal:

| Signal | Sent to a child | Sent to the parent |
|--------|-----------------|--------------------|
| `SIGUSR1` | Print the gate's state and seconds since start | Forward `SIGUSR1` to every child |
| `SIGUSR2` | Flip the gate (open ↔ closed) | |
| `SIGTERM` | Exit | Terminate every child, wait for each one, then exit |

Each child also prints its state every 15 seconds. The parent must always keep N children alive: if a child dies, the parent reaps it and starts a replacement. If a child is stopped, the parent resumes it.

**How it works.** All handlers are installed with `sigaction`. A child sets up its 15-second timer with `alarm` and re-arms it inside the `SIGALRM` handler. The parent's `SIGCHLD` handler calls `waitpid` with `WUNTRACED`. An exited child is replaced with a new one that has the gate's original state. A stopped child gets `SIGCONT`.

```bash
cd lab2
make
./parent ft
```

In a second terminal, send signals with `kill -SIGUSR2 <pid>`, `kill -SIGTERM <pid>`, and so on. Run `./parent` from the `lab2` folder, because it starts the children as `./child`.

```
[PARENT/PID=78210] Created child 0 (PID=78214) and initial state 'f'
[PARENT/PID=78210] Created child 1 (PID=78215) and initial state 't'
[ID=0/PID=78214/TIME=0s] The gates are closed!
[ID=1/PID=78215/TIME=0s] The gates are open!
$ kill -SIGUSR2 78214
[ID=0/PID=78214/TIME=0s] The gates are open!
$ kill -SIGTERM 78214
[PARENT/PID=78210] Child 0 with PID=78214 exited
[PARENT/PID=78210] Created new child for gate 0 (PID 78253) and initial state 'f'
[ID=0/PID=78253/TIME=0s] The gates are closed!
$ kill -SIGTERM 78210
[PARENT/PID=78210] Waiting for 2 children to exit
[PARENT/PID=78210] Child with PID=78253 terminated successfully with exit status code 0!
[PARENT/PID=78210] Waiting for 1 children to exit
[PARENT/PID=78210] Child with PID=78215 terminated successfully with exit status code 0!
[PARENT/PID=78210] All children exited, terminating as well
```

## Lab 3: Pipes and I/O multiplexing

**Task.** The parent creates `n` child processes and hands them jobs typed by the user. Jobs are assigned either `--round-robin` (the default) or `--random`. The parent accepts these commands on standard input:

- `help`: print `Type a number to send job to a child!`
- `exit`: terminate every child, then exit
- an integer: pick a child and send it the number

A child takes the number, decrements it, waits 10 seconds to simulate work, and sends the result back. The parent must keep accepting commands while children are busy.

**How it works.** Each child has two pipes: one from the parent to the child and one back. The parent uses `poll` to watch standard input and every return pipe at the same time, so it never blocks waiting on one child. Numbers travel through the pipes as raw `int`s.

```bash
cd lab3
gcc -Wall lab3.c -o ask3
./ask3 2 --round-robin
```

```
help
Type a number to send job to a child!
12
[Parent] [81457] Assigned 12 to child 0.
[Child 0] [81460] Child received 12!
7
[Parent] [81457] Assigned 7 to child 1.
[Child 1] [81461] Child received 7!
[Child 0] [81460] Child finished hard work, writing back 11.
[Parent] [81457] Recieved 11 from child 0.
[Child 1] [81461] Child finished hard work, writing back 6.
[Parent] [81457] Recieved 6 from child 1.
exit
[Parent] [81457] Child 0 with PID=81460 terminated successfully!
[Parent] [81457] Child 1 with PID=81461 terminated successfully!
[Parent] [81457] All children exited, terminating as well.
```

## Lab 4: TCP client

**Task.** Write a client for a course server that reports readings from an IoT sensor and handles requests for a permit to leave home during a quarantine, modelled on the COVID-19 lockdowns. The program accepts the following options:

```
./ask4 [--host HOST] [--port PORT] [--debug]
```

The defaults are `os4.iot.dslab.ds.open-cloud.xyz` and port `20241`. With `--debug`, every message sent or received is printed. The client accepts these commands:

| Command | Effect |
|---------|--------|
| `get` | Fetch the latest sensor event. The server replies `X YYY ZZZZ WWWWWWWWWW`: event type (0 boot, 1 setup, 2 interval, 3 button, 4 motion), light level, temperature × 100, and a UNIX timestamp. |
| `N name surname reason` | Request a permit. The server replies with a verification code, which the user types back to receive an `ACK`. |
| `help` | Print the available commands |
| `exit` | Close the connection and exit |

**How it works.** The client resolves the host name, connects a TCP socket, and then uses `select` to wait on standard input and the socket together. Lines typed by the user go to the server unchanged. Each server reply is identified by its shape: sensor data, a verification code, `ACK`, `try again` or `invalid code`. Sensor data is printed in a readable form.

```bash
cd lab4
gcc -Wall lab4.c -o ask4
./ask4 --debug
```

```
Connected!
get
[DEBUG] sent 'get'
[DEBUG] read '2 058 2950 1589989296'
---------------------------
Latest event:
interval (2)
Temperature is: 29.50
Light level is: 58
Timestamp is: Wed May 20 18:41:36 2020
---------------------------
1 jane doe groceries
[DEBUG] sent '1 jane doe groceries'
[DEBUG] read '5fdd09689ffe'
Send verification code : '5fdd09689ffe'
5fdd09689ffe
[DEBUG] sent '5fdd09689ffe'
[DEBUG] read 'ACK 1 jane doe groceries'
Response: 'ACK 1 jane doe groceries'
```

The course server is no longer online. To try the client, point `--host` and `--port` at any TCP server that speaks the same line-based protocol.

## Known limitations

The code is kept as it was submitted, apart from two one-line fixes: a missing `#include <sys/wait.h>` in lab 1, which newer GCC versions reject, and an uninitialised `getline` buffer in lab 3, which crashed on macOS. While writing this README, I reviewed the code again and found the issues below. None of them affect the normal runs shown above, but they are worth knowing if you build on this code:

- **Lab 1**'s `write` error check compares a signed result with an unsigned length, so a failed write (`-1`) is not detected.
- **Labs 2 and 3** check the arguments before checking for `--help`, so `--help` prints the usage message but exits with code 1 instead of 0.
- **Labs 3 and 4** use `\033[30m` as the "reset colour" code. That code sets the text to black, so on a dark terminal everything printed after a coloured message can be hard to read. The real reset code is `\033[0m`.
- **Lab 3** divides by zero when started with `./ask3 0`. If standard input closes (Ctrl-D), the parent exits without terminating its children.
- **Lab 2** replaces a child only if it exited normally. A child killed by a signal, for example `kill -9`, is not replaced. The handlers also call `printf`, which is not async-signal-safe. This is common in coursework, but production code would only set a flag inside the handler.
- **Lab 4** copies `--host` into a buffer sized for the default host name. A longer name is truncated and left without a terminating NUL. The client also assumes that each `read` from the socket returns exactly one complete line.

## For students taking the course

This repository is here to help you understand the material: how the system calls fit together and what the expected behaviour looks like. Write your own solutions. The assignments are graded through oral examination, and code you did not write will not get you through it.

The `man` pages are the best reference for every call used here, for example `man 2 fork` and `man 7 signal`.
