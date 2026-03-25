# Yasvi Editor E2E Integration Tests

End-to-end integration tests for the Yasvi editor using Python and pexpect.

## Quick Start (Make Targets)

From the project root, use these Make targets:

```bash
# Build the editor
make

# Run unit tests only (C tests)
make ut

# Run integration tests (Python E2E tests) - PARALLEL by default (fast!)
make integration

# Run integration tests sequentially (slower, use if parallel has issues)
make integration-seq

# Run all tests (unit + integration) - PARALLEL by default
make test

# Run all tests sequentially (if parallel has issues)
make test-seq

# Quick integration tests (smoke + navigation only)
make integration-quick        # Parallel (~5s)
make integration-quick-seq    # Sequential (~25s)

# Specific test categories - PARALLEL by default (fast)
make integration-smoke        # Basic tests only (~4s)
make integration-editing      # Text editing tests (~4s)
make integration-navigation   # Navigation tests (~4s)
make integration-fileops      # File operations (~5s)

# Same categories - SEQUENTIAL (slower, no warnings)
make integration-smoke-seq
make integration-editing-seq
make integration-navigation-seq
make integration-fileops-seq

# Verbose output with all details
make integration-verbose

# Clean everything
make clean        # Remove build files
make cleanall     # Remove build files + venv + test artifacts
```

## Parallel Execution (Default)

Integration tests run in parallel by default for maximum speed:

```bash
# Use auto-detected number of CPU cores (default)
make integration

# Use specific number of workers
make integration PARALLEL_WORKERS=4

# Direct pytest with parallel execution
cd tests/integration
pytest -n auto          # Auto-detect CPU cores
pytest -n 4             # Use 4 workers
pytest -n logical       # Use logical CPU count
```

**Performance Comparison:**

| Mode | Time | Speedup |
|------|------|---------|
| Sequential | ~81s | 1x |
| **Parallel (default)** | **~8s** | **10x** 🚀 |

**Note:** Parallel execution shows deprecation warnings about `forkpty()` in multi-threaded processes. These are harmless warnings from Python 3.12+ and the tests pass correctly. If you want zero warnings, use the `-seq` variants (e.g., `make integration-seq`).
**Note:** The `make` integration targets export `PYTHONWARNINGS="ignore::DeprecationWarning:pty"`, so `make integration` and `make test` stay quiet by default. If you run raw `pytest -n ...` directly, Python 3.14 may still print the `forkpty()` deprecation warning unless you pass the same warning filter yourself.

## Status

✅ **34 tests passing** across 5 test modules:

| Module | Tests | Description |
|--------|-------|-------------|
| `test_smoke.py` | 7 | Basic operations (launch, quit, create/open files) |
| `test_navigation.py` | 9 | Cursor movement (hjkl, w/b/e, G/gg) |
| `test_editing.py` | 8 | Text editing (insert, delete, append) |
| `test_file_ops.py` | 7 | File operations (save, save-as, :wq, :q!) |
| `test_workflows.py` | 5 | Complex scenarios (C coding, config editing) |

## Manual Setup (if needed)

If you prefer to run tests directly without Make:

```bash
# 1. Build the editor first
cd /home/mateusz/repos/yasvi
make

# 2. Setup Python environment
cd tests/integration
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt

# 3. Run all tests (parallel by default)
pytest -n auto

# 4. Run tests sequentially
pytest

# 5. Run specific test categories
pytest -m smoke          # Basic tests only
pytest -m editing        # Editing tests
pytest -m navigation     # Navigation tests
pytest -m fileops        # File operations
pytest -m workflow       # Complex workflows

# 6. Debug mode (verbose output)
pytest -v -s
```

## Architecture

```
┌─────────────────┐     keystrokes      ┌─────────────┐
│  Python Test    │ ──────────────────► │   Editor    │
│  (pexpect)      │                     │   (ncurses) │
│                 │ ◄────────────────── │             │
└─────────────────┘    screen output    └──────┬──────┘
                                               │
                                               ▼ file I/O
                                        ┌─────────────┐
                                        │  Test Files │
                                        │  (temp dir) │
                                        └─────────────┘
```

## EditorSession API

```python
# Create session and open file
editor = EditorSession(
    editor_path="build/vi",
    working_dir=temp_dir,
    screenshots_dir="screenshots"
)
editor.open_file("test.txt")

# Edit using vim-like commands
editor.enter_insert_mode()
editor.type_text("Hello, World!")
editor.exit_insert_mode()

# Navigate
editor.send_key("j")          # down
editor.send_keys("3j")        # down 3 lines
editor.send_key("G")          # go to bottom

# Save and quit
editor.save_and_quit()
```

