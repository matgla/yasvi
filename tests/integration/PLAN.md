# End-to-End Integration Test Plan for Yasvi Editor

## Overview

This document outlines the strategy for end-to-end (E2E) integration tests for the Yasvi editor using Python. These tests will validate realistic user workflows by interacting with the actual editor binary through terminal emulation.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    Python Test Framework                         │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐  │
│  │   pytest    │  │  pexpect    │  │    File Assertions      │  │
│  │   runner    │  │  (pty/tty)  │  │    (checksum/diff)      │  │
│  └──────┬──────┘  └──────┬──────┘  └─────────────────────────┘  │
└─────────┼────────────────┼───────────────────────────────────────┘
          │                │
          ▼                ▼
┌─────────────────────────────────────────────────────────────────┐
│              Terminal Emulator (pty)                             │
│                   ↓ keystrokes                                   │
│              ┌──────────┐                                       │
│              │  yasvi   │  ← ncurses UI                         │
│              │  editor  │                                       │
│              └────┬─────┘                                       │
│                   ↓ file I/O                                     │
│              ┌──────────┐                                       │
│              │  temp    │  ← test fixtures & outputs            │
│              │  files   │                                       │
│              └──────────┘                                       │
└─────────────────────────────────────────────────────────────────┘
```

## Test Framework Components

### 1. Test Harness (`conftest.py`)

**Purpose**: Shared fixtures and utilities for all tests.

**Key Components**:
- `editor_path`: Path to compiled `build/vi` binary
- `temp_dir`: Temporary workspace for test files
- `EditorSession` class: Wrapper around pexpect for editor interaction
- Screen capture utilities for debugging

### 2. Editor Session Wrapper (`editor_session.py`)

**Purpose**: Abstract interaction with the ncurses UI.

**API Design**:
```python
class EditorSession:
    def __init__(self, editor_path: str, filename: str | None = None)
    def send_key(self, key: str)              # Send single key
    def send_keys(self, keys: str)            # Send sequence
    def send_command(self, cmd: str)          # Send :command<Enter>
    def enter_insert_mode(self)               # Press 'i'
    def exit_insert_mode(self)                # Press Escape
    def get_screen_text(self) -> str          # Capture visible screen
    def wait_for_text(self, text: str, timeout: float = 2.0)
    def screenshot(self, name: str)           # Save screen for debugging
    def close(self)                           # Clean exit (:q!)
