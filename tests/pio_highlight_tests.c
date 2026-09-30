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
#include "acutest.h"

#include "buffer.h"
#include "filetypes/pio.h"

static Buffer* pio_buffer(const char* line) {
  Buffer* buffer = buffer_alloc();
  buffer->filetype = &g_filetype_pio;
  buffer_append_line(buffer, line);
  return buffer;
}

/* Checks that columns [start, end) all carry the expected token */
static void check_range(const BufferRow* row, int start, int end,
                        EHighlightToken token) {
  for (int i = start; i < end; ++i) {
    TEST_CHECK_(row->highlight_data[i] == (char)token,
                "column %d of '%s' is %d, expected %d", i, row->data,
                row->highlight_data[i], token);
  }
}

/* Detection */

void test_pio_detected_by_extension(void) {
  TEST_CHECK(filetype_by_extension("mmc_spi.pio") == &g_filetype_pio);
  TEST_CHECK(filetype_detect("hal/sdio.pio", NULL) == &g_filetype_pio);
  TEST_CHECK(filetype_by_extension("main.c") != &g_filetype_pio);
}

/* Directives, instructions, operands */

void test_pio_directive(void) {
  Buffer* buffer = pio_buffer(".program mmc_spi");
  BufferRow* row = buffer->current_row;

  check_range(row, 0, 8, EHighlightToken_Preprocessor);
  check_range(row, 9, 16, EHighlightToken_Normal);

  buffer_free(buffer);
}

void test_pio_unknown_directive_is_plain(void) {
  Buffer* buffer = pio_buffer(".bogus 1");
  check_range(buffer->current_row, 0, 6, EHighlightToken_Normal);
  check_range(buffer->current_row, 7, 8, EHighlightToken_Digit);
  buffer_free(buffer);
}

void test_pio_instruction_line(void) {
  Buffer* buffer = pio_buffer("    out pins, 1 side 0 [7]");
  BufferRow* row = buffer->current_row;

  check_range(row, 4, 7, EHighlightToken_Keyword);    /* out */
  check_range(row, 8, 12, EHighlightToken_Type);      /* pins */
  check_range(row, 12, 13, EHighlightToken_Symbol);   /* , */
  check_range(row, 14, 15, EHighlightToken_Digit);    /* 1 */
  check_range(row, 16, 20, EHighlightToken_Keyword2); /* side */
  check_range(row, 21, 22, EHighlightToken_Digit);    /* 0 */
  check_range(row, 23, 24, EHighlightToken_Symbol2);  /* [ */
  check_range(row, 24, 25, EHighlightToken_Digit);    /* 7 */

  buffer_free(buffer);
}

void test_pio_keywords_ignore_case(void) {
  Buffer* buffer = pio_buffer("JMP X-- loop");
  BufferRow* row = buffer->current_row;

  check_range(row, 0, 3, EHighlightToken_Keyword);
  check_range(row, 4, 5, EHighlightToken_Type);
  check_range(row, 5, 7, EHighlightToken_Symbol);
  check_range(row, 8, 12, EHighlightToken_Normal);

  buffer_free(buffer);
}

void test_pio_label_and_public(void) {
  Buffer* buffer = pio_buffer("public entry:");
  BufferRow* row = buffer->current_row;

  check_range(row, 0, 6, EHighlightToken_Keyword2);
  check_range(row, 7, 12, EHighlightToken_Normal);
  check_range(row, 12, 13, EHighlightToken_Symbol);

  buffer_free(buffer);
}

void test_pio_mov_operands(void) {
  Buffer* buffer = pio_buffer("mov isr, ::~osr");
  BufferRow* row = buffer->current_row;

  check_range(row, 0, 3, EHighlightToken_Keyword);
  check_range(row, 4, 7, EHighlightToken_Type);
  check_range(row, 9, 12, EHighlightToken_Symbol);
  check_range(row, 12, 15, EHighlightToken_Type);

  buffer_free(buffer);
}

void test_pio_keyword_prefix_is_not_keyword(void) {
  Buffer* buffer = pio_buffer("jmpx pinsy");
  check_range(buffer->current_row, 0, 10, EHighlightToken_Normal);
  buffer_free(buffer);
}

/* Numbers */

