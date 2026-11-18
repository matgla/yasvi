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

#include "record.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

void recorder_init(Recorder* recorder) {
  recorder->file = NULL;
  recorder->source = NULL;
  recorder->failed = false;
  recorder->pending = false;
  recorder->last_ms = 0;
  recorder->start_ms = 0;
  recorder->start_time = 0;
}

bool record_wanted(const char* filename) {
  if (filename == NULL || filename[0] == '\0') {
    return false;
  }
  const size_t length = strlen(filename);
  const size_t suffix = sizeof(RECORD_SUFFIX) - 1;
  return length < suffix ||
         strcmp(filename + length - suffix, RECORD_SUFFIX) != 0;
}

char* record_path_for(const char* filename) {
  const size_t length = strlen(filename);
  char* path = malloc(length + sizeof(RECORD_SUFFIX));
  if (path != NULL) {
    memcpy(path, filename, length);
    memcpy(path + length, RECORD_SUFFIX, sizeof(RECORD_SUFFIX));
  }
  return path;
}

void record_key_name(int key, const char* curses_name, char* out, size_t size) {
  if (curses_name != NULL) {
    snprintf(out, size, "%s", curses_name);
  } else if (key == ' ') {
    snprintf(out, size, "SPACE");
  } else if (key == '\t') {
    snprintf(out, size, "TAB");
  } else if (key == '\n') {
    snprintf(out, size, "ENTER");
  } else if (key == 27) {
    snprintf(out, size, "ESC");
  } else if (key == 127) {
    snprintf(out, size, "BS");
  } else if (key >= 0 && key < 32) {
    snprintf(out, size, "^%c", '@' + key);
  } else if (key > 32 && key < 127) {
    snprintf(out, size, "%c", key);
  } else {
    snprintf(out, size, "K%d", key);  // a curses key editor.c has no name for
  }
}

static void recorder_forget_source(Recorder* recorder) {
  free(recorder->source);
  recorder->source = NULL;
  recorder->failed = false;
}

void recorder_flush(Recorder* recorder) {
  if (recorder->file != NULL && recorder->pending) {
    fflush(recorder->file);
  }
  recorder->pending = false;
}

void recorder_close(Recorder* recorder) {
  if (recorder->file != NULL) {
    fclose(recorder->file);
    recorder->file = NULL;
  }
  recorder->pending = false;
  recorder_forget_source(recorder);
}

void recorder_idle(Recorder* recorder, long long now_ms) {
  if (recorder->pending && now_ms - recorder->last_ms >= RECORD_FLUSH_MS) {
    recorder_flush(recorder);
  }
}

// The basename of `filename`, for the header.
static const char* record_basename(const char* filename) {
  const char* slash = strrchr(filename, '/');
  return slash != NULL ? slash + 1 : filename;
}

void record_format_time(time_t when, char* out, size_t size) {
  struct tm* local = localtime(&when);
  if (local == NULL || strftime(out, size, "%Y-%m-%d %H:%M:%S", local) == 0) {
    snprintf(out, size, "%lld", (long long)when);
  }
}

// Opens FILENAME.rec for appending and starts a session in it.
static bool recorder_open(Recorder* recorder,
                          const char* filename,
                          int lines,
                          long long now_ms) {
  recorder->source = strdup(filename);
  if (recorder->source == NULL) {
    return false;
  }
  char* path = record_path_for(filename);
  if (path == NULL) {
    recorder->failed = true;
    return false;
  }
  struct stat info;
  const bool is_new = stat(path, &info) != 0 || info.st_size == 0;
  recorder->file = fopen(path, "a");
  free(path);
  if (recorder->file == NULL) {
    recorder->failed = true;
    return false;
  }
  if (is_new) {
    fprintf(recorder->file,
            "# yasvi recording of %s\n"
            "# session DATE TIME unix SECONDS lines N\n"
            "# AT_MS PAUSE_MS CODE KEY   (AT: since the session started, "
            "PAUSE: since the key before)\n"
            "# pause DATE TIME after SECONDS s   (before a key after a pause of "
            "%d s or more)\n"
            "# term AT_MS focus | text AT_MS focus   (keys go to the :term panel "
            "/ the text)\n"
            "# term AT_MS $ LINE | out AT_MS LINE | term AT_MS exit N   (a line "
            "sent to the shell, a line it printed, it exited)\n",
            record_basename(filename), RECORD_PAUSE_MS / 1000);
  }
  recorder->start_time = time(NULL);
  recorder->start_ms = now_ms;
  recorder->last_ms = now_ms;
  char date[32];
  record_format_time(recorder->start_time, date, sizeof(date));
  fprintf(recorder->file, "session %s unix %lld lines %d\n", date,
          (long long)recorder->start_time, lines);
  recorder->pending = true;
  return true;
}

// Makes `filename`'s recording the open one. False when it cannot be
// opened now (reported by the caller); true without a file when it failed
// before.
static bool recorder_use(Recorder* recorder,
                         const char* filename,
                         int lines,
                         long long now_ms) {
  if (recorder->source == NULL || strcmp(recorder->source, filename) != 0) {
    recorder_close(recorder);
    if (!recorder_open(recorder, filename, lines, now_ms)) {
      return false;
    }
  }
  return true;
}

bool recorder_note(Recorder* recorder,
                   const char* filename,
                   int lines,
                   const char* word,
                   const char* text,
                   long long now_ms) {
  if (recorder->file == NULL && !recorder->failed) {
    if (filename == NULL) {
      return true;  // no recording to put it in
    }
    if (!recorder_use(recorder, filename, lines, now_ms)) {
      return false;
    }
  }
  if (recorder->failed || recorder->file == NULL) {
    return true;
  }
  const long long at =
    (now_ms > recorder->start_ms ? now_ms : recorder->start_ms) - recorder->start_ms;
  fprintf(recorder->file, "%s %lld%s%s\n", word, at, text[0] != '\0' ? " " : "", text);
  recorder->pending = true;
  return true;
}

bool recorder_key(Recorder* recorder,
                  const char* filename,
                  int lines,
                  int key,
                  const char* name,
                  long long now_ms) {
  if (!recorder_use(recorder, filename, lines, now_ms)) {
    return false;
  }
  if (recorder->failed || recorder->file == NULL) {
    return true;  // already reported
  }
  if (now_ms < recorder->last_ms) {
    now_ms = recorder->last_ms;  // the monotonic clock never goes back; be safe
  }
  const long long pause = now_ms - recorder->last_ms;
  const long long at = now_ms - recorder->start_ms;
  if (pause >= RECORD_PAUSE_MS) {
    char date[32];
    record_format_time(recorder->start_time + (time_t)(at / 1000), date,
                       sizeof(date));
    fprintf(recorder->file, "pause %s after %lld.%03lld s\n", date, pause / 1000,
            pause % 1000);
  }
  fprintf(recorder->file, "%lld %lld %d %s\n", at, pause, key, name);
  recorder->last_ms = now_ms;
  recorder->pending = true;
  return true;
}
