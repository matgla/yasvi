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

#include "acutest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "buffer.h"
#include "tags.h"

static void write_file(const char* path, const char* content) {
  FILE* file = fopen(path, "w");
  TEST_ASSERT(file != NULL);
  fputs(content, file);
  fclose(file);
}

static Buffer* buffer_with_lines(const char* const* lines, int count) {
  Buffer* buffer = buffer_alloc();
  for (int i = 0; i < count; ++i) {
    buffer_append_line(buffer, lines[i]);
  }
  return buffer;
}

static void clear_entry(TagEntry* entry) {
  free(entry->name);
  free(entry->filename);
  free(entry->pattern);
}

void test_tags_parse_anchored_pattern(void) {
  TagEntry entry;
  TEST_CHECK(tags_parse_line("main\tmain.c\t/^int main(void)$/;\"\tf", NULL, &entry));
  TEST_CHECK(strcmp(entry.name, "main") == 0);
  TEST_CHECK(strcmp(entry.filename, "main.c") == 0);
  TEST_CHECK(strcmp(entry.pattern, "int main(void)") == 0);
  TEST_CHECK(entry.anchor_start);
  TEST_CHECK(entry.anchor_end);
  TEST_CHECK(entry.line == 0);
  clear_entry(&entry);
}

void test_tags_parse_escapes(void) {
  TagEntry entry;
  TEST_CHECK(tags_parse_line("x\tx.c\t/^a\\/b \\\\ c$/;\"\tv", NULL, &entry));
  TEST_CHECK(strcmp(entry.pattern, "a/b \\ c") == 0);
  TEST_MSG("pattern: [%s]", entry.pattern);
  TEST_CHECK(entry.anchor_end);
  clear_entry(&entry);
}

void test_tags_parse_escaped_backslash_before_end_anchor(void) {
  TagEntry entry;
  /* A line ending in a backslash: `\\` is one backslash of text and the `$`
   * after it is still the end anchor. */
  TEST_CHECK(tags_parse_line("q\tq.c\t/^#define X a\\\\$/;\"\td", NULL, &entry));
  TEST_CHECK(strcmp(entry.pattern, "#define X a\\") == 0);
  TEST_MSG("pattern: [%s]", entry.pattern);
  TEST_CHECK(entry.anchor_end);
  clear_entry(&entry);
}

void test_tags_parse_truncated_pattern_has_no_end_anchor(void) {
  TagEntry entry;
  TEST_CHECK(tags_parse_line("f\tf.c\t/^static int f(int a, int b/;\"\tf", NULL, &entry));
  TEST_CHECK(entry.anchor_start);
  TEST_CHECK(!entry.anchor_end);
  clear_entry(&entry);
}

void test_tags_parse_line_number(void) {
  TagEntry entry;
  TEST_CHECK(tags_parse_line("SMP\tKconfig\t42;\"\tc", NULL, &entry));
  TEST_CHECK(entry.line == 42);
  TEST_CHECK(entry.pattern == NULL);
  clear_entry(&entry);
}

void test_tags_parse_rejects_pseudo_tags_and_junk(void) {
  TagEntry entry;
  TEST_CHECK(!tags_parse_line("!_TAG_FILE_FORMAT\t2\t/extended format/", NULL, &entry));
  TEST_CHECK(!tags_parse_line("", NULL, &entry));
  TEST_CHECK(!tags_parse_line("name-only", NULL, &entry));
  TEST_CHECK(!tags_parse_line("n\tf.c\t/^unterminated", NULL, &entry));
}

void test_tags_parse_resolves_filename_against_tags_dir(void) {
  TagEntry entry;
  TEST_CHECK(tags_parse_line("a\t./src/a.c\t1;\"", "proj", &entry));
  TEST_CHECK(strcmp(entry.filename, "proj/src/a.c") == 0);
  clear_entry(&entry);
  TEST_CHECK(tags_parse_line("a\t./a.c\t1;\"", ".", &entry));
  TEST_CHECK(strcmp(entry.filename, "a.c") == 0);
  clear_entry(&entry);
  TEST_CHECK(tags_parse_line("a\t/abs/a.c\t1;\"", "proj", &entry));
  TEST_CHECK(strcmp(entry.filename, "/abs/a.c") == 0);
  clear_entry(&entry);
  TEST_CHECK(tags_parse_line("a\tusr/a.c\t1;\"", "/", &entry));
  TEST_CHECK(strcmp(entry.filename, "/usr/a.c") == 0);
  clear_entry(&entry);
}

