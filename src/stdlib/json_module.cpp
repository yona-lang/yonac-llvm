#include "stdlib/json_module.h"
#include "stdlib/native_args.h"
#include <sstream>
#include <cctype>
#include <cmath>

namespace yona::stdlib {

using namespace std;
using namespace yona::interp::runtime;

JsonModule::JsonModule() : NativeModule({"Std", "Json"}) {}

void JsonModule::initialize() {
    module->exports["parse"] = make_native_function("parse", 1, parse);
    module->exports["stringify"] = make_native_function("stringify", 1, stringify);
}

// Simple recursive descent JSON parser
class JsonParser {
    const string& input;
    size_t pos;

    void skip_whitespace() {
        while (pos < input.size() && isspace(static_cast<unsigned char>(input[pos]))) pos++;
    }

    char peek() {
        skip_whitespace();
        if (pos >= input.size()) throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "parse: unexpected end of JSON input");
        return input[pos];
    }

    char advance() {
        skip_whitespace();
        if (pos >= input.size()) throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "parse: unexpected end of JSON input");
        return input[pos++];
    }

    void expect(char c) {
        char got = advance();
        if (got != c) {
            throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME,
                "parse: expected '" + string(1, c) + "' but got '" + string(1, got) + "' at position " + to_string(pos - 1));
        }
    }

    string parse_string_value() {
        expect('"');
        string result;
        while (pos < input.size() && input[pos] != '"') {
            if (input[pos] == '\\') {
                pos++;
                if (pos >= input.size()) throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "parse: unterminated string escape");
                switch (input[pos]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        pos++;
                        if (pos + 4 > input.size()) throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "parse: incomplete unicode escape");
                        string hex = input.substr(pos, 4);
                        pos += 3; // will be incremented by the outer loop
                        unsigned int codepoint = stoul(hex, nullptr, 16);
                        if (codepoint <= 0x7F) {
                            result += static_cast<char>(codepoint);
                        } else if (codepoint <= 0x7FF) {
                            result += static_cast<char>(0xC0 | (codepoint >> 6));
                            result += static_cast<char>(0x80 | (codepoint & 0x3F));
                        } else {
                            result += static_cast<char>(0xE0 | (codepoint >> 12));
                            result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                            result += static_cast<char>(0x80 | (codepoint & 0x3F));
                        }
                        break;
                    }
                    default:
                        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME,
                            "parse: invalid escape character '" + string(1, input[pos]) + "'");
                }
            } else {
                result += input[pos];
            }
            pos++;
        }
        if (pos >= input.size()) throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "parse: unterminated string");
        pos++; // skip closing quote
        return result;
    }

    RuntimeObjectPtr parse_number() {
        size_t start = pos;
        bool is_float = false;

        if (input[pos] == '-') pos++;
        while (pos < input.size() && isdigit(static_cast<unsigned char>(input[pos]))) pos++;
        if (pos < input.size() && input[pos] == '.') {
            is_float = true;
            pos++;
            while (pos < input.size() && isdigit(static_cast<unsigned char>(input[pos]))) pos++;
        }
        if (pos < input.size() && (input[pos] == 'e' || input[pos] == 'E')) {
            is_float = true;
            pos++;
            if (pos < input.size() && (input[pos] == '+' || input[pos] == '-')) pos++;
            while (pos < input.size() && isdigit(static_cast<unsigned char>(input[pos]))) pos++;
        }

        string num_str = input.substr(start, pos - start);
        if (is_float) {
            return make_float(stod(num_str));
        } else {
            try {
                long long val = stoll(num_str);
                return make_int(static_cast<int>(val));
            } catch (...) {
                return make_float(stod(num_str));
            }
        }
    }

    RuntimeObjectPtr parse_value() {
        char c = peek();
        switch (c) {
            case '"':
                return make_string(parse_string_value());
            case '{': {
                advance(); // skip '{'
                auto dict = make_shared<DictValue>();
                if (peek() != '}') {
                    do {
                        auto key = make_string(parse_string_value());
                        expect(':');
                        auto value = parse_value();
                        dict->fields.emplace_back(key, value);
                    } while (peek() == ',' && (advance(), true));
                }
                expect('}');
                return make_shared<RuntimeObject>(yona::interp::runtime::Dict, dict);
            }
            case '[': {
                advance(); // skip '['
                auto seq = make_shared<SeqValue>();
                if (peek() != ']') {
                    do {
                        seq->fields.push_back(parse_value());
                    } while (peek() == ',' && (advance(), true));
                }
                expect(']');
                return make_shared<RuntimeObject>(Seq, seq);
            }
            case 't':
                if (input.substr(pos, 4) == "true") { pos += 4; return make_bool(true); }
                throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "parse: invalid token at position " + to_string(pos));
            case 'f':
                if (input.substr(pos, 5) == "false") { pos += 5; return make_bool(false); }
                throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "parse: invalid token at position " + to_string(pos));
            case 'n':
                if (input.substr(pos, 4) == "null") { pos += 4; return make_unit(); }
                throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "parse: invalid token at position " + to_string(pos));
            default:
                if (c == '-' || isdigit(static_cast<unsigned char>(c))) {
                    return parse_number();
                }
                throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME,
                    "parse: unexpected character '" + string(1, c) + "' at position " + to_string(pos));
        }
    }

