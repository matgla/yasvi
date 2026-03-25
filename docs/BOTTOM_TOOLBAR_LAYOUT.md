# Bottom Toolbar Layout Reference

## Visual Layout Diagrams

### Single-Line Layout (Current Style)

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                                                                              │
│                         EDITOR CONTENT AREA                                  │
│                                                                              │
├──────────────────────────────────────────────────────────────────────────────┤  <- height - 2
│  [MODE] filename.c [+]                Line:Col [ENC] [BRANCH] [DBG: keycode] │  <- Left | Right
├──────────────────────────────────────────────────────────────────────────────┤  <- height - 1
│  :w test.txt                                    [keyseq]          [ERR: msg] │  <- Command + Debug
└──────────────────────────────────────────────────────────────────────────────┘
   ↑                                              ↑
   Command prompt area (interactive)              Debug/Message area
```

### Two-Line Layout (Vim-Style with Airline)

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                                                                              │
│                         EDITOR CONTENT AREA                                  │
│                                                                              │
├──────────────────────────────────────────────────────────────────────────────┤  <- height - 3
│  NORMAL  ┃  filename.c [+] [RO]  ┃        ┃  utf-8 ┃  c  ┃  15:23 ┃  45%    │  <- Left | Center | Right
├──────────────────────────────────────────────────────────────────────────────┤  <- height - 2
│  [ DBG: 'x' 120 0x78 ]  [keyseq]                         [git:main*] [cpu:2%] │  <- Debug/Status
├──────────────────────────────────────────────────────────────────────────────┤  <- height - 1
│  :w                                                                      1,1 │  <- Command
└──────────────────────────────────────────────────────────────────────────────┘
```

## Widget Position Reference

### Left-Aligned Widgets (Order: priority ascending)

```
┌─────────────────────────────────────────────────────────────┐
│ [1] [2]    [3]                                              │
│  ↓   ↓      ↓                                               │
│ NORMAL file.c [+]                                           │
│  ↑     ↑    ↑                                               │
│ mode filename modified                                      │
└─────────────────────────────────────────────────────────────┘
```

| Priority | Widget | Content | Min Width |
|----------|--------|---------|-----------|
| 1 | mode | `NORMAL`, `INSERT`, `COMMAND` | 10 |
| 2 | filename | `filename.c` + `[+]` if modified | 20 |
| 3 | readonly | `[RO]` | 4 |
| 4 | filetype | `c`, `python`, `md` | 10 |

### Right-Aligned Widgets (Order: priority ascending)

```
┌─────────────────────────────────────────────────────────────┐
│                                               [3] [2]  [1]  │
│                                                ↓   ↓    ↓   │
│                                              utf-8 5:12 45% │
│                                               ↑    ↑    ↑   │
│                                            encoding pos scroll│
└─────────────────────────────────────────────────────────────┘
```

| Priority | Widget | Content | Min Width |
|----------|--------|---------|-----------|
| 1 | scrollbar | `45%` or visual bar | 4 |
| 2 | position | `15:23` (line:col) | 8 |
| 3 | encoding | `utf-8`, `ascii` | 8 |
| 4 | debug_keystroke | `[k:127]` | 12 |
| 5 | git_branch | `[main*]` | 10 |

### Center Widgets (Single, centered)

```
┌─────────────────────────────────────────────────────────────┐
│                    [centered widget]                        │
│                         ↑                                   │
│                   Search: pattern                           │
└─────────────────────────────────────────────────────────────┘
```

## Debug Keystroke Widget Layout Variants

### Variant 1: Compact (Default for debug builds)

```
┌─────────────────────────────────────────────────────────────┐
│                                               [k:127|DEL]    │
└─────────────────────────────────────────────────────────────┘
```
**Width**: ~12 chars
**Use case**: Always visible, minimal space

### Variant 2: Full Info

```
┌─────────────────────────────────────────────────────────────┐
│                                    ['\x7f' 127 0x7f DEL]    │
└─────────────────────────────────────────────────────────────┘
```
**Width**: ~25 chars
**Use case**: Detailed debugging

### Variant 3: With History (Last 3 keys)

```
┌─────────────────────────────────────────────────────────────┐
│                                          [ESC][k:107][DEL]   │
└─────────────────────────────────────────────────────────────┘
```
**Width**: ~30 chars for 3 keys
**Use case**: Sequence debugging (e.g., multi-key commands)

