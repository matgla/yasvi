/*
 Test shim for `#include <ncurses.h>` in the editor sources.

 The editor tests force-include mock_ncurses.h and define __NCURSES_H on the
 command line, so this file only has to exist to satisfy the include; the mock
 provides the whole API.
 */
