# yasvi - Yet Another Simple VI editor

## Project Overview

yasvi is a minimalistic vi/vim-like text editor written in C, designed as a lightweight terminal-based editor. It uses ncurses for terminal handling and provides essential vi-style editing capabilities including modal editing, command mode, and syntax highlighting for C files.

The project is licensed under GNU General Public License v3.0 (GPL-3.0).

## Technology Stack

- **Language**: C (C11 standard)
- **Build System**: GNU Make
- **UI Library**: ncurses
- **Test Framework**: acutest (single-header C testing framework)
- **Supported Compilers**: 
  - Primary: `armv8m-tcc` (TinyCC for ARMv8-M)
  - Fallback: `gcc` with AddressSanitizer support

## Project Structure

```
.
├── main.c              # Application entry point
├── editor.c/h          # Core editor logic, key handling, state management
├── buffer.c/h          # Text buffer management (linked list of rows)
├── buffer_row.c/h      # Individual line operations and syntax highlighting
├── file_manager.c/h    # File manager sidebar (NERDTree-like)
├── window.c/h          # ncurses window initialization and color pairs
├── command.c/h         # Command line handling (ex commands)
├── help.c/h            # :help built-in help text (shown in its own buffer)
├── tags.c/h            # ctags "tags" file lookup (Ctrl-], :tag, Ctrl-T)
├── completion.c/h      # insert-mode word completion (Ctrl-N / Ctrl-P)
├── autosave.c/h        # crash-recovery autosave, :recover / :delautosave
├── man.c/h             # :man / K manual page lookup (MANPATH, aliases, .gz)
├── make.c/h            # :make settings (.yasvirc, :set), script, error list,
│                       # makemode=exec session file
├── run.c/h             # vfork/execve/waitpid for :make, zcat and xzcat
├── cursor.h            # Cursor position structure definition
├── highlight.h         # Syntax highlighting token types enum
├── filetypes/          # File type specific configurations
│   ├── filetype.c/h    # File type registry and detection (extension, shebang)
│   ├── c.c/h           # C highlighter (default when no filetype matches)
│   ├── shell.c/h       # Shell highlighter
│   ├── zig.c/h         # Zig highlighter (.zig, .zon)
│   └── pio.c/h         # RP2040/RP2350 PIO assembly highlighter (.pio)
├── tests/              # Unit tests
│   ├── acutest.h       # Single-header test framework
│   ├── buffer_tests.c  # Buffer and buffer_row tests
│   └── command_tests.c # Command tests (minimal)
├── Makefile            # Main build configuration
├── .clang-format       # Code formatting rules
└── LICENSE             # GPL-3.0 license
```

## Architecture

### Core Components

1. **Editor** (`editor.c/h`): Central state machine managing:
   - Editor states (Running, CollectingCommand, ProcessingCommand, EditMode, Exiting)
   - Key sequence processing and vi-style commands
   - Screen redraw coordination
   - Multiple buffer management
   - Status bar and error messages

2. **Buffer** (`buffer.c/h`): Text storage using a doubly-linked list of rows:
   - File loading and saving
   - Row insertion, deletion, and navigation
   - Line joining and breaking operations

3. **FileManager** (`file_manager.c/h`): NERDTree-like file browser sidebar:
   - Directory tree navigation with expandable folders
   - File selection to open in editor
   - Toggle visibility with Ctrl+B
   - Vim-style navigation (j/k for up/down, h/l for parent/enter)
   - Visual indicator: `-- FILEMGR --` mode in status bar

4. **BufferRow** (`buffer_row.c/h`): Individual line operations:
   - Character insertion and deletion
   - Word navigation (next/prev word)
   - Syntax highlighting for C language
   - Dirty tracking for efficient redraws

4. **Window** (`window.c/h`): ncurses terminal interface:
   - Terminal initialization and cleanup
   - Color pair definitions for syntax highlighting
   - Screen refresh operations

5. **Command** (`command.c/h`): Ex-style command line:
   - Dynamic command buffer with auto-resize
   - Command parsing infrastructure

## Build Instructions

### Prerequisites

- ncurses development libraries
- make
- gcc or armv8m-tcc compiler