### Variant 4: Special Key Names

```
┌─────────────────────────────────────────────────────────────┐
│                                                              │
│  Key         ASCII    Dec    Hex    Oct    Name             │
│  ─────────────────────────────────────────────────────      │
│  Escape      \x1b     27     0x1b   033    ESC              │
│  Enter       \n       10     0x0a   012    LF                │
│  Tab         \t       9      0x09   011    TAB              │
│  Backspace   \b       8      0x08   010    BS               │
│  Delete      \x7f     127    0x7f   177    DEL              │
│  Space       ' '      32     0x20   040    SPC              │
│  Arrow Up    ?        259    0x103  0403   UP               │
│  Ctrl+A      \x01     1      0x01   001    CTL-A            │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```
**Use case**: Reference during UART debugging

## UART Debugging Scenarios

### Scenario 1: Wrong Backspace Code

**Problem**: Backspace sends `0x08` (BS) instead of `0x7F` (DEL)

```
┌─────────────────────────────────────────────────────────────┐
│  User presses Backspace...                                  │
│                                                             │
│  Expected: [k:127|DEL]   or   ['\x7f' 127 0x7f DEL]        │
│  Actual:   [k:8|BS]      or   ['\b' 8 0x08 010 BS]         │
│                                                             │
│  → UART driver sends wrong code                             │
└─────────────────────────────────────────────────────────────┘
```

### Scenario 2: Escape Sequence Issues

**Problem**: Arrow keys send incomplete sequences

```
┌─────────────────────────────────────────────────────────────┐
│  User presses Arrow Up...                                   │
│                                                             │
│  Expected sequence: ESC [ A  (3 bytes: 27 91 65)            │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  [ESC]  →  ['\x1b' 27 0x1b ESC]                     │   │
│  │  [ [ ]  →  ['[' 91 0x5b '[']                        │   │
│  │  [ A ]  →  ['A' 65 0x41 'A']  UP                    │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  Actual (broken UART):                                      │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  [ESC]  →  ['\x1b' 27 0x1b ESC]                     │   │
│  │  [ ? ]  →  ['?' 63 0x3f]   ← Missing '[' prefix!    │   │
│  │  [ ? ]  →  ['?' 65 0x41]                            │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  → UART drops bytes or timing issues                        │
└─────────────────────────────────────────────────────────────┘
```

### Scenario 3: Character Encoding Issues

**Problem**: Non-ASCII characters garbled

```
┌─────────────────────────────────────────────────────────────┐
│  User types 'é' (UTF-8: 0xC3 0xA9)...                       │
│                                                             │
│  Expected:                                                  │
│  [é] → ['é' 50089 0xC3A9]  (UTF-8 decoded)                 │
│                                                             │
│  Actual (ASCII-only UART):                                  │
│  [?] → ['?' 195 0xC3]        ← First byte only              │
│  [?] → ['?' 169 0xA9]        ← Second byte (wrong char)     │
│                                                             │
│  → UART configured for 7-bit or wrong encoding              │
└─────────────────────────────────────────────────────────────┘
```

## Configuration Examples

### Minimal Debug Setup

```c
// For basic UART debugging
DebugKeystrokeConfig minimal = {
  .show_ascii_char = false,
  .show_decimal_code = true,
  .show_hex_code = false,
  .show_octal_code = false,
  .compact_mode = true,
  .history_size = 0
};
// Output: [k:27]
```

### Verbose Debug Setup

```c
// For comprehensive debugging
DebugKeystrokeConfig verbose = {
  .show_ascii_char = true,
  .show_decimal_code = true,
  .show_hex_code = true,
  .show_octal_code = true,
  .compact_mode = false,
  .history_size = 3
};
// Output: ['\x1b' 27 0x1b 033 ESC] [ [ ] [ A ]
```

### Target-Specific Setup (Embedded)

```c
// For embedded targets with constrained UART
DebugKeystrokeConfig embedded = {
  .show_ascii_char = true,
  .show_decimal_code = true,
  .show_hex_code = true,      // Most useful for embedded
  .show_octal_code = false,
  .compact_mode = true,
  .history_size = 1           // Keep it small
};
// Output: [0x1b:27]
```