## Test Categories

### Smoke Tests (Tier 1)
- `test_launch_and_quit` - Basic launch and exit
- `test_create_new_file` - Create and save new file
- `test_open_existing_file` - Open existing file
- `test_quit_without_save` - Discard changes with :q!

### Navigation Tests (Tier 2)
- `test_hjkl_navigation` - Basic movement
- `test_word_navigation` - w/b/e word movement
- `test_line_navigation` - ^/$ line boundaries
- `test_go_to_bottom` - G to go to end
- `test_scroll_large_file` - Scroll through 1000+ lines

### Editing Tests (Tier 3)
- `test_insert_text_at_cursor` - Insert mode (i)
- `test_append_text_after_cursor` - Append mode (a)
- `test_delete_character_with_x` - Delete char (x)
- `test_type_multiline_text` - Multi-line editing
- `test_edit_existing_file` - Modify existing content

### File Operations (Tier 4)
- `test_save_existing_file` - :w save
- `test_save_as_new_file` - :w filename
- `test_save_and_quit` - :wq
- `test_force_quit_no_save` - :q!
- `test_multiple_saves` - Multiple :w during session

### Workflows (Tier 5)
- `test_write_c_function` - Write complete C function
- `test_add_include_to_c_file` - Add #include directive
- `test_edit_config_file` - Modify config file
- `test_quick_notes` - Note-taking workflow

## Test Fixtures

Sample files for testing:

| File | Purpose |
|------|---------|
| `simple.txt` | Basic multi-line text (6 lines) |
| `code.c` | C source with syntax highlighting |
| `large.txt` | 1000 lines for scroll testing |
| `empty.txt` | Empty file edge case |
| `special_chars.txt` | Unicode, tabs, special chars |

## Debugging Failed Tests

1. **Screenshots**: Check `screenshots/` directory for captured state on failure
2. **Verbose output**: `pytest -v -s` shows print statements
3. **Single test**: `pytest test_cases/test_smoke.py::test_launch_and_quit -v -s`
4. **Keep temp files**: `pytest --keep-temp` preserves test directories
5. **Sequential for debugging**: Run tests sequentially if parallel causes issues: `make integration-seq`

## Continuous Integration

For CI environments (GitHub Actions, etc.):

```yaml
- name: Install dependencies
  run: |
    sudo apt-get install -y xvfb

- name: Build and test
  run: |
    make
    xvfb-run make test   # Parallel by default (fast!)
```

Or with specific worker count:

```yaml
- name: Test
  run: |
    make
    xvfb-run make integration PARALLEL_WORKERS=2
```

Xvfb provides a virtual framebuffer for headless environments.

## Adding New Tests

```python
import pytest

@pytest.mark.editing
def test_my_feature(editor, temp_dir):
    """Test description."""
    editor.new_file("test.txt")
    
    # Act
    editor.enter_insert_mode()
    editor.type_text("Hello")
    editor.exit_insert_mode()
    editor.save_and_quit()
    
    # Assert
    content = (temp_dir / "test.txt").read_text()
    assert "Hello" in content
```

## Directory Structure

```
tests/integration/
├── README.md              # This file
├── PLAN.md                # Detailed plan document
├── INTEGRATION_TESTS.md   # Architecture documentation
├── requirements.txt       # Python dependencies
├── pytest.ini            # pytest configuration
├── conftest.py           # Shared fixtures
├── editor_session.py     # Core wrapper class
├── fixtures/             # Test data files
│   ├── simple.txt
│   ├── code.c
│   ├── large.txt
│   ├── empty.txt
│   └── special_chars.txt
├── screenshots/          # Debug output (gitignored)
└── test_cases/           # Test modules
    ├── test_smoke.py
    ├── test_navigation.py
    ├── test_editing.py
    ├── test_file_ops.py
    └── test_workflows.py
```

## Troubleshooting

**Test fails with "Editor binary not found"**
- Run `make` in the project root to build the editor

**Tests timeout**
- Increase timeout in `editor_session.py` or `pytest.ini`
- Check if editor is hanging with `ps aux | grep vi`

**"Terminal not found" errors**
- Tests require a terminal environment
- Use `xvfb-run` for headless CI environments

**Screenshots not captured**
- Check `screenshots/` directory exists and is writable
- Look for files named `FAILURE_*.txt`

**Parallel test warnings about forkpty**
- These are harmless Python 3.12+ deprecation warnings
- The tests pass correctly despite the warnings
- Use sequential mode (`make integration-seq`) if you want zero warnings

**Tests fail in parallel but pass sequentially**
- Some tests may have timing dependencies
- Use `make integration-seq` (sequential) for debugging
- Or run specific failing test in isolation