### Build Commands

```bash
# Build the editor (default compiler: armv8m-tcc)
make

# Build with specific compiler
make CC=gcc

# Build and run tests
make test

# Clean build artifacts
make clean

# Install to system (default: /usr/local/bin)
make install

# Install to custom prefix
make install PREFIX=/custom/path
```

### Build Output

The build produces:
- `build/vi` - The editor executable
- Object files in `build/*.o`

## Testing

Tests are located in the `tests/` directory and use the [acutest](https://github.com/mity/acutest) single-header testing framework.

### Running Tests

```bash
# Run all tests
make test

# Or manually
cd tests && make run
```

### Test Structure

- `buffer_tests.c`: Tests for buffer allocation, row operations, character manipulation, and word navigation
- `command_tests.c`: Basic tests for command buffer operations
- `man_tests.c`: `:man` page lookup, `:man` and `K`
- `make_tests.c`: `:make` settings, scripts, error list, session, `-S`

### Adding New Tests

1. Create a test function: `void test_feature(void) { TEST_CHECK(condition); }`
2. Add to `TEST_LIST` macro at the bottom of the test file
3. Update `tests/Makefile` if new source files under test are added

## Code Style Guidelines

The project uses `.clang-format` with Chromium-based style:

- **Indentation**: 2 spaces
- **Column limit**: 85 characters
- **Function formatting**: Never allow short functions on a single line
- **If statements**: Never allow short if statements without braces
- **Line breaks**: Before commas in constructor initializers

### Formatting Code

```bash
# Format all source files
clang-format -i *.c *.h

# Format specific file
clang-format -i editor.c
```

### Coding Conventions

1. **File headers**: All files include GPL-3.0 license header with copyright
2. **Include guards**: Use `#pragma once` for header guards
3. **Naming**:
   - Functions: `module_verb_noun` (e.g., `buffer_append_line`)
   - Types: `PascalCase` (e.g., `EditorState`)
   - Enums: Prefix with `E` (e.g., `EHighlightToken`)
   - Structs: `PascalCase` (e.g., `BufferRow`)
4. **Comments**: Use C-style `/* */` for multi-line comments
5. **Error handling**: Return `bool` for success/failure where appropriate

## Key Features

### Editor Modes

- **Normal mode**: Navigation and command execution
- **Insert mode**: Text insertion
- **Command mode**: Ex-style commands (`:w`, `:q`, etc.)
- **File Manager mode**: File browser navigation (toggle with Ctrl+B)

### Vi-style Commands

- Navigation: `h`, `j`, `k`, `l`, `w`, `b`, `0`, `$`, `gg`, `G`
- Editing: `i`, `a`, `o`, `x`, `dd`, `dw`
- Commands: `:w`, `:q`, `:wq`, `:recover`, `:delautosave`, `:man`, `:make`,
  `:set`, `:cn`, `:cp`, `:cc`, `:cl`; `K` opens a man page
- Repeat counts: Numeric prefixes for commands

### File Manager (Ctrl+B to toggle)

When file manager sidebar is visible:
- `j` / `↓`: Move cursor down
- `k` / `↑`: Move cursor up
- `h` / `←`: Go to parent directory
- `l` / `→` / `Enter`: Open file or expand directory
- `Ctrl-H`: Open the file manager from the editor (vim-style window
  switch left; `Ctrl-L` inside it closes the sidebar and returns to the
  editor window). `Ctrl+B` still toggles either way.
- `Space` / `o`: Toggle directory expand/collapse
- `Escape`: Return to normal mode
- `Ctrl+B`: Close file manager sidebar

### ctags Navigation

With a `tags` file from `ctags -R` (or `ctags *.c`), `tags.c` looks it up
and `editor.c` does the jumping:
- `Ctrl-]`: jump to the definition of the identifier under the cursor
- `:tag NAME` / `:ta NAME`: jump to NAME
- `Ctrl-T` / `:pop`: return to where the last jump started (20-deep stack)

The tags file is searched in the edited file's directory, then each parent,
then the working directory. Both address forms work: `/^pattern$/` (the
default; ctags cuts long patterns at 96 characters and drops the `$`, which
then matches as a prefix) and line numbers (`--excmd=number`). With several
definitions of a name, one in the current file wins, otherwise the first;
the status bar says "tag 1 of N". The cursor lands on the name itself.

### Word Completion (insert mode)

`completion.c` gathers candidates for the word before the cursor; `editor.c`
cycles them and draws the menu (8 rows, under the word or above it when the
screen ends):
- `Ctrl-N` / `Ctrl-P`: next / previous candidate; after the last one the word
  as typed comes back round
- `Ctrl-E`: put back the word as typed
- `Ctrl-Y`: keep the candidate; any other key keeps it too and then does what
  it always does (`Esc` leaves insert mode)

Candidates, without duplicates: words of the current buffer (nearest
following first, wrapping round), then of the other open buffers, then the
filetype's `completion_words` (keywords and types; Zig adds its `@builtins`,
and its `word_prefix_chars` "@" lets `@im` complete to `@import`), then names
from the `tags` file. At most `COMPLETION_MAX_ITEMS` (256).