void test_pio_numbers(void) {
  Buffer* buffer = pio_buffer("set pindirs, 0x1f ; 0b101");
  BufferRow* row = buffer->current_row;

  check_range(row, 13, 17, EHighlightToken_Digit);
  check_range(row, 18, 25, EHighlightToken_Comment);

  buffer_free(buffer);
}

void test_pio_float(void) {
  Buffer* buffer = pio_buffer(".clock_div 2.5");
  check_range(buffer->current_row, 0, 10, EHighlightToken_Preprocessor);
  check_range(buffer->current_row, 11, 14, EHighlightToken_Digit);
  buffer_free(buffer);
}

/* Comments */

void test_pio_comments(void) {
  Buffer* buffer = pio_buffer("nop ; jmp");
  check_range(buffer->current_row, 0, 3, EHighlightToken_Keyword);
  check_range(buffer->current_row, 4, 9, EHighlightToken_Comment);
  buffer_append_line(buffer, "nop // jmp");
  check_range(buffer_get_row(buffer, 1), 4, 10, EHighlightToken_Comment);
  buffer_free(buffer);
}

void test_pio_block_comment_spans_lines(void) {
  Buffer* buffer = pio_buffer("nop /* start");
  buffer_append_line(buffer, "jmp */ jmp");
  BufferRow* row = buffer_get_row(buffer, 1);

  check_range(buffer_get_row(buffer, 0), 4, 12, EHighlightToken_Comment);
  check_range(row, 0, 6, EHighlightToken_Comment);
  check_range(row, 7, 10, EHighlightToken_Keyword);

  buffer_free(buffer);
}

/* .lang_opt */

void test_pio_lang_opt(void) {
  Buffer* buffer = pio_buffer(".lang_opt python out_init = \"pico.PIO.OUT\"");
  BufferRow* row = buffer->current_row;

  check_range(row, 0, 9, EHighlightToken_Preprocessor);
  check_range(row, 10, 26, EHighlightToken_Normal);
  check_range(row, 29, row->len, EHighlightToken_String);

  buffer_free(buffer);
}

/* Code blocks */

void test_pio_c_sdk_block_is_c(void) {
  Buffer* buffer = pio_buffer("% c-sdk {");
  buffer_append_line(buffer, "static inline void f(int x) { /* in");
  buffer_append_line(buffer, "jmp */ return; }");
  buffer_append_line(buffer, "%}");
  buffer_append_line(buffer, "jmp x");

  check_range(buffer_get_row(buffer, 0), 0, 9, EHighlightToken_Preprocessor);

  BufferRow* c = buffer_get_row(buffer, 1);
  check_range(c, 0, 6, EHighlightToken_Keyword);   /* static */
  check_range(c, 14, 18, EHighlightToken_Type);    /* void */
  check_range(c, 30, 35, EHighlightToken_Comment); /* in C comment */

  BufferRow* c2 = buffer_get_row(buffer, 2);
  check_range(c2, 0, 6, EHighlightToken_Comment);  /* still C comment */
  check_range(c2, 7, 13, EHighlightToken_Keyword); /* return */

  check_range(buffer_get_row(buffer, 3), 0, 2, EHighlightToken_Preprocessor);

  BufferRow* after = buffer_get_row(buffer, 4);
  check_range(after, 0, 3, EHighlightToken_Keyword);
  check_range(after, 4, 5, EHighlightToken_Type);

  buffer_free(buffer);
}

void test_pio_pio_words_are_not_keywords_in_c(void) {
  Buffer* buffer = pio_buffer("% c-sdk {");
  buffer_append_line(buffer, "jmp; // x");
  BufferRow* row = buffer_get_row(buffer, 1);

  check_range(row, 0, 3, EHighlightToken_Normal);
  check_range(row, 5, 9, EHighlightToken_Comment);

  buffer_free(buffer);
}

void test_pio_zig_block_is_zig(void) {
  Buffer* buffer = pio_buffer("% zig {");
  buffer_append_line(buffer, "pub fn f() void {");
  buffer_append_line(buffer, "    _ = @as(u8, 1);");
  buffer_append_line(buffer, "%}");

  BufferRow* row = buffer_get_row(buffer, 1);
  check_range(row, 0, 3, EHighlightToken_Keyword);
  check_range(row, 4, 6, EHighlightToken_Keyword);
  check_range(row, 11, 15, EHighlightToken_Type);
  check_range(buffer_get_row(buffer, 2), 8, 11, EHighlightToken_Preprocessor);

  buffer_free(buffer);
}

