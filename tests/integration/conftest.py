"""
Pytest configuration and shared fixtures for Yasvi E2E tests.
"""

import os
import pytest
import tempfile
import shutil
from pathlib import Path
from typing import Generator

# Add parent dir to path for importing editor_session
import sys
sys.path.insert(0, str(Path(__file__).parent))

from editor_session import EditorSession


def pytest_configure(config):
    """Register custom markers."""
    config.addinivalue_line("markers", "smoke: Smoke tests (basic operations)")
    config.addinivalue_line("markers", "navigation: Navigation tests")
    config.addinivalue_line("markers", "editing: Text editing tests")
    config.addinivalue_line("markers", "fileops: File operation tests")
    config.addinivalue_line("markers", "workflow: Complex workflow tests")
    config.addinivalue_line("markers", "edgecase: Edge case and error handling tests")
    config.addinivalue_line("markers", "slow: Tests that take longer to run")


def pytest_addoption(parser):
    """Add custom command line options."""
    parser.addoption(
        "--screenshots-on-failure",
        action="store_true",
        default=True,
        help="Capture screenshots on test failure"
    )
    parser.addoption(
        "--keep-temp",
        action="store_true",
        default=False,
        help="Keep temporary directories after tests"
    )


@pytest.fixture(scope="session")
def editor_path() -> Path:
    """Path to the compiled editor binary."""
    # Look for the binary relative to test directory
    test_dir = Path(__file__).parent
    project_root = test_dir.parent.parent
    binary = project_root / "build" / "vi"
    
    if not binary.exists():
        raise FileNotFoundError(
            f"Editor binary not found at {binary}. "
            "Please build the project first with 'make'"
        )
    
    return binary


@pytest.fixture
def temp_dir(request) -> Generator[Path, None, None]:
    """Create a temporary directory for test files."""
    tmpdir = tempfile.mkdtemp(prefix="yasvi_e2e_")
    yield Path(tmpdir)
    
    # Cleanup unless --keep-temp is specified
    if not request.config.getoption("--keep-temp"):
        shutil.rmtree(tmpdir, ignore_errors=True)


@pytest.fixture
def fixtures_dir() -> Path:
    """Path to test fixtures directory."""
    return Path(__file__).parent / "fixtures"


@pytest.fixture
def screenshots_dir() -> Path:
    """Path to screenshots directory for debug output."""
    worker_id = os.environ.get("PYTEST_XDIST_WORKER", "main")
    ss_dir = Path(__file__).parent / "screenshots" / worker_id
    ss_dir.mkdir(exist_ok=True)
    return ss_dir


@pytest.fixture
def editor(editor_path: Path, temp_dir: Path, screenshots_dir: Path) -> Generator[EditorSession, None, None]:
    """
    Create an editor session for testing.
    
    This fixture provides a clean editor instance that will be
    properly cleaned up after the test.
    """
    session = EditorSession(
        editor_path=editor_path,
        working_dir=temp_dir,
        screenshots_dir=screenshots_dir
    )
    
    try:
        yield session
    finally:
        # Ensure editor is closed, even if test failed
        session.close(force=True)


@pytest.fixture
def sample_text_file(fixtures_dir: Path, temp_dir: Path) -> Path:
    """Create a sample text file for testing."""
    src = fixtures_dir / "simple.txt"
    if src.exists():
        dst = temp_dir / "simple.txt"
        shutil.copy(src, dst)
        return dst
    
    # Create default if fixture doesn't exist yet
    dst = temp_dir / "simple.txt"
    dst.write_text("""Line 1: Hello World
Line 2: This is a test file
Line 3: For editor testing
Line 4: Multiple lines
Line 5: To navigate through
Line 6: End of file
""")
    return dst


@pytest.fixture
def sample_c_file(fixtures_dir: Path, temp_dir: Path) -> Path:
    """Create a sample C file for testing."""
    src = fixtures_dir / "code.c"
    if src.exists():
        dst = temp_dir / "code.c"
        shutil.copy(src, dst)
        return dst
    
    # Create default if fixture doesn't exist yet
    dst = temp_dir / "code.c"
    dst.write_text("""#include <stdio.h>

int main(void) {
    printf("Hello, World!\\n");
    return 0;
}
""")
    return dst


# Hook to capture screenshots on failure
def pytest_runtest_makereport(item, call):
    """Capture screenshot on test failure."""
    if call.when == "call" and call.excinfo is not None:
        if not item.config.getoption("--screenshots-on-failure"):
            return
        # Test failed
        editor = item.funcargs.get("editor")
        if editor is not None:
            try:
                test_name = item.nodeid.replace("::", "_").replace("/", "_")
                editor.screenshot_on_failure(f"FAILED_{test_name}")
            except Exception:
                pass  # Don't let screenshot failure mask the original error
