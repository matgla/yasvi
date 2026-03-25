# yasvi Testing Improvement Plan

## Current State Analysis

### Existing Tests
| File | Coverage | Status |
|------|----------|--------|
| `buffer_tests.c` | Buffer allocation, row operations, word navigation, character insertion/removal | ✅ Good |
| `command_tests.c` | Only basic malloc/realloc test (placeholder) | ❌ Minimal |

### Test Coverage Gaps

#### 1. BufferRow (`buffer_row.c`) - 400+ lines
**Untested Functions:**
- `buffer_row_has_whitespace_at_position()`
- `buffer_row_get_length()`
- `buffer_row_get_offset_to_first_char()`
- `buffer_row_replace_line()`
- `buffer_row_remove_chars()` (plural - bulk removal)
- `buffer_row_insert_chars()` (plural - bulk insertion)
- `buffer_row_trim()`
- `buffer_row_append_char()`
- `buffer_row_append_str()`
- `buffer_row_break_line()`
- `buffer_row_get_next()` / `buffer_row_get_prev()`
- `buffer_row_mark_dirty()`
- `buffer_row_set_highlight()`
- `buffer_row_highlight_line()` - **Critical**: Complex C syntax highlighting

#### 2. Buffer (`buffer.c`) - 350+ lines
**Untested Functions:**
- `buffer_load_from_file()` - File I/O operations
- `buffer_get_first_row()` / `buffer_get_row()` / `buffer_get_current_line()`
- `buffer_remove_row()` - Row removal from middle of list
- `buffer_remove_current_row()` - Edge cases (first/last/only row)
- `buffer_scroll_rows()` - Navigation with boundary conditions
- `buffer_scroll_to_top()`
- `buffer_current_is_first_row()` / `buffer_current_is_last_row()`
- `buffer_get_number_of_lines()`
- `buffer_break_current_line()` - Line breaking for 'o'/'O' commands
- `buffer_join_current_line_with_previous()` - For backspace at line start
- `buffer_get_filename()`

#### 3. Command (`command.c`) - 58 lines
**Untested Functions:**
- `command_init()` - Initialization state
- `command_append()` - Buffer growth/reallocation
- `command_deinit()` - Cleanup
- `command_error()` - Error message formatting

#### 4. Editor (`editor.c`) - 700+ lines - **No Tests**
**Key Areas to Test:**
- State machine transitions (Normal → Insert → Command → Exit)
- Key sequence processing
- Cursor movement and positioning
- Command parsing (`:w`, `:q`, `:wq`, etc.)
- File save/load integration
- Screen coordination logic

#### 5. Window (`window.c`) - UI Initialization
**Untested Functions:**
- `window_init()` / `window_deinit()`
- Color pair setup

---

## Testing Strategy

### Approach: Layered Testing Pyramid

```
       /\
      /  \     Integration Tests (End-to-end)
     /____\    
    /      \   Module Tests (Editor, File I/O)
   /________\  
  /          \ Unit Tests (Buffer, BufferRow, Command)
 /____________\
```

### Phase 1: BufferRow Unit Tests (Priority: HIGH)

**Goal**: Achieve >90% coverage for `buffer_row.c`

**Test Categories:**

#### 1.1 Character Operations
```c
// buffer_row_character_tests.c
void test_buffer_row_insert_char_at_start();
void test_buffer_row_insert_char_at_end();
void test_buffer_row_insert_char_at_middle();
void test_buffer_row_insert_char_reallocation();
void test_buffer_row_remove_char_at_start();
void test_buffer_row_remove_char_at_end();
void test_buffer_row_remove_char_at_middle();
void test_buffer_row_remove_char_invalid_index();
void test_buffer_row_append_char();
void test_buffer_row_append_char_reallocation();
```

#### 1.2 String Operations
```c
// buffer_row_string_tests.c
void test_buffer_row_replace_line();
void test_buffer_row_replace_line_shorter();
void test_buffer_row_replace_line_longer();
void test_buffer_row_insert_chars();
void test_buffer_row_insert_chars_bulk();
void test_buffer_row_append_str();
void test_buffer_row_trim();
void test_buffer_row_trim_empty();
```

#### 1.3 Navigation & Utilities
```c
// buffer_row_navigation_tests.c
void test_buffer_row_get_length();
void test_buffer_row_get_length_empty();
void test_buffer_row_has_whitespace_at_position();
void test_buffer_row_get_offset_to_first_char();
void test_buffer_row_get_offset_to_first_char_all_whitespace();
void test_buffer_row_get_next();
void test_buffer_row_get_prev();
void test_buffer_row_mark_dirty();
```

#### 1.4 Line Operations
```c
// buffer_row_line_tests.c
void test_buffer_row_break_line_at_start();
void test_buffer_row_break_line_at_end();
void test_buffer_row_break_line_at_middle();
```

#### 1.5 Syntax Highlighting (Complex)
```c
// buffer_row_highlight_tests.c
void test_highlight_keywords();
void test_highlight_types();
void test_highlight_strings();
void test_highlight_strings_multiline();
void test_highlight_comments_single_line();
void test_highlight_comments_multiline();
void test_highlight_preprocessor();
void test_highlight_numbers();
void test_highlight_symbols();
void test_highlight_mixed_content();
```

