/*
 Copyright (c) 2025 Mateusz Stadnik

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE

#include "help.h"

#include <stdlib.h>
#include <string.h>

// The built-in help, one row per entry; ':help' shows it in its own buffer.
static const char* g_help_lines[] = {
  "yasvi help",
  "",
  "Movement",
  "  h j k l      move left/down/up/right (arrow keys work too)",
  "  0 ^ $          start of line / first char / last char",
  "  w e b          next word / end of word / previous word",
  "  gg             jump to the first line, G to the last",
  "",
  "Editing",
  "  i a            insert here / after the cursor",
  "  x              delete the character under the cursor",
  "  dd             delete the line, dw delete a word, de to word end",
  "",
  "Insert mode",
  "  Ctrl-N Ctrl-P  cycle word candidates; Ctrl-E put back, Ctrl-Y keep",
  "",
  "Search",
  "  / ?            search forward / backward; n N repeat",
  "",
  "Buffers (open files, tabs across the top)",
  "  :bN :bn :bp    switch to buffer N / next / previous",
  "  gt gT          next / previous buffer",
  "  :bd            close the current buffer",
  "",
  "File manager sidebar",
  "  Ctrl-B         toggle; then j k h l navigate, l or Enter opens,",
  "  Ctrl-H Ctrl-L  open it from the editor / close it back",
  "  Space/o        expand/collapse a folder, Esc returns",
  "",
  "ctags navigation (needs a 'tags' file from ctags)",
  "  Ctrl-]         jump to the definition under the cursor",
  "  :tag NAME      jump by name; Ctrl-T or :pop come back",
  "",
  "Manual pages ($MANPATH, else /usr/share/man)",
  "  :man NAME      open the page for NAME in a buffer (printf, stdio.h)",
  "  :man           open the whatis index of every page",
  "  K              the page for the word under the cursor",
  "",
  "Building",
  "  :make [args]   write modified files, run makeprg args; the output",
  "                 shows in the terminal and is kept in /tmp/yasvi-make.log",
  "  :cn :cp        next / previous error or warning from the build",
  "  :cc [N]        the current (or Nth) error again; :cl shows the log",
  "  :set           show makeprg and makemode",
  "  :set makeprg=CMD    the build command (default ./build.sh with bash,",
  "                      else make when there is a Makefile)",
  "  :set makemode=wait  vi waits for the build (default)",
  "  :set makemode=exec  vi exits for the build and comes back after it,",
  "                      leaving the build all of its RAM",
  "  .yasvirc (here, else in $HOME) holds the same as lines makeprg=CMD,",
  "  set makemode=exec, # comments",
  "",
  "Ex commands",
  "  :w [file]      save, :wq save and quit, :q quit",
  "  :s/old/new/    substitute on this line (g = all of it)",
  "  :%s/old/new/g  substitute in the whole buffer",
  "  :help          show this help again (a buffer you can edit)",
  "",
  "Autosave (crash recovery)",
  "  Edits are copied 2 s after typing stops, or every 200 edits, to",
  "  .NAME.yasvi-autosave beside the file, or when that directory is",
  "  read-only to ~/.cache/yasvi/ (then /tmp/yasvi/). :w and quitting",
  "  delete it, so one found when a file opens was left by a crash:",
  "  :recover         replace the text with it (then :w to keep it)",
  "  :delautosave     delete it instead",
  "  Unnamed buffers go to ~/.cache/yasvi/noname-PID.yasvi-autosave.",
};

Buffer* help_alloc_buffer() {
  Buffer* buffer = buffer_alloc();
  if (buffer == NULL) {
    return NULL;
  }
  buffer->filename = strdup(HELP_BUFFER_NAME);
  buffer->autosave_disabled = true;  // not a file: nothing to recover
  if (buffer->filename == NULL) {
    buffer_free(buffer);
    return NULL;
  }
  for (size_t i = 0;
       i < sizeof(g_help_lines) / sizeof(g_help_lines[0]); ++i) {
    if (!buffer_append_line(buffer, g_help_lines[i])) {
      buffer_free(buffer);
      return NULL;
    }
  }
  return buffer;
}
