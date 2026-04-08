# filepath: /home/mateusz/repos/yasvi/tests/Makefile

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -I. -I..
LDFLAGS =

ifeq ($(CC), gcc)
CFLAGS += -fsanitize=address
LDFLAGS += -fsanitize=address
endif

# System Under Test sources (will be compiled with tests)
SUT_SRCS = ../buffer.c ../buffer_row.c ../command.c ../search.c \
           ../filetypes/filetype.c ../filetypes/c.c ../filetypes/shell.c

# File manager sources (need ncurses)
FM_SRCS = ../file_manager.c

# Toolbar sources (needed for editor tests)
TOOLBAR_SRCS = ../toolbar.c ../toolbar_widget.c ../toolbar_widgets.c

# Mock sources for editor tests
MOCK_SRCS = mock_ncurses.c

# Test sources
TEST_NAMES = buffer_tests command_tests \
             buffer_row_character_tests \
             buffer_row_string_tests \
             buffer_row_navigation_tests \
             buffer_row_line_tests \
             buffer_row_highlight_tests \
             buffer_lifecycle_tests \
             buffer_row_management_tests \
             buffer_navigation_tests \
             buffer_line_ops_tests \
             buffer_file_tests \
             editor_state_tests \
             editor_command_tests \
             editor_key_tests \
             file_manager_unit_tests \
             file_manager_integration_tests \
             buffer_tab_integration_tests \
             search_tests \
             search_cycle_tests

TEST_BINS = $(patsubst %,build/%,$(TEST_NAMES))

all: $(TEST_BINS) run

build:
	mkdir -p build

# Pattern rule for test executables - compile SUT sources with each test
build/buffer_tests: buffer_tests.c $(SUT_SRCS) acutest.h | build
	$(CC) $(CFLAGS) -o $@ buffer_tests.c $(SUT_SRCS) $(LDFLAGS)

build/command_tests: command_tests.c $(SUT_SRCS) acutest.h | build
	$(CC) $(CFLAGS) -o $@ command_tests.c $(SUT_SRCS) $(LDFLAGS)

build/buffer_row_character_tests: buffer_row_character_tests.c $(SUT_SRCS) acutest.h | build
	$(CC) $(CFLAGS) -o $@ buffer_row_character_tests.c $(SUT_SRCS) $(LDFLAGS)

build/buffer_row_string_tests: buffer_row_string_tests.c $(SUT_SRCS) acutest.h | build
	$(CC) $(CFLAGS) -o $@ buffer_row_string_tests.c $(SUT_SRCS) $(LDFLAGS)

build/buffer_row_navigation_tests: buffer_row_navigation_tests.c $(SUT_SRCS) acutest.h | build
	$(CC) $(CFLAGS) -o $@ buffer_row_navigation_tests.c $(SUT_SRCS) $(LDFLAGS)

build/buffer_row_line_tests: buffer_row_line_tests.c $(SUT_SRCS) acutest.h | build
	$(CC) $(CFLAGS) -o $@ buffer_row_line_tests.c $(SUT_SRCS) $(LDFLAGS)

build/buffer_row_highlight_tests: buffer_row_highlight_tests.c $(SUT_SRCS) acutest.h | build
	$(CC) $(CFLAGS) -o $@ buffer_row_highlight_tests.c $(SUT_SRCS) $(LDFLAGS)

build/buffer_lifecycle_tests: buffer_lifecycle_tests.c $(SUT_SRCS) acutest.h | build
	$(CC) $(CFLAGS) -o $@ buffer_lifecycle_tests.c $(SUT_SRCS) $(LDFLAGS)

build/buffer_row_management_tests: buffer_row_management_tests.c $(SUT_SRCS) acutest.h | build
	$(CC) $(CFLAGS) -o $@ buffer_row_management_tests.c $(SUT_SRCS) $(LDFLAGS)

build/buffer_navigation_tests: buffer_navigation_tests.c $(SUT_SRCS) acutest.h | build
	$(CC) $(CFLAGS) -o $@ buffer_navigation_tests.c $(SUT_SRCS) $(LDFLAGS)

build/buffer_line_ops_tests: buffer_line_ops_tests.c $(SUT_SRCS) acutest.h | build
	$(CC) $(CFLAGS) -o $@ buffer_line_ops_tests.c $(SUT_SRCS) $(LDFLAGS)

