# Functions in struse.h

This document is a cleaned-up reference for the API surface exposed by [struse.h](struse.h). It groups related helpers by purpose so the file is easier to scan, maintain, and expand into fuller API documentation.

## Overview

The header centers around a lightweight string view type, `strref`, plus mutable string helpers such as `strmod`, `strown`, and `strovl`.

The API is organized around a few core themes:

- substring extraction and slicing
- prefix/suffix and whole-string comparisons
- searching, tokenizing, and line-oriented parsing
- whitespace handling and UTF-8 helpers
- mutable string building and formatting

## `strref`

`strref` is a non-owning view over a contiguous character buffer. It is the main parsing and inspection type in the library.

### Construction and state

- `strref()` — creates an empty reference.
- `strref(const char *str)` — wraps a null-terminated C string.
- `strref(char *str)` — wraps a mutable C string as a read-only view.
- `strref(const char *str, strl_t len)` — wraps a substring with an explicit length.
- `valid()` — reports whether the reference points to a non-empty valid range.
- `clear()` — resets the reference to empty.
- `set(const char *str)` — rebinds the reference to a new C string.
- `get()`, `get_u()`, `get_len()` — expose the raw pointer, unsigned-byte view, and length.
- `get_first()`, `get_last()`, `pop_first()` — inspect or consume the first character.
- `is_substr()`, `substr_offs()`, `substr_end_offs()` — perform pointer-based substring checks.

### Conversion and hashing

- `fnv1a()`, `fnv1a_lower()`, `fnv1a_append()`, `fnv1a_16()`, `fnv1a_64()` — compute hash values for the referenced string.
- `atoi()`, `atoui()`, `atof()`, `atod()` — parse integer and floating-point values.
- `atoi_skip()` — parses an integer and advances past the consumed characters.
- `ahextoi()`, `ahextoui()`, `ahextou64()` — parse hexadecimal values.
- `ahextoui_skip()`, `abinarytoui_skip()` — parse numeric input and move past the consumed characters.
- `writeln()` — writes the string to stdout with a trailing newline.

### Character classification

- `num_to_char()` — converts a small integer to its decimal or hexadecimal digit character.
- `is_ws()`, `is_number()`, `is_hex()`, `is_alphabetic()`, `is_alphanumeric()`, `is_valid_label()` — classify characters using the library’s conventions.
- `is_sep_ws()`, `is_control()` — identify separators and control-like characters.
- `tolower()`, `toupper()`, and their platform-specific variants — provide character case conversion helpers.

### Comparisons and matching

- `same_str()`, `same_str_case()` — compare the whole string.
- `same_substr()`, `same_substr_case()`, `same_substr_esc()` — compare a substring from a given offset.
- `prefix_len()`, `suffix_len()`, `has_prefix()`, `has_suffix()`, `is_prefix_of()`, `is_suffix_of()` — work with prefix/suffix logic.
- `is_word()`, `is_word_case()` — test whether the string is a whole word.
- `find_wildcard()`, `next_wildcard()`, `wildcard_after()` — support wildcard-based matching.
- `char_matches_ranges()` — test whether a character fits an explicit range pattern.

#### Wildcards reference

