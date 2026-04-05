import toml;
import std;

int failed = 0;

void check(bool cond, std::string_view msg) {
    if (!cond) {
        std::println(std::cerr, "FAIL: {}", msg);
        ++failed;
    }
}

void test_string() {
    auto t = toml::parse(R"(name = "toml")");
    check(t.at("name").is_string(), "string type");
    check(t.at("name").as_string() == "toml", "string value");
}

void test_integer() {
    auto t = toml::parse("port = 8080");
    check(t.at("port").is_integer(), "integer type");
    check(t.at("port").as_integer() == 8080, "integer value");
}

void test_negative_integer() {
    auto t = toml::parse("offset = -10");
    check(t.at("offset").as_integer() == -10, "negative integer");
}

void test_float() {
    auto t = toml::parse("pi = 3.14");
    check(t.at("pi").is_float(), "float type");
    check(std::abs(t.at("pi").as_float() - 3.14) < 1e-9, "float value");
}

void test_float_exponent() {
    auto t = toml::parse("val = 1e10");
    check(t.at("val").is_float(), "float exponent type");
    check(t.at("val").as_float() == 1e10, "float exponent value");
}

void test_bool() {
    auto t = toml::parse("enabled = true\ndisabled = false");
    check(t.at("enabled").as_bool() == true, "bool true");
    check(t.at("disabled").as_bool() == false, "bool false");
}

void test_array() {
    auto t = toml::parse("nums = [1, 2, 3]");
    check(t.at("nums").is_array(), "array type");
    auto const& arr = t.at("nums").as_array();
    check(arr.size() == 3, "array size");
    check(arr[0].as_integer() == 1, "array[0]");
    check(arr[2].as_integer() == 3, "array[2]");
}

void test_empty_array() {
    auto t = toml::parse("arr = []");
    check(t.at("arr").is_array(), "empty array type");
    check(t.at("arr").as_array().empty(), "empty array size");
}

void test_nested_array() {
    auto t = toml::parse("arr = [[1, 2], [3]]");
    auto const& outer = t.at("arr").as_array();
    check(outer.size() == 2, "nested array outer size");
    check(outer[0].as_array()[1].as_integer() == 2, "nested array value");
}

void test_table() {
    auto t = toml::parse("[server]\nhost = \"localhost\"\nport = 80");
    check(t.at("server").is_table(), "table type");
    check(t.at("server").as_table().at("host").as_string() == "localhost", "table string");
    check(t.at("server").as_table().at("port").as_integer() == 80, "table integer");
}

void test_nested_table() {
    auto t = toml::parse("[a.b]\nc = 1");
    check(t.at("a").as_table().at("b").as_table().at("c").as_integer() == 1, "nested table");
}

void test_array_of_tables() {
    auto t = toml::parse("[[items]]\nname = \"a\"\n[[items]]\nname = \"b\"");
    auto const& arr = t.at("items").as_array();
    check(arr.size() == 2, "array of tables size");
    check(arr[0].as_table().at("name").as_string() == "a", "array of tables [0]");
    check(arr[1].as_table().at("name").as_string() == "b", "array of tables [1]");
}

void test_inline_table() {
    auto t = toml::parse(R"(point = { x = 1, y = 2 })");
    check(t.at("point").is_table(), "inline table type");
    auto const& p = t.at("point").as_table();
    check(p.at("x").as_integer() == 1, "inline table x");
    check(p.at("y").as_integer() == 2, "inline table y");
}

void test_empty_inline_table() {
    auto t = toml::parse("empty = {}");
    check(t.at("empty").is_table(), "empty inline table type");
    check(t.at("empty").as_table().empty(), "empty inline table size");
}

void test_inline_table_mixed_values() {
    auto t = toml::parse(R"(dep = { name = "fmt", version = "11.0.0", features = ["xchar", "ranges"] })");
    auto const& d = t.at("dep").as_table();
    check(d.at("name").as_string() == "fmt", "inline table string");
    check(d.at("version").as_string() == "11.0.0", "inline table version");
    check(d.at("features").is_array(), "inline table nested array");
    check(d.at("features").as_array().size() == 2, "inline table array size");
    check(d.at("features").as_array()[0].as_string() == "xchar", "inline table array[0]");
}

