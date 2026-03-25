"""
Tier 4: File Operations Tests - Saving, loading, and file management.

These tests verify file I/O functionality.
"""

import pytest

pytestmark = [pytest.mark.fileops]


class TestSaveOperations:
    """Test save operations."""
    
    def test_save_existing_file(self, editor, sample_text_file):
        """E2E-301: Save changes to existing file."""
        original_mtime = sample_text_file.stat().st_mtime
        
        editor.open_file("simple.txt")
        
        # Make an edit
        editor.enter_insert_mode()
        editor.type_text("SAVED ")
        editor.exit_insert_mode()
        
        # Save
        editor.save()
        
        # Verify file was modified
        new_mtime = sample_text_file.stat().st_mtime
        assert new_mtime >= original_mtime, "File should be modified after save"
        
        content = sample_text_file.read_text()
        assert "SAVED " in content, f"Expected 'SAVED ' in content, got: {content}"
        
        editor.quit()
    
    def test_save_as_new_file(self, editor, temp_dir):
        """E2E-302: Save to a different filename."""
        editor.new_file("original.txt")
        
        # Add content
        editor.enter_insert_mode()
        editor.type_text("Content for new file")
        editor.exit_insert_mode()
        
        # Save as different name
        editor.command("w saved_as.txt")
        
        editor.quit(force=True)
        
        # Verify new file exists
        new_file = temp_dir / "saved_as.txt"
        assert new_file.exists(), f"File {new_file} should exist"
        content = new_file.read_text()
        assert "Content for new file" in content, f"Expected content in saved file, got: {content}"
    
    def test_save_and_quit(self, editor, temp_dir):
        """E2E-303: Save and quit in one command."""
        editor.new_file("sq_test.txt")
        
        editor.enter_insert_mode()
        editor.type_text("Save and quit test")
        editor.exit_insert_mode()
        
        editor.save_and_quit()
        
        # Verify file and editor closed
        assert not editor.is_running(), "Editor should have exited"
        
        saved_file = temp_dir / "sq_test.txt"
        assert saved_file.exists(), f"File {saved_file} should exist"
        content = saved_file.read_text()
        assert "Save and quit test" in content, f"Expected content, got: {content}"
    
    def test_force_quit_no_save(self, editor, sample_text_file):
        """E2E-304: Force quit without saving."""
        original_content = sample_text_file.read_text()
        
        editor.open_file("simple.txt")
        
        # Make edits
        editor.enter_insert_mode()
        editor.type_text("SHOULD NOT BE SAVED")
        editor.exit_insert_mode()
        
        # Force quit
        editor.quit(force=True)
        
        # Verify original file unchanged
        content = sample_text_file.read_text()
        assert content == original_content, \
            f"File should be unchanged.\nOriginal: {original_content}\nCurrent: {content}"
    
    def test_multiple_saves(self, editor, temp_dir):
        """E2E-305: Multiple saves during a session."""
        editor.new_file("multi_save.txt")
        
        # First edit and save - type First with newline
        editor.enter_insert_mode()
        editor.type_text("First")
        editor.send_key("return")
        editor.exit_insert_mode()
        editor.save()
        
        # Check intermediate state
        content = (temp_dir / "multi_save.txt").read_text()
        assert "First" in content, f"Expected 'First' after first save, got: {content}"
        
        # Second edit and save - type Second
        editor.enter_insert_mode()
        editor.type_text("Second")
        editor.exit_insert_mode()
        editor.save()
        
        content = (temp_dir / "multi_save.txt").read_text()
        assert "First" in content
        assert "Second" in content, f"Expected 'Second' after second save, got: {content}"
        
        editor.quit(force=True)


class TestFileCreation:
    """Test creating new files."""
    
    def test_create_multiple_files(self, editor, temp_dir):
        """Create and edit multiple files in sequence."""
        # First file
        editor.new_file("file1.txt")
        editor.enter_insert_mode()
        editor.type_text("File 1 content")
        editor.exit_insert_mode()
        editor.save_and_quit()
        
        # Second file - need new session
        editor2 = EditorSession(
            editor_path=editor.editor_path,
            working_dir=temp_dir,
            screenshots_dir=editor.screenshots_dir
        )
        editor2.new_file("file2.txt")
        editor2.enter_insert_mode()
        editor2.type_text("File 2 content")
        editor2.exit_insert_mode()
        editor2.save_and_quit()
        
        # Verify both files
        content1 = (temp_dir / "file1.txt").read_text()
        content2 = (temp_dir / "file2.txt").read_text()
        assert "File 1 content" in content1
        assert "File 2 content" in content2
    
    def test_edit_and_save_c_file(self, editor, sample_c_file):
        """Edit and save a C source file."""
        editor.open_file("code.c")
        
        # Add a comment at the top
        editor.send_key("^")
        editor.enter_insert_mode()
        editor.type_text("/* Modified */ ")
        editor.exit_insert_mode()
        
        editor.save_and_quit()
        
        content = sample_c_file.read_text()
        assert "/* Modified */" in content, f"Expected '/* Modified */' in C file, got: {content}"


# Need to import EditorSession for the multiple files test
from editor_session import EditorSession