void test_tags_lookup_collects_every_match(void) {
  char dir[] = "/tmp/yasvi_tags_XXXXXX";
  TEST_ASSERT(mkdtemp(dir) != NULL);
  char path[256];
  snprintf(path, sizeof(path), "%s/tags", dir);
  write_file(path,
             "!_TAG_FILE_SORTED\t1\t/0=unsorted, 1=sorted/\n"
             "add\ta.c\t/^int add(int a, int b)$/;\"\tf\n"
             "add\tb.c\t/^static int add(int a)$/;\"\tf\n"
             "addx\ta.c\t/^int addx;$/;\"\tv\n"
             "main\tmain.c\t/^int main(void)$/;\"\tf\n");

  TagEntry* entries = NULL;
  int count = tags_lookup(path, "add", &entries);
  TEST_CHECK(count == 2);
  if (count == 2) {
    char expected[300];
    snprintf(expected, sizeof(expected), "%s/a.c", dir);
    TEST_CHECK(strcmp(entries[0].filename, expected) == 0);
    TEST_MSG("filename: %s", entries[0].filename);
    snprintf(expected, sizeof(expected), "%s/b.c", dir);
    TEST_CHECK(strcmp(entries[1].filename, expected) == 0);
  }
  tags_free_entries(entries, count);

  TEST_CHECK(tags_lookup(path, "missing", &entries) == 0);
  TEST_CHECK(entries == NULL);
  TEST_CHECK(tags_lookup("/nonexistent/tags", "add", &entries) == 0);

  unlink(path);
  rmdir(dir);
}

void test_tags_find_line_by_pattern(void) {
  const char* lines[] = {
    "#include <stdio.h>\n",
    "static int helper(int a, int b, int c, int d)\n",
    "int main(void)\n",
    "int main(void) /* not this one */\n",
  };
  Buffer* buffer = buffer_with_lines(lines, 4);
  TagEntry entry;

  TEST_CHECK(tags_parse_line("main\tm.c\t/^int main(void)$/;\"\tf", NULL, &entry));
  TEST_CHECK(tags_find_line(&entry, buffer) == 2);
  clear_entry(&entry);

  /* Truncated: prefix match. */
  TEST_CHECK(tags_parse_line("helper\tm.c\t/^static int helper(int a, int/;\"\tf", NULL,
                             &entry));
  TEST_CHECK(tags_find_line(&entry, buffer) == 1);
  clear_entry(&entry);

  TEST_CHECK(tags_parse_line("gone\tm.c\t/^int gone(void)$/;\"\tf", NULL, &entry));
  TEST_CHECK(tags_find_line(&entry, buffer) == -1);
  clear_entry(&entry);

  buffer_free(buffer);
}

void test_tags_find_line_by_number_clamps(void) {
  const char* lines[] = {"a\n", "b\n", "c\n"};
  Buffer* buffer = buffer_with_lines(lines, 3);
  TagEntry entry;
  TEST_CHECK(tags_parse_line("b\tf\t2;\"", NULL, &entry));
  TEST_CHECK(tags_find_line(&entry, buffer) == 1);
  clear_entry(&entry);
  TEST_CHECK(tags_parse_line("z\tf\t99;\"", NULL, &entry));
  TEST_CHECK(tags_find_line(&entry, buffer) == 2);
  clear_entry(&entry);
  buffer_free(buffer);
}

void test_tags_find_file_walks_up_from_the_edited_file(void) {
  char dir[] = "/tmp/yasvi_tags_XXXXXX";
  TEST_ASSERT(mkdtemp(dir) != NULL);
  char sub[256];
  char tags[256];
  char edited[300];
  snprintf(sub, sizeof(sub), "%s/src", dir);
  TEST_ASSERT(mkdir(sub, 0755) == 0);
  snprintf(tags, sizeof(tags), "%s/tags", dir);
  write_file(tags, "x\tsrc/x.c\t1;\"\n");
  snprintf(edited, sizeof(edited), "%s/x.c", sub);

  char* found = tags_find_file(edited);
  TEST_CHECK(found != NULL && strcmp(found, tags) == 0);
  TEST_MSG("found: %s", found ? found : "(null)");
  free(found);

  unlink(tags);
  rmdir(sub);
  rmdir(dir);
}

TEST_LIST = {
  {"tags_parse_anchored_pattern", test_tags_parse_anchored_pattern},
  {"tags_parse_escapes", test_tags_parse_escapes},
  {"tags_parse_escaped_backslash_before_end_anchor",
   test_tags_parse_escaped_backslash_before_end_anchor},
  {"tags_parse_truncated_pattern_has_no_end_anchor",
   test_tags_parse_truncated_pattern_has_no_end_anchor},
  {"tags_parse_line_number", test_tags_parse_line_number},
  {"tags_parse_rejects_pseudo_tags_and_junk",
   test_tags_parse_rejects_pseudo_tags_and_junk},
  {"tags_parse_resolves_filename_against_tags_dir",
   test_tags_parse_resolves_filename_against_tags_dir},
  {"tags_lookup_collects_every_match", test_tags_lookup_collects_every_match},
  {"tags_find_line_by_pattern", test_tags_find_line_by_pattern},
  {"tags_find_line_by_number_clamps", test_tags_find_line_by_number_clamps},
  {"tags_find_file_walks_up_from_the_edited_file",
   test_tags_find_file_walks_up_from_the_edited_file},
  {NULL, NULL}};
