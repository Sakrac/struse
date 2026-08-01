# STRUSE DOCUMENTATION

USAGE

Add this #define to *one* C++ file before #include "struse.h" to create the implementation:

 #define STRUSE_IMPLEMENTATION

[Samples](../../tree/master/samples) contains basic, precompile hash, xml parsing and diff/patch samples using struse classes in a variety of ways.

[x65](https://github.com/sakrac/x65) Is a 6502 assembler built around struse.h.

struse is a collection of string parsing and string modification classes that are intended to work together to simplify parsing blocks of text, building blocks of text.

Please note that struse is not by itself utf-8 aware so functions like ```toupper``` will only consider one byte at a time. ```get_utf8``` and ```pop_utf8``` are helper functions to parse strings that contain utf-8. For most cases treating utf-8 strings as C strings works fine, and for performing actual uppercase/lowercase on Unicode strings please use operating system calls that are aware of the user locale.

StrUse is a little bit different from built-in string types and classes in that it has multiple classes to interface with string data.

* '**strref**' is a non-owning string view that refers to text without copying it. It is designed for parsing, searching, and iterating over text blocks.
* '**strown**' is a fixed-capacity, owned string type backed by an inline buffer.
* '**strovl**' is similar to 'strown' but uses caller-provided storage for the backing buffer instead of an inline array.
* '**strcol**' is a compact collection of strings stored in a single fixed buffer.

None of these classes perform allocations so string data and string memory is provided by the caller.

Think of strref as a const char pointer with a length. This allows for searches and iteration on constant text data and is primarily used as a parsing tool.

UTF-8 support in this header is helper-oriented rather than fully transparent. Functions such as `get_utf8()`, `push_utf8()`, `pop_utf8()`, `tolower_utf8()`, and `toupper_utf8()` operate on UTF-8 code points, while the rest of the API remains byte-oriented and ASCII-friendly for the common parsing helpers.

### Quick example

```cpp
#define STRUSE_IMPLEMENTATION
#include "struse.h"

int main() {
    strref text("alpha,beta,gamma");
    while (strref token = text.next_token(',')) {
        // token is a non-owning view over the original buffer
    }
}
```

This shows the core pattern of parsing a buffer with `strref` and consuming tokens without allocating.

**strown** is similar to a string builder in C# or Java. You create it with a fixed maximum capacity, and it supports most of the same parsing and mutation helpers that `strref` exposes.

**strovl** has the same feature set as `strown` but uses caller-provided memory instead of an inline buffer. This makes it suitable for overlaying a string over an existing buffer, such as memory loaded from a file.

**strcol** is a very basic list of string copies stored in a single fixed buffer. It is intended for lightweight string collection scenarios without dynamic allocation.

To support printf formatting with non-zero terminated strings there are two macros that work together:

```
printf("\"" STRREF_FMT "\"\n", STRREF_ARG(strref));
```

### Reliability:

An effort has been made to check that all write operations fit within given space and that all reading operations access only the part of memory that has been assigned to a string. That is not to say that this code is without bugs, use at your own risk but let me know of any issues. This code is provided to assist with often tedious string management issues and a certain knowledge of how string manipulation works may be required to understand these classes.

If anything is unclear I strongly suggest looking at the header file or the code. As far as reasonable the code is written to be easy to read.

### Next steps

* Clean up test code and add to the depot
* Clean up project files and include in the depot
* Clean up sample code (xml, json, cfg, csv, etc. parsing) and include
* More documentation

### Future considerations

* wchar_t support

---

## strref overview

The intent of strref is to iterate over text and easily reference substrings without allocations to hold what is basically copies. To achieve this strref holds both a pointer to text and the length of the text.

The most common application of strref is to load in a text file into memory and create a strref from the pointer and file size.

With a strref it is easy to iterate through each line of the text by:

```
while (strref current_line = text.line()) {
    do something with current_line
}
```

If the format is fairly straightforward, current_line may be as simple as variable = value. Separating the two sides of the equal sign can be done by splitting it up and trimming remaining whitespace by calling

```
strref value = current_line.split_token_trim('=');
```

Assuming that the original line was "    numbers = 73.4, 12.2, 13, 19.2"
current_line would now be "numbers" and value would be "73.4, 12.2, 13, 19.2"

In order to iterate over the individual numbers:

```
while (strref number = value.next_token(',')) {
    int value = number.atoi();
    ...
}
```

So putting this together:

```
while (strref current_line = text.line()) {
  strref value = current_line.split_token_trim('=');
  if (current_line.same_str_case("numbers")) {
    while (strref number = value.next_token(',')) {
      int value = number.atoi();
      ...
    }
  }
  ...
}
```

This will go through all lines in a file, split by equal signs and handle tokenized lists of numbers.

strref has a variety of helper functions to avoid string duplication and code duplication including comparisons, searches and wildcard matching.



## strref specification

Function naming rules:

In order to simplify the features of class functions there are some naming conventions that are followed:

* find* returns a position of a string where a match if found or -1 if not
* skip*/clip* trims beginning or end of string based on condition
* before*/after* returns a substring matching the condition
* is_* returns a bool for a character or whole string test
* len_* return the number of characters matching the condition from the start of the string
* *_last indicates that search is done from the end of the string
* *_rh indicates a rolling hash search is used
* *_esc indicates that the search string allows for escape codes (\x => x)
* same_str* is a full string compare and returns true or false based on condtions
* *_range indicates that the result is filtered by a given set of valid characters

### Operators

* strref += (int) / strref++: Move start of string forward and reduce length
* strref > strref / strref < strref: Returns alphabetical sorting order greater than / less than
* strref[(int)]: Return character at position

For function descriptions, look at [Functions.md](Functions.md) or refer to struse.h.



## strref wildcard find

As an alternative to setting up a series of string scanning calls there is built-in wildcard matching support. Using wildcards is similar to most software searches that allow wildcards.

Wildcard control characters:

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

(words are groups of letters not containing whitespace or separators which are alphanumeric characters plus apostrophe ('))

```
result = text.find_wildcard(pattern, 0, true)
```

will find the pattern in the strref text and return a `strref` result.

```
result = text.next_wildcard(pattern, prev, true)
```

will find the next match after the previously found `prev` begins.

```
result = text.wildcard_after(pattern, prev, true)
```

will find the next match after the previously found `prev` ends.

### Example:

```
strref result;
while (result = text.wildcard_after("full*{! }.png", result)) {
    printf(STRREF_FMT "\n", STRREF_ARG(result));
}
```

will find all matches of the pattern in the text and print them.


## Token iteration support:

Reading in token separated values is a common function of text parsers. This
can be done in a number of ways depending on what is needed.

Given a string like: '23,12,5,91,54,8,23,17,67' each number can be fetched with
this loop:

```
while (strref num = line.next_token(',')) {
	int value = num.atoi();
}
```

For CSV-style input with embedded commas, a quoted field can be read with the parsing helpers that skip over quoted content:

```cpp
strref row("alpha,\"value,with,commas\",beta");
strref first = row.get_csv_cell();
strref second = row.get_csv_cell();
```

In this case, the second cell preserves the inner commas while the parser remains outside the quoted region.

## strown / strovl support:

strown and strovl share the same base (strmod) and share the same code. The difference is that strown includes the memory for the string and strovl requires user provided space.

The most straightforward way to put text into strown is to create the string with the text:

```
strown<256> test("test string");
```

You can also copy a string or `strref` into a `strown` with the `copy` function:

```cpp
test.copy(test_strref);
```

Other ways to add text to a `strown` or `strovl` include:

* **insert**(string, pos): move text forward at pos by string size and insert string at position.
* **append**(string): add string at end
* **append**(char): add character at end
* **prepend**(string): insert at start
* **format**(format_string, args): format a string c# string style with {n} where n is a number indicating which of the strref args to insert
* **sprintf**(format, ...): use sprintf formatting with zero terminated c style strings and other data types.

```cpp
strown<128> builder;
builder.append("src/../lib\\file.cpp");
builder.cleanup_path();

strref args[] = { strref("box"), strref("42") };
builder.format("Hello {0} {1}", args);
```

This shows the common pattern of normalizing a path-like string and then formatting it with numbered placeholders.

## API reference overview

The current header exposes a compact set of building blocks rather than a large object-oriented API. The most important entry points are:

### `strref`

- constructors and accessors for non-owning string views
- hashing and numeric conversion helpers (`fnv1a`, `atoi`, `atoui`, `atof`, `atod`)
- character classification and case conversion utilities
- prefix/suffix, equality, and wildcard matching helpers
- search helpers for single characters, substrings, ranges, and rolling-hash lookups
- whitespace, token, and line parsing helpers

### `strmod`

`strmod` is the mutable string base class used by `strown` and `strovl`.

It provides:

- buffer state management (`clear`, `valid`, `empty`, `full`, `set_len`, `add_len`)
- content mutation (`append`, `prepend`, `insert`, `replace`, `erase`, `remove`)
- formatting helpers (`sprintf`, `format`, `append_num`)
- path and text utilities (`cleanup_path`, `relative_path`)

### `strown` and `strovl`

- `strown<capacity>` stores its backing buffer inline and is suitable for stack-based or small fixed-size strings.
- `strovl` uses caller-provided storage and is useful for overlays over an existing buffer.
- both classes share the same `strmod` feature set and therefore support most of the same text-building operations.

### `strcol`

- `strcol<capacity>` stores a compact collection of strings in a single fixed buffer.
- it is intended for lightweight string-list handling without dynamic allocation.

### Notes

For full signatures and implementation details, consult [struse.h](struse.h). The README here focuses on the high-level behavior, while the header remains the authoritative source for exact method names and overloads.
