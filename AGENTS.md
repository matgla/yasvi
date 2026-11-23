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
├── fileops.c/h         # paths, file operations (tree), file name completion
├── indent.c/h          # automatic indentation rules (autoindent), no curses
├── window.c/h          # ncurses window initialization and color pairs
├── command.c/h         # Command line handling (ex commands)
├── help.c/h            # :help built-in help text (shown in its own buffer)
├── tags.c/h            # ctags "tags" file lookup (Ctrl-], :tag, Ctrl-T)
├── completion.c/h      # insert-mode word completion (Ctrl-N / Ctrl-P)
├── autosave.c/h        # crash-recovery autosave, :recover / :delautosave
├── man.c/h             # :man / K manual page lookup (MANPATH, aliases, .gz)
├── make.c/h            # :make settings (.yasvirc, :set), script, error list,
│                       # makemode=exec session file
├── config.c/h          # editor settings: ~/.config/vi.conf, :set (record)
├── record.c/h          # typing recorder: keys, times, pauses to FILE.rec
├── termpanel.c/h       # :term panel: a shell on pipes, scrollback, history
├── run.c/h             # vfork/execve/waitpid for :make, zcat and xzcat
├── cursor.h            # Cursor position structure definition
├── highlight.h         # Syntax highlighting token types enum
├── filetypes/          # File type specific configurations
│   ├── filetype.c/h    # File type registry and detection (name, extension, shebang)
│   ├── c.c/h           # C highlighter (default when no filetype matches); the
│   │                   # yasc dialect's words where tcc takes them as keywords
│   ├── shell.c/h       # Shell highlighter
│   ├── zig.c/h         # Zig highlighter (.zig, .zon)
│   ├── pio.c/h         # RP2040/RP2350 PIO assembly highlighter (.pio)
│   ├── conf.c/h        # config files: vi.conf, .yasvirc, .conf/.ini, fstab
│   └── makefile.c/h    # Makefile, *.mk: highlighting, rule lines, hard tabs
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
   - Vim-style navigation (j/k for up/down, h/l for up/open)
   - File operations (new, rename/move, copy, delete) through `fileops.c`
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
- `record_tests.c`: config file and `:set record`, the `.rec` format, recording
  through the editor
- `shell_command_tests.c`: `:!cmd`, `:r !cmd`, `:r FILE`, `%` expansion
- `file_ops_tests.c`: paths, file operations, completion, the tree's refresh
  and navigation, `:e`/`:open`, the tree's a/A/r/c/d keys, Tab completion
- `indent_tests.c`: block openers, bracket matching, the indent step, and
  Enter/o/O/closing brackets/Tab/Backspace through the editor, Backspace
  joining lines; Makefiles' hard tabs and how a tab is drawn; `:set ts`,
  `et`/`noet`/`et!`, `sw` per buffer and the status bar text
- `make_highlight_tests.c`: the make filetype (detection, highlighting,
  continuations, define bodies, `makefile_is_rule`)
- `termpanel_tests.c`: output cleaning, scrollback, history, a real shell,
  Ctrl-C, and the panel's keys through the editor

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
- Editing: `i`, `a`, `o`, `O`, `x`, `dd`, `dw`
- Commands: `:w`, `:q`, `:wq`, `:recover`, `:delautosave`, `:man`, `:make`,
  `:set` (`record`, `autoindent`/`ai`, `tabstop`/`ts`, `expandtab`/`et`,
  `shiftwidth`/`sw`, `makeprg`, `makemode`), `:cn`, `:cp`, `:cc`, `:cl`,
  `:!cmd`, `:r !cmd`, `:r FILE`, `:term [cmd]`, `:termclose`, `:e`/`:open FILE`, `:e!`; `K` opens a
  man page. `:q` in `:help`, a
  `:man` page or the `:cl` log (`autosave_disabled` buffers) closes that
  buffer instead of exiting, unless it is the only one
- Repeat counts: Numeric prefixes for commands

### File Manager (Ctrl+B to toggle)

When file manager sidebar is visible:
- `j` / `↓`: Move cursor down
- `k` / `↑`: Move cursor up
- `h` / `←`: Close an expanded directory, else go to the directory above,
  else make the root's parent the root
- `l` / `→` / `Enter`: Open file (the tree stays shown, the keys go to the
  file) or expand directory
