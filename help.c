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
  "  o O            open a new line below / above, indented",
  "  x              delete the character under the cursor",
  "  dd             delete the line, dw delete a word, de to word end",
  "",
  "Insert mode",
  "  Ctrl-N Ctrl-P  cycle word candidates; Ctrl-E put back, Ctrl-Y keep:",
  "                 words of open files, keywords, the tags file and, in C,",
  "                 the C library's functions, macros and types",
  "  Enter          keeps the indentation, a step more after { ( [ (shell:",
  "                 then do else); } ) ] typed first go under their opener;",
  "                 Tab and Backspace in the indentation go by steps",
  "  :set noai       no automatic indentation (to paste text into the",
  "                 terminal); :set ai or autoindent turns it back on",
  "",
  "Search",
  "  / ?            search forward / backward; n N repeat",
  "",
  "Buffers (open files, tabs across the top)",
  "  :bN :bn :bp    switch to buffer N / next / previous",
  "  gt gT          next / previous buffer",
  "  :bd            close the current buffer",
  "",
  "File tree (sidebar; read again whenever it is shown)",
  "  Ctrl-B         show / hide it; q in it hides it too",
  "  Ctrl-H Ctrl-L  keys to the tree / back to the text (it stays shown);",
  "                 while typing, Ctrl-H is Backspace",
  "  j k            move; l or Enter opens a file or a folder",
  "  h              close a folder, go to the one above, then the parent",
  "  Space/o        expand/collapse a folder; C makes it the root",
  "  a              new file in the folder (name/ makes a folder)",
  "  A              new folder; R reads the tree again",
  "  r (m)          rename or move (an existing folder takes it inside)",
  "  c              copy (folders with everything in them)",
  "  d              delete, y to confirm; Esc cancels a question",
  "",
  "ctags navigation (a 'tags' file: :!ctags -R in the project)",
  "  Ctrl-]         jump to the definition under the cursor (in C also the",
  "                 C library's, /usr/share/yasvi/libc.tags)",
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
  "Settings file and recording",
  "  ~/.config/vi.conf ($XDG_CONFIG_HOME/vi.conf) is read first, then",
  "  .yasvirc; both take any :set line (record, ai, makeprg=, makemode=)",
  "  :set record     keep every key typed in a file in FILE.rec beside it:",
  "                  the session's date and time, each key's time and the",
  "                  pause before it (and a pause line after 2 s or more)",
  "                  :term panel: focus changes, the lines sent to the",
  "                  shell, what it printed and its exit go there too",
  "  :set norecord   stop recording (record=on / record=off work too)",
  "",
  "Ex commands",
  "  :w [file]      save, :wq save and quit, :q quit",
  "  :e FILE        open FILE (:open FILE too); a folder opens in the tree",
  "  :e  :e!        read this file again (:e! drops the changes)",
  "                 Tab completes file names after :e :open :r :w",
  "  :s/old/new/    substitute on this line (g = all of it)",
  "  :%s/old/new/g  substitute in the whole buffer",
  "  :help          show this help again (a buffer you can edit)",
  "  :!cmd          run cmd in the shell with the terminal (interactive",
  "                 programs too), then press Enter to come back",
  "  :r !cmd        insert cmd's output (stdout and stderr) below the line",
  "  :r FILE        insert FILE below the line",
  "                 in cmd, % is this file's name and \\% a plain %",
  "",
  "Terminal panel (a shell under the text, on pipes: no terminal for",
  "programs, so no vi or curses programs in it)",
  "  :term [cmd]    open the panel (start the shell), type there; cmd is sent",
  "  Enter          send the line; Up/Down history; PgUp/PgDn scroll",
  "  Ctrl-C         interrupt what the shell runs (the shell stays)",
  "  Ctrl-D         end the shell's input (it exits) on an empty line",
  "  Esc, Ctrl-K    back to the text; Ctrl-J (Enter) in normal mode: to",
  "                 the panel; Ctrl-W j/k/w move, Ctrl-W c closes it",
  "  :termclose     close the panel and its shell",
  "  :q             in this help, a :man page or the :cl log: close it",
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