```

### 3. File Fixtures (`fixtures/`)

**Purpose**: Realistic test files of various types.

**Sample Files**:
- `simple.txt`: Basic text file (10-20 lines)
- `code.c`: C source code with syntax highlighting
- `large.txt`: 1000+ line file for scroll testing
- `empty.txt`: Empty file edge case
- `single_line.txt`: Single line edge case
- `special_chars.txt`: Unicode, tabs, special characters

## Test Scenarios

### Tier 1: Basic Operations (Smoke Tests)

| Test ID | Scenario | Steps | Validation |
|---------|----------|-------|------------|
| E2E-001 | Launch and quit | Open editor, :q | Clean exit, no crash |
| E2E-002 | Create new file | vi newfile.txt, type, :wq | File created with content |
| E2E-003 | Open existing | vi existing.txt | File loaded, content visible |
| E2E-004 | Quit without save | Edit file, :q! | Original file unchanged |

### Tier 2: Navigation

| Test ID | Scenario | Steps | Validation |
|---------|----------|-------|------------|
| E2E-101 | Arrow navigation | Open file, hjkl movement | Cursor position correct |
| E2E-102 | Word navigation | Use w, b, e across words | Cursor at word boundaries |
| E2E-103 | Line navigation | ^, $, 0 for line bounds | Cursor at line start/end |
| E2E-104 | Document navigation | G (bottom), gg (top) | View shows correct lines |
| E2E-105 | Scroll through large file | j/k through 1000 lines | Screen scrolls, cursor updates |

### Tier 3: Editing Operations

| Test ID | Scenario | Steps | Validation |
|---------|----------|-------|------------|
| E2E-201 | Insert text | i<type>Esc | Text inserted at cursor |
| E2E-202 | Append text | a<type>Esc | Text appended after cursor |
| E2E-203 | Delete character | x on various positions | Char removed, cursor stable |
| E2E-204 | Delete line | dd (when implemented) | Line removed |
| E2E-205 | Type full paragraph | Multiple insert sessions | Content matches expected |
| E2E-206 | Edit at line end | $a<text>Esc | Appended at end |
| E2E-207 | Edit at line start | ^i<text>Esc | Inserted at first char |

### Tier 4: File Operations

| Test ID | Scenario | Steps | Validation |
|---------|----------|-------|------------|
| E2E-301 | Save existing | :w | File timestamp updated, content same |
| E2E-302 | Save as | :w newname.txt | New file created with content |
| E2E-303 | Save and quit | :wq | File saved, editor exits |
| E2E-304 | Force quit | :q! | No save, editor exits |
| E2E-305 | Edit multiple times | Multiple :w during session | Final file has all changes |

### Tier 5: Complex Workflows (Realistic Scenarios)

| Test ID | Scenario | Steps | Validation |
|---------|----------|-------|------------|
| E2E-401 | Write a function | Open .c, navigate, insert code, :wq | File compiles, content correct |
| E2E-402 | Edit configuration | Open config, modify setting, save | Config file valid |
| E2E-403 | Quick note taking | vi note.txt, i<fast type>:wq | No data loss, clean file |
| E2E-404 | Bug fix simulation | Navigate to line, delete word, type fix | Line correctly modified |
| E2E-405 | Large file edit | Jump to middle, edit, save | Only intended changes made |

### Tier 6: Edge Cases & Error Handling

| Test ID | Scenario | Steps | Validation |
|---------|----------|-------|------------|
| E2E-501 | Read-only file | Open read-only, attempt save | Error message displayed |
| E2E-502 | Nonexistent file path | :w /bad/path/file | Error handled gracefully |
| E2E-503 | Very long lines | Edit 500+ char line | No crash, display OK |
| E2E-504 | Binary file | Open binary, attempt edit | Graceful handling |
| E2E-505 | Rapid key input | Send keys rapidly | No race conditions |

## Directory Structure

```
tests/
├── integration/
│   ├── PLAN.md                 # This document
│   ├── requirements.txt        # Python dependencies
│   ├── pytest.ini             # pytest configuration
│   ├── conftest.py            # Shared fixtures
│   ├── editor_session.py      # Editor interaction wrapper
│   ├── README.md              # Running instructions
│   │
│   ├── fixtures/              # Test files
│   │   ├── simple.txt
│   │   ├── code.c
│   │   ├── large.txt
│   │   ├── empty.txt
│   │   └── special_chars.txt
│   │
│   ├── screenshots/           # Debug output (gitignored)
│   │
│   └── test_cases/            # Test modules
│       ├── test_smoke.py      # Tier 1
│       ├── test_navigation.py # Tier 2
│       ├── test_editing.py    # Tier 3
│       ├── test_file_ops.py   # Tier 4
│       ├── test_workflows.py  # Tier 5
│       └── test_edge_cases.py # Tier 6
```

## Implementation Phases

### Phase 1: Foundation (Week 1)
- [ ] Set up Python virtual environment
- [ ] Install pexpect, pytest dependencies
- [ ] Create `EditorSession` wrapper with basic send/receive
- [ ] Implement screen capture for debugging
- [ ] Create fixture files

### Phase 2: Smoke Tests (Week 1-2)
- [ ] Implement Tier 1 tests
- [ ] Verify test stability (flaky test detection)
- [ ] Set up CI integration
- [ ] Add screenshot capture on failure

### Phase 3: Core Features (Week 2-3)
- [ ] Implement Tier 2 (navigation) tests
- [ ] Implement Tier 3 (editing) tests
- [ ] Implement Tier 4 (file ops) tests
- [ ] Add timing/benchmark collection

### Phase 4: Realistic Scenarios (Week 3-4)
- [ ] Implement Tier 5 workflow tests
- [ ] Create helper utilities for common patterns
- [ ] Add file comparison utilities

### Phase 5: Edge Cases & Polish (Week 4)
- [ ] Implement Tier 6 edge case tests
- [ ] Add stress tests (large files, rapid input)
- [ ] Documentation and examples
- [ ] Performance baseline establishment

## Running the Tests

### Prerequisites
```bash
# Build editor
cd /home/mateusz/repos/yasvi && make

# Set up Python environment
cd tests/integration
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### Run Commands
```bash
# All tests
pytest

# Specific tier
pytest test_cases/test_smoke.py

# With screenshots on failure
pytest --screenshots-on-failure

# Verbose with live output
pytest -v -s

# Specific test
pytest test_cases/test_editing.py::test_insert_text

# Generate HTML report
pytest --html=report.html
```

## Success Criteria

1. **Coverage**: All Tier 1-4 tests passing
2. **Stability**: <1% flaky test rate
3. **Performance**: Each test completes in <5 seconds
4. **Maintainability**: Tests are readable and well-documented
5. **Debuggability**: Screenshots/logs available for failures

## Future Enhancements

1. **Visual Regression**: Compare screenshots for UI consistency
2. **Performance Tests**: Measure startup time, file load time
3. **Fuzzing**: Randomized input testing
4. **Multi-session**: Test multiple editor instances
5. **Script Recording**: Record and replay user sessions
6. **Property-based Testing**: Use Hypothesis for generative tests