* **?**: any single character
* **#**: any single number
* **[]**: any single between the brackets
* **[-]**: any single in the range from character before - to character after
* **[!]**: any single not between the brackets
* **&lt;**: start of word
* **&gt**: end of word
* **@**: start of line
* **^**: end of line
* **\***: any substring
* **\*%**: any substring excluding whitespace
* **\*@**: any substring on same line
* **\*$**: any substring containing alphanumeric ascii characters
* **\*{}**: any substring only containing characters between parenthesis
* **\*{!}**: any substring not containing characters between parenthesis
* **\?**, **\[**, **\\\***, etc.: search for character after backslash
* **\n**, **\t**, etc.: search for linefeed, tab etc.

### Searching and scanning

- `find()`, `find_case()`, `find_last()`, `find_last_case()` — locate substrings or characters.
- `find_any_char_of()`, `find_any_char_or_range()`, `find_any_not_in_range()` — search for characters according to a set or range.
- `find_skip_parens()` — finds a token while skipping over balanced parentheses.
- `substr_count()`, `substr_case_count()`, `substr_label_case_count()` — count occurrences of a substring.
- `find_rh()`, `find_rh_case()`, `find_rh_after()`, `find_rh_case_after()` — use rolling-hash search helpers.

Example:

```cpp
strref text("alpha beta gamma");
int pos = text.find_rh(strref("beta"));
```

### Whitespace and line helpers

- `len_whitespace()`, `len_grayspace()`, `len_non_sep_ws()` — compute run lengths of whitespace or non-whitespace content.
- `skip_whitespace()`, `skip_to_whitespace()`, `trim_whitespace()`, `clip_trailing_whitespace()` — adjust the view around whitespace.
- `len_alphanumeric()`, `len_word()`, `len_label()` — measure identifier-like content.
- `len_eol()`, `len_next_line()`, `count_lines()` — inspect line boundaries and line counts.
- `start_line_pos()` and `end_line_pos()` — return the start and end offsets of the line that contains a given position.

### Substring and token helpers

- `get_substr()`, `get_skipped()`, `get_clipped()`, `get_word()`, `get_word_ws()` — extract parts of the view.
- `before()`, `after()`, `before_last()`, `after_last()`, `before_or_full()`, `after_or_full()` — split strings around delimiters.
- `split()`, `split_token()`, `split_range()`, `split_label()`, `split_lang()`, `split_num()` — break input into logical pieces.
- `get_line()`, `next_line()`, `line()` — operate on line-based content.
- `find_token()`, `token_chunk()`, `within_last()` — work with tokenized input.
- `get_csv_cell()`, `get_quote_xml()`, `find_quoted()`, `skip_chunk()` — support CSV/XML-like parsing helpers.

## `strmod`

`strmod` is a mutable string builder type that mirrors much of the `strref` API while allowing the backing buffer to grow and change.

### Core operations

- `clear()`, `valid()`, `empty()`, `full()` — manage string state.
- `copy()`, `append()`, `prepend()`, `insert()` — alter the contents safely.
- `set_len()`, `add_len()`, `fit_add()` — manage storage size and capacity.
- `replace()`, `replace_bookend()`, `exchange()`, `remove()`, `erase()` — perform in-place edits.

### Formatting and output

- `sprintf()`, `sprintf_at()`, `sprintf_append()` — format into the buffer using C-style format strings.
- `format()`, `format_append()`, `format_prepend()`, `format_insert()` — replace `{n}` placeholders with provided `strref` arguments.
- `append_num()` — appends a number formatted in a chosen radix.

Example:

```cpp
strown<64> out;
out.append("Hello ");
out.format_append("{0} {1}", args);
```

### Unicode and case conversion

- `tolower()`, `toupper()`, and the platform-specific variants — mutate the buffer’s case.
- `tolower_utf8()`, `toupper_utf8()` — apply UTF-8-aware case transforms.
- `write_utf8()`, `push_utf8()`, `get_utf8()` — read and write single UTF-8 code points.

Example:

```cpp
strown<32> utf8;
utf8.push_utf8(0x00E9);
```

### Path and text helpers

- `cleanup_path()` — normalizes a path-like string.
- `relative_path()` — computes a relative path between two paths.

## `strown`, `strovl`, and related helpers

These are concrete string classes built on top of the shared buffer-management machinery.

- `strown<capacity>` — fixed-capacity owned string.
- `strovl` — overlay string that uses an externally supplied buffer.
- `strref_rel` — stores a relative substring reference for string collections that may move.
- `strcol<capacity>` — compact collection of strings stored in a shared buffer.

## Notes for future documentation

The next step would be to add:

- one-paragraph descriptions for each major function
- example usage snippets
- notes about edge cases, especially around parsing, UTF-8, and path handling
