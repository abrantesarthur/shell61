# Shell

This is shell61, a shell I built as part of a class. I wrote everything on `sh61.c` (i.e., the shell implementation) and `sh61.h` (i.e., token definitions)

## Make targets

`make sh61` builds the shell's executable `sh61`

`make check` runs tests

## Supported Commands

After building the `sh61` executable by running `make sh61`, execute it and play around with some of the commands below.

- `echo`, `sleep`, `grep`, `wc`, `sort`, `cat`, `true`, `false`, etc.
- background command `&`
  - `sleep 1 & echo hello` immediately prints hello, since `sleep 1` runs in the background
- commmand lists `;`
  - `echo hello ; sleep 1 ; echo world` prints hello, waits one second, and prints world
- conditionals `&&` and `||`
  - `true && echo true` prints true
  - `false && echo false` does not print false
  - `true || echo true` prints nothing
  - `false || echo false` prints false
- pipelines `|`
  - `echo Bad | grep -c B` prints 1
- redirections `>`, `<`, and `2>`
  - `grep sleep < check.pl` outputs all lines containing `sleep` in the `check.pl` file

## Source Files

| File        | Description                     |
| ----------- | ------------------------------- |
| `helpers.c` | helper functions (e.g., parser) |
| `sh61.c`    | shell implementation            |
| `sh61.h`    | token definitions               |
