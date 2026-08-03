#include <iostream>

#define STRUSE_IMPLEMENTATION
#include "struse.h"

namespace {

bool check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        return false;
    }
    return true;
}

bool test_basic_parsing() {
    bool ok = true;

    strref text("Hello, World!");
    ok &= check(text.find("World") == 7, "find() should locate a substring");
    ok &= check(text.find_case("world") < 0, "find_case() should be case-sensitive");
    ok &= check(text.has_prefix("Hello"), "has_prefix() should match the start");
    ok &= check(text.has_suffix("!"), "has_suffix() should match the end");
    ok &= check(text.count_char('l') == 3, "count_char() should count occurrences");
    ok &= check(text.get_word().same_str("Hello"), "get_word() should return the first token");
    ok &= check(text.get_trimmed_ws().same_str(text), "get_trimmed_ws() should preserve content");

    strref header("typedef struct box { int width; char *name; } box_t;");
    ok &= check(header.before('{').same_str("typedef struct box "), "before() should slice up to a delimiter");
    ok &= check(header.after('{').has_prefix(" int width;"), "after() should return the remainder");
    ok &= check(header.prefix_len(strref("typedef")) == 7, "prefix_len() should report a matching prefix");
    ok &= check(header.find_last(';') >= 0, "find_last() should locate the final semicolon");

    strref json("{\"name\":\"box\",\"enabled\":true}");
    ok &= check(json.find("\"name\"") >= 0, "find() should locate a JSON field name");
    ok &= check(json.find("true") >= 0, "find() should locate boolean literals");
    ok &= check(json.find_last('}') >= 0, "find_last() should locate the closing brace");

    strref tokens("alpha beta\t gamma");
    ok &= check(tokens.get_word_ws().same_str("alpha"), "get_word_ws() should return the first whitespace-delimited word");
    ok &= check(tokens.get_skip_ws().same_str("alpha beta\t gamma"), "get_skip_ws() should skip leading whitespace");

    strref token_string("one,two,three");
    strref token_value = token_string.split_token(',');
    ok &= check(token_value.same_str("one"), "split_token() should split a CSV-like token");

    strref token_lookup("alpha|beta|gamma");
    strref found_token = token_lookup.find_token("beta", '|');
    ok &= check(found_token.same_str("beta"), "find_token() should locate a token by content");

    return ok;
}

bool test_wildcards() {
    bool ok = true;

    strref source("project/src/main.cpp");
    strref wildcard_match = source.find_wildcard(strref("*.cpp"));
    ok &= check(wildcard_match.valid(), "find_wildcard() should find a wildcard match");
    ok &= check(wildcard_match.find(".cpp") >= 0, "find_wildcard() should return the matched text");
    ok &= check(source.find_any_char_of(strref("/\\")) == 7, "find_any_char_of() should find a character from a range");

    strref wildcard_numeric("file-01.txt");
    strref wildcard_numeric_match = wildcard_numeric.find_wildcard(strref("file-##.txt"));
    ok &= check(wildcard_numeric_match.valid(), "find_wildcard() should match numeric placeholders");

    strref wildcard_single("abc");
    strref wildcard_single_match = wildcard_single.find_wildcard(strref("a?c"));
    ok &= check(wildcard_single_match.valid(), "find_wildcard() should match single-character wildcards");

    strref wildcard_class("abc1def");
    strref wildcard_class_match = wildcard_class.find_wildcard(strref("abc[0-9]def"));
    ok &= check(wildcard_class_match.valid(), "find_wildcard() should match character classes");

    strref wildcard_word("alpha beta");
    strref wildcard_word_match = wildcard_word.find_wildcard(strref("<alpha>"));
    ok &= check(wildcard_word_match.valid(), "find_wildcard() should match word-boundary tokens");

    strref wildcard_line("first\nsecond");
    strref wildcard_line_match = wildcard_line.find_wildcard(strref("@second"));
    ok &= check(wildcard_line_match.valid(), "find_wildcard() should match line-start anchors");

    strref wildcard_end("first\nsecond");
    strref wildcard_end_match = wildcard_end.find_wildcard(strref("first^"));
    ok &= check(wildcard_end_match.valid(), "find_wildcard() should match line-end anchors");

    strref wildcard_word_end("alpha beta");
    strref wildcard_word_end_match = wildcard_word_end.find_wildcard(strref("alpha>"));
    ok &= check(wildcard_word_end_match.valid(), "find_wildcard() should match word-end anchors");

    strref wildcard_exclusive("abcadef");
    strref wildcard_exclusive_match = wildcard_exclusive.find_wildcard(strref("abc[!0-9]def"));
    ok &= check(wildcard_exclusive_match.valid(), "find_wildcard() should support excluded character ranges");

    strref wildcard_filter("alpha123beta");
    strref wildcard_filter_match = wildcard_filter.find_wildcard(strref("alpha*%beta"));
    ok &= check(wildcard_filter_match.valid(), "find_wildcard() should honor character-filtered wildcard segments");

    return ok;
}