### Phase 2: Buffer Unit Tests (Priority: HIGH)

**Goal**: Achieve >90% coverage for `buffer.c`

**Test Categories:**

#### 2.1 Lifecycle & Accessors
```c
// buffer_lifecycle_tests.c
void test_buffer_alloc_initialization();
void test_buffer_free_null();
void test_buffer_free_with_rows();
void test_buffer_get_first_row();
void test_buffer_get_row_valid();
void test_buffer_get_row_out_of_bounds();
void test_buffer_get_current_line();
void test_buffer_get_number_of_lines();
```

#### 2.2 Row Management
```c
// buffer_row_management_tests.c
void test_buffer_append_line_single();
void test_buffer_append_line_multiple();
void test_buffer_append_line_strips_newlines();
void test_buffer_append_line_null();
void test_buffer_remove_row_middle();
void test_buffer_remove_row_head();
void test_buffer_remove_row_tail();
void test_buffer_remove_current_row_only_row();
void test_buffer_remove_current_row_first();
void test_buffer_remove_current_row_last();
void test_buffer_remove_current_row_middle();
```

#### 2.3 Navigation
```c
// buffer_navigation_tests.c
void test_buffer_scroll_rows_forward();
void test_buffer_scroll_rows_backward();
void test_buffer_scroll_rows_past_end();
void test_buffer_scroll_rows_past_start();
void test_buffer_scroll_to_top();
void test_buffer_current_is_first_row();
void test_buffer_current_is_last_row();
```

#### 2.4 Line Operations
```c
// buffer_line_ops_tests.c
void test_buffer_break_current_line_at_start();
void test_buffer_break_current_line_at_end();
void test_buffer_break_current_line_at_middle();
void test_buffer_break_current_line_empty_buffer();
void test_buffer_join_current_line_with_previous_normal();
void test_buffer_join_current_line_with_previous_first_row();
void test_buffer_join_current_line_with_previous_returns_count();
```

#### 2.5 File I/O
```c
// buffer_file_tests.c
void test_buffer_load_from_file_existing();
void test_buffer_load_from_file_nonexistent();
void test_buffer_load_from_file_empty();
void test_buffer_load_from_file_large();
void test_buffer_get_filename();
```

### Phase 3: Command Unit Tests (Priority: MEDIUM)

**Goal**: Achieve 100% coverage for `command.c`

```c
// command_tests.c
void test_command_init();
void test_command_append_single();
void test_command_append_multiple();
void test_command_append_reallocation();
void test_command_append_many_chars();
void test_command_deinit();
void test_command_error_simple();
void test_command_error_with_content();
```

### Phase 4: Editor Unit Tests with Mocking (Priority: MEDIUM)

**Challenge**: `editor.c` depends on ncurses for UI.

**Solution**: Create testable core + mocking layer

#### 4.1 Refactoring for Testability
Create `editor_core.c` with pure logic (no ncurses calls):
- State machine transitions
- Command parsing
- Cursor position calculations
- Buffer operations coordination

#### 4.2 Mock Framework
```c
// mock_ncurses.h
// Stub implementations for ncurses functions used in tests
void mock_initscr(void);
void mock_endwin(void);
int mock_getch(void);
void mock_mvaddch(int y, int x, char c);
// ... etc
```

#### 4.3 Editor Test Categories
```c
// editor_state_tests.c
void test_editor_init();
void test_editor_deinit();
void test_editor_state_transition_normal_to_insert();
void test_editor_state_transition_insert_to_normal();
void test_editor_state_transition_normal_to_command();
void test_editor_should_exit();

// editor_key_sequence_tests.c
void test_editor_process_key_sequence_h();
void test_editor_process_key_sequence_j();
void test_editor_process_key_sequence_k();
void test_editor_process_key_sequence_l();
void test_editor_process_key_sequence_w();
void test_editor_process_key_sequence_b();
void test_editor_process_key_sequence_dd();
void test_editor_process_key_sequence_dw();
void test_editor_process_key_sequence_numbers();

// editor_command_tests.c
void test_editor_command_save();
void test_editor_command_quit();
void test_editor_command_save_and_quit();
void test_editor_command_invalid();
void test_editor_command_cancel();
```

### Phase 5: Integration Tests (Priority: LOW)

**Goal**: Test full user workflows

#### 5.1 File-based Integration
```c
// integration_file_tests.c
void test_create_edit_save_file();
void test_load_edit_save_file();
void test_multiple_buffers();
```

#### 5.2 Script-based Testing
Create a test harness that:
1. Starts editor with input script
2. Feeds keystrokes
3. Captures output
4. Verifies file content

Example test script format:
```
# test_basic_edit.txt
iHello World\x1b:wq test_output.txt\n
# Expected: test_output.txt contains "Hello World"
```

### Phase 6: Test Infrastructure Improvements (Priority: MEDIUM)