- `Ctrl-H`: Keys to the tree, showing it if hidden (vim-style window switch
  left); `Ctrl-L` gives the keys back to the text and leaves the tree shown.
  `Ctrl+B` toggles it either way, `q` in it hides it. Only from normal mode
  and the tree: in insert mode, on the `:` line and in a search Ctrl-H is
  Backspace (as in vim). yasos_curses passes 0x08 through and maps only
  0x7F to `KEY_BACKSPACE` (it used to map both, so the board never saw
  Ctrl-H; host ncurses always told them apart).
- `Space` / `o`: Toggle directory expand/collapse; `C` makes it the root
- `a`: new file in the selected directory (or the selected file's), opened
  at once; a name ending in `/` makes a directory, missing directories in
  the name are made. `A`: new directory
- `r` / `m`: rename or move; the answer is taken from the entry's directory
  (absolute and `~` work), an existing directory or a name ending in `/`
  takes the entry inside. Open buffers follow the new name (their autosave
  is written again under it)
- `c`: copy (directories recursively); `d`: delete, `y` confirms (a
  directory with everything in it). Nothing ever overwrites an existing name
- `R`: read the tree again. It is also read again when shown (Ctrl-B,
  Ctrl-H) and after `:w`; expanded directories stay expanded and the cursor
  stays on the same path (or the nearest directory above it)
- `Escape`: Return to normal mode (a question on the command line: cancel)

The cursor's line is counted from the tree on every move
(`file_manager_sync_cursor`), never tracked by hand: the old incremental
count went out of step after `h`, and Enter then acted on the entry below
the highlight. The marks are ASCII (`+ dir/`, `- dir/`, `@ link`) because
yasos_curses counts bytes as columns. The selection is reverse video while
the tree has the keys, bold otherwise (`FileManager.focused`, set by
editor.c before drawing). The prompt state lives in `FileManager`, not in
`Editor`, so the test files' copies of the `Editor` struct stay as they are.

### Opening files

- `:e FILE`, `:edit FILE`, `:o FILE`, `:open FILE`: an open buffer of the
  same file (any spelling: `path_same` compares `path_absolute` of both) is
  switched to; a missing file is a new buffer that `:w` creates; a
  directory opens in the tree as its root. The blank `[No Name]` buffer vi
  starts with is replaced rather than kept as a tab.
- `:e` / `:e!` read the current file again (`:e` refuses while modified).
- Tab on the command line after `:e`, `:open`, `:r`, `:w` (and kin)
  completes file names: the common part of the matches, then a list of them
  on the message line. Tests: `tests/file_ops_tests.c`.

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
from the `tags` file, then -- in C buffers -- the C library's tags
(`YASVI_SYSTEM_TAGS`, `/usr/share/yasvi/libc.tags`, `editor_system_tags`).
At most `COMPLETION_MAX_ITEMS` (256). `:tag` and Ctrl-] fall back to the same
file when the project's tags (or no tags file) lack a name, so `printf`
jumps to its prototype in `/usr/include/stdio.h`. `build_rootfs.sh` writes it
with the host's ctags over `rootfs/usr/include` (functions, macros, types,
enum values; no members, no `_` names, no tcc `tcclib.h`; absolute paths,
`--fields=k`: about 180 KB, 2700 names) and skips it with a warning when the
host has no ctags. The tests point `YASVI_SYSTEM_TAGS` at
`/tmp/yasvi_libc_test.tags` (`EDITOR_TEST_CFLAGS`).

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

### Settings File (`~/.config/vi.conf`)

