# Simple Shell

A lightweight, interactive command-line shell written in C. This shell supports basic command execution, piping (`|`), background processes (`&`), command history, and common built-in commands like `cd`, `pwd`, `history`, and `exit`.

## Features

- **Interactive Prompt**: Displays username and current directory (e.g., `nikita Documents> `).
- **Command Parsing**: Properly tokenizes input, handling single (`'`) and double (`"`) quotes to preserve spaces within arguments.
- **Built-in Commands**:
  - `exit`: Exit the shell.
  - `history`: Show the last 10 commands with IDs.
  - `pwd`: Print the current working directory.
  - `cd [path]`: Change directory (supports `cd` to home, `cd -` to previous, or `cd <path>`).
- **External Commands**: Runs any executable via `execvp`.
- **Piping**: Supports multi-stage pipelines (e.g., `ls | grep '.c'`).
- **Background Execution**: Run commands in the background (e.g., `sleep 10 &`).
- **Signal Handling**: Ctrl+C (SIGINT) returns to the prompt; background children are reaped automatically.
- **Command History**: Circular buffer storing up to 10 recent commands.

## Compilation

```bash
gcc -o myshell shell.c -ledit
```

## Usage

Run the shell:

```bash
./myshell
```

Enter commands at the prompt. Type `exit` to quit.

### Examples

- List files: `ls -l`
- Grep with quotes: `ls | grep ".c"`
- Change directory: `cd ..`
- Previous directory: `cd -`
- Background sleep: `sleep 5 &`
- History: `history`

## Limitations

- No support for input/output redirection (`>`, `<`, `>>`).
- No variable expansion (`$VAR`), globbing (`*`), or job control (`fg`, `bg`, `jobs`).
- Quoted strings do not support escaping (e.g., `\"`).
- Basic pipe validation: rejects invalid syntax (e.g., starting/ending with `|`, consecutive `|`).
