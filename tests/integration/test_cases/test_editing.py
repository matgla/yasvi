"""
Tier 3: Editing Tests - Text manipulation operations.

These tests verify the core editing functionality of the editor.
"""

import pytest

pytestmark = [pytest.mark.editing]


class TestInsertMode:
    """Test insert mode operations."""
    
    def test_insert_text_at_cursor(self, editor, temp_dir):
        """E2E-201: Insert text at cursor position."""
        editor.new_file("insert_test.txt")
        
        # Enter insert mode and type
        editor.enter_insert_mode()
        editor.type_text("Hello, World!")
        editor.exit_insert_mode()
        
        # Save and verify
        editor.save_and_quit()
        
        content = (temp_dir / "insert_test.txt").read_text()
        assert "Hello, World!" in content, f"Expected 'Hello, World!' in content, got: {content}"
    
    def test_append_text_after_cursor(self, editor, temp_dir):
        """E2E-202: Append text after cursor using 'a'."""
        editor.new_file("append_test.txt")
        
        # Type something first
        editor.enter_insert_mode()
        editor.type_text("Hello")
        editor.exit_insert_mode()
        
        # Move back and append
        editor.send_key("left")
        editor.send_key("a")  # append mode
        editor.type_text(" World")
        editor.exit_insert_mode()
        
        editor.save_and_quit()
        
        content = (temp_dir / "append_test.txt").read_text()
        assert "Hello World" in content, f"Expected 'Hello World', got: {content}"
    
    def test_type_multiline_text(self, editor, temp_dir):
        """E2E-205: Type a full paragraph with multiple lines."""
        editor.new_file("multiline_test.txt")
        
        editor.enter_insert_mode()
        editor.type_text("Line 1")
        editor.send_key("return")
        editor.type_text("Line 2")
        editor.send_key("return")
        editor.type_text("Line 3")
        editor.exit_insert_mode()
        
        editor.save_and_quit()
        
        content = (temp_dir / "multiline_test.txt").read_text()
        lines = content.strip().split("\n")
        assert len(lines) == 3, f"Expected 3 lines, got {len(lines)}: {lines}"
        assert "Line 1" in lines[0]
        assert "Line 2" in lines[1]
        assert "Line 3" in lines[2]
    
    def test_insert_at_line_start(self, editor, sample_text_file, temp_dir):
        """E2E-207: Insert text at beginning of line using ^ and i."""
        editor.open_file("simple.txt")
        
        # Go to start of line and insert
        editor.send_key("^")  # Go to first non-blank
        editor.enter_insert_mode()
        editor.type_text("PREFIX ")
        editor.exit_insert_mode()
        
        editor.save_and_quit()
        
        content = sample_text_file.read_text()
        assert "PREFIX" in content, f"Expected 'PREFIX' in content, got: {content}"


class TestDeletion:
    """Test deletion operations."""
    
    def test_delete_character_with_x(self, editor, temp_dir):
        """E2E-203: Delete character at cursor using 'x'."""
        editor.new_file("delete_test.txt")
        
        # Type something
        editor.enter_insert_mode()
        editor.type_text("Helxo World")
        editor.exit_insert_mode()
        
        # Navigate to the 'x' and delete it
        # Start at end, move left 7 to reach 'x'
        for _ in range(7):
            editor.send_key("h")
        editor.send_key("x")  # Delete character
        
        editor.save_and_quit()
        
        content = (temp_dir / "delete_test.txt").read_text()
        assert "Helo World" in content, f"Expected 'Helo World' (x deleted), got: {content}"


class TestComplexEdits:
    """Test more complex editing scenarios."""
    
    def test_edit_existing_file(self, editor, sample_text_file):
        """Edit an existing file and verify changes."""
        editor.open_file("simple.txt")
        
        # Navigate to a line and modify it
        editor.send_key("j")  # Move down 1 line
        editor.send_key("j")  # Move down 1 more line (line 3)
        editor.send_key("^")  # Go to start
        editor.enter_insert_mode()
        editor.type_text("MODIFIED: ")
        editor.exit_insert_mode()
        
        editor.save_and_quit()
        
        content = sample_text_file.read_text()
        assert "MODIFIED:" in content, f"Expected 'MODIFIED:' in content, got: {content}"
    
    def test_multiple_edits_and_saves(self, editor, temp_dir):
        """Make multiple edits with intermediate saves."""
        editor.new_file("multi_edit.txt")
        
        # First edit - type First and newline
        editor.enter_insert_mode()
        editor.type_text("First")
        editor.send_key("return")
        editor.exit_insert_mode()
        editor.save()
        
        # Verify first save
        content = (temp_dir / "multi_edit.txt").read_text()
        assert "First" in content
        
        # Second edit - add Second and newline
        editor.enter_insert_mode()
        editor.type_text("Second")
        editor.send_key("return")
        editor.exit_insert_mode()
        editor.save()
        
        # Third edit - add Third
        editor.enter_insert_mode()
        editor.type_text("Third")
        editor.exit_insert_mode()
        editor.save_and_quit()
        
        content = (temp_dir / "multi_edit.txt").read_text()
        assert "First" in content
        assert "Second" in content
        assert "Third" in content
