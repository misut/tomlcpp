import toml;
import txn;
import std;

#include "txn_describe.h"

// --- Test structs ---

struct Server {
    std::string host;
    int port;
    std::optional<bool> debug;
};
TXN_DESCRIBE(Server, host, port, debug)

struct Config {
    Server server;
    std::vector<std::string> tags;
};
TXN_DESCRIBE(Config, server, tags)

struct Entry {
    std::string name;
    int value;
};
TXN_DESCRIBE(Entry, name, value)

struct WithArrayOfTables {
    std::vector<Entry> entries;
};
TXN_DESCRIBE(WithArrayOfTables, entries)

// --- Test helpers ---

int failed = 0;

void check(bool cond, std::string_view msg) {
    if (!cond) {
        std::println(std::cerr, "FAIL: {}", msg);
        ++failed;
    }
}

// --- Tests ---

void test_parse_and_deserialize() {
    auto table = toml::parse(R"(
tags = ["web", "prod"]

[server]
host = "localhost"
port = 8080
    )");
    auto cfg = txn::from_value<Config>(toml::Value{std::move(table)});
    check(cfg.server.host == "localhost", "parse+deser host");
    check(cfg.server.port == 8080, "parse+deser port");
    check(cfg.server.debug == std::nullopt, "parse+deser optional absent");
    check(cfg.tags.size() == 2, "parse+deser tags size");
    check(cfg.tags[0] == "web" && cfg.tags[1] == "prod", "parse+deser tags values");
}

void test_parse_with_optional_present() {
    auto table = toml::parse(R"(
tags = []

[server]
host = "0.0.0.0"
port = 443
debug = true
    )");
    auto cfg = txn::from_value<Config>(toml::Value{std::move(table)});
    check(cfg.server.debug.has_value() && *cfg.server.debug == true, "optional present");
}

void test_array_of_tables() {
    auto table = toml::parse(R"(
        [[entries]]
        name = "alpha"
        value = 1

        [[entries]]
        name = "beta"
        value = 2
    )");
    auto w = txn::from_value<WithArrayOfTables>(toml::Value{std::move(table)});
    check(w.entries.size() == 2, "array of tables size");
    check(w.entries[0].name == "alpha" && w.entries[0].value == 1, "array of tables [0]");
    check(w.entries[1].name == "beta" && w.entries[1].value == 2, "array of tables [1]");
}

void test_serialize_to_toml_value() {
    Server s{"example.com", 443, true};
    auto v = txn::to_value<toml::Value>(s);
    check(v.is_table(), "serialize is table");
    check(v["host"].as_string() == "example.com", "serialize host");
    check(v["port"].as_integer() == 443, "serialize port");
    check(v["debug"].as_bool() == true, "serialize debug");
}

void test_roundtrip() {
    auto input = R"(
tags = ["api", "v3"]

[server]
host = "prod.example.com"
port = 8443
debug = false
    )";
    auto original = txn::from_value<Config>(toml::Value{toml::parse(input)});
    auto serialized = txn::to_value<toml::Value>(original);
    auto restored = txn::from_value<Config>(serialized);

    check(restored.server.host == "prod.example.com", "roundtrip host");
    check(restored.server.port == 8443, "roundtrip port");
    check(restored.server.debug.has_value() && *restored.server.debug == false, "roundtrip debug");
    check(restored.tags.size() == 2, "roundtrip tags size");
    check(restored.tags[0] == "api" && restored.tags[1] == "v3", "roundtrip tags");
}

void test_error_missing_key() {
    auto table = toml::parse("host = \"h\"");
    bool caught = false;
    try {
        txn::from_value<Server>(toml::Value{std::move(table)});
    } catch (txn::ConversionError const& e) {
        caught = true;
        check(std::string{e.path()} == "port", "missing key path");
    }
    check(caught, "missing key throws");
}

void test_error_type_mismatch() {
    auto table = toml::parse("host = 123\nport = 80");
    bool caught = false;
    try {
        txn::from_value<Server>(toml::Value{std::move(table)});
    } catch (txn::ConversionError const& e) {
        caught = true;
        check(std::string{e.path()} == "host", "type mismatch path");
    }
    check(caught, "type mismatch throws");
}

int main() {
    test_parse_and_deserialize();
    test_parse_with_optional_present();
    test_array_of_tables();
    test_serialize_to_toml_value();
    test_roundtrip();
    test_error_missing_key();
    test_error_type_mismatch();

    if (failed > 0) {
        std::println(std::cerr, "\n{} test(s) failed", failed);
        return 1;
    }
    std::println("all tests passed");
    return 0;
}