`config.c` holds the editor's own settings (`EditorConfig` in
`Editor.config`; `:make`'s stay in `make.c`'s `MakeConfig`). At startup
`editor_load_config` reads the user's file, `$XDG_CONFIG_HOME/vi.conf`, else
`$HOME/.config/vi.conf`, else `/root/.config/vi.conf` (`config_user_path`),
then `.yasvirc` (working directory, else `$HOME`), so a project overrides the
user. Every line of either goes through `editor_apply_setting`: `config_set`
first, then `make_config_set` for a key it does not know, so both files and
`:set` take every setting. A file with lines neither knows is named in the
first message, with the first such line and the settings there are
(`~/.config/vi.conf:1: unknown "set recording" (record, ai, makeprg,
makemode)`, `(+N more)` when there are others; `~` for `$HOME` keeps it
within 80 columns, and a centred toolbar widget wider than the screen now
starts at column 0 and is cut instead of vanishing). Lines are `key=value` or
`set key=value`, `#` comments; a boolean also takes `key`, `nokey` and `key?`
(on/off/yes/no/true/false/1/0 as values). A new boolean is one row in
`g_config_bools` plus its field (`inverted` when the default is on but a
zeroed `Editor` must get it: `expandtab` is the field `noexpandtab`); a
number is a row in `g_config_ints` (`tabstop` 1-32, `shiftwidth` 0-32, an
`unsigned char` each: test files keep copies of the `Editor` struct, and
`EditorConfig` must stay within 8 bytes for their layout). `key!` turns a
boolean over; a bad value gets `config_expected_value` ("tabstop is a
number from 1 to 32"). `:set` alone shows all (`norecord autoindent
expandtab tabstop=8 shiftwidth=0 makeprg=... makemode=...`); `:set record`
shows just `record` (`config_describe_setting`). A boolean may have a short
name (`ai`). `config_init` (from `main.c`) sets the defaults; tests that
zero the `Editor` get every boolean off, so autoindent stays out of the
older tests, and a tabstop of 0 means 8.

Tabs and spaces (`editor_uses_tabs`, `editor_shift_width`): a buffer is in
tab mode (Tab inserts `\t`, autoindent's step is `\t`) when `:set noet`
said so for it (`Buffer.indent`), else when its filetype has `hard_tabs`
(make), else when `indent_detect` finds most indented lines start with a
tab, else when vi.conf said `noexpandtab`. In space mode the step is the
buffer's `:set sw` (`Buffer.shiftwidth`), else the file's detected step,
else vi.conf's `shiftwidth`, else `tab_size`. `:set et`/`noet`/`et!`/`sw` in
a session go to the current buffer (`editor_set_option` sets them from what
the buffer uses now, so `et!` flips the effective mode, then puts the
defaults back); from vi.conf or .yasvirc (read before any buffer exists)
they are the defaults. `:set ts` is global and redraws the screen. The
toolbar's `indent` widget (right side, left of the position) shows
`editor_describe_indent`: `tabs:<tabstop>` or `spaces:<shiftwidth>`.

### Automatic Indentation (`autoindent`, default on)

`indent.c` holds the rules without curses; `editor.c`'s "Automatic
indentation" section applies them. Language by filetype name: C, Zig, PIO
have brackets, shell also `then`/`do`/`else`/`case ... in`, anything else
only keeps the line's indentation. Enter (`editor_insert_newline`, also `o`)
copies the indentation before the cursor, adds one step after a block
opener, strips the moved text's leading and the old line's trailing white
space, puts a moved closing bracket under its opener, and splits `{|}` into
three lines. A `}` `)` `]` typed as the first thing on a line moves under its
opener (`indent_find_opener`: brackets outside strings, char literals and
line comments, 2000 lines up; block comments are not skipped). The step is a
tab in tab mode, else `editor_shift_width` spaces (see Tabs and spaces under
Settings File; `indent_detect` reads the file: a tab if most indented lines
start with one, else the most common increase between lines, first 500). Tab and
Backspace in an all-space indentation go to the next/previous step. A line
that only holds Enter's indentation (`g_autoindent_row`) becomes empty on
Esc or another Enter. A filetype with `hard_tabs` (make) always steps by a
tab (`editor_indent_unit`), Tab inserts one tab character there instead of
spaces, and its `IndentLanguage_Make` opens a block after a rule line
(`makefile_is_rule`: `target: deps`, not an assignment, a target-specific
variable, a directive or a recipe); it has no brackets
(`indent_has_brackets`), so a `)` in a recipe stays put. `O` (`editor_open_line_above`) copies the line's
indentation, a step more above a closing bracket. Backspace at column 0
joins with the line above; `buffer_join_current_line_with_previous` leaves
the joined line current (it used to leave the one below, which the screen
then drew in place of the joined line).

### Typing Recorder (`record`)

With `record` on, `editor_process_key` first hands the key to
`editor_record_key` (before processing it, so a key that switches buffers
belongs to the buffer it left). Keys typed while a named file that is not
`autosave_disabled` and not itself a `.rec` is current are appended to
`FILE.rec` beside it (`record.c`, no curses). One recording is open at a time
(`Editor.recorder`, allocated on first use): a key for another file closes it
and that file's next key starts a new session. The file is appended to, never
rewritten, so it holds every session:

```
# yasvi recording of t.c                     (4 comment lines, new file only)
session 2026-11-18 09:40:44 unix 1794991244 lines 1
0 0 111 o                                    AT_MS PAUSE_MS CODE KEY
100 100 104 h
401 151 27 ESC
pause 2026-11-18 09:40:46 after 2.504 s      before a key after >= 2 s
2905 2504 58 :
```

AT_MS counts from the session's start and PAUSE_MS from the key before
(monotonic clock, `autosave_now_ms`); the dates are local time from `time()`
at the session start plus AT (so a board without a set clock writes 1970).
KEY is the character, SPACE/TAB/ENTER/ESC/BS, `^X`, or a curses key's name
(UP, DOWN, ..., KBS for KEY_BACKSPACE) from `editor_curses_key_name` -- named in
editor.c because the codes differ between yasos_curses, ncurses and the test
mock. Keys are buffered by stdio: `editor_idle` flushes once typing paused
`RECORD_FLUSH_MS` (1 s), `:make` flushes before handing over the terminal or
exec'ing (exec drops stdio buffers), `:set norecord` and exit close it. A crash
loses at most the last second. An unwritable directory is reported once per
file ("Cannot record to FILE.rec"). Not carried across makemode=exec: a
`:set record` typed in the session (vi.conf is read again on the way back).

The `:term` panel is recorded as lines that start with a word (never keys):
`term AT focus` / `text AT focus` after a key that moved the focus
(`editor_process_key` wraps `editor_handle_key` to see it), and from the
panel's `TermPanel.on_line` hook (`editor_record_terminal_line`): `term AT $
LINE` sent, `out AT LINE` printed (cleaned, one per scrollback line), `term AT
exit N`. They go through `recorder_note` to the open recording, else the
current file's; with no named file they are not kept. Keys typed in the
panel stay ordinary key lines. Test: `record_tests.c`
`editor_records_terminal`.

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
`~/.config/vi.conf` and then `.yasvirc` in the working directory, else
`$HOME/.yasvirc` (see Settings File): lines `key=value` or
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

### Shell Commands (`:!cmd`, `:r !cmd`, `:r FILE`)

`editor.c` (`editor_bang`, `editor_read`) on top of `run.c`. In the command,
`%` is the current file's name, shell-quoted, and `\%` a plain `%`
(`run_expand_command`; a `%` in an unnamed buffer: "No file name for %").
- `:!cmd`: like `:make`'s wait mode -- flush the recording, `endwin`, print the
  command, `run_shell_wait` (`/usr/bin/bash -c`, the terminal as stdin/stdout,
  so interactive programs work), "[exit N] press Enter", `editor_resume_screen`.
  The status bar says "!cmd: exit N".
- `:r !cmd`: `run_shell_to_file` (stdin `/dev/null`, stdout and stderr into
  `RUN_OUTPUT_PATH`, /tmp/yasvi-cmd.out, removed afterwards), then
  `editor_insert_file_below`: the lines go below the current line, the cursor
  to the first of them, one change for autosave/`+`. "!cmd: N lines, exit S".
- `:r FILE`: the same insertion from a file.

No pseudo-terminals on YasOS (pipes, poll, O_NONBLOCK and signals exist), so a
program run by vi gets either the real console (`:!`) or no terminal at all.
Tests: `tests/shell_command_tests.c` (the host's bash; `RUN_OUTPUT_PATH` points
into `tests/build`).

### Terminal Panel (`:term`)

`termpanel.c` (no curses) runs a persistent `/usr/bin/bash` reading its
commands from a pipe (`run_shell_piped`: stdin one pipe, stdout and stderr
another; vi's ends `O_NONBLOCK` and `FD_CLOEXEC`, `TERM=dumb`). YasOS has no
pseudo-terminals, so nothing in the panel has a terminal: vi, curses programs
and password prompts do not work there (`:!` gives the real console), stdio
output may arrive in blocks, and a program reading stdin reads the next lines
typed. The panel echoes each sent line as `$ line` (the shell prints no prompt
on a pipe). Output is cleaned for a text view (`termpanel_feed`: ANSI escapes
dropped, `\r\n` ends a line, a lone `\r` restarts it, `\b`, tabs to 8,
other control characters dropped), lines over 1024 bytes are split, and 500
lines are kept (a ring). Ctrl-C does not signal the shell: it finds the
shell's descendants through `/proc/PID/status` `PPid` lines
(`termpanel_descendants`) and sends them SIGINT, as the console's Ctrl-C
reaches only the foreground job. `termpanel_stop` SIGKILLs them and the shell
and reaps it; the shell exiting by itself is noticed by `termpanel_poll`
(`waitpid WNOHANG`) as "[shell exited N]", and Enter starts a new one.

`editor.c`: `Editor.terminal` (allocated by `:term`, freed by Ctrl-W c,
`:termclose` and exit). The panel takes `rows` (a third of the window, at
least 5, leaving 3 text rows) between the text and the toolbar: every height
of the text area goes through `editor_bottom_height` (toolbar + panel), and
opening it moves the cursor up into the text that is left
(`editor_fit_cursor_to_text`). `editor_draw_terminal` redraws the whole panel
each time (title in reverse video, output newest at the bottom with long lines
wrapped, `$ ` and the typed line); with the focus there the cursor sits at
the end of the typed line. `main.c` redraws when `editor_idle` (which calls
`termpanel_poll`) says output arrived, not only after a key.

Keys (`editor_terminal_handles_key`, right after recording, so the panel's
keys are recorded too): with the focus in the panel, printable keys edit the
line, Enter sends it, Backspace (127, ^H), Ctrl-U clears, Up/Down history (16
lines), PgUp/PgDn scroll (`[+N]` in the title), Ctrl-C interrupt, Ctrl-D on
an empty line closes the shell's stdin, Esc and Ctrl-K go back to the text.
From the text in normal mode, Ctrl-J goes down to the panel: yasos_curses
turns CR and LF both into `\n`, so Ctrl-J is Enter, which does nothing else in
normal mode (insert mode keeps it). Ctrl-W then j/k/w (move) or c (close)
works from either window except while typing text, a command or a search.
Ctrl-H/L still move between the file manager and the text.

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

Configuration files (`filetypes/conf.c`: `.conf` -- so `vi.conf` --
`.yasvirc`, `.cfg`, `.ini`, `.toml`, git's, and by whole name `fstab` and
`inittab` through `Filetype.filenames`, which `filetype_by_extension` checks
first) highlight whole-line `#`/`;` comments, `[section]`, a leading `set`,
the key -- yasvi's settings (`vi_settings`: config.c's and make.c's; keep it
in step) as keywords, a `no` before one as a literal, any other key as a
type, so a misspelt setting shows in another colour -- then `=` `:` `?` and
the value: numbers and on/off/yes/no/true/false/wait/exec as literals,
quoted strings, `$VAR`, and `%` (`:make`'s current file). A `#` later in a
line is part of the value, as config.c reads it. Test:
`tests/conf_highlight_tests.c`.

Makefiles (`filetypes/makefile.c`, filetype `make`: `Makefile`, `makefile`,
`GNUmakefile`, `Makefile.in`/`.am` by name, `.mk` `.mak`) highlight
directives (`include`, `ifeq`, `define`, `export`...; `else ifeq` and
`export X = 1` go on to the statement after them, `export = 1` sets a
variable), an assignment's variable as a type and its `= := ::= :::= ?= +=
!=` as a symbol, a rule's targets as Keyword2 (special ones like `.PHONY` as
keywords, `%` as Symbol2), `:`/`::` and `|`, `$(VAR)` `${VAR}` `$@` `$<`
references (Preprocessor) and `$(func args)` calls with the function as a
keyword, `#` comments. A line starting with a tab is a recipe: `@ - +`
prefixes as symbols, the rest through the shell highlighter, references on
top; `target: deps ; cmd` the same after the `;`. `lang_state` carries a
backslash-continued value, recipe (with the shell's open quote) or comment,
and `define ... endef` (nesting depth) to the next row. Tests:
`tests/make_highlight_tests.c`.

Tab characters are drawn as spaces to the next multiple of `tabstop`
(`editor_tab_stop`, default 8), counted from the start of the line, by
`editor_decorate_and_draw_line`. The cursor stays a character index
(`cursor.x`, `start_column`); `editor_restore_cursor_position` turns it into
the screen column on a line with tabs (every `move` to the text cursor goes
through it). Horizontal scrolling still counts characters, so on a long
line with tabs the cursor can reach the window's edge (it is clamped there)
before the view scrolls. Before this a tab went to the terminal as a raw
`\t` and the screen went out of step with the cursor.

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
