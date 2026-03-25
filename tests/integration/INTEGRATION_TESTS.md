# Yasvi Editor - End-to-End Integration Tests

## What Are Integration Tests?

Integration tests verify that the entire editor works correctly by simulating **real user interactions**. Unlike unit tests that check individual functions, these tests:

- Launch the actual editor binary
- Send real keystrokes via terminal emulation
- Operate on real files
- Verify end-to-end workflows

## Why Python + pexpect?

| Approach | Pros | Cons |
|----------|------|------|
| **Python + pexpect** | Realistic terminal interaction, readable tests, rich ecosystem | Requires Python runtime |
| Pure C unit tests | Fast, no dependencies | Can't test ncurses UI |
| Expect/Tcl | Good for terminal apps | Obscure syntax, limited ecosystem |

Python with pexpect gives us the best balance of realism and maintainability.

## How It Works

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

## Test Organization

Tests are organized by **complexity tiers**:

### Tier 1: Smoke Tests
Quick checks that basic functionality works.
```python
def test_launch_and_quit(editor):
    editor.new_file("test.txt")
    assert editor.is_running()
    editor.quit(force=True)
    assert not editor.is_running()
```

### Tier 2: Navigation
Test cursor movement commands.
- `hjkl` - Basic movement
- `w`, `b`, `e` - Word navigation
- `^`, `$` - Line boundaries
- `G`, `gg` - Document navigation

### Tier 3: Editing
Test text manipulation.
- `i` - Insert mode
- `a` - Append mode
- `x` - Delete character
- Multi-line editing

### Tier 4: File Operations
Test file I/O.
- `:w` - Save
- `:w filename` - Save as
- `:wq` - Save and quit
- `:q!` - Force quit

### Tier 5: Complex Workflows (Planned)
Realistic user scenarios.
- Write a function in C
- Edit configuration file
- Bug fix simulation

### Tier 6: Edge Cases (Planned)
Error handling and stress tests.
- Read-only files
- Very long lines
- Rapid key input

## The EditorSession API

The `EditorSession` class abstracts editor interaction:

```python
# Create session
editor = EditorSession(
    editor_path="build/vi",
    working_dir=temp_dir,
    screenshots_dir="screenshots"
)

# Open file
editor.open_file("test.txt")

# Edit (vim commands)
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

## Writing a Test

```python
import pytest

@pytest.mark.editing  # Categorize the test
def test_insert_text_at_cursor(editor, temp_dir):
    """Test that text can be inserted in insert mode."""
    # Arrange
    editor.new_file("test.txt")
    
    # Act
    editor.enter_insert_mode()
    editor.type_text("Hello, World!")
    editor.exit_insert_mode()
    editor.save_and_quit()
    
    # Assert
    content = (temp_dir / "test.txt").read_text()
    assert "Hello, World!" in content
```

## Running Tests

```bash
# Setup
cd tests/integration
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt

# Run all tests
pytest

# Run specific category
pytest -m smoke
pytest -m editing
pytest -m navigation
pytest -m fileops

# Debug mode (see output)
pytest -v -s

# Specific test
pytest test_cases/test_editing.py::test_insert_text_at_cursor
```

## Test Fixtures

Sample files for testing:

| File | Purpose |
|------|---------|
| `simple.txt` | Basic multi-line text |
| `code.c` | C source with syntax highlighting |
| `large.txt` | 1000 lines for scroll testing |
| `empty.txt` | Empty file edge case |
| `special_chars.txt` | Unicode, tabs, special chars |

## Debugging Failed Tests

1. **Screenshots**: Check `screenshots/` directory for captured screen state
2. **Verbose output**: `pytest -v -s` shows print statements
3. **Interactive**: Add `import pdb; pdb.set_trace()` to pause execution

## Continuous Integration

For CI environments (GitHub Actions, etc.), you may need:

```yaml
# Example GitHub Actions setup
- name: Install dependencies
  run: |
    sudo apt-get install -y xvfb
    pip install -r tests/integration/requirements.txt

- name: Run E2E tests
  run: |
    xvfb-run pytest tests/integration/
```

Xvfb provides a virtual framebuffer for headless environments.

## Future Enhancements

1. **Visual regression** - Compare screenshots between runs
2. **Performance tests** - Measure startup time, file load time
3. **Fuzzing** - Randomized key sequence testing
4. **Recording** - Capture and replay real user sessions

## Summary

This integration test framework provides:

- ✅ Realistic end-to-end testing
- ✅ Readable, maintainable Python code
- ✅ Organized by test tiers
- ✅ Good debugging capabilities
- ✅ Extensible architecture

The tests live in `tests/integration/` and can be run with `pytest`.
