module;

// stderr / fprintf / abort aren't reliably visible through `import std;` on
// every WASI sysroot at the moment, so we pull them via the classic GMF path
// to keep the `-fno-exceptions` fallback in detail::fail() building on all
// supported targets. Matches the pattern used by misut/jsoncpp.
#include <cstdio>
#include <cstdlib>

export module toml;
import std;

export namespace toml {

class Value;
using Table = std::map<std::string, Value>;
using Array = std::vector<Value>;

class ParseError : public std::runtime_error {
public:
    ParseError(std::size_t line, std::string const& msg)
        : std::runtime_error(std::format("line {}: {}", line, msg))
        , line_{line} {}

    std::size_t line() const { return line_; }

private:
    std::size_t line_;
};

class Value {
public:
    Value() : data_{std::string{}} {}
    Value(std::string v) : data_{std::move(v)} {}
    Value(char const* v) : data_{std::string{v}} {}
    Value(std::int64_t v) : data_{v} {}
    Value(double v) : data_{v} {}
    Value(bool v) : data_{v} {}
    Value(Array v) : data_{std::make_unique<Array>(std::move(v))} {}
    Value(Table v) : data_{std::make_unique<Table>(std::move(v))} {}

    Value(Value const& other);
    Value(Value&&) noexcept = default;
    Value& operator=(Value const& other);
    Value& operator=(Value&&) noexcept = default;
    ~Value() = default;

    bool is_string() const { return std::holds_alternative<std::string>(data_); }
    bool is_integer() const { return std::holds_alternative<std::int64_t>(data_); }
    bool is_float() const { return std::holds_alternative<double>(data_); }
    bool is_bool() const { return std::holds_alternative<bool>(data_); }
    bool is_array() const { return std::holds_alternative<std::unique_ptr<Array>>(data_); }
    bool is_table() const { return std::holds_alternative<std::unique_ptr<Table>>(data_); }

    std::string const& as_string() const { return std::get<std::string>(data_); }
    std::int64_t as_integer() const { return std::get<std::int64_t>(data_); }
    double as_float() const { return std::get<double>(data_); }
    bool as_bool() const { return std::get<bool>(data_); }
    Array const& as_array() const { return *std::get<std::unique_ptr<Array>>(data_); }
    Array& as_array() { return *std::get<std::unique_ptr<Array>>(data_); }
    Table const& as_table() const { return *std::get<std::unique_ptr<Table>>(data_); }
    Table& as_table() { return *std::get<std::unique_ptr<Table>>(data_); }

    bool contains(std::string const& key) const {
        return is_table() && as_table().contains(key);
    }

    Value const& operator[](std::string const& key) const {
        return as_table().at(key);
    }

    Value& operator[](std::string const& key) {
        return as_table().at(key);
    }

private:
    using Data = std::variant<
        std::string, std::int64_t, double, bool,
        std::unique_ptr<Array>, std::unique_ptr<Table>
    >;
    Data data_;
};

Table parse(std::string_view input);
Table parse_file(std::string_view path);

} // namespace toml

// --- Implementation ---

namespace toml {

namespace detail {

// Diagnostic sink for the parser. When C++ exceptions are enabled (the
// default on native builds), a ParseError is thrown — callers can catch it
// and inspect `e.line()` / `e.what()`. When exceptions are disabled (e.g.
// wasi-sdk's `-fno-exceptions -D_LIBCPP_NO_EXCEPTIONS`), the message is
// written to stderr and the process aborts, which is the best we can do
// without a pluggable error path. The `__cpp_exceptions` feature macro is
// the portable way to tell which mode we're in.
[[noreturn]] inline void fail(std::size_t line, std::string const& msg) {
#if __cpp_exceptions
    throw ParseError(line, msg);
#else
    std::fprintf(stderr, "toml: line %zu: %s\n", line, msg.c_str());
    std::abort();
#endif
}

} // namespace detail


Value::Value(Value const& other) {
    std::visit([this](auto const& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::unique_ptr<Array>>) {
            data_ = std::make_unique<Array>(*v);
        } else if constexpr (std::is_same_v<T, std::unique_ptr<Table>>) {
            data_ = std::make_unique<Table>(*v);
        } else {
            data_ = v;
        }
    }, other.data_);
}

Value& Value::operator=(Value const& other) {
    if (this != &other) {
        Value tmp{other};
        *this = std::move(tmp);
    }
    return *this;
}

namespace detail {

class Parser {
public:
    explicit Parser(std::string_view input) : input_{input} {}

