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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "acutest.h"
#include "buffer.h"
#include "completion.h"
#include "filetypes/filetype.h"

/* A buffer holding `lines`, its current row set to line `current`. */
static Buffer* make_buffer(const char* const* lines, int current, const Filetype* filetype) {
  Buffer* buffer = buffer_alloc();
  for (int i = 0; lines[i] != NULL; ++i) {
    buffer_append_line(buffer, lines[i]);
  }
  buffer->filetype = filetype;
  buffer->current_row = buffer_get_row(buffer, current);
  return buffer;
}

static int index_of(const CompletionList* list, const char* word) {
  for (int i = 0; i < list->count; ++i) {
    if (strcmp(list->items[i], word) == 0) {
      return i;
    }
  }
  return -1;
}

static void collect(CompletionList* out, Buffer* buffer, const char* prefix, int start) {
  CompletionSources sources = {.current = buffer};
  completion_collect(out, prefix, start, &sources);
}

void test_word_start_plain_identifier(void) {
  const char* line = "  foo_bar1 + x";
  TEST_CHECK(completion_word_start(line, 14, 10, NULL) == 2);  // after "foo_bar1"
  TEST_CHECK(completion_word_start(line, 14, 5, NULL) == 2);   // inside it
  TEST_CHECK(completion_word_start(line, 14, 11, NULL) == 11); // after a space
  TEST_CHECK(completion_word_start(line, 14, 0, NULL) == 0);
}

void test_word_start_takes_a_leading_prefix_char(void) {
  const char* line = "const s = @imp";
  TEST_CHECK(completion_word_start(line, 14, 14, "@") == 10);
  TEST_CHECK(completion_word_start(line, 14, 14, NULL) == 11);
  TEST_CHECK(completion_word_start(line, 14, 11, "@") == 10);  // just "@"
}

void test_nearest_following_word_comes_first(void) {
  const char* lines[] = {"counter_a", "coun", "counter_b", "counter_c", NULL};
  Buffer* buffer = make_buffer(lines, 1, NULL);
  CompletionList list;
  collect(&list, buffer, "coun", 0);
  // Forward from the cursor row, then wrapping round to the top.
  TEST_CHECK(list.count == 3);
  TEST_CHECK(index_of(&list, "counter_b") == 0);
  TEST_CHECK(index_of(&list, "counter_c") == 1);
  TEST_CHECK(index_of(&list, "counter_a") == 2);
  completion_list_free(&list);
  buffer_free(buffer);
}

void test_word_being_completed_and_duplicates_are_left_out(void) {
  const char* lines[] = {"value value valueX", "val", "value", NULL};
  Buffer* buffer = make_buffer(lines, 1, NULL);
  CompletionList list;
  collect(&list, buffer, "val", 0);
  TEST_CHECK(list.count == 2);
  TEST_CHECK(index_of(&list, "value") == 0);
  TEST_CHECK(index_of(&list, "valueX") == 1);
  TEST_CHECK(index_of(&list, "val") == -1);
  completion_list_free(&list);
  buffer_free(buffer);
}

void test_numbers_are_not_words(void) {
  const char* lines[] = {"x1 = 10 + x2", "x", NULL};
  Buffer* buffer = make_buffer(lines, 1, NULL);
  CompletionList list;
  collect(&list, buffer, "", 0);
  TEST_CHECK(index_of(&list, "x1") >= 0);
  TEST_CHECK(index_of(&list, "x2") >= 0);
  TEST_CHECK(index_of(&list, "10") == -1);
  completion_list_free(&list);
  buffer_free(buffer);
}

void test_zig_keywords_types_and_builtins(void) {
  const char* lines[] = {"const std = @import(\"std\");", "", NULL};
  Buffer* buffer = make_buffer(lines, 1, &g_filetype_zig);
  CompletionList list;

  collect(&list, buffer, "comp", 0);
  TEST_CHECK(index_of(&list, "comptime") >= 0);
  TEST_CHECK(index_of(&list, "comptime_int") >= 0);
  completion_list_free(&list);

  collect(&list, buffer, "@int", 0);
  TEST_CHECK(index_of(&list, "@intCast") >= 0);
  TEST_CHECK(index_of(&list, "@intFromEnum") >= 0);
  TEST_CHECK(index_of(&list, "@import") == -1);
  completion_list_free(&list);

  // "@im": the @import already in the buffer comes before the builtin list,
  // and only once.
  collect(&list, buffer, "@im", 0);
  TEST_CHECK(list.count == 1);
  TEST_CHECK(index_of(&list, "@import") == 0);
  completion_list_free(&list);

  // Without '@' the builtins do not match, but the buffer's words do.
  collect(&list, buffer, "st", 0);
  TEST_CHECK(index_of(&list, "std") == 0);
  TEST_CHECK(index_of(&list, "struct") > 0);
  completion_list_free(&list);
  buffer_free(buffer);
}

