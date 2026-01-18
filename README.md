# Custom C Shell

A Unix-like shell implemented in C, inspired by POSIX sh and Bash

## Features

### Command Execution

- Executes external programs using `fork()` and `execvp()`
- Foreground execution waits for completion

### Background Execution

- Commands ending with `&` run in the background
- Background processes are automatically reaped

Example:

```sh
sleep 5 &
```

### Pipelines

- Supports pipelines using `|`
- Each pipeline stage runs in its own process
- Pipes connected using `pipe()` and `dup2()`

Example:

```sh
ls -la | grep .c | wc -l
```

---

## Built-in Commands

- `exit` – Exit the shell
- `history` – Show command history
- `pwd` – Print current working directory
- `cd` – Change directory

### cd Behavior

```sh
cd        # go to $HOME
cd <path> # go to path
cd -      # go to $OLDPWD
```