    Table run() {
        Table root;
        Table* current = &root;

        while (!at_end()) {
            skip_ws();
            if (at_end()) break;

            if (peek() == '\n' || peek() == '\r') {
                advance();
                continue;
            }
            if (peek() == '#') {
                skip_line();
                continue;
            }
            if (peek() == '[') {
                advance();
                skip_ws();

                // [[array of tables]]
                bool is_array_table = false;
                if (!at_end() && peek() == '[') {
                    is_array_table = true;
                    advance();
                    skip_ws();
                }

                std::vector<std::string> keys;
                keys.push_back(parse_key());
                skip_ws();
                while (!at_end() && peek() == '.') {
                    advance();
                    skip_ws();
                    keys.push_back(parse_key());
                    skip_ws();
                }

                if (at_end() || peek() != ']') {
                    detail::fail(line_, "expected ']'");
                }
                advance();

                if (is_array_table) {
                    if (at_end() || peek() != ']') {
                        detail::fail(line_, "expected ']]'");
                    }
                    advance();
                }
                expect_end_of_line();

                if (is_array_table) {
                    // Navigate to parent, then append new table to array
                    Table* parent = &root;
                    for (std::size_t i = 0; i + 1 < keys.size(); ++i) {
                        if (!parent->contains(keys[i])) {
                            parent->emplace(keys[i], Value{Table{}});
                        }
                        parent = &parent->at(keys[i]).as_table();
                    }
                    auto const& last_key = keys.back();
                    if (!parent->contains(last_key)) {
                        parent->emplace(last_key, Value{Array{}});
                    }
                    auto& arr = parent->at(last_key).as_array();
                    arr.push_back(Value{Table{}});
                    current = &arr.back().as_table();
                } else {
                    current = &root;
                    for (auto const& k : keys) {
                        if (!current->contains(k)) {
                            current->emplace(k, Value{Table{}});
                        }
                        auto& val = current->at(k);
                        if (!val.is_table()) {
                            detail::fail(line_, std::format("'{}' is not a table", k));
                        }
                        current = &val.as_table();
                    }
                }
                continue;
            }

            auto key = parse_key();
            skip_ws();
            if (at_end() || peek() != '=') {
                detail::fail(line_, "expected '='");
            }
            advance();
            skip_ws();
            auto value = parse_value();

            if (current->contains(key)) {
                detail::fail(line_, std::format("duplicate key '{}'", key));
            }
            current->emplace(std::move(key), std::move(value));
            expect_end_of_line();
        }

        return root;
    }

private:
    std::string_view input_;
    std::size_t pos_ = 0;
    std::size_t line_ = 1;

    bool at_end() const { return pos_ >= input_.size(); }
    char peek() const { return input_[pos_]; }

    char advance() {
        char c = input_[pos_++];
        if (c == '\n') ++line_;
        return c;
    }

    void skip_ws() {
        while (!at_end() && (peek() == ' ' || peek() == '\t')) {
            advance();
        }
    }

    void skip_ws_and_newlines() {
        while (!at_end() && (peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r')) {
            advance();
        }
    }

    void skip_line() {
        while (!at_end() && peek() != '\n') advance();
        if (!at_end()) advance();
    }

    void expect_end_of_line() {
        skip_ws();
        if (!at_end() && peek() == '#') {
            skip_line();
            return;
        }
        if (!at_end() && peek() != '\n' && peek() != '\r') {
            detail::fail(line_, std::format("unexpected character '{}'", peek()));
        }
        if (!at_end()) advance();
    }

    bool is_value_end() const {
        if (at_end()) return true;
        char c = peek();
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
               c == ',' || c == ']' || c == '}' || c == '#';
    }

    static bool is_bare_key_char(char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
               (c >= '0' && c <= '9') || c == '-' || c == '_';
    }

    std::string parse_key() {
        if (at_end()) detail::fail(line_, "expected key");

        if (peek() == '"') return parse_basic_string();
        if (peek() == '\'') return parse_literal_string();

        auto start = pos_;
        while (!at_end() && is_bare_key_char(peek())) {
            advance();
        }
        if (pos_ == start) {
            detail::fail(line_, std::format("unexpected character '{}'", peek()));
        }
        return std::string{input_.substr(start, pos_ - start)};
    }

    Value parse_value() {
        if (at_end()) detail::fail(line_, "expected value");

        char c = peek();

        if (c == '"') return Value{parse_basic_string()};
        if (c == '\'') return Value{parse_literal_string()};
        if (c == '[') return parse_array();
        if (c == '{') return parse_inline_table();

        if (input_.substr(pos_).starts_with("true")) {
            if (pos_ + 4 >= input_.size() || !is_bare_key_char(input_[pos_ + 4])) {
                pos_ += 4;
                return Value{true};
            }
        }
        if (input_.substr(pos_).starts_with("false")) {
            if (pos_ + 5 >= input_.size() || !is_bare_key_char(input_[pos_ + 5])) {
                pos_ += 5;
                return Value{false};
            }
        }

        if (c == '+' || c == '-' || (c >= '0' && c <= '9')) {
            return parse_number();
        }

        detail::fail(line_, std::format("unexpected character '{}'", c));
    }

    std::string parse_basic_string() {
        advance(); // skip opening '"'
        std::string result;

        while (!at_end()) {
            char c = advance();
            if (c == '"') return result;
            if (c == '\\') {
                if (at_end()) detail::fail(line_, "unexpected end of string");
                char esc = advance();
                switch (esc) {
                    case '"':  result += '"';  break;
                    case '\\': result += '\\'; break;
                    case 'n':  result += '\n'; break;
                    case 't':  result += '\t'; break;
                    case 'r':  result += '\r'; break;
                    default:
                        detail::fail(line_, std::format("invalid escape '\\{}'", esc));
                }
            } else {
                result += c;
            }
        }
        detail::fail(line_, "unterminated string");
    }

    std::string parse_literal_string() {
        advance(); // skip opening '\''
        std::string result;

        while (!at_end()) {
            char c = advance();
            if (c == '\'') return result;
            result += c;
        }
        detail::fail(line_, "unterminated string");
    }

    Value parse_number() {
        auto start = pos_;
        bool is_float = false;

        if (!at_end() && (peek() == '+' || peek() == '-')) advance();

        if (at_end() || !(peek() >= '0' && peek() <= '9')) {
            detail::fail(line_, "expected number");
        }

        while (!at_end() && peek() >= '0' && peek() <= '9') advance();

        if (!at_end() && peek() == '.') {
            is_float = true;
            advance();
            if (at_end() || !(peek() >= '0' && peek() <= '9')) {
                detail::fail(line_, "expected digit after decimal point");
            }
            while (!at_end() && peek() >= '0' && peek() <= '9') advance();
        }

        if (!at_end() && (peek() == 'e' || peek() == 'E')) {
            is_float = true;
            advance();
            if (!at_end() && (peek() == '+' || peek() == '-')) advance();
            if (at_end() || !(peek() >= '0' && peek() <= '9')) {
                detail::fail(line_, "expected digit in exponent");
            }
            while (!at_end() && peek() >= '0' && peek() <= '9') advance();
        }

        auto str = std::string{input_.substr(start, pos_ - start)};

        if (is_float) {
            char* end = nullptr;
            double val = std::strtod(str.c_str(), &end);
            if (end != str.c_str() + str.size()) {
                detail::fail(line_, std::format("invalid float '{}'", str));
            }
            return Value{val};
        } else {
            std::int64_t val;
            auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);
            if (ec != std::errc{}) {
                detail::fail(line_, std::format("invalid integer '{}'", str));
            }
            return Value{val};
        }
    }