void test_other_buffers_follow_the_current_one(void) {
  const char* here[] = {"alpha_here", "alp", NULL};
  const char* there[] = {"alpha_there alpha_here", NULL};
  Buffer* current = make_buffer(here, 1, NULL);
  Buffer* other = make_buffer(there, 0, NULL);
  const Buffer* all[] = {current, other};
  CompletionSources sources = {.current = current, .others = all, .number_of_others = 2};
  CompletionList list;
  completion_collect(&list, "alp", 0, &sources);
  TEST_CHECK(list.count == 2);
  TEST_CHECK(index_of(&list, "alpha_here") == 0);
  TEST_CHECK(index_of(&list, "alpha_there") == 1);
  completion_list_free(&list);
  buffer_free(current);
  buffer_free(other);
}

void test_tag_names_come_last(void) {
  char path[] = "/tmp/yasvi_completion_tags_XXXXXX";
  int fd = mkstemp(path);
  TEST_ASSERT(fd >= 0);
  FILE* file = fdopen(fd, "w");
  fputs("!_TAG_FILE_SORTED\t1\t/0=unsorted/\n", file);
  fputs("parse_header\tsrc/p.c\t/^int parse_header(void)$/;\"\tf\n", file);
  fputs("parse_header\tsrc/q.c\t/^int parse_header(void)$/;\"\tf\n", file);
  fputs("parse_line\tsrc/p.c\t12;\"\tf\n", file);
  fputs("print_all\tsrc/p.c\t20;\"\tf\n", file);
  fclose(file);

  const char* lines[] = {"parse_local", "pars", NULL};
  Buffer* buffer = make_buffer(lines, 1, NULL);
  CompletionSources sources = {.current = buffer, .tags_path = path};
  CompletionList list;
  completion_collect(&list, "pars", 0, &sources);
  TEST_CHECK(list.count == 3);
  TEST_CHECK(index_of(&list, "parse_local") == 0);
  TEST_CHECK(index_of(&list, "parse_header") == 1);
  TEST_CHECK(index_of(&list, "parse_line") == 2);
  completion_list_free(&list);
  buffer_free(buffer);
  unlink(path);
}

void test_list_is_capped(void) {
  Buffer* buffer = buffer_alloc();
  char word[32];
  for (int i = 0; i < COMPLETION_MAX_ITEMS + 50; ++i) {
    snprintf(word, sizeof(word), "w%d", i);
    buffer_append_line(buffer, word);
  }
  buffer_append_line(buffer, "w");
  buffer->current_row = buffer->tail;
  CompletionList list;
  collect(&list, buffer, "w", 0);
  TEST_CHECK(list.count == COMPLETION_MAX_ITEMS);
  completion_list_free(&list);
  buffer_free(buffer);
}

void test_step_walks_round_through_the_typed_word(void) {
  Completion completion = {0};
  completion.original = strdup("ab");
  completion.selected = -1;
  const char* lines[] = {"abc abd", "ab", NULL};
  Buffer* buffer = make_buffer(lines, 1, NULL);
  collect(&completion.list, buffer, "ab", 0);
  TEST_ASSERT(completion.list.count == 2);

  TEST_CHECK(strcmp(completion_step(&completion, 1), "abc") == 0);
  TEST_CHECK(strcmp(completion_step(&completion, 1), "abd") == 0);
  TEST_CHECK(strcmp(completion_step(&completion, 1), "ab") == 0);   // typed word
  TEST_CHECK(strcmp(completion_step(&completion, 1), "abc") == 0);
  TEST_CHECK(strcmp(completion_step(&completion, -1), "ab") == 0);
  TEST_CHECK(strcmp(completion_step(&completion, -1), "abd") == 0);

  completion_reset(&completion);
  TEST_CHECK(completion.original == NULL);
  TEST_CHECK(completion.list.count == 0);
  buffer_free(buffer);
}

TEST_LIST = {
  {"test_word_start_plain_identifier", test_word_start_plain_identifier},
  {"test_word_start_takes_a_leading_prefix_char", test_word_start_takes_a_leading_prefix_char},
  {"test_nearest_following_word_comes_first", test_nearest_following_word_comes_first},
  {"test_word_being_completed_and_duplicates_are_left_out",
   test_word_being_completed_and_duplicates_are_left_out},
  {"test_numbers_are_not_words", test_numbers_are_not_words},
  {"test_zig_keywords_types_and_builtins", test_zig_keywords_types_and_builtins},
  {"test_other_buffers_follow_the_current_one", test_other_buffers_follow_the_current_one},
  {"test_tag_names_come_last", test_tag_names_come_last},
  {"test_list_is_capped", test_list_is_capped},
  {"test_step_walks_round_through_the_typed_word", test_step_walks_round_through_the_typed_word},
  {NULL, NULL}};