void test_inline_table_in_section() {
    auto t = toml::parse("[deps]\nfmt = { version = \"11.0.0\", features = [\"xchar\"] }");
    auto const& deps = t.at("deps").as_table();
    check(deps.at("fmt").is_table(), "inline table inside section");
    check(deps.at("fmt").as_table().at("version").as_string() == "11.0.0", "inline table in section value");
}

void test_inline_table_trailing_comma() {
    auto t = toml::parse("x = { a = 1, }");
    check(t.at("x").as_table().at("a").as_integer() == 1, "inline table trailing comma");
}

void test_unterminated_inline_table_error() {
    bool caught = false;
    try {
        toml::parse("x = { a = 1");
    } catch (toml::ParseError const&) {
        caught = true;
    }
    check(caught, "unterminated inline table error");
}

void test_escape_sequences() {
    auto t = toml::parse(R"(s = "hello\tworld\n")");
    check(t.at("s").as_string() == "hello\tworld\n", "escape sequences");
}

void test_literal_string() {
    auto t = toml::parse("s = 'no\\escape'");
    check(t.at("s").as_string() == "no\\escape", "literal string");
}

void test_quoted_key() {
    auto t = toml::parse(R"("quoted key" = 1)");
    check(t.at("quoted key").as_integer() == 1, "quoted key");
}

void test_comment() {
    auto t = toml::parse("# comment\nkey = 1 # inline");
    check(t.at("key").as_integer() == 1, "comment");
}

void test_trailing_comma_array() {
    auto t = toml::parse("arr = [1, 2,]");
    check(t.at("arr").as_array().size() == 2, "trailing comma array");
}

void test_multiline_array() {
    auto t = toml::parse("arr = [\n  1,\n  2,\n  # comment\n  3\n]");
    check(t.at("arr").as_array().size() == 3, "multiline array");
}

void test_contains() {
    auto t = toml::parse("a = 1");
    toml::Value v{std::move(t)};
    check(v.contains("a"), "contains existing key");
    check(!v.contains("b"), "contains missing key");
}

void test_value_copy() {
    auto t = toml::parse("[x]\ny = [1, 2]");
    toml::Value v1{std::move(t)};
    toml::Value v2 = v1;
    check(v2["x"]["y"].as_array().size() == 2, "value copy");
}

void test_duplicate_key_error() {
    bool caught = false;
    try {
        toml::parse("a = 1\na = 2");
    } catch (toml::ParseError const&) {
        caught = true;
    }
    check(caught, "duplicate key error");
}

void test_unterminated_string_error() {
    bool caught = false;
    try {
        toml::parse(R"(a = "unterminated)");
    } catch (toml::ParseError const&) {
        caught = true;
    }
    check(caught, "unterminated string error");
}

void test_empty_input() {
    auto t = toml::parse("");
    check(t.empty(), "empty input");
}

void test_whitespace_only() {
    auto t = toml::parse("  \n\n  ");
    check(t.empty(), "whitespace only");
}

int main() {
    test_string();
    test_integer();
    test_negative_integer();
    test_float();
    test_float_exponent();
    test_bool();
    test_array();
    test_empty_array();
    test_nested_array();
    test_table();
    test_nested_table();
    test_array_of_tables();
    test_inline_table();
    test_empty_inline_table();
    test_inline_table_mixed_values();
    test_inline_table_in_section();
    test_inline_table_trailing_comma();
    test_unterminated_inline_table_error();
    test_escape_sequences();
    test_literal_string();
    test_quoted_key();
    test_comment();
    test_trailing_comma_array();
    test_multiline_array();
    test_contains();
    test_value_copy();
    test_duplicate_key_error();
    test_unterminated_string_error();
    test_empty_input();
    test_whitespace_only();

    if (failed > 0) {
        std::println(std::cerr, "\n{} test(s) failed", failed);
        return 1;
    }
    std::println("all tests passed");
    return 0;
}
