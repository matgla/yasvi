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

#include "completion.h"

#include <stdlib.h>
#include <string.h>

#include "filetypes/filetype.h"
#include "tags.h"

static bool completion_is_ident(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
         c == '_';
}

static bool completion_is_digit(char c) {
  return c >= '0' && c <= '9';
}

static bool completion_is_prefix_char(char c, const char* prefix_chars) {
  return c != '\0' && prefix_chars != NULL && strchr(prefix_chars, c) != NULL;
}

int completion_word_start(const char* line, int len, int cursor, const char* prefix_chars) {
  if (line == NULL || cursor <= 0) {
    return cursor < 0 ? 0 : cursor;
  }
  if (cursor > len) {
    cursor = len;
  }
  int start = cursor;
  while (start > 0 && completion_is_ident(line[start - 1])) {
    --start;
  }
  if (start > 0 && completion_is_prefix_char(line[start - 1], prefix_chars)) {
    --start;
  }
  return start;
}

typedef struct {
  CompletionList* out;
  const char* prefix;
  size_t prefix_len;
} Collector;

// Adds a candidate. Returns false once the list is full.
static bool completion_add(Collector* collector, const char* word, size_t len) {
  CompletionList* out = collector->out;
  if (out->count >= COMPLETION_MAX_ITEMS) {
    return false;
  }
  if (len <= collector->prefix_len ||
      strncmp(word, collector->prefix, collector->prefix_len) != 0) {
    return true;
  }
  for (int i = 0; i < out->count; ++i) {
    if (strncmp(out->items[i], word, len) == 0 && out->items[i][len] == '\0') {
      return true;
    }
  }
  char* copy = (char*)malloc(len + 1);
  if (copy == NULL) {
    return false;
  }
  memcpy(copy, word, len);
  copy[len] = '\0';
  if (out->items == NULL) {
    out->items = (char**)malloc(sizeof(char*) * COMPLETION_MAX_ITEMS);
    if (out->items == NULL) {
      free(copy);
      return false;
    }
  }
  out->items[out->count++] = copy;
  return true;
}

static bool completion_visit_tag(void* context, const char* name, size_t len) {
  return completion_add((Collector*)context, name, len);
}

// Adds the words of `row` whose start column is in [from, to), except the one
// starting at `skip` (the word being completed; -1 for none).
static bool completion_add_row(Collector* collector,
                               const BufferRow* row,
                               const char* prefix_chars,
                               int from,
                               int to,
                               int skip) {
  const char* data = row->data;
  const int len = row->len;
  int i = 0;
  while (i < len) {
    int word = i;
    int ident = i;
    if (completion_is_prefix_char(data[i], prefix_chars) && i + 1 < len &&
        completion_is_ident(data[i + 1]) && (i == 0 || !completion_is_ident(data[i - 1]))) {
      ident = i + 1;
    } else if (!completion_is_ident(data[i])) {
      ++i;
      continue;
    }
    int end = ident;
    while (end < len && completion_is_ident(data[end])) {
      ++end;
    }
    // A word that starts with a digit is a number, not something to complete.
    if (!completion_is_digit(data[ident]) && word >= from && word < to && word != skip) {
      if (!completion_add(collector, &data[word], (size_t)(end - word))) {
        return false;
      }
    }
    i = end;
  }
  return true;
}

static bool completion_add_buffer(Collector* collector, const Buffer* buffer, const char* prefix_chars) {
  for (const BufferRow* row = buffer->head; row != NULL; row = row->next) {
    if (!completion_add_row(collector, row, prefix_chars, 0, row->len, -1)) {
      return false;
    }
  }
  return true;
}

// The current buffer, nearest following word first: the rest of the cursor
// row, the rows below, then from the top back down to the cursor.
static bool completion_add_current(Collector* collector,
                                   const Buffer* buffer,
                                   const char* prefix_chars,
                                   int start) {
  const BufferRow* current = buffer->current_row;
  if (current == NULL) {
    return completion_add_buffer(collector, buffer, prefix_chars);
  }
  if (!completion_add_row(collector, current, prefix_chars, start, current->len, start)) {
    return false;
  }
  for (const BufferRow* row = current->next; row != NULL; row = row->next) {
    if (!completion_add_row(collector, row, prefix_chars, 0, row->len, -1)) {
      return false;
    }
  }
  for (const BufferRow* row = buffer->head; row != NULL && row != current; row = row->next) {
    if (!completion_add_row(collector, row, prefix_chars, 0, row->len, -1)) {
      return false;
    }
  }
  return completion_add_row(collector, current, prefix_chars, 0, start, start);
}

void completion_collect(CompletionList* out,
                        const char* prefix,
                        int start,
                        const CompletionSources* sources) {
  out->items = NULL;
  out->count = 0;
  if (prefix == NULL || sources == NULL || sources->current == NULL) {
    return;
  }
  Collector collector = {out, prefix, strlen(prefix)};
  const Filetype* filetype = sources->current->filetype;
  const char* prefix_chars = filetype != NULL ? filetype->word_prefix_chars : NULL;

  if (!completion_add_current(&collector, sources->current, prefix_chars, start)) {
    return;
  }
  for (size_t i = 0; i < sources->number_of_others; ++i) {
    const Buffer* other = sources->others[i];
    if (other != NULL && other != sources->current &&
        !completion_add_buffer(&collector, other, prefix_chars)) {
      return;
    }
  }
  if (filetype != NULL && filetype->completion_words != NULL) {
    for (const char*** list = filetype->completion_words; *list != NULL; ++list) {
      for (const char** word = *list; *word != NULL; ++word) {
        if (!completion_add(&collector, *word, strlen(*word))) {
          return;
        }
      }
    }
  }
  if (sources->tags_path != NULL) {
    tags_for_each_name(sources->tags_path, prefix, completion_visit_tag, &collector);
  }
}

void completion_list_free(CompletionList* list) {
  for (int i = 0; i < list->count; ++i) {
    free(list->items[i]);
  }
  free(list->items);
  list->items = NULL;
  list->count = 0;
}

const char* completion_current_text(const Completion* completion) {
  if (completion->selected < 0 || completion->selected >= completion->list.count) {
    return completion->original != NULL ? completion->original : "";
  }
  return completion->list.items[completion->selected];
}

const char* completion_step(Completion* completion, int step) {
  const int count = completion->list.count;
  if (count > 0) {
    // Positions -1 (the original) .. count-1, walked round in a ring.
    int position = completion->selected + 1 + step;
    position %= count + 1;
    if (position < 0) {
      position += count + 1;
    }
    completion->selected = position - 1;
  }
  return completion_current_text(completion);
}

void completion_reset(Completion* completion) {
  completion_list_free(&completion->list);
  free(completion->original);
  completion->original = NULL;
  completion->active = false;
  completion->row = NULL;
  completion->start = 0;
  completion->inserted_len = 0;
  completion->selected = -1;
}
