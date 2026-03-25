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

#include "acutest.h"

#include "buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void test_buffer_load_from_file_existing(void) {
  // Create a temporary test file
  const char* test_file = "/tmp/yasvi_test_file.txt";
  FILE* f = fopen(test_file, "w");
  TEST_CHECK(f != NULL);
  fprintf(f, "Line 1\nLine 2\nLine 3\n");
  fclose(f);

  Buffer* buffer = buffer_alloc();
  buffer_load_from_file(buffer, test_file);

  TEST_CHECK(buffer->number_of_rows == 3);
  TEST_CHECK(strcmp(buffer->head->data, "Line 1") == 0);
  TEST_CHECK(strcmp(buffer->head->next->data, "Line 2") == 0);
  TEST_CHECK(strcmp(buffer->tail->data, "Line 3") == 0);

  buffer_free(buffer);
  remove(test_file);
}

void test_buffer_load_from_file_nonexistent(void) {
  const char* test_file = "/tmp/yasvi_nonexistent_file_xyz.txt";
  // Make sure file doesn't exist
  remove(test_file);

  Buffer* buffer = buffer_alloc();
  buffer_load_from_file(buffer, test_file);

  // Should have one empty line for new file
  TEST_CHECK(buffer->number_of_rows == 1);
  TEST_CHECK(buffer->head != NULL);
  TEST_CHECK(buffer->filename != NULL);
  TEST_CHECK(strcmp(buffer->filename, test_file) == 0);

  buffer_free(buffer);
}

void test_buffer_load_from_file_empty(void) {
  // Create an empty test file
  const char* test_file = "/tmp/yasvi_empty_test_file.txt";
  FILE* f = fopen(test_file, "w");
  TEST_CHECK(f != NULL);
  fclose(f);

  Buffer* buffer = buffer_alloc();
  buffer_load_from_file(buffer, test_file);

  // Should have at least one line
  TEST_CHECK(buffer->number_of_rows >= 1);

  buffer_free(buffer);
  remove(test_file);
}

void test_buffer_load_from_file_large(void) {
  // Create a larger test file
  const char* test_file = "/tmp/yasvi_large_test_file.txt";
  FILE* f = fopen(test_file, "w");
  TEST_CHECK(f != NULL);

  const int num_lines = 100;
  for (int i = 0; i < num_lines; i++) {
    fprintf(f, "This is line number %d with some content\n", i);
  }
  fclose(f);

  Buffer* buffer = buffer_alloc();
  buffer_load_from_file(buffer, test_file);

  TEST_CHECK(buffer->number_of_rows == num_lines);
  TEST_CHECK(strcmp(buffer->head->data, "This is line number 0 with some content") == 0);
  TEST_CHECK(strcmp(buffer->tail->data, "This is line number 99 with some content") == 0);

  // Verify some middle line
  BufferRow* row = buffer_get_row(buffer, 50);
  TEST_CHECK(row != NULL);
  TEST_CHECK(strcmp(row->data, "This is line number 50 with some content") == 0);

  buffer_free(buffer);
  remove(test_file);
}

void test_buffer_load_from_file_no_trailing_newline(void) {
  // Create a file without trailing newline
  const char* test_file = "/tmp/yasvi_no_nl_test_file.txt";
  FILE* f = fopen(test_file, "w");
  TEST_CHECK(f != NULL);
  fprintf(f, "Line 1\nLine 2\nLine 3");  // No newline at end
  fclose(f);

  Buffer* buffer = buffer_alloc();
  buffer_load_from_file(buffer, test_file);

  TEST_CHECK(buffer->number_of_rows == 3);
  TEST_CHECK(strcmp(buffer->head->data, "Line 1") == 0);
  TEST_CHECK(strcmp(buffer->tail->data, "Line 3") == 0);

  buffer_free(buffer);
  remove(test_file);
}

void test_buffer_load_from_file_crlf(void) {
  // Create a file with CRLF line endings
  const char* test_file = "/tmp/yasvi_crlf_test_file.txt";
  FILE* f = fopen(test_file, "w");
  TEST_CHECK(f != NULL);
  fprintf(f, "Line 1\r\nLine 2\r\nLine 3\r\n");
  fclose(f);

  Buffer* buffer = buffer_alloc();
  buffer_load_from_file(buffer, test_file);

  TEST_CHECK(buffer->number_of_rows == 3);
  TEST_CHECK(strcmp(buffer->head->data, "Line 1") == 0);
  TEST_CHECK(strcmp(buffer->head->next->data, "Line 2") == 0);
  TEST_CHECK(strcmp(buffer->tail->data, "Line 3") == 0);

  buffer_free(buffer);
  remove(test_file);
}

void test_buffer_load_from_file_null(void) {
  // NULL buffer should not crash
  buffer_load_from_file(NULL, "/tmp/test.txt");

  Buffer* buffer = buffer_alloc();
  // NULL filename should not crash
  buffer_load_from_file(buffer, NULL);

  TEST_CHECK(buffer->number_of_rows == 0);

  buffer_free(buffer);
}

void test_buffer_get_filename(void) {
  const char* test_file = "/tmp/yasvi_filename_test.txt";
  FILE* f = fopen(test_file, "w");
  fprintf(f, "test content\n");
  fclose(f);

  Buffer* buffer = buffer_alloc();

  // Before loading, filename should be NULL
  TEST_CHECK(buffer_get_filename(buffer) == NULL);

  buffer_load_from_file(buffer, test_file);

  // After loading, filename should be set
  const char* filename = buffer_get_filename(buffer);
  TEST_CHECK(filename != NULL);
  TEST_CHECK(strcmp(filename, test_file) == 0);

  buffer_free(buffer);
  remove(test_file);
}

void test_buffer_get_filename_null(void) {
  // NULL buffer should return NULL
  TEST_CHECK(buffer_get_filename(NULL) == NULL);
}

TEST_LIST = {
    {"test_buffer_load_from_file_existing", test_buffer_load_from_file_existing},
    {"test_buffer_load_from_file_nonexistent", test_buffer_load_from_file_nonexistent},
    {"test_buffer_load_from_file_empty", test_buffer_load_from_file_empty},
    {"test_buffer_load_from_file_large", test_buffer_load_from_file_large},
    {"test_buffer_load_from_file_no_trailing_newline", test_buffer_load_from_file_no_trailing_newline},
    {"test_buffer_load_from_file_crlf", test_buffer_load_from_file_crlf},
    {"test_buffer_load_from_file_null", test_buffer_load_from_file_null},
    {"test_buffer_get_filename", test_buffer_get_filename},
    {"test_buffer_get_filename_null", test_buffer_get_filename_null},

    {NULL, NULL}  // zeroed record marking the end of the list
};