bool test_whitespace_and_lines() {
    bool ok = true;

    strref spaced("  value  \n");
    ok &= check(spaced.len_whitespace() == 2, "len_whitespace() should count leading spaces");
    ok &= check(spaced.get_trimmed_ws().same_str("value"), "trimmed whitespace should remove outer whitespace");
    ok &= check(spaced.len_eol() == 9, "len_eol() should count to the end of the line");

    strref multiline("first\nsecond\nthird");
    ok &= check(multiline.count_lines() == 2, "count_lines() should count newline separators");
    ok &= check(multiline.get_line(1).same_str("second"), "get_line() should return a specific line");
    ok &= check(multiline.start_line_pos(6) == 0, "start_line_pos() should return the start of the line containing the requested offset");
    ok &= check(multiline.end_line_pos(6) == 12, "end_line_pos() should return the end of the line containing the requested offset");

    strref escaped("name\\=value");
    ok &= check(escaped.find_esc(strref("\\="), 0) == 5, "find_esc() should handle escape codes in the search string");

    return ok;
}

bool test_mutation_and_formatting() {
    bool ok = true;

    strown<256> xml;
    xml.append("<?xml version=\"1.0\"?>\n");
    xml.append("<root>\n");
    xml.append("  <item id=\"1\">boxbrew</item>\n");
    xml.append("</root>\n");
    ok &= check(xml.find("<root>") >= 0, "strown append() should build XML content");
    ok &= check(xml.find("boxbrew") >= 0, "strown should retain appended content");
    xml.prepend("<!-- generated -->\n");
    ok &= check(xml.find("<!-- generated -->") >= 0, "prepend() should insert content at the start");
    xml.replace("boxbrew", "boxbrew-test");
    ok &= check(xml.find("boxbrew-test") >= 0, "replace() should update inserted text");

    strown<128> json_builder;
    json_builder.sprintf("{\"name\":\"%s\",\"count\":%d}", "box", 42);
    ok &= check(json_builder.find("\"count\":42") >= 0, "sprintf() should format JSON-like output");
    json_builder.append('\n');
    json_builder.append("\"done\"\n");
    ok &= check(json_builder.find("done") >= 0, "append() should extend formatted output");
    json_builder.remove('"');
    ok &= check(json_builder.find('"') < 0, "remove() should strip selected characters");

    return ok;
}