### Autosave and Crash Recovery

Every edit of the text calls `buffer_note_change` (from `editor.c`'s
`editor_note_change`; a new edit path must call it too). `Buffer.changes`
against `saved_changes` is "modified" (the tab bar `+`), against
`autosaved_changes` is "needs an autosave". `main.c` calls `editor_idle` on
every pass of its loop; `editor_autosave_tick` writes a buffer's autosave once
its edits paused `AUTOSAVE_IDLE_MS` (2 s) or `AUTOSAVE_MAX_PENDING` (200)
piled up. The file is a plain-text copy, written to `<path>.tmp`, fsync'd and
renamed over the old one.

Where (`autosave_candidates`, first that can be written wins and is kept in
`Buffer.autosave_path`):
1. `<dir>/.<name>.yasvi-autosave` beside the file (like vim's `.swp`; the file's
   own directory, not the working directory, so it is found whatever directory
   vi is started from)
2. `$XDG_CACHE_HOME/yasvi`, else `$HOME/.cache/yasvi`, else `/root/.cache/yasvi`,
   named after the absolute path with `/` as `%` -- for read-only directories
3. `/tmp/yasvi`, same naming (survives vi, not a reboot)
Unnamed buffers use 2 and 3 as `noname-<pid>.yasvi-autosave`.