public:
    JsonParser(const string& input) : input(input), pos(0) {}

    RuntimeObjectPtr parse_json() {
        auto result = parse_value();
        skip_whitespace();
        if (pos < input.size()) {
            throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "parse: unexpected trailing content at position " + to_string(pos));
        }
        return result;
    }
};

// JSON stringify helper
static void stringify_value(const RuntimeObjectPtr& obj, ostringstream& oss) {
    switch (obj->type) {
        case Int:
            oss << obj->get<int>();
            break;
        case Float: {
            double d = obj->get<double>();
            if (isinf(d) || isnan(d)) {
                oss << "null";
            } else {
                oss << d;
                // Ensure there's a decimal point for whole numbers
                string s = to_string(d);
                if (s.find('.') == string::npos && s.find('e') == string::npos && s.find('E') == string::npos) {
                    oss << ".0";
                }
            }
            break;
        }
        case String: {
            oss << '"';
            for (char c : obj->get<string>()) {
                switch (c) {
                    case '"': oss << "\\\""; break;
                    case '\\': oss << "\\\\"; break;
                    case '\b': oss << "\\b"; break;
                    case '\f': oss << "\\f"; break;
                    case '\n': oss << "\\n"; break;
                    case '\r': oss << "\\r"; break;
                    case '\t': oss << "\\t"; break;
                    default:
                        if (static_cast<unsigned char>(c) < 0x20) {
                            char buf[8];
                            snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                            oss << buf;
                        } else {
                            oss << c;
                        }
                }
            }
            oss << '"';
            break;
        }
        case Bool:
            oss << (obj->get<bool>() ? "true" : "false");
            break;
        case Unit:
            oss << "null";
            break;
        case Seq: {
            auto seq = obj->get<shared_ptr<SeqValue>>();
            oss << '[';
            for (size_t i = 0; i < seq->fields.size(); i++) {
                if (i > 0) oss << ',';
                stringify_value(seq->fields[i], oss);
            }
            oss << ']';
            break;
        }
        case yona::interp::runtime::Dict: {
            auto dict = obj->get<shared_ptr<DictValue>>();
            oss << '{';
            for (size_t i = 0; i < dict->fields.size(); i++) {
                if (i > 0) oss << ',';
                // Key must be stringified as a JSON string
                auto& key = dict->fields[i].first;
                if (key->type == String) {
                    stringify_value(key, oss);
                } else {
                    // Convert non-string keys to strings
                    ostringstream key_oss;
                    key_oss << *key;
                    oss << '"';
                    for (char c : key_oss.str()) {
                        switch (c) {
                            case '"': oss << "\\\""; break;
                            case '\\': oss << "\\\\"; break;
                            default: oss << c;
                        }
                    }
                    oss << '"';
                }
                oss << ':';
                stringify_value(dict->fields[i].second, oss);
            }
            oss << '}';
            break;
        }
        case Tuple: {
            auto tuple = obj->get<shared_ptr<TupleValue>>();
            oss << '[';
            for (size_t i = 0; i < tuple->fields.size(); i++) {
                if (i > 0) oss << ',';
                stringify_value(tuple->fields[i], oss);
            }
            oss << ']';
            break;
        }
        case Symbol: {
            auto sym = obj->get<shared_ptr<SymbolValue>>();
            oss << '"' << sym->name << '"';
            break;
        }
        default: {
            // For unsupported types, use the RuntimeObject ostream operator
            ostringstream tmp;
            tmp << *obj;
            oss << '"';
            for (char c : tmp.str()) {
                if (c == '"') oss << "\\\"";
                else if (c == '\\') oss << "\\\\";
                else oss << c;
            }
            oss << '"';
            break;
        }
    }
}

RuntimeObjectPtr JsonModule::parse(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("parse", 1);
    string input = ARG_STRING(0);
    JsonParser parser(input);
    return parser.parse_json();
}

RuntimeObjectPtr JsonModule::stringify(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("stringify", 1);
    ostringstream oss;
    stringify_value(args[0], oss);
    return make_string(oss.str());
}

} // namespace yona::stdlib