void test_pio_other_block_is_plain(void) {
  Buffer* buffer = pio_buffer("% python {");
  buffer_append_line(buffer, "def f(): return 1 # jmp");
  buffer_append_line(buffer, "%}");
  buffer_append_line(buffer, "nop");

  check_range(buffer_get_row(buffer, 1), 0, 23, EHighlightToken_Normal);
  check_range(buffer_get_row(buffer, 3), 0, 3, EHighlightToken_Keyword);

  buffer_free(buffer);
}

void test_pio_indented_close_does_not_end_block(void) {
  Buffer* buffer = pio_buffer("% c-sdk {");
  buffer_append_line(buffer, "  %}");
  buffer_append_line(buffer, "int");

  check_range(buffer_get_row(buffer, 2), 0, 3, EHighlightToken_Type);

  buffer_free(buffer);
}

void test_pio_percent_without_brace_is_not_block(void) {
  Buffer* buffer = pio_buffer("% c-sdk");
  buffer_append_line(buffer, "jmp");

  check_range(buffer_get_row(buffer, 1), 0, 3, EHighlightToken_Keyword);

  buffer_free(buffer);
}

void test_pio_closing_block_rehighlights_below(void) {
  Buffer* buffer = pio_buffer("% c-sdk {");
  buffer_append_line(buffer, "%");
  buffer_append_line(buffer, "jmp");
  BufferRow* close = buffer_get_row(buffer, 1);
  BufferRow* below = buffer_get_row(buffer, 2);

  check_range(below, 0, 3, EHighlightToken_Normal);

  /* Typing the '}' of "%}" ends the block: the line below is pio again */
  buffer_row_insert_char(close, 1, '}');
  check_range(close, 0, 2, EHighlightToken_Preprocessor);
  check_range(below, 0, 3, EHighlightToken_Keyword);

  buffer_free(buffer);
}

void test_pio_first_row_does_not_inherit_its_own_state(void) {
  Buffer* buffer = pio_buffer("% c-sdk {");
  BufferRow* row = buffer_get_row(buffer, 0);

  /* Re-highlighting the opening line must start outside the block again */
  buffer_row_insert_char(row, row->len, ' ');
  check_range(row, 0, 9, EHighlightToken_Preprocessor);

  buffer_free(buffer);
}

TEST_LIST = {
    {"test_pio_detected_by_extension", test_pio_detected_by_extension},
    /* Words */
    {"test_pio_directive", test_pio_directive},
    {"test_pio_unknown_directive_is_plain", test_pio_unknown_directive_is_plain},
    {"test_pio_instruction_line", test_pio_instruction_line},
    {"test_pio_keywords_ignore_case", test_pio_keywords_ignore_case},
    {"test_pio_label_and_public", test_pio_label_and_public},
    {"test_pio_mov_operands", test_pio_mov_operands},
    {"test_pio_keyword_prefix_is_not_keyword",
     test_pio_keyword_prefix_is_not_keyword},
    /* Numbers */
    {"test_pio_numbers", test_pio_numbers},
    {"test_pio_float", test_pio_float},
    /* Comments */
    {"test_pio_comments", test_pio_comments},
    {"test_pio_block_comment_spans_lines", test_pio_block_comment_spans_lines},
    {"test_pio_lang_opt", test_pio_lang_opt},
    /* Code blocks */
    {"test_pio_c_sdk_block_is_c", test_pio_c_sdk_block_is_c},
    {"test_pio_pio_words_are_not_keywords_in_c",
     test_pio_pio_words_are_not_keywords_in_c},
    {"test_pio_zig_block_is_zig", test_pio_zig_block_is_zig},
    {"test_pio_other_block_is_plain", test_pio_other_block_is_plain},
    {"test_pio_indented_close_does_not_end_block",
     test_pio_indented_close_does_not_end_block},
    {"test_pio_percent_without_brace_is_not_block",
     test_pio_percent_without_brace_is_not_block},
    {"test_pio_closing_block_rehighlights_below",
     test_pio_closing_block_rehighlights_below},
    {"test_pio_first_row_does_not_inherit_its_own_state",
     test_pio_first_row_does_not_inherit_its_own_state},

    {NULL, NULL} /* zeroed record marking the end of the list */
};