`:w` of the buffer's own file, `:bd` and a clean exit delete it. On load
(`editor_check_autosave`) an autosave equal to the loaded text is deleted; any
other was left by a crash: it goes to `Buffer.stale_autosave`, the status bar
says so, and that buffer is not autosaved (so the crash's copy is kept) until
`:recover` (the text is replaced by it and it becomes the buffer's autosave;
`:w` keeps it) or `:delautosave`. `:w name` on an unnamed buffer names it.
The help buffer sets `autosave_disabled`. Tests: `tests/autosave_tests.c`.

### Manual Pages (`:man`, `K`)

`man.c` finds a page the way `/usr/bin/man` (yasos.zig `usr/bin/man`,
`find_page`/`in_dir`) does, from a MANPATH string it is given (colon-separated,
default `/usr/share/man`): `DIR/NAME.txt`, else `DIR/NAME.txt.gz`, else the
line `NAME BLOCK OFFSET LENGTH` of `DIR/index`, in each directory in turn;
then the first line `NAME PAGE` of each `DIR/aliases` (`fprintf printf`,
`sys/stat.h sys-stat.h`) whose PAGE one of those finds (`man_locate`, a
`ManPage`). The built pages are all in the index: scripts/man/build_man.py
packs them into xz blocks of about 64 KB (`DIR/bNN.xz`; a 64 KB block unpacks
in 0.09 s on the Pico Plus 2, 128 KB took 0.6 s). `DIR/whatis` is the
one-line list. `editor.c`:
- `:man NAME` opens the page in a buffer (`autosave_disabled`); an open buffer
  of it is reused. No page: "No manual entry for NAME".
- `:man` opens the whatis index.
- `K` opens the page for the word under the cursor. The word also takes `.`,
  `/` and `-` (so `stdio.h` and `sys/stat.h` work), drops punctuation at its
  ends, and falls back to the plain identifier when the wide word has no page.
- A packed page: `xzcat BLOCK > /tmp/yasvi-man-NAME.txt` (`run_to_file`:
  vfork/execve of `/bin/xzcat`, else `/usr/bin/xzcat`, and waitpid; no pipe),
  then `man_cut` keeps its OFFSET/LENGTH. A `.gz` page the same way with zcat.
  That copy is the buffer, named after the page, so `:man` of the page or any
  alias of it finds the open buffer. There is no read-only mode; `:w` would write the
  copy (or the plain page's file).

Tests: `tests/man_tests.c` (lookup with temp dirs; `:man` and `K` through the
editor; a page gzipped with the host's gzip, read through zcat; a two-page
xz block and index, read through the host's xzcat and man_cut).

### Building (`:make`) and the Error List

`make.c` holds everything without terminal I/O, `run.c` spawns, `editor.c`
glues it to curses (`editor_make`, `editor_make_report`, `MakeState` in
`Editor.make`, allocated on first use).

Settings (`MakeConfig`): `makeprg` (a shell command) and `makemode` (`wait`,
default, or `exec`). Read at startup (`editor_load_config` from `main.c`) from
`.yasvirc` in the working directory, else `$HOME/.yasvirc`: lines `key=value` or
`set key=value`, `#` comments, the value is the rest of the line. `:set
key=value` changes one, `:set` shows both (an unset makeprg shows its default).
Unset makeprg: `/usr/bin/bash ./build.sh` if `build.sh` exists, else `make` if
there is a `Makefile`, else `:make` says how to set it.

`:make [args]`:
1. autowrite: every modified buffer with a filename that is not
   `autosave_disabled` is written (named in the terminal: "wrote a.c b.c"); a
   failed write stops the build.
2. the script `: > LOG.rc; { MAKEPRG ARGS; echo $? > LOG.rc; } 2>&1 | tee LOG`
   (LOG = /tmp/yasvi-make.log). The board's bash is toybox sh, which rejects
   `set -o pipefail` ("bad -o pipefail", checked on the Pico Plus 2), so the
   build's status is saved from inside the pipeline (a brace group in a
   pipeline works there) and read back from LOG.rc. It runs as
   `/usr/bin/bash -c SCRIPT` (`make_build_script`), so the output streams to
   the terminal and is kept; the group takes a compound makeprg (`a && b`)
   whole. vfork + execve of the shell, a shebang script and a shell pipeline
   with tee were all run on the Pico Plus 2 (2026-11-16) without the HardFault
   apps/prun's header warns about.
3. `wait`: `endwin`, print the command, `run_shell_wait` (vfork, execve,
   waitpid -- vi sits in waitpid, which is what lets the kernel swap it out
   for the build), "[build exited N] press Enter", then `window_init` and
   `editor_mark_dirty_whole_screen` (`editor_resume_screen`).
   `exec` ("swap vi out"): refused while an unnamed buffer is modified. Writes
   `/tmp/yasvi-session` (`make_session_write`: the settings, the current index,
   `file LINE COL NOAUTOSAVE PATH` per named buffer except the help), `endwin`,
   and execs bash with `make_build_exec_script`: the build as above, its
   status into `/tmp/yasvi-make.log.rc`, a wait for Enter, `exec vi -S
   /tmp/yasvi-session` (vi's own argv[0] when it has a `/`, else `vi` from
   PATH). `vi -S FILE` (`editor_restore_session`) loads the buffers, puts the
   cursor back, deletes the session and `.rc` files, then reports as wait does.
4. `editor_make_report` parses the log (`make_parse_log`:
   `file:line[:col]: error|fatal error|warning: msg`, tcc's `f.c:12: error:`
   and gcc/zig's `f:12:5: error:`; notes and make's own lines are skipped) and
   says "build ok" or "build failed (exit N), M errors" (plus warnings), and
   jumps to the first error (not warning) with its text after the summary.

`:cn` / `:cp` go to the next / previous entry (errors and warnings, in log
order), `:cc [N]` to the current (or Nth); the status bar shows "(i of n)
file:line: error: msg". `:cl` opens the log as a buffer (`autosave_disabled`,
reloaded if already open). Messages here use `editor_set_status_message`,
which does not append the ": 'command'" of `editor_set_error_message`.

Spawning (`run.c`) follows apps/prun: argv and envp are copied into a static
arena (3 KiB; the kernel's uaccess refuses malloc'd memory in execve), the
environment is a copy of `environ` (never `environ` itself) with a default
`PATH` added if missing, and the vforked child only opens/dup2s/execs/_exits.
Only libc calls libs/libc implements are used (vfork, execve, waitpid, open,
dup2, read, remove).

Tests: `tests/make_tests.c` (settings, scripts, error lines, the session file;
through the editor `:set`, the error list, `-S`, and a real wait-mode `:make`
running bash on the host with stdin on /dev/null). The tests build with
`MAKE_LOG_PATH`/`MAKE_SESSION_PATH` pointed into `tests/build`. Not testable on
the host: curses suspend/resume on the real terminal, and exec mode's exec.

### Built-in Help (`:help`)

`:help` (also its prefixes `:h`, `:he`, `:hel`) shows the command reference in
a dedicated buffer named `yasvi-help` — like vim's `:help`, it opens (or
reuses) that buffer and switches to it, so the help can be scrolled and edited.
`help.c` owns the text (`g_help_lines`) and builds the buffer
(`help_alloc_buffer`); `editor.c`'s `editor_show_help` finds an already-open
help buffer by name and reuses it, never duplicating.