#### 6.1 Coverage Reporting
```makefile
# Add to Makefile
coverage:
    $(CC) $(CFLAGS) --coverage -c $< -o $@
    gcov $(SRCS)
    lcov --capture --directory . --output-file coverage.info
    genhtml coverage.info --output-directory coverage_html
```

#### 6.2 Continuous Integration
- GitHub Actions workflow for automated testing
- Run tests on multiple compilers (gcc, clang, armv8m-tcc)
- AddressSanitizer and UBSan in CI

#### 6.3 Test Organization
```
tests/
├── unit/
│   ├── buffer_row/
│   │   ├── character_tests.c
│   │   ├── string_tests.c
│   │   ├── navigation_tests.c
│   │   ├── line_tests.c
│   │   └── highlight_tests.c
│   ├── buffer/
│   │   ├── lifecycle_tests.c
│   │   ├── row_management_tests.c
│   │   ├── navigation_tests.c
│   │   ├── line_ops_tests.c
│   │   └── file_tests.c
│   ├── command/
│   │   └── command_tests.c
│   └── editor/
│       ├── state_tests.c
│       ├── key_sequence_tests.c
│       └── command_tests.c
├── integration/
│   └── file_tests.c
├── fixtures/
│   ├── empty_file.c
│   ├── single_line.c
│   ├── multi_line.c
│   ├── syntax_highlight.c
│   └── large_file.c
├── mocks/
│   └── mock_ncurses.c
├── acutest.h
└── Makefile
```

---

## Implementation Priority

### Immediate (Week 1-2)
1. **Expand BufferRow tests** - Critical functionality, many untested functions
2. **Expand Buffer tests** - Core data structure, file I/O needs testing
3. **Proper Command tests** - Currently placeholder only

### Short-term (Week 3-4)
4. **Syntax highlighting tests** - Complex logic prone to bugs
5. **Test infrastructure** - Coverage reporting, better organization
6. **Edge case documentation** - Document expected behavior for edge cases

### Medium-term (Month 2)
7. **Editor core refactoring** - Separate UI from logic
8. **Editor unit tests** - With mocking framework
9. **Integration tests** - End-to-end workflows

### Long-term (Month 3+)
10. **CI/CD pipeline** - Automated testing on commits
11. **Performance tests** - Large file handling
12. **Fuzzing** - Input validation testing

---

## Test Case Examples

### Example: Buffer Row Character Removal Edge Cases
```c
void test_buffer_row_remove_char_edge_cases(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "ab");
  BufferRow* row = buffer->current_row;
  
  // Remove at index 0
  buffer_row_remove_char(row, 0);
  TEST_CHECK(strcmp(row->data, "b") == 0);
  TEST_CHECK(row->len == 1);
  
  // Remove at last index
  buffer_row_remove_char(row, 0);
  TEST_CHECK(strcmp(row->data, "") == 0);
  TEST_CHECK(row->len == 0);
  
  // Try remove from empty (should handle gracefully)
  buffer_row_remove_char(row, 0);  // Should not crash
  
  buffer_free(buffer);
}
```

### Example: Syntax Highlighting Test
```c
void test_highlight_c_keywords(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "int main() { return 0; }");
  BufferRow* row = buffer->current_row;
  
  // Check that 'int' is marked as Type
  TEST_CHECK(row->highlight_data[0] == EHighlightToken_Type);
  TEST_CHECK(row->highlight_data[1] == EHighlightToken_Type);
  TEST_CHECK(row->highlight_data[2] == EHighlightToken_Type);
  
  // Check that 'return' is marked as Keyword
  // (position depends on exact string)
  
  buffer_free(buffer);
}
```

### Example: File I/O Test
```c
void test_buffer_load_from_file(void) {
  // Create temporary test file
  const char* test_file = "/tmp/test_file.txt";
  FILE* f = fopen(test_file, "w");
  fprintf(f, "Line 1\nLine 2\nLine 3\n");
  fclose(f);
  
  Buffer* buffer = buffer_alloc();
  buffer_load_from_file(buffer, test_file);
  
  TEST_CHECK(buffer->number_of_rows == 3);
  TEST_CHECK(strcmp(buffer->head->data, "Line 1") == 0);
  TEST_CHECK(strcmp(buffer->tail->data, "Line 3") == 0);
  
  buffer_free(buffer);
  remove(test_file);
}
```

---

## Success Metrics

| Metric | Current | Target |
|--------|---------|--------|
| Total test functions | ~6 | 100+ |
| Line coverage (buffer_row.c) | ~30% | >90% |
| Line coverage (buffer.c) | ~40% | >90% |
| Line coverage (command.c) | ~10% | 100% |
| Line coverage (editor.c) | 0% | >70% |
| Test execution time | <1s | <5s |
| CI/CD pass rate | N/A | 100% |

---

## Appendix: Existing TODO Items Integration

From `todo` file:
- `[ ] backspace joins lines` → Add test: `test_buffer_join_current_line_with_previous()`
- `[ ] 'dw' removes word` → Add test: `test_editor_process_key_sequence_dw()`
