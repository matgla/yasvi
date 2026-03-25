CC ?= armv8m-tcc
CFLAGS = -Wall -Wextra -g 
LDFLAGS = 
SRCS = $(wildcard *.c) $(wildcard filetypes/*.c)
OBJS = $(patsubst %.c, build/%.o, $(SRCS))

ifeq ($(CC), armv8m-tcc)
CFLAGS += -I../../rootfs/usr/include
LDFLAGS += -L../../rootfs/lib -lncurses
else
CFLAGS += -I../../libs/yasos_curses/include
LDFLAGS += -L../../libs/yasos_curses/build -Wl,-rpath=$(PWD)/../../libs/yasos_curses/build -lncurses
endif

TARGET = build/vi

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
INCLUDEDIR ?= $(PREFIX)/include

# Integration test settings
INTEGRATION_DIR := tests/integration
VENV_DIR := $(INTEGRATION_DIR)/venv
PYTHON := $(VENV_DIR)/bin/python
PIP := $(VENV_DIR)/bin/pip
PYTEST := $(VENV_DIR)/bin/pytest
PYTEST_WARNINGS := PYTHONWARNINGS="ignore::DeprecationWarning:pty"

# Number of parallel workers (auto = number of CPU cores)
PARALLEL_WORKERS ?= auto

# Rules
all: $(TARGET)

build/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

install: $(TARGET) 
	mkdir -p $(BINDIR)
	cp $(TARGET) $(BINDIR)

# Clean build artifacts
clean:
	rm -rf build

# Clean everything including integration test environment
cleanall: clean
	rm -rf $(VENV_DIR)
	rm -rf $(INTEGRATION_DIR)/screenshots
	rm -rf $(INTEGRATION_DIR)/.pytest_cache
	rm -rf $(INTEGRATION_DIR)/__pycache__
	find $(INTEGRATION_DIR) -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
	find $(INTEGRATION_DIR) -name "*.pyc" -delete 2>/dev/null || true

TEST_TARGET := tests

# Run unit tests (C tests)
.PHONY: ut
ut: $(TARGET)
	$(MAKE) -C $(TEST_TARGET) run

# Setup integration test environment
.PHONY: integration-setup
integration-setup: $(TARGET)
	@echo "Setting up integration test environment..."
	@test -d $(VENV_DIR) || python3 -m venv $(VENV_DIR)
	$(PIP) install -q -r $(INTEGRATION_DIR)/requirements.txt
	@echo "Integration test environment ready!"

# Run integration tests (Python E2E tests) - PARALLEL by default (fast)
.PHONY: integration
integration: integration-setup
	@echo "Running integration tests in parallel with $(PARALLEL_WORKERS) workers..."
	cd $(INTEGRATION_DIR) && $(PYTEST_WARNINGS) ../../$(PYTEST) test_cases/ -v --tb=short -n $(PARALLEL_WORKERS)

# Run integration tests sequentially (slower, use if parallel has issues)
.PHONY: integration-seq
integration-seq: integration-setup
	@echo "Running integration tests sequentially..."
	cd $(INTEGRATION_DIR) && $(PYTEST_WARNINGS) ../../$(PYTEST) test_cases/ -v --tb=short

# Run integration tests with verbose output
.PHONY: integration-verbose
integration-verbose: integration-setup
	cd $(INTEGRATION_DIR) && $(PYTEST_WARNINGS) ../../$(PYTEST) test_cases/ -v -s --tb=long -n $(PARALLEL_WORKERS)

# Run specific integration test category - PARALLEL by default
.PHONY: integration-smoke
integration-smoke: integration-setup
	cd $(INTEGRATION_DIR) && $(PYTEST_WARNINGS) ../../$(PYTEST) -m smoke -v --tb=short -n $(PARALLEL_WORKERS)

.PHONY: integration-editing
integration-editing: integration-setup
	cd $(INTEGRATION_DIR) && $(PYTEST_WARNINGS) ../../$(PYTEST) -m editing -v --tb=short -n $(PARALLEL_WORKERS)

.PHONY: integration-navigation
integration-navigation: integration-setup
	cd $(INTEGRATION_DIR) && $(PYTEST_WARNINGS) ../../$(PYTEST) -m navigation -v --tb=short -n $(PARALLEL_WORKERS)

.PHONY: integration-fileops
integration-fileops: integration-setup
	cd $(INTEGRATION_DIR) && $(PYTEST_WARNINGS) ../../$(PYTEST) -m fileops -v --tb=short -n $(PARALLEL_WORKERS)

# Run specific integration test category - sequential (slower, stable)
.PHONY: integration-smoke-seq
integration-smoke-seq: integration-setup
	cd $(INTEGRATION_DIR) && $(PYTEST_WARNINGS) ../../$(PYTEST) -m smoke -v --tb=short

.PHONY: integration-editing-seq
integration-editing-seq: integration-setup
	cd $(INTEGRATION_DIR) && $(PYTEST_WARNINGS) ../../$(PYTEST) -m editing -v --tb=short

.PHONY: integration-navigation-seq
integration-navigation-seq: integration-setup
	cd $(INTEGRATION_DIR) && $(PYTEST_WARNINGS) ../../$(PYTEST) -m navigation -v --tb=short

.PHONY: integration-fileops-seq
integration-fileops-seq: integration-setup
	cd $(INTEGRATION_DIR) && $(PYTEST_WARNINGS) ../../$(PYTEST) -m fileops -v --tb=short

# Run integration tests quickly (just smoke + navigation) - PARALLEL
.PHONY: integration-quick
integration-quick: integration-setup
	cd $(INTEGRATION_DIR) && $(PYTEST_WARNINGS) ../../$(PYTEST) -m "smoke or navigation" --tb=no -q -n $(PARALLEL_WORKERS)

# Run integration tests quickly - sequential
.PHONY: integration-quick-seq
integration-quick-seq: integration-setup
	cd $(INTEGRATION_DIR) && $(PYTEST_WARNINGS) ../../$(PYTEST) -m "smoke or navigation" --tb=no -q

# Run all tests (unit + integration) - PARALLEL by default
.PHONY: test
test: ut integration
	@echo "=================================="
	@echo "All tests completed!"
	@echo "=================================="

# Run all tests sequentially (if parallel has issues)
.PHONY: test-seq
test-seq: ut integration-seq
	@echo "=================================="
	@echo "All tests completed (sequential)!"
	@echo "=================================="

# Legacy test target (alias for ut)
.PHONY: test-old
test-old: $(TEST_TARGET)
	$(MAKE) -C $(TEST_TARGET) run
