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

#include "zig.h"

#include <stdbool.h>
#include <string.h>

static const char* zig_extensions[] = {".zig", ".zon", NULL};

// Zig keywords (async/await/usingnamespace kept for pre-0.15 sources)
static const char* keywords_1[] = {
  "addrspace",   "align",      "allowzero",   "and",      "anyframe",
  "anytype",     "asm",        "async",       "await",    "break",
  "callconv",    "catch",      "comptime",    "const",    "continue",
  "defer",       "else",       "enum",        "errdefer", "error",
  "export",      "extern",     "fn",          "for",      "if",
  "inline",      "linksection","noalias",     "noinline", "nosuspend",
  "opaque",      "or",         "orelse",      "packed",   "pub",
  "resume",      "return",     "struct",      "suspend",  "switch",
  "test",        "threadlocal","try",         "union",    "unreachable",
  "usingnamespace", "var",     "volatile",    "while",    NULL};

// Primitive types other than the iN/uN integers, which is_int_type matches
static const char* types[] = {
  "isize",       "usize",      "c_char",      "c_short",  "c_ushort",
  "c_int",       "c_uint",     "c_long",      "c_ulong",  "c_longlong",
  "c_ulonglong", "c_longdouble", "f16",       "f32",      "f64",
  "f80",         "f128",       "bool",        "anyopaque","void",
  "noreturn",    "type",       "anyerror",    "comptime_int",
  "comptime_float", NULL};

// Primitive values
static const char* keywords_2[] = {"true", "false", "null", "undefined", NULL};

static const char* symbols = "+-|<>=:?!(),;/%^~.";
static const char* symbols2 = "*&{}[]";

static bool is_token(const char** array, const char* word, int n) {
  for (const char** kw = array; *kw != NULL; ++kw) {
    if ((int)strlen(*kw) != n) {
      continue;
    }
    if (strncmp(*kw, word, n) == 0) {
      return true;
    }
  }
  return false;
}

static bool is_digit(char c) {
  return c >= '0' && c <= '9';
}

