/*
 Copyright (c) 2026 Mateusz Stadnik

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

#include "termpanel.h"

#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "fileops.h"
#include "run.h"

void termpanel_init(TermPanel* panel) {
  memset(panel, 0, sizeof(*panel));
  panel->to_shell = -1;
  panel->from_shell = -1;
  panel->exit_status = -1;
  panel->rows = 8;
}

bool termpanel_running(const TermPanel* panel) {
  return panel->pid > 0;
}

// ---------------------------------------------------------------------------
// Scrollback
// ---------------------------------------------------------------------------

static void termpanel_push_line(TermPanel* panel, const char* text, size_t length) {
  char* line = malloc(length + 1);
  if (line == NULL) {
    return;
  }
  memcpy(line, text, length);
  line[length] = '\0';
  if (panel->count == TERMPANEL_SCROLLBACK) {
    free(panel->lines[panel->first]);
    panel->lines[panel->first] = line;
    panel->first = (panel->first + 1) % TERMPANEL_SCROLLBACK;
  } else {
    panel->lines[(panel->first + panel->count) % TERMPANEL_SCROLLBACK] = line;
    ++panel->count;
    if (panel->scroll > 0) {
      ++panel->scroll;  // what the reader looks at stays put
    }
  }
}

static void termpanel_report(TermPanel* panel, TermPanelLineKind kind, const char* text) {
  if (panel->on_line != NULL) {
    panel->on_line(panel->on_line_context, kind, text);
  }
}

static void termpanel_end_line(TermPanel* panel) {
  termpanel_push_line(panel, panel->partial, panel->partial_length);
  panel->partial[panel->partial_length] = '\0';
  termpanel_report(panel, TermPanelLine_Output, panel->partial);
  panel->partial_length = 0;
}

int termpanel_line_count(const TermPanel* panel) {
  return panel->count;
}

const char* termpanel_line(const TermPanel* panel, int index) {
  if (index < 0 || index >= panel->count) {
    return NULL;
  }
  return panel->lines[(panel->first + index) % TERMPANEL_SCROLLBACK];
}

static void termpanel_put(TermPanel* panel, char c) {
  if (panel->partial_length == TERMPANEL_LINE_MAX) {
    termpanel_end_line(panel);
  }
  panel->partial[panel->partial_length++] = c;
}

void termpanel_feed(TermPanel* panel, const char* data, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    const unsigned char c = (unsigned char)data[i];
    if (panel->escape == 1) {
      // ESC [ (CSI) and ESC ] (OSC) run on; any other ESC x is two bytes.
      panel->escape = (c == '[' || c == ']') ? (c == '[' ? 2 : 3) : 0;
      continue;
    }
    if (panel->escape == 2) {  // CSI: parameters, then a final byte @..~
      if (c >= 0x40 && c <= 0x7e) {
        panel->escape = 0;
      }
      continue;
    }
    if (panel->escape == 3) {  // OSC: until BEL or ESC (of ESC \)
      if (c == 7) {
        panel->escape = 0;
      } else if (c == 27) {
        panel->escape = 1;
      }
      continue;
    }
    switch (c) {
      case 27:
        panel->escape = 1;
        break;
      case '\n':
        termpanel_end_line(panel);
        break;
      case '\r':
        if (i + 1 < length && data[i + 1] == '\n') {
          break;  // \r\n: the \n ends the line
        }
        panel->partial_length = 0;  // a progress line starts over
        break;
      case '\b':
        if (panel->partial_length > 0) {
          --panel->partial_length;
        }
        break;
      case '\t':
        do {
          termpanel_put(panel, ' ');
        } while (panel->partial_length % 8 != 0);
        break;
      default:
        if (c >= 32 && c != 127) {
          termpanel_put(panel, (char)c);
        }
        break;
    }
  }
}

// ---------------------------------------------------------------------------
// The shell
// ---------------------------------------------------------------------------

bool termpanel_start(TermPanel* panel) {
  if (termpanel_running(panel)) {
    return true;
  }
  int to_shell = -1;
  int from_shell = -1;
  const int pid = run_shell_piped(&to_shell, &from_shell);
  if (pid < 0) {
    return false;
  }
  panel->pid = pid;
  panel->to_shell = to_shell;
  panel->from_shell = from_shell;
  panel->exit_status = -1;
  panel->escape = 0;
  return true;
}

static void termpanel_close_pipes(TermPanel* panel) {
  if (panel->to_shell >= 0) {
    close(panel->to_shell);
    panel->to_shell = -1;
  }
  if (panel->from_shell >= 0) {
    close(panel->from_shell);
    panel->from_shell = -1;
  }
}

static int termpanel_status_code(int status) {
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return 128 + WTERMSIG(status);
}

// The shell is gone: say so in the scrollback.
static void termpanel_reaped(TermPanel* panel, int status) {
  panel->exit_status = termpanel_status_code(status);
  panel->pid = 0;
  termpanel_close_pipes(panel);
  if (panel->partial_length > 0) {
    termpanel_end_line(panel);
  }
  char message[48];
  const int length =
    snprintf(message, sizeof(message), "[shell exited %d]", panel->exit_status);
  termpanel_push_line(panel, message, (size_t)length);
  char code[16];
  snprintf(code, sizeof(code), "%d", panel->exit_status);
  termpanel_report(panel, TermPanelLine_Exit, code);
}

bool termpanel_poll(TermPanel* panel) {
  bool changed = false;
  if (panel->from_shell >= 0) {
    char chunk[512];
    for (int reads = 0; reads < 16; ++reads) {  // at most 8 KiB a pass
      const ssize_t got = read(panel->from_shell, chunk, sizeof(chunk));
      if (got > 0) {
        termpanel_feed(panel, chunk, (size_t)got);
        changed = true;
        continue;
      }
      break;  // 0: everyone closed the write end; -1: nothing there now
    }
  }
  if (panel->pid > 0) {
    int status = 0;
    const pid_t done = waitpid(panel->pid, &status, WNOHANG);
    if (done == panel->pid) {
      // What it wrote before exiting is still in the pipe.
      if (panel->from_shell >= 0) {
        char chunk[512];
        ssize_t got;
        while ((got = read(panel->from_shell, chunk, sizeof(chunk))) > 0) {
          termpanel_feed(panel, chunk, (size_t)got);
        }
      }
      termpanel_reaped(panel, status);
      changed = true;
    }
  }
  return changed;
}

static void termpanel_history_add(TermPanel* panel, const char* line) {
  if (line[0] == '\0') {
    return;
  }
  if (panel->history_count > 0 &&
      strcmp(panel->history[panel->history_count - 1], line) == 0) {
    panel->history_index = panel->history_count;
    return;
  }
  char* copy = strdup(line);
  if (copy == NULL) {
    return;
  }
  if (panel->history_count == TERMPANEL_HISTORY) {
    free(panel->history[0]);
    memmove(panel->history, panel->history + 1,
            sizeof(panel->history[0]) * (TERMPANEL_HISTORY - 1));
    --panel->history_count;
  }
  panel->history[panel->history_count++] = copy;
  panel->history_index = panel->history_count;
}

bool termpanel_send_line(TermPanel* panel, const char* line) {
  if (!termpanel_running(panel) || panel->to_shell < 0) {
    return false;
  }
  const size_t length = strlen(line);
  char* data = malloc(length + 2);
  if (data == NULL) {
    return false;
  }
  memcpy(data, line, length);
  data[length] = '\n';
  data[length + 1] = '\0';
  size_t sent = 0;
  int tries = 0;
  while (sent < length + 1 && tries < 100) {
    const ssize_t n = write(panel->to_shell, data + sent, length + 1 - sent);
    if (n > 0) {
      sent += (size_t)n;
      continue;
    }
    if (n < 0 && errno != EAGAIN && errno != EINTR) {
      break;
    }
    ++tries;  // the pipe is full: let the shell read a little
    struct timespec nap = {0, 1000000};
    nanosleep(&nap, NULL);
  }
  free(data);
  if (panel->partial_length > 0) {
    termpanel_end_line(panel);  // a prompt-less shell: keep its last output whole
  }
  char echo[TERMPANEL_INPUT_MAX + 8];
  const int echo_length = snprintf(echo, sizeof(echo), "$ %s", line);
  termpanel_push_line(panel, echo, (size_t)echo_length < sizeof(echo)
                                     ? (size_t)echo_length
                                     : sizeof(echo) - 1);
  termpanel_report(panel, TermPanelLine_Command, line);
  termpanel_history_add(panel, line);
  panel->scroll = 0;
  return sent == length + 1;
}

bool termpanel_submit(TermPanel* panel) {
  panel->input[panel->input_length] = '\0';
  const bool sent = termpanel_send_line(panel, panel->input);
  termpanel_input_clear(panel);
  return sent;
}

void termpanel_input_char(TermPanel* panel, char c) {
  if (panel->input_length < TERMPANEL_INPUT_MAX) {
    panel->input[panel->input_length++] = c;
    panel->input[panel->input_length] = '\0';
  }
}

void termpanel_input_backspace(TermPanel* panel) {
  if (panel->input_length > 0) {
    panel->input[--panel->input_length] = '\0';
  }
}

void termpanel_input_clear(TermPanel* panel) {
  panel->input_length = 0;
  panel->input[0] = '\0';
  panel->history_index = panel->history_count;
}

// ---------------------------------------------------------------------------
// Tab completion
// ---------------------------------------------------------------------------

char* termpanel_shell_cwd(const TermPanel* panel) {
  if (!termpanel_running(panel)) {
    return NULL;
  }
  char path[64];
  snprintf(path, sizeof(path), "/proc/%d/cwd", (int)panel->pid);
  char target[512];
  const ssize_t n = readlink(path, target, sizeof(target) - 1);
  if (n <= 0) {
    return NULL;
  }
  target[n] = '\0';
  return strdup(target);
}

// The words the shell runs itself (toybox sh), offered with PATH's programs
static const char* g_shell_builtins[] = {
  "alias", "bg",    "cd",     "echo",  "eval",  "exec",   "exit",  "export",
  "false", "fg",    "jobs",   "printf", "pwd",  "read",   "set",   "shift",
  "source", "test", "trap",   "true",  "type",  "umask",  "unalias", "unset",
  "wait",  NULL};

// Characters a completed name gets a backslash before
static bool termpanel_needs_escape(char c) {
  return strchr(" \t'\"\\$&;|<>()*?`#!", c) != NULL;
}

static bool termpanel_separator(char c) {
  return strchr(";|&<>(", c) != NULL;
}

// The word being typed at the end of the line: where it starts in the input,
// its text without quotes and backslashes (into `word`), and whether it names
// a command (first word of the line or after ; | & ( ).
static size_t termpanel_last_word(const char* input, size_t length, char* word,
                                  size_t word_size, bool* command) {
  size_t start = 0;
  size_t used = 0;
  bool command_position = true;
  bool in_word = false;
  char quote = '\0';
  for (size_t i = 0; i < length; ++i) {
    const char c = input[i];
    if (quote == '\0' && (c == ' ' || c == '\t' || termpanel_separator(c))) {
      if (in_word) {
        command_position = false;
        in_word = false;
      }
      if (termpanel_separator(c)) {
        command_position = c != '<' && c != '>';
      }
      start = i + 1;
      used = 0;
      continue;
    }
    if (!in_word) {
      in_word = true;
      start = i;
      used = 0;
    }
    if (quote == '\0' && c == '\\' && i + 1 < length) {
      ++i;
      if (used + 1 < word_size) {
        word[used++] = input[i];
      }
      continue;
    }
    if ((c == '\'' || c == '"') && (quote == '\0' || quote == c)) {
      quote = quote == '\0' ? c : '\0';
      continue;
    }
    if (used + 1 < word_size) {
      word[used++] = c;
    }
  }
  word[used] = '\0';
  *command = command_position;
  return start;
}

static int termpanel_compare(const void* a, const void* b) {
  return strcmp(*(char* const*)a, *(char* const*)b);
}

static bool termpanel_add_match(char*** matches, int* count, int* capacity,
                                const char* name) {
  if (*count == *capacity) {
    const int grown_capacity = *capacity == 0 ? 32 : *capacity * 2;
    char** grown = realloc(*matches, sizeof(char*) * (size_t)grown_capacity);
    if (grown == NULL) {
      return false;
    }
    *matches = grown;
    *capacity = grown_capacity;
  }
  char* copy = strdup(name);
  if (copy == NULL) {
    return false;
  }
  (*matches)[(*count)++] = copy;
  return true;
}

// Programs in PATH and shell builtins that start with `prefix`, sorted,
// without duplicates
static int termpanel_command_matches(const char* prefix, char*** out) {
  char** matches = NULL;
  int count = 0;
  int capacity = 0;
  const size_t prefix_length = strlen(prefix);
  for (const char** b = g_shell_builtins; *b != NULL; ++b) {
    if (strncmp(*b, prefix, prefix_length) == 0) {
      termpanel_add_match(&matches, &count, &capacity, *b);
    }
  }
  const char* env_path = getenv("PATH");
  char* path = strdup(env_path != NULL && env_path[0] != '\0' ? env_path : "/bin:/usr/bin");
  for (char* dir = path != NULL ? strtok(path, ":") : NULL; dir != NULL;
       dir = strtok(NULL, ":")) {
    DIR* d = opendir(dir);
    if (d == NULL) {
      continue;
    }
    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
      const char* name = entry->d_name;
      if (name[0] == '.' || strncmp(name, prefix, prefix_length) != 0) {
        continue;
      }
      char* full = path_join(dir, name);
      struct stat st;
      const bool program = full != NULL && stat(full, &st) == 0 && !S_ISDIR(st.st_mode);
      free(full);
      if (program) {
        termpanel_add_match(&matches, &count, &capacity, name);
      }
    }
    closedir(d);
  }
  free(path);
  if (count > 1) {
    qsort(matches, (size_t)count, sizeof(char*), termpanel_compare);
    int unique = 1;
    for (int i = 1; i < count; ++i) {
      if (strcmp(matches[i], matches[unique - 1]) == 0) {
        free(matches[i]);
      } else {
        matches[unique++] = matches[i];
      }
    }
    count = unique;
  }
  *out = matches;
  return count;
}

// Puts `text` (escaped) in place of the input from `start`, plus `suffix`.
// False when the line would be too long.
static bool termpanel_replace_word(TermPanel* panel, size_t start, const char* text,
                                   const char* suffix) {
  char line[TERMPANEL_INPUT_MAX + 1];
  size_t used = start;
  memcpy(line, panel->input, start);
  for (const char* c = text; *c != '\0'; ++c) {
    if (used + 2 > TERMPANEL_INPUT_MAX) {
      return false;
    }
    if (termpanel_needs_escape(*c)) {
      line[used++] = '\\';
    }
    line[used++] = *c;
  }
  const size_t suffix_length = strlen(suffix);
  if (used + suffix_length > TERMPANEL_INPUT_MAX) {
    return false;
  }
  memcpy(line + used, suffix, suffix_length);
  used += suffix_length;
  memcpy(panel->input, line, used);
  panel->input[used] = '\0';
  panel->input_length = used;
  return true;
}

// The matches as scrollback lines of at most `width` columns: file names
// without their directory part
static void termpanel_list_matches(TermPanel* panel, char** matches, int count,
                                   bool files, int width) {
  enum { kListMax = 200 };
  if (width < 20 || width > TERMPANEL_LINE_MAX) {
    width = 80;
  }
  if (panel->partial_length > 0) {
    termpanel_end_line(panel);
  }
  char line[TERMPANEL_LINE_MAX + 1];
  size_t used = 0;
  const int shown = count < kListMax ? count : kListMax;
  for (int i = 0; i < shown; ++i) {
    const char* name = matches[i];
    if (files) {
      const size_t length = strlen(name);
      const char* slash = NULL;
      for (size_t j = 0; j + 1 < length; ++j) {
        if (name[j] == '/') {
          slash = name + j;
        }
      }
      if (slash != NULL) {
        name = slash + 1;
      }
    }
    const size_t length = strlen(name);
    if (used > 0 && used + 2 + length > (size_t)width) {
      termpanel_push_line(panel, line, used);
      used = 0;
    }
    if (used > 0) {
      line[used++] = ' ';
      line[used++] = ' ';
    }
    const size_t n = length < (size_t)width ? length : (size_t)width;
    memcpy(line + used, name, n);
    used += n;
  }
  if (used > 0) {
    termpanel_push_line(panel, line, used);
  }
  if (count > shown) {
    const int n = snprintf(line, sizeof(line), "... %d more", count - shown);
    termpanel_push_line(panel, line, (size_t)n);
  }
  panel->scroll = 0;
}

int termpanel_complete(TermPanel* panel, int width) {
  char word[TERMPANEL_INPUT_MAX + 1];
  bool command = false;
  const size_t start = termpanel_last_word(panel->input, panel->input_length, word,
                                           sizeof(word), &command);
  char** matches = NULL;
  int count;
  const bool files = !command || strchr(word, '/') != NULL;
  if (!files) {
    if (word[0] == '\0') {
      return 0;  // every program there is: not useful
    }
    count = termpanel_command_matches(word, &matches);
  } else {
    char* cwd = termpanel_shell_cwd(panel);
    count = path_complete_in(cwd, word, &matches);
    free(cwd);
  }
  if (count == 1) {
    const size_t length = strlen(matches[0]);
    const bool directory = files && length > 0 && matches[0][length - 1] == '/';
    termpanel_replace_word(panel, start, matches[0], directory ? "" : " ");
  } else if (count > 1) {
    char* common = path_common_prefix(matches, count);
    if (common != NULL && strlen(common) > strlen(word)) {
      termpanel_replace_word(panel, start, common, "");
    } else {
      termpanel_list_matches(panel, matches, count, files, width);
    }
    free(common);
  }
  path_matches_free(matches, count);
  return count;
}

void termpanel_history_step(TermPanel* panel, int step) {
  const int index = panel->history_index + step;
  if (index < 0 || index > panel->history_count) {
    return;
  }
  panel->history_index = index;
  const char* text = index == panel->history_count ? "" : panel->history[index];
  size_t length = strlen(text);
  if (length > TERMPANEL_INPUT_MAX) {
    length = TERMPANEL_INPUT_MAX;
  }
  memcpy(panel->input, text, length);
  panel->input[length] = '\0';
  panel->input_length = length;
}

// ---------------------------------------------------------------------------
// Signals to what the shell runs
// ---------------------------------------------------------------------------

// The PPid line of /proc/PID/status, or -1.
static pid_t termpanel_parent_of(pid_t pid) {
  char path[32];
  snprintf(path, sizeof(path), "/proc/%d/status", (int)pid);
  FILE* file = fopen(path, "r");
  if (file == NULL) {
    return -1;
  }
  char line[96];
  pid_t parent = -1;
  while (fgets(line, sizeof(line), file) != NULL) {
    if (strncmp(line, "PPid:", 5) == 0) {
      parent = (pid_t)atoi(line + 5);
      break;
    }
  }
  fclose(file);
  return parent;
}

int termpanel_descendants(pid_t root, pid_t* out, int max) {
  // Every process and its parent, on the heap (vi's stack is 16 KiB; YasOS
  // has at most 256 processes, a host many more).
  int capacity = 64;
  int total = 0;
  pid_t* pids = malloc(sizeof(pid_t) * 2 * (size_t)capacity);
  if (pids == NULL) {
    return 0;
  }
  DIR* proc = opendir("/proc");
  if (proc == NULL) {
    free(pids);
    return 0;
  }
  struct dirent* entry;
  while ((entry = readdir(proc)) != NULL) {
    const char* name = entry->d_name;
    if (name[0] < '1' || name[0] > '9') {
      continue;
    }
    const pid_t pid = (pid_t)atoi(name);
    const pid_t parent = termpanel_parent_of(pid);
    if (parent < 0) {
      continue;
    }
    if (total == capacity) {
      pid_t* grown = realloc(pids, sizeof(pid_t) * 4 * (size_t)capacity);
      if (grown == NULL) {
        break;
      }
      pids = grown;
      capacity *= 2;
    }
    pids[2 * total] = pid;
    pids[2 * total + 1] = parent;
    ++total;
  }
  closedir(proc);
  // From root down: out[] doubles as the queue of parents still to expand.
  int found = 0;
  for (int next = -1; next < found; ++next) {
    const pid_t parent = next < 0 ? root : out[next];
    for (int i = 0; i < total && found < max; ++i) {
      if (pids[2 * i + 1] == parent && pids[2 * i] != root) {
        out[found++] = pids[2 * i];
      }
    }
  }
  free(pids);
  return found;
}

static int termpanel_signal_descendants(TermPanel* panel, int sig) {
  if (!termpanel_running(panel)) {
    return 0;
  }
  pid_t below[64];
  const int count = termpanel_descendants(panel->pid, below, 64);
  for (int i = 0; i < count; ++i) {
    kill(below[i], sig);
  }
  return count;
}

int termpanel_interrupt(TermPanel* panel) {
  const int count = termpanel_signal_descendants(panel, SIGINT);
  const char* note = count > 0 ? "^C" : "^C (nothing running)";
  termpanel_push_line(panel, note, strlen(note));
  panel->scroll = 0;
  return count;
}

void termpanel_stop(TermPanel* panel) {
  if (termpanel_running(panel)) {
    termpanel_signal_descendants(panel, SIGKILL);
    kill(panel->pid, SIGKILL);
    int status = 0;
    pid_t done;
    do {
      done = waitpid(panel->pid, &status, 0);
    } while (done < 0 && errno == EINTR);
    panel->exit_status = done == panel->pid ? termpanel_status_code(status) : -1;
    panel->pid = 0;
  }
  termpanel_close_pipes(panel);
}

void termpanel_deinit(TermPanel* panel) {
  termpanel_stop(panel);
  for (int i = 0; i < panel->count; ++i) {
    free(panel->lines[(panel->first + i) % TERMPANEL_SCROLLBACK]);
  }
  panel->count = 0;
  panel->first = 0;
  for (int i = 0; i < panel->history_count; ++i) {
    free(panel->history[i]);
  }
  panel->history_count = 0;
}