### Syntax Highlighting

C language support with highlighting for:
- Keywords (`if`, `else`, `while`, `for`, `return`, etc.)
- Types (`int`, `char`, `float`, `void`, etc.)
- Preprocessor directives (`#include`, `#define`, etc.)
- Strings and comments
- Numbers

Shell scripts (by extension or shebang) and Zig (`.zig`, `.zon`) have their
own highlighters. Zig highlights keywords, primitive types (including any
`iN`/`uN`), `true`/`false`/`null`/`undefined`, `@builtins` and `@"ids"`,
`//` comments, strings and `\\` multiline string lines, and number literals
(hex/binary/octal, `_` separators, exponents; `0..5` stays a range). Nothing
in Zig spans lines, so its highlighter never carries state to the next row.

PIO assembly (`.pio`, the syntax `apps/pioasm` reads) highlights directives
(`.program`, `.side_set`...), instructions, their sources/destinations
(`pins`, `x`, `osr`...) and modifiers (`side`, `block`, `rel`...), all
case-insensitively like pioasm, plus numbers, `;`, `//` and `/* */` comments.
A `% lang {` ... `%}` code block is highlighted by the language's own
highlighter (`c-sdk` as C, `zig` as Zig, anything else plain). Which language
a row starts in is carried in `HighlightContext.lang_state`, stored per row as
`highlight_lang_state`: a filetype-private int that, like the comment/string
state, the first row starts at 0 and `buffer_row_rehighlight` cascades on.

## Development Notes

### Adding New File Types

1. Add `filetypes/<lang>.c/h` defining a `Highlighter` and a `Filetype`
   (extensions, optional shebangs)
2. Declare it in `filetypes/filetype.h` and add it to `g_filetypes[]` in
   `filetypes/filetype.c`
3. Add the source to `SUT_SRCS` in `tests/Makefile` and write
   `tests/<lang>_highlight_tests.c` (see `zig_highlight_tests.c`)

### Memory Management

- All buffers and rows use dynamic allocation
- Cleanup is handled through `*_free()` and `*_deinit()` functions
- Tests run with AddressSanitizer to detect memory issues

### Debugging

The project includes `.gdb_history` indicating GDB is commonly used for debugging. To debug:

```bash
gcc -g -O0 *.c -lncurses -o vi_debug
gdb ./vi_debug
```

## Security Considerations

- Input validation is performed on all user-provided file paths
- Buffer sizes are dynamically managed with bounds checking
- Integer overflow protection in buffer operations
- Always run tests with AddressSanitizer during development

## TODO Items

Current development items tracked in `todo`:
- Backspace to join lines
- `dw` command to remove words

## License

This project is licensed under the GNU General Public License v3.0. See LICENSE file for details.