bool test_path_and_format_helpers() {
    bool ok = true;

    strown<128> path;
    path.append("src/../lib\\file.cpp");
    path.cleanup_path();
    ok &= check(path.same_str("lib\\file.cpp"), "cleanup_path() should normalize separators and dot-dot segments");

    strown<128> rel;
    rel.relative_path(strref("C:/proj/src"), strref("C:/proj/include/foo.h"));
    ok &= check(rel.same_str("include/foo.h"), "relative_path() should compute a relative path");

    strown<128> fmt;
    strref args[] = { strref("box"), strref("42") };
    fmt.format("Hello {0} {1}", args);
    ok &= check(fmt.same_str("Hello box 42"), "format() should replace numbered placeholders");

    strown<128> append_fmt;
    append_fmt.append("world");
    append_fmt.format_prepend("Hello {0} ", args);
    ok &= check(append_fmt.same_str("Hello box world"), "format_prepend() should insert content at the front");

    strown<64> num;
    num.append_num(42, 2, 10);
    ok &= check(num.same_str("42"), "append_num() should append a formatted number");

    return ok;
}

bool test_collections_and_numbers() {
    bool ok = true;

    strcol<256> rows;
    ok &= check(rows.push_back(strref("name")), "strcol should accept a new string");
    ok &= check(rows.push_back(strref("enabled")), "strcol should accept another string");
    ok &= check(rows[0].same_str("name"), "strcol indexing should return stored strings");
    ok &= check(rows[1].same_str("enabled"), "strcol should store multiple values");

    strref number("2a");
    ok &= check(number.atoi() == 2, "atoi() should parse integers");
    ok &= check(number.ahextoui() == 42, "ahextoui() should parse hexadecimal input safely");

    strref quoted("\"json value\",next");
    ok &= check(quoted.find_quoted(',') >= 0, "find_quoted() should ignore quoted content when scanning for delimiters");

    strref xml_text("<tag attr=\"hello > world\">value</tag>");
    ok &= check(xml_text.find_quoted_xml('>') >= 0, "find_quoted_xml() should skip delimiters inside quoted XML content");

    strref range_text("abc123");
    ok &= check(range_text.find_any_not_in_range(strref("0-9")) == 0, "find_any_not_in_range() should find the first character outside the allowed range");

    strref repeated("one one one");
    ok &= check(repeated.substr_count(strref("one")) == 3, "substr_count() should count repeated substrings");

    return ok;
}

bool test_delimiter_and_bookend_helpers() {
    bool ok = true;

    strref line_pos("a\nb\nc");
    ok &= check(line_pos.start_line_pos(2) == 0, "start_line_pos() should return the start of the line containing the given offset");
    ok &= check(line_pos.end_line_pos(2) == 3, "end_line_pos() should return the line end for the given offset");

    strref comment("/* comment */ text");
    ok &= check(comment.scoped_block_comment_len() == 0, "scoped_block_comment_len() should report no block when the input is not a scope opener");

    strref block("{ outer { inner } }");
    ok &= check(block.scoped_block_skip(false).same_str(" outer { inner } "), "scoped_block_skip() should return the content inside balanced braces");

    strref pipe_text("alpha|beta|gamma");
    strref pipe_before = pipe_text.before_or_full('|');
    ok &= check(pipe_before.same_str("alpha"), "before_or_full() should split on a delimiter");
    strref pipe_after = pipe_text.after_or_full('|');
    ok &= check(pipe_after.same_str("beta|gamma"), "after_or_full() should return the remainder after a delimiter");

    strref within_text("(alpha) beta");
    strref within = within_text.within_last('(', ')');
    ok &= check(within.same_str("alpha"), "within_last() should return content between matching delimiters");

    strref token_split("one,two,three");
    strref token_chunk = token_split.split_token(',');
    ok &= check(token_chunk.same_str("one"), "split_token() should return the first chunk");

    strref before_after("a:b:c");
    ok &= check(before_after.before(':').same_str("a"), "before() should return the prefix before a delimiter");
    ok &= check(before_after.after(':').same_str("b:c"), "after() should return the suffix after a delimiter");
    ok &= check(before_after.after_last(':').same_str("c"), "after_last() should return the suffix after the last delimiter");
    ok &= check(before_after.before_or_full_case(strref(":b")).same_str("a"), "before_or_full_case() should return the prefix before a case-sensitive match");
    ok &= check(before_after.before_last(':').same_str("a:b"), "before_last() should return the prefix before the last delimiter");
    ok &= check(before_after.after_last_or_full(':').same_str("c"), "after_last_or_full() should return the suffix after the last delimiter");

    strref split_lookup("a,b,c");
    ok &= check(split_lookup.find_or_full(',', 0) == 1, "find_or_full() should return the delimiter position when present");
    ok &= check(split_lookup.find_or_full('x', 0) == split_lookup.get_len(), "find_or_full() should return the full length when a delimiter is absent");
    ok &= check(split_lookup.find_after_last('a', ',') == 3, "find_after_last() should find the next delimiter after the last matching separator");

    strref split_lookup_alt("xaybz");
    ok &= check(split_lookup_alt.find_after_last('x', 'y', 'z') == 4, "find_after_last() should support the two-delimiter overload");

    strref bookend_text("abcabc");
    ok &= check(bookend_text.find_last_bookend(strref("bc"), strref("a-z")) == 1, "find_last_bookend() should locate the bounded substring position found while scanning backward");
    ok &= check(bookend_text.substr_count_bookend(strref("bc"), strref("a-z")) == 2, "substr_count_bookend() should count bounded substring occurrences");

    strref range_word_text("abc123");
    ok &= check(range_word_text.get_range_word(strref("a-z")).same_str("abc"), "get_range_word() should return the matching range word");

    strref marker_text("abc123xyz");
    ok &= check(marker_text.match_chars_str(strref("0-9"), strref("a-z")) == 0, "match_chars_str() should stop immediately when a terminator is encountered");

    return ok;
}

