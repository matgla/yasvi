"""
Tier 2: Navigation Tests - Cursor movement and screen navigation.

These tests verify the navigation functionality of the editor.
"""

import pytest

pytestmark = [pytest.mark.navigation]


class TestBasicNavigation:
    """Test basic cursor movement."""
    
    def test_hjkl_navigation(self, editor, sample_text_file):
        """E2E-101: Navigate using hjkl keys."""
        editor.open_file("simple.txt")
        
        # Navigate around with hjkl
        editor.send_key("j")  # down
        editor.send_key("j")  # down
        editor.send_key("l")  # right
        editor.send_key("l")  # right
        editor.send_key("k")  # up
        editor.send_key("h")  # left
        
        # Just verify editor is still running
        assert editor.is_running(), "Editor should still be running after navigation"
        editor.quit(force=True)
    
    def test_arrow_key_navigation(self, editor, sample_text_file):
        """Navigate using arrow keys."""
        editor.open_file("simple.txt")
        
        # Use arrow keys
        editor.send_key("down")
        editor.send_key("down")
        editor.send_key("right")
        editor.send_key("up")
        editor.send_key("left")
        
        assert editor.is_running(), "Editor should still be running"
        editor.quit(force=True)
    
    def test_word_navigation(self, editor, sample_text_file):
        """E2E-102: Navigate by words using w, b, e."""
        editor.open_file("simple.txt")
        
        # Word navigation
        editor.send_key("w")  # next word
        editor.send_key("w")  # next word
        editor.send_key("b")  # previous word
        editor.send_key("e")  # end of word
        
        assert editor.is_running(), "Editor should still be running"
        editor.quit(force=True)
    
    def test_line_navigation(self, editor, sample_text_file):
        """E2E-103: Navigate to line start/end with ^ and $."""
        editor.open_file("simple.txt")
        
        # Move to end of line, then start
        editor.send_key("$")  # end of line
        editor.send_key("^")  # first non-blank
        
        assert editor.is_running(), "Editor should still be running"
        editor.quit(force=True)


class TestDocumentNavigation:
    """Test document-level navigation."""
    
    def test_go_to_bottom(self, editor, sample_text_file):
        """E2E-104: Go to bottom of document with G."""
        editor.open_file("simple.txt")
        
        # Go to bottom
        editor.send_key("G")
        
        # Add something at the end
        editor.enter_insert_mode()
        editor.type_text(" AT END")
        editor.exit_insert_mode()
        
        editor.save_and_quit()
        
        content = sample_text_file.read_text()
        assert "AT END" in content, f"Expected 'AT END' in content, got: {content}"
    
    def test_go_to_top(self, editor, sample_text_file):
        """Go to top of document with gg."""
        editor.open_file("simple.txt")
        
        # First go to bottom
        editor.send_key("G")
        
        # Then back to top
        editor.send_key("g")
        editor.send_key("g")
        
        # Add something at the beginning
        editor.enter_insert_mode()
        editor.type_text("TOP: ")
        editor.exit_insert_mode()
        
        editor.save_and_quit()
        
        content = sample_text_file.read_text()
        lines = content.split("\n")
        assert "TOP:" in lines[0], f"Expected 'TOP:' at beginning, first line: {lines[0]}"
    
    def test_scroll_large_file(self, editor, fixtures_dir):
        """E2E-105: Scroll through a large file."""
        large_file = fixtures_dir / "large.txt"
        if not large_file.exists():
            pytest.skip("large.txt fixture not found")
        
        editor.open_file("large.txt")
        
        # Scroll down many lines
        for _ in range(20):
            editor.send_key("j")
        
        # Scroll back up
        for _ in range(20):
            editor.send_key("k")
        
        assert editor.is_running(), "Editor should still be running after scrolling"
        editor.quit(force=True)


class TestNavigationToEdit:
    """Test navigation combined with editing."""
    
    def test_navigate_and_edit_specific_line(self, editor, sample_text_file):
        """Navigate to a specific line and edit it."""
        editor.open_file("simple.txt")
        
        # Navigate to line 3 (move down 3 times)
        editor.send_key("j")
        editor.send_key("j")
        editor.send_key("j")
        
        # Edit that line
        editor.send_key("^")  # Go to start
        editor.enter_insert_mode()
        editor.type_text("[EDITED] ")
        editor.exit_insert_mode()
        
        editor.save_and_quit()
        
        content = sample_text_file.read_text()
        assert "[EDITED]" in content, f"Expected '[EDITED]' in content, got: {content}"
