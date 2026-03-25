"""
Tier 5: Workflow Tests - Complex realistic user scenarios.

These tests simulate real-world editing tasks.
"""

import pytest
import time

pytestmark = [pytest.mark.workflow]


class TestProgrammingWorkflows:
    """Test programming-related editing workflows."""
    
    def test_write_c_function(self, editor, temp_dir):
        """E2E-401: Write a complete C function from scratch."""
        editor.new_file("new_function.c")
        
        # Write a complete function
        editor.enter_insert_mode()
        editor.type_text("int factorial(int n) {")
        editor.send_key("return")
        editor.type_text("    if (n <= 1) {")
        editor.send_key("return")
        editor.type_text("        return 1;")
        editor.send_key("return")
        editor.type_text("    }")
        editor.send_key("return")
        editor.type_text("    return n * factorial(n - 1);")
        editor.send_key("return")
        editor.type_text("}")
        editor.exit_insert_mode()
        
        editor.save_and_quit()
        
        # Verify the file
        content = (temp_dir / "new_function.c").read_text()
        assert "int factorial(int n)" in content
        assert "if (n <= 1)" in content
        assert "return n * factorial" in content
    
    def test_add_include_to_c_file(self, editor, sample_c_file):
        """Add an #include directive to a C file."""
        editor.open_file("code.c")
        
        # Go to beginning of file
        editor.send_key("g")
        editor.send_key("g")
        editor.send_key("^")
        
        # Insert at beginning
        editor.enter_insert_mode()
        editor.type_text("#include <stdlib.h>")
        editor.send_key("return")
        editor.exit_insert_mode()
        
        editor.save_and_quit()
        
        content = sample_c_file.read_text()
        lines = content.split("\n")
        assert "#include <stdlib.h>" in lines[0]
    
    def test_modify_function_body(self, editor, sample_c_file):
        """E2E-404: Simulate a bug fix in a function."""
        editor.open_file("code.c")
        
        # Find the printf line and modify it
        # Navigate to line with printf
        for _ in range(5):  # Move down to main function
            editor.send_key("j")
        
        # Go to end of line and add a comment
        editor.send_key("$")
        editor.enter_insert_mode()
        editor.type_text("  // Print result")
        editor.exit_insert_mode()
        
        editor.save_and_quit()
        
        content = sample_c_file.read_text()
        assert "// Print result" in content


class TestConfigurationEditing:
    """Test configuration file editing workflows."""
    
    def test_edit_config_file(self, editor, temp_dir):
        """E2E-402: Edit a configuration file."""
        # Create a config file
        config_file = temp_dir / "config.txt"
        config_file.write_text("""setting1=value1
setting2=value2
setting3=value3
""")
        
        editor.open_file("config.txt")
        
        # Navigate to end of file and add a new setting
        editor.send_key("G")  # Go to bottom
        editor.send_key("$")  # End of line
        editor.enter_insert_mode()
        editor.send_key("return")
        editor.type_text("setting4=value4")
        editor.exit_insert_mode()
        
        editor.save_and_quit()
        
        content = config_file.read_text()
        assert "setting4=value4" in content


class TestNoteTaking:
    """Test note-taking workflows."""
    
    def test_quick_notes(self, editor, temp_dir):
        """E2E-403: Quickly jot down notes."""
        editor.new_file("notes.txt")
        
        editor.enter_insert_mode()
        editor.type_text("Meeting Notes - " + time.strftime("%Y-%m-%d"))
        editor.send_key("return")
        editor.send_key("return")
        editor.type_text("- Action item 1")
        editor.send_key("return")
        editor.type_text("- Action item 2")
        editor.send_key("return")
        editor.type_text("- Action item 3")
        editor.exit_insert_mode()
        
        editor.save_and_quit()
        
        content = (temp_dir / "notes.txt").read_text()
        assert "Meeting Notes" in content
        assert "Action item 1" in content
        assert "Action item 2" in content
        assert "Action item 3" in content