bool test_case_and_utf8_helpers() {
    bool ok = true;

    strref reverse_text("abcabc");
    ok &= check(reverse_text.find_last('b') == 4, "find_last() should search backward for a character");
    ok &= check(reverse_text.find_last(strref("bc")) == 4, "find_last() should search backward for a substring");
    ok &= check(!reverse_text.same_str_case(strref("ABCABC")), "same_str_case() should be case-sensitive");
    ok &= check(reverse_text.count_repeat('a', 0) == 1, "count_repeat() should count only consecutive matches from a position");
    ok &= check(reverse_text.count_repeat_reverse('c', 4) == 0, "count_repeat_reverse() should count only consecutive matches backward from a position");

    const unsigned char utf8_bytes[] = { 0xC3, 0xA9, 0 };
    strref utf8_text((const char*)utf8_bytes);
    ok &= check(utf8_text.get_utf8() == 0x00E9, "get_utf8() should decode a two-byte UTF-8 sequence");

    const unsigned char utf8_bytes_euro[] = { 0xE2, 0x82, 0xAC, 0 };
    strref utf8_euro((const char*)utf8_bytes_euro);
    ok &= check(utf8_euro.get_utf8() == 0x20AC, "get_utf8() should decode a three-byte UTF-8 sequence");

    const unsigned char utf8_pop_bytes[] = { 'A', 0xC3, 0xA9, 'B', 0 };
    strref utf8_pop((const char*)utf8_pop_bytes);
    strref utf8_pop_copy = utf8_pop;
    ok &= check(utf8_pop_copy.pop_utf8() == 'A', "pop_utf8() should return the first ASCII code point");
    ok &= check(utf8_pop_copy.pop_utf8() == 0x00E9, "pop_utf8() should advance past a multi-byte UTF-8 code point");
    ok &= check(utf8_pop_copy.pop_utf8() == 'B', "pop_utf8() should continue after the UTF-8 sequence");

    strown<64> utf8_builder;
    utf8_builder.push_utf8(0x00E9);
    utf8_builder.push_utf8(0x20AC);
    ok &= check(utf8_builder.get_utf8(0) == 0x00E9, "push_utf8() should store a two-byte UTF-8 code point");
    ok &= check(utf8_builder.get_utf8(2) == 0x20AC, "get_utf8() should read a three-byte UTF-8 code point from the builder");

    return ok;
}

