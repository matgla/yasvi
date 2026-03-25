"""
EditorSession - A wrapper for interacting with the Yasvi editor via pexpect.

This module provides a high-level interface for controlling the editor
in end-to-end tests, abstracting away the details of terminal interaction.
"""

import pexpect
import time
import os
from pathlib import Path
from typing import Callable, Optional, Tuple


class EditorSession:
    """
    Manages an editor session for E2E testing.
    
    Usage:
        session = EditorSession(editor_path="build/vi")
        session.open_file("test.txt")
        session.send_keys("iHello World")
        session.send_key("esc")
        session.command("wq")
        session.close()
    """
    
    # Key mappings for special keys
    KEY_ESC = "\x1b"
    KEY_ENTER = "\r"
    KEY_BACKSPACE = "\x7f"
    KEY_TAB = "\t"
    KEY_UP = "\x1b[A"
    KEY_DOWN = "\x1b[B"
    KEY_RIGHT = "\x1b[C"
    KEY_LEFT = "\x1b[D"
    KEY_HOME = "\x1b[H"
    KEY_END = "\x1b[F"
    KEY_DELETE = "\x1b[3~"
    DEFAULT_LAUNCH_DELAY = 0.05
    DEFAULT_INPUT_DELAY = 0.002
    DEFAULT_COMMAND_DELAY = 0.03
    DEFAULT_POLL_INTERVAL = 0.01
    DEFAULT_EXIT_TIMEOUT = 1.0
    
    def __init__(
        self,
        editor_path: Path,
        working_dir: Path,
        screenshots_dir: Path,
        dimensions: Tuple[int, int] = (24, 80),
        timeout: float = 5.0
    ):
        """
        Initialize the editor session.
        
        Args:
            editor_path: Path to the editor binary
            working_dir: Directory for test files
            screenshots_dir: Directory for debug screenshots
            dimensions: Terminal dimensions (rows, cols)
            timeout: Default timeout for operations
        """
        self.editor_path = Path(editor_path)
        self.working_dir = Path(working_dir)
        self.screenshots_dir = Path(screenshots_dir)
        self.dimensions = dimensions
        self.timeout = timeout
        
        self.process: Optional[pexpect.spawn] = None
        self.current_file: Optional[Path] = None
        self._screenshot_counter = 0
        self._closed = False
        self.launch_delay = self.DEFAULT_LAUNCH_DELAY
        self.input_delay = self.DEFAULT_INPUT_DELAY
        self.command_delay = self.DEFAULT_COMMAND_DELAY
        
        # Ensure screenshots directory exists
        self.screenshots_dir.mkdir(parents=True, exist_ok=True)
        
    def open_file(self, filename: str, wait_for_load: bool = True) -> "EditorSession":
        """
        Open a file in the editor.
        
        Args:
            filename: Name of file to open (relative to working_dir)
            wait_for_load: Whether to wait for editor to load
            
        Returns:
            self for method chaining
        """
        if self.process is not None:
            raise RuntimeError("Editor already started. Call close() first.")
        
        self.current_file = self.working_dir / filename
        
        # Set up terminal environment
        env = os.environ.copy()
        env["LINES"] = str(self.dimensions[0])
        env["COLUMNS"] = str(self.dimensions[1])
        env["TERM"] = "xterm-256color"
        
        cmd = f"{self.editor_path} {self.current_file}"
        
        # Use spawnu for unicode mode
        self.process = pexpect.spawn(
            cmd,
            cwd=self.working_dir,
            env=env,
            dimensions=self.dimensions,
            timeout=self.timeout,
            encoding="utf-8",
            codec_errors="ignore"
        )
        
        if wait_for_load:
            self.wait(self.launch_delay)
            
        return self

    def _resolve_delay(self, delay: Optional[float], default: float) -> float:
        return default if delay is None else delay

    def _wait_until(self, predicate: Callable[[], bool], timeout: float) -> bool:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if predicate():
                return True
            time.sleep(self.DEFAULT_POLL_INTERVAL)
        return predicate()
    
    def new_file(self, filename: str) -> "EditorSession":
        """Create a new file in the editor."""
        return self.open_file(filename)
    
    def send_key(self, key: str, delay: Optional[float] = None) -> "EditorSession":
        """
        Send a single key press.
        
        Special keys: 'esc', 'enter', 'tab', 'up', 'down', 'left', 'right',
                     'home', 'end', 'backspace', 'delete'
        """
        if self.process is None:
            raise RuntimeError("Editor not started. Call open_file() first.")
        
        if self._closed:
            raise RuntimeError("Editor session already closed.")
        
        key_map = {
            "esc": self.KEY_ESC,
            "enter": self.KEY_ENTER,
            "return": self.KEY_ENTER,
            "tab": self.KEY_TAB,
            "up": self.KEY_UP,
            "down": self.KEY_DOWN,
            "left": self.KEY_LEFT,
            "right": self.KEY_RIGHT,
            "home": self.KEY_HOME,
            "end": self.KEY_END,
            "backspace": self.KEY_BACKSPACE,
            "delete": self.KEY_DELETE,
        }
        
        key_lower = key.lower()
        if key_lower in key_map:
            self.process.send(key_map[key_lower])
        else:
            self.process.send(key)
        
        delay = self._resolve_delay(delay, self.input_delay)
        if delay > 0:
            time.sleep(delay)
            
        return self
    
    def send_keys(self, keys: str, delay: Optional[float] = None) -> "EditorSession":
        """Send a sequence of keys."""
        for key in keys:
            self.send_key(key, delay=delay)
        return self
    
    def enter_insert_mode(self) -> "EditorSession":
        """Enter insert mode by pressing 'i'."""
        return self.send_key("i")
    
    def exit_insert_mode(self) -> "EditorSession":
        """Exit insert mode by pressing Escape."""
        return self.send_key("esc")
    
    def type_text(self, text: str, delay: Optional[float] = None) -> "EditorSession":
        """
        Type text while in insert mode.
        
        Automatically handles newlines.
        """
        delay = self._resolve_delay(delay, self.input_delay)
        for char in text:
            if char == "\n":
                self.send_key("return", delay=delay)
            else:
                self.process.send(char)
                if delay > 0:
                    time.sleep(delay)
        return self
    
    def command(self, cmd: str, delay: Optional[float] = None) -> "EditorSession":
        """
        Execute a command (e.g., 'w', 'q', 'wq').
        
        Automatically adds ':' prefix and Enter suffix.
        """
        delay = self._resolve_delay(delay, self.command_delay)
        self.send_key(":", delay=delay)
        self.send_keys(cmd, delay=self.input_delay)
        self.send_key("enter", delay=delay)
        return self
    
    def save(self) -> "EditorSession":
        """Save the current file (:w)."""
        return self.command("w", delay=self.command_delay)

    def wait_until_closed(self, timeout: float = DEFAULT_EXIT_TIMEOUT) -> "EditorSession":
        """Wait until the editor process exits."""
        if self.process is None:
            return self

        self._wait_until(lambda: not self.is_running(), timeout)
        if self.process is not None and not self.process.isalive():
            self.process = None
        return self
    
    def quit(self, force: bool = False) -> "EditorSession":
        """
        Quit the editor (:q or :q!).
        
        Args:
            force: If True, use :q! to force quit without saving
        """
        if self.process is None or not self.is_running():
            self.process = None
            return self

        cmd = "q!" if force else "q"
        self.command(cmd, delay=self.command_delay)
        return self.wait_until_closed()
    
    def save_and_quit(self) -> "EditorSession":
        """Save and quit (:wq)."""
        self.command("wq", delay=self.command_delay)
        return self.wait_until_closed()
    
    def navigate(self, direction: str, count: int = 1) -> "EditorSession":
        """
        Navigate in a direction.
        
        Args:
            direction: 'h', 'j', 'k', 'l', 'up', 'down', 'left', 'right',
                      'w', 'b', 'e', 'gg', 'G', '^', '$'
            count: Number of times to repeat
        """
        if direction in ("gg", "G"):
            # These are single commands, not repeatable
            self.send_keys(direction)
        else:
            if count > 1:
                self.send_keys(str(count))
            self.send_keys(direction)
        return self
    
    def get_screen_content(self) -> str:
        """Get the current screen content as text (best effort)."""
        if self.process is None:
            return ""
        # Read any pending output
        try:
            # This is a simplified approach - reading terminal output is complex
            return self.process.before or ""
        except:
            return ""
    
    def screenshot(self, name: Optional[str] = None) -> Path:
        """
        Capture a screenshot of the current terminal state.
        
        Args:
            name: Screenshot name (auto-generated if None)
            
        Returns:
            Path to saved screenshot
        """
        if name is None:
            self._screenshot_counter += 1
            name = f"screenshot_{self._screenshot_counter:03d}"
        
        screenshot_path = self.screenshots_dir / f"{name}.txt"
        
        # Save current state info
        content = []
        content.append(f"=== Screenshot: {name} ===")
        content.append(f"Current file: {self.current_file}")
        content.append(f"Editor running: {self.is_running()}")
        content.append(f"Screen content (raw):\n{self.get_screen_content()}")
        
        # Try to get file content
        file_content = self.get_file_content()
        if file_content:
            content.append(f"\n=== File content ===\n{file_content}")
        
        screenshot_path.write_text("\n".join(content))
        
        return screenshot_path
    
    def screenshot_on_failure(self, test_name: str) -> None:
        """Capture screenshot if test fails."""
        self.screenshot(f"FAILURE_{test_name}")
    
    def close(self, force: bool = False) -> None:
        """
        Close the editor session.
        
        Args:
            force: If True, force kill the process
        """
        if self._closed:
            return

        if self.process is None:
            self._closed = True
            return
        
        if force:
            try:
                self.process.terminate(force=True)
                self.wait_until_closed(timeout=0.2)
            except:
                pass
        else:
            try:
                # Try graceful quit
                if self.is_running():
                    self.quit(force=True)
            except:
                try:
                    self.process.terminate(force=True)
                except:
                    pass

        self.process = None
        self._closed = True
    
    def is_running(self) -> bool:
        """Check if the editor process is still running."""
        if self.process is None:
            return False
        return self.process.isalive()
    
    def get_file_content(self) -> Optional[str]:
        """Read the current file from disk."""
        if self.current_file and self.current_file.exists():
            return self.current_file.read_text()
        return None
    
    def wait(self, seconds: float) -> "EditorSession":
        """Wait for specified seconds."""
        time.sleep(seconds)
        return self
    
    def __enter__(self) -> "EditorSession":
        """Context manager entry."""
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        """Context manager exit - ensure cleanup."""
        self.close(force=True)