static bool is_word_start(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_word_char(char c) {
  return is_word_start(c) || is_digit(c);
}

// Arbitrary-width integers: i0..i65535, u0..u65535
static bool is_int_type(const char* word, int n) {
  if (n < 2 || (word[0] != 'i' && word[0] != 'u')) {
    return false;
  }
  for (int i = 1; i < n; ++i) {
    if (!is_digit(word[i])) {
      return false;
    }
  }
  return true;
}

static void mark_range(HighlightContext* ctx, int start, int end,
                       EHighlightToken token) {
  for (int i = start; i < end; ++i) {
    ctx->tokens[i] = (char)token;
  }
}

static int scan_word(const char* line, int len, int i) {
  while (i < len && is_word_char(line[i])) {
    ++i;
  }
  return i;
}

// Number literal starting at a digit: 42, 1_000, 0xff, 0b1010, 0o17, 1.5e-3,
// 0x1p+4. A '.' only continues the literal when a digit follows, so the
// range in 0..5 stays three tokens.
static int scan_number(const char* line, int len, int i) {
  bool hex = line[i] == '0' && i + 1 < len &&
             (line[i + 1] == 'x' || line[i + 1] == 'X');
  while (i < len) {
    char c = line[i];
    if (is_word_char(c)) {
      ++i;
    } else if (c == '.' && i + 1 < len && is_digit(line[i + 1])) {
      ++i;
    } else if ((c == '+' || c == '-') &&
               (line[i - 1] == 'p' || line[i - 1] == 'P' ||
                (!hex && (line[i - 1] == 'e' || line[i - 1] == 'E')))) {
      ++i;
    } else {
      break;
    }
  }
  return i;
}

// String or character literal starting at the quote at i. Zig literals never
// span lines, so an unterminated one ends at the end of the line.
static int highlight_quoted(HighlightContext* ctx, int i) {
  char quote = ctx->line[i];
  ctx->tokens[i++] = (char)EHighlightToken_String;
  while (i < ctx->len) {
    if (ctx->line[i] == '\\') {
      ctx->tokens[i++] = (char)EHighlightToken_Digit;
      if (i < ctx->len) {
        ctx->tokens[i++] = (char)EHighlightToken_Digit;
      }
      continue;
    }
    ctx->tokens[i] = (char)EHighlightToken_String;
    if (ctx->line[i++] == quote) {
      return i;
    }
  }
  return ctx->len;
}

static void highlight_word(HighlightContext* ctx, int start, int end) {
  const char* word = &ctx->line[start];
  int n = end - start;
  EHighlightToken token = EHighlightToken_Normal;
  if (is_token(keywords_1, word, n)) {
    token = EHighlightToken_Keyword;
  } else if (is_token(keywords_2, word, n)) {
    token = EHighlightToken_Keyword2;
  } else if (is_token(types, word, n) || is_int_type(word, n)) {
    token = EHighlightToken_Type;
  }
  mark_range(ctx, start, end, token);
}

static void zig_highlight(HighlightContext* ctx) {
  if (!ctx || !ctx->line || !ctx->tokens) {
    return;
  }

  const char* line = ctx->line;
  int len = ctx->len;
  int i = 0;

  while (i < len) {
    char c = line[i];
    if (c == '/' && i + 1 < len && line[i + 1] == '/') {
      // Line comment, including /// and //! doc comments
      mark_range(ctx, i, len, EHighlightToken_Comment);
      break;
    } else if (c == '\\' && i + 1 < len && line[i + 1] == '\\') {
      // Multiline string literal: one \\ line at a time
      mark_range(ctx, i, len, EHighlightToken_String);
      break;
    } else if (c == '"' || c == '\'') {
      i = highlight_quoted(ctx, i);
    } else if (c == '@') {
      ctx->tokens[i++] = (char)EHighlightToken_Preprocessor;
      if (i < len && line[i] == '"') {
        // @"quoted identifier"
        i = highlight_quoted(ctx, i);
      } else {
        // @builtinFunction
        int end = scan_word(line, len, i);
        mark_range(ctx, i, end, EHighlightToken_Preprocessor);
        i = end;
      }
    } else if (is_digit(c)) {
      int end = scan_number(line, len, i);
      mark_range(ctx, i, end, EHighlightToken_Digit);
      i = end;
    } else if (is_word_start(c)) {
      int end = scan_word(line, len, i);
      highlight_word(ctx, i, end);
      i = end;
    } else {
      if (c != '\0' && strchr(symbols, c) != NULL) {
        ctx->tokens[i] = (char)EHighlightToken_Symbol;
      } else if (c != '\0' && strchr(symbols2, c) != NULL) {
        ctx->tokens[i] = (char)EHighlightToken_Symbol2;
      } else {
        ctx->tokens[i] = (char)EHighlightToken_Normal;
      }
      ++i;
    }
  }

  // Nothing in Zig carries over to the next line
  ctx->in_multiline_comment = false;
  ctx->in_string = false;
  ctx->string_char = 0;
}

static const Highlighter zig_highlighter = {
  .name = "zig",
  .highlight_line = zig_highlight,
};

// Builtin functions, for completion (lib/std/zig/BuiltinFn.zig of the Zig
// the device runs). The highlighter colours any @name, so it needs no list.
static const char* builtins[] = {
  "@abs", "@addrSpaceCast", "@addWithOverflow", "@alignCast", "@alignOf",
  "@as", "@atomicLoad", "@atomicRmw", "@atomicStore", "@backingInt",
  "@bitCast", "@bitOffsetOf", "@bitReverse", "@bitSizeOf", "@branchHint",
  "@breakpoint", "@byteSwap", "@call", "@ceil", "@clz", "@cmpxchgStrong",
  "@cmpxchgWeak", "@compileError", "@compileLog", "@constCast", "@cos", "@ctz",
  "@cVaArg", "@cVaCopy", "@cVaEnd", "@cVaStart", "@disableInstrumentation",
  "@disableIntrinsics", "@divCeil", "@divExact", "@divFloor", "@divTrunc",
  "@embedFile", "@Enum", "@enumFromInt", "@EnumLiteral", "@errorCast",
  "@errorFromInt", "@errorName", "@errorReturnTrace", "@exp", "@exp2",
  "@export", "@extern", "@field", "@fieldParentPtr", "@FieldType",
  "@floatCast", "@floatFromInt", "@floor", "@Fn", "@frame", "@Frame",
  "@frameAddress", "@fromBackingInt", "@hasDecl", "@hasField", "@import",
  "@inComptime", "@Int", "@intCast", "@intFromBool", "@intFromEnum",
  "@intFromError", "@intFromFloat", "@intFromPtr", "@log", "@log10", "@log2",
  "@max", "@memcpy", "@memmove", "@memset", "@min", "@mod", "@mulAdd",
  "@mulWithOverflow", "@offsetOf", "@panic", "@Pointer", "@popCount",
  "@prefetch", "@ptrCast", "@ptrFromInt", "@reduce", "@rem", "@returnAddress",
  "@round", "@select", "@setEvalBranchQuota", "@setFloatMode",
  "@setRuntimeSafety", "@shlExact", "@shlWithOverflow", "@shrExact",
  "@shuffle", "@sin", "@sizeOf", "@SpirvType", "@splat", "@sqrt", "@src",
  "@Struct", "@subWithOverflow", "@tagName", "@tan", "@This", "@trap",
  "@trunc", "@truncate", "@Tuple", "@typeInfo", "@typeName", "@TypeOf",
  "@Union", "@unionInit", "@Vector", "@volatileCast", "@wasmMemoryGrow",
  "@wasmMemorySize", "@workGroupId", "@workGroupSize", "@workItemId", NULL};

static const char** completion_words[] = {keywords_1, types, keywords_2, builtins, NULL};

const Filetype g_filetype_zig = {
  .name = "zig",
  .extensions = zig_extensions,
  .shebangs = NULL,
  .highlighter = &zig_highlighter,
  .completion_words = completion_words,
  .word_prefix_chars = "@",
};