bool test_rolling_hash_and_format_variants() {
    bool ok = true;

    strref text("alpha beta gamma");
    int rh = text.find_rh(strref("beta"));
    int rh_case = text.find_rh_case(strref("BETA"));
    int rh_after = text.find_rh_after(strref("beta"), strref("alpha"));
    ok &= check(rh == -6, "find_rh() should report a successful rolling-hash match using the helper's return convention");
    ok &= check(rh_case < 0, "find_rh_case() should be case-sensitive");
    ok &= check(rh_after == -6, "find_rh_after() should find a match after the previous match");

    strown<128> fmt_append;
    strref fmt_args[] = { strref("world"), strref("42") };
    fmt_append.append("Hello ");
    fmt_append.format_append("{0} {1}", fmt_args);
    ok &= check(fmt_append.same_str("Hello world 42"), "format_append() should append formatted content");

    strown<128> fmt_insert;
    fmt_insert.append("!" );
    fmt_insert.format_insert("{0}{1}", fmt_args, 0);
    ok &= check(fmt_insert.same_str("world42!"), "format_insert() should insert formatted content at a given position");

    strown<64> sprintf_out;
    sprintf_out.sprintf("value=%d", 7);
    ok &= check(sprintf_out.same_str("value=7"), "sprintf() should write formatted output");

    strown<64> sprintf_append_out;
    sprintf_append_out.append("prefix");
    sprintf_append_out.sprintf_append(":%d", 9);
    ok &= check(sprintf_append_out.same_str("prefix:9"), "sprintf_append() should append formatted output");

    return ok;
}

bool test_quoted_and_parsing_edge_cases() {
    bool ok = true;

    strref quoted("a,\"b,c\",d");
    int quoted_pos = quoted.find_quoted(',');
    ok &= check(quoted_pos == 1, "find_quoted() should skip over quoted content when looking for the next delimiter outside quotes");

    strref xml_quoted("<tag attr=\"x,y\">value</tag>");
    int xml_quoted_pos = xml_quoted.find_quoted_xml('>');
    ok &= check(xml_quoted_pos == 15, "find_quoted_xml() should ignore delimiters inside quoted XML content");

    strref split_text("a||b||c");
    ok &= check(split_text.before_or_full('|').same_str("a"), "before_or_full() should return the prefix before the first delimiter");
    ok &= check(split_text.after_or_full('|').same_str("|b||c"), "after_or_full() should return the remainder after the first delimiter");

    strref escaped_text("a\\=b");
    int esc_pos = escaped_text.find_esc(strref("\\="), 0);
    ok &= check(esc_pos == 2, "find_esc() should locate an escaped delimiter");

    strref escaped_quotes("a,\"b\\\\\",d");
    int escaped_quote_pos = escaped_quotes.find_quoted(',');

    ok &= check(escaped_quote_pos == 1, "find_quoted() should stop at the delimiter outside the quoted content");

    strref csv_text("alpha,\"value,with,commas\",beta");
    strref csv_first = csv_text.get_csv_cell();
    ok &= check(csv_first.same_str("alpha"), "get_csv_cell() should read the first unquoted field");
    strref csv_second = csv_text.get_csv_cell();
    ok &= check(csv_second.same_str("value,with,commas"), "get_csv_cell() should preserve commas inside a quoted field");

    return ok;
}

} // namespace

int main() {
    bool ok = true;
    ok &= test_basic_parsing();
    ok &= test_wildcards();
    ok &= test_whitespace_and_lines();
    ok &= test_mutation_and_formatting();
    ok &= test_path_and_format_helpers();
    ok &= test_collections_and_numbers();
    ok &= test_delimiter_and_bookend_helpers();
    ok &= test_case_and_utf8_helpers();
    ok &= test_rolling_hash_and_format_variants();
    ok &= test_quoted_and_parsing_edge_cases();

    if (!ok) {
        return 1;
    }

    std::cout << "struse tests passed" << std::endl;
    return 0;
}
