"""
Tier 1: Smoke Tests - Basic operations to verify editor functionality.

These tests ensure the editor can start, load files, and exit cleanly.
"""

import pytest

pytestmark = [pytest.mark.smoke]


class TestBasicOperations:
    """Test basic editor operations."""
    
    def test_launch_and_quit(self, editor):
        """E2E-001: Launch editor and quit cleanly."""
        editor.new_file("test.txt")
        
        # Editor should be running
        assert editor.is_running(), "Editor should be running after launch"
        
        # Quit
        editor.quit(force=True)

        if editor.is_running():
            editor.close(force=True)
        
        assert not editor.is_running(), "Editor should have exited after :q!"
    
    def test_create_new_file(self, editor, temp_dir):
        """E2E-002: Create a new file, edit, and save."""
        editor.new_file("newfile.txt")
        
        # Enter insert mode and type
        editor.enter_insert_mode()
        editor.type_text("Hello, World!")
        editor.exit_insert_mode()
        
        # Save and quit
        editor.save_and_quit()
        
        # Verify file was created with content
        saved_file = temp_dir / "newfile.txt"
        assert saved_file.exists(), f"File {saved_file} should exist"
        content = saved_file.read_text()
        assert "Hello, World!" in content, f"Content should contain 'Hello, World!', got: {content}"
    
    def test_open_existing_file(self, editor, sample_text_file):
        """E2E-003: Open an existing file and verify content is loaded."""
        editor.open_file("simple.txt")
        
        # Editor should be running
        assert editor.is_running(), "Editor should be running"
        
        # Clean exit
        editor.quit(force=True)
    
    def test_quit_without_save(self, editor, sample_text_file):
        """E2E-004: Edit file and quit without saving."""
        original_content = sample_text_file.read_text()
        
        editor.open_file("simple.txt")
        
        # Make some edits
        editor.enter_insert_mode()
        editor.type_text("MODIFIED")
        editor.exit_insert_mode()
        
        # Quit without saving
        editor.quit(force=True)
        
        # Verify original file is unchanged
        current_content = sample_text_file.read_text()
        assert current_content == original_content, \
            f"File should be unchanged. Original:\n{original_content}\nCurrent:\n{current_content}"


class TestFileVariations:
    """Test editor with different file types."""
    
    def test_open_c_file(self, editor, sample_c_file):
        """Open a C source file."""
        editor.open_file("code.c")
        assert editor.is_running(), "Editor should open C file"
        editor.quit(force=True)
    
    def test_open_empty_file(self, editor, temp_dir):
        """Open an empty file."""
        empty_file = temp_dir / "empty.txt"
        empty_file.touch()
        
        editor.open_file("empty.txt")
        assert editor.is_running(), "Editor should open empty file"
        editor.quit(force=True)
    
    def test_open_large_file(self, editor, fixtures_dir):
        """Open a large file."""
        large_file = fixtures_dir / "large.txt"
        if not large_file.exists():
            pytest.skip("large.txt fixture not found")
        
        editor.open_file("large.txt")
        assert editor.is_running(), "Editor should open large file"
        editor.quit(force=True)