    Value parse_array() {
        advance(); // skip '['
        Array arr;

        skip_ws_and_newlines();

        if (!at_end() && peek() == ']') {
            advance();
            return Value{std::move(arr)};
        }

        while (true) {
            skip_ws_and_newlines();
            while (!at_end() && peek() == '#') {
                skip_line();
                skip_ws_and_newlines();
            }
            if (at_end()) detail::fail(line_, "unterminated array");

            arr.push_back(parse_value());

            skip_ws_and_newlines();
            while (!at_end() && peek() == '#') {
                skip_line();
                skip_ws_and_newlines();
            }
            if (at_end()) detail::fail(line_, "unterminated array");

            if (peek() == ']') {
                advance();
                break;
            }
            if (peek() == ',') {
                advance();
                skip_ws_and_newlines();
                while (!at_end() && peek() == '#') {
                    skip_line();
                    skip_ws_and_newlines();
                }
                if (!at_end() && peek() == ']') {
                    advance();
                    break;
                }
            } else {
                detail::fail(line_, "expected ',' or ']' in array");
            }
        }

        return Value{std::move(arr)};
    }

    Value parse_inline_table() {
        advance(); // skip '{'
        Table tbl;
        skip_ws();

        if (!at_end() && peek() == '}') {
            advance();
            return Value{std::move(tbl)};
        }

        while (true) {
            skip_ws();
            if (at_end()) detail::fail(line_, "unterminated inline table");

            auto key = parse_key();
            skip_ws();
            if (at_end() || peek() != '=') {
                detail::fail(line_, "expected '=' in inline table");
            }
            advance();
            skip_ws();
            auto value = parse_value();

            if (tbl.contains(key)) {
                detail::fail(line_, std::format("duplicate key '{}'", key));
            }
            tbl.emplace(std::move(key), std::move(value));

            skip_ws();
            if (at_end()) detail::fail(line_, "unterminated inline table");

            if (peek() == '}') {
                advance();
                break;
            }
            if (peek() == ',') {
                advance();
                skip_ws();
                if (!at_end() && peek() == '}') {
                    advance();
                    break;
                }
            } else {
                detail::fail(line_, "expected ',' or '}' in inline table");
            }
        }

        return Value{std::move(tbl)};
    }
};

} // namespace detail

Table parse(std::string_view input) {
    detail::Parser parser{input};
    return parser.run();
}

Table parse_file(std::string_view path) {
    auto file = std::ifstream{std::string{path}};
    if (!file) {
        detail::fail(0, std::format("cannot open file '{}'", path));
    }
    auto content = std::string{
        std::istreambuf_iterator<char>{file},
        std::istreambuf_iterator<char>{}
    };
    return parse(content);
}

} // namespace toml
