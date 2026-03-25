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
├── window.c/h          # ncurses window initialization and color pairs
├── command.c/h         # Command line handling (ex commands)
├── cursor.h            # Cursor position structure definition
├── highlight.h         # Syntax highlighting token types enum
├── filetypes/          # File type specific configurations
│   ├── filetype.h      # File type detection header
│   └── c.ft            # C language keywords and syntax rules
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

3. **BufferRow** (`buffer_row.c/h`): Individual line operations:
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

### Vi-style Commands

- Navigation: `h`, `j`, `k`, `l`, `w`, `b`, `0`, `$`, `gg`, `G`
- Editing: `i`, `a`, `o`, `x`, `dd`, `dw`
- Commands: `:w`, `:q`, `:wq`
- Repeat counts: Numeric prefixes for commands

### Syntax Highlighting

C language support with highlighting for:
- Keywords (`if`, `else`, `while`, `for`, `return`, etc.)
- Types (`int`, `char`, `float`, `void`, etc.)
- Preprocessor directives (`#include`, `#define`, etc.)
- Strings and comments
- Numbers

## Development Notes

### Adding New File Types

1. Create a new `.ft` file in `filetypes/` directory
2. Define keywords and syntax rules
3. Update file type detection logic in `buffer_row.c`

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