build/buffer_file_tests: buffer_file_tests.c $(SUT_SRCS) acutest.h | build
	$(CC) $(CFLAGS) -o $@ buffer_file_tests.c $(SUT_SRCS) $(LDFLAGS)

# Editor tests use a special build that compiles editor.c with mock ncurses
# We use -include to force mock_ncurses.h to be included first, and -D to prevent real ncurses
EDITOR_TEST_CFLAGS = $(CFLAGS) -include mock_ncurses.h -D__NCURSES_H

build/editor_state_tests: editor_state_tests.c $(SUT_SRCS) $(MOCK_SRCS) $(TOOLBAR_SRCS) $(FM_SRCS) ../editor.c ../window.c acutest.h mock_ncurses.h | build
	$(CC) $(EDITOR_TEST_CFLAGS) -o $@ editor_state_tests.c $(SUT_SRCS) $(MOCK_SRCS) $(TOOLBAR_SRCS) $(FM_SRCS) ../editor.c ../window.c $(LDFLAGS)

build/editor_command_tests: editor_command_tests.c $(SUT_SRCS) $(MOCK_SRCS) $(TOOLBAR_SRCS) $(FM_SRCS) ../editor.c ../window.c acutest.h mock_ncurses.h | build
	$(CC) $(EDITOR_TEST_CFLAGS) -o $@ editor_command_tests.c $(SUT_SRCS) $(MOCK_SRCS) $(TOOLBAR_SRCS) $(FM_SRCS) ../editor.c ../window.c $(LDFLAGS)

build/editor_key_tests: editor_key_tests.c $(SUT_SRCS) $(MOCK_SRCS) $(TOOLBAR_SRCS) $(FM_SRCS) ../editor.c ../window.c acutest.h mock_ncurses.h | build
	$(CC) $(EDITOR_TEST_CFLAGS) -o $@ editor_key_tests.c $(SUT_SRCS) $(MOCK_SRCS) $(TOOLBAR_SRCS) $(FM_SRCS) ../editor.c ../window.c $(LDFLAGS)

build/file_manager_unit_tests: file_manager_unit_tests.c $(SUT_SRCS) $(FM_SRCS) acutest.h | build
	$(CC) $(CFLAGS) -o $@ file_manager_unit_tests.c $(SUT_SRCS) $(FM_SRCS) $(LDFLAGS) -lncurses

build/file_manager_integration_tests: file_manager_integration_tests.c $(SUT_SRCS) $(MOCK_SRCS) $(TOOLBAR_SRCS) $(FM_SRCS) ../editor.c ../window.c acutest.h mock_ncurses.h | build
	$(CC) $(EDITOR_TEST_CFLAGS) -o $@ file_manager_integration_tests.c $(SUT_SRCS) $(MOCK_SRCS) $(TOOLBAR_SRCS) $(FM_SRCS) ../editor.c ../window.c $(LDFLAGS)

build/buffer_tab_integration_tests: buffer_tab_integration_tests.c $(SUT_SRCS) $(MOCK_SRCS) $(TOOLBAR_SRCS) $(FM_SRCS) ../editor.c ../window.c acutest.h mock_ncurses.h | build
	$(CC) $(EDITOR_TEST_CFLAGS) -o $@ buffer_tab_integration_tests.c $(SUT_SRCS) $(MOCK_SRCS) $(TOOLBAR_SRCS) $(FM_SRCS) ../editor.c ../window.c $(LDFLAGS)

build/search_tests: search_tests.c $(SUT_SRCS) acutest.h | build
	$(CC) $(CFLAGS) -o $@ search_tests.c $(SUT_SRCS) $(LDFLAGS)

build/search_cycle_tests: search_cycle_tests.c $(SUT_SRCS) acutest.h | build
	$(CC) $(CFLAGS) -o $@ search_cycle_tests.c $(SUT_SRCS) $(LDFLAGS)

run: $(TEST_BINS)
	@for test in $(TEST_NAMES); do \
		echo "Running build/$$test..."; \
		./build/$$test || exit 1; \
	done

clean:
	rm -rf build

.PHONY: all clean run
