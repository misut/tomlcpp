# tomlcpp

A TOML parser for C++. No dependencies.

## Installation

### exon

```toml
[dependencies]
"github.com/misut/tomlcpp" = "0.3.0"
```

### CMake

```cmake
add_library(tomlcpp)
target_sources(tomlcpp PUBLIC FILE_SET CXX_MODULES FILES path/to/toml.cppm)
target_link_libraries(your_target PRIVATE tomlcpp)
```

## Quick Start

```cpp
import toml;

auto table = toml::parse(R"(
    [server]
    host = "localhost"
    port = 8080
)");

auto host = table.at("server").as_table().at("host").as_string();
auto port = table.at("server").as_table().at("port").as_integer();
```

## API

### Parsing

```cpp
toml::Table parse(std::string_view input);
toml::Table parse_file(std::string_view path);
```

### Types

| Type | Alias |
|------|-------|
| `toml::Table` | `std::map<std::string, Value>` |
| `toml::Array` | `std::vector<Value>` |
| `toml::Value` | variant of string, int64, double, bool, Array, Table |

### Value Methods

```cpp
// Type checks
v.is_string()   v.is_integer()  v.is_float()
v.is_bool()     v.is_array()    v.is_table()

// Accessors
v.as_string()   v.as_integer()  v.as_float()
v.as_bool()     v.as_array()    v.as_table()

// Table helpers
v.contains("key")
v["key"]
```

### Errors

```cpp
try {
    auto t = toml::parse(input);
} catch (toml::ParseError const& e) {
    // e.what() → "line 3: expected '='"
    // e.line() → 3
}
```

## Supported TOML features

- Strings (basic, literal, escape sequences)
- Integers, floats (including exponent notation), booleans
- Arrays (including nested, trailing comma, multi-line with comments)
- Tables and nested tables (`[a.b.c]`)
- Arrays of tables (`[[items]]`)
- Inline tables (`x = { a = 1, b = [2, 3] }`)
- Quoted keys, comments

## License

MIT
