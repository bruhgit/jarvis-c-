#include "util/json.h"

#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace jarvis::util {

namespace {

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    JsonValue parse() {
        skipWhitespace();
        JsonValue value = parseValue();
        skipWhitespace();
        if (!eof()) {
            throw std::runtime_error("JSON parse hatasi: beklenmeyen karakter.");
        }
        return value;
    }

private:
    JsonValue parseValue() {
        if (eof()) {
            throw std::runtime_error("JSON parse hatasi: beklenmeyen son.");
        }

        const char ch = peek();
        if (ch == '"') {
            return JsonValue(parseString());
        }
        if (ch == '{') {
            return JsonValue(parseObject());
        }
        if (ch == '[') {
            return JsonValue(parseArray());
        }
        if (ch == 't') {
            consumeLiteral("true");
            return JsonValue(true);
        }
        if (ch == 'f') {
            consumeLiteral("false");
            return JsonValue(false);
        }
        if (ch == 'n') {
            consumeLiteral("null");
            return JsonValue(nullptr);
        }
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            return JsonValue(parseNumber());
        }

        throw std::runtime_error("JSON parse hatasi: gecersiz deger.");
    }

    JsonValue::Object parseObject() {
        JsonValue::Object object;
        consume('{');
        skipWhitespace();
        if (peekIf('}')) {
            consume('}');
            return object;
        }

        while (true) {
            skipWhitespace();
            std::string key = parseString();
            skipWhitespace();
            consume(':');
            skipWhitespace();
            object.emplace(std::move(key), parseValue());
            skipWhitespace();

            if (peekIf('}')) {
                consume('}');
                break;
            }

            consume(',');
            skipWhitespace();
        }

        return object;
    }

    JsonValue::Array parseArray() {
        JsonValue::Array array;
        consume('[');
        skipWhitespace();
        if (peekIf(']')) {
            consume(']');
            return array;
        }

        while (true) {
            skipWhitespace();
            array.push_back(parseValue());
            skipWhitespace();
            if (peekIf(']')) {
                consume(']');
                break;
            }
            consume(',');
            skipWhitespace();
        }

        return array;
    }

    std::string parseString() {
        consume('"');
        std::string out;

        while (!eof()) {
            const char ch = get();
            if (ch == '"') {
                return out;
            }
            if (ch != '\\') {
                out.push_back(ch);
                continue;
            }

            if (eof()) {
                break;
            }

            const char escaped = get();
            switch (escaped) {
                case '"':
                case '\\':
                case '/':
                    out.push_back(escaped);
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                case 'u': {
                    std::string hex;
                    for (int index = 0; index < 4; ++index) {
                        if (eof()) {
                            throw std::runtime_error("JSON parse hatasi: eksik unicode escape.");
                        }
                        hex.push_back(get());
                    }
                    unsigned int code = 0;
                    std::istringstream(hex) >> std::hex >> code;
                    if (code <= 0x7F) {
                        out.push_back(static_cast<char>(code));
                    } else {
                        out.push_back('?');
                    }
                    break;
                }
                default:
                    throw std::runtime_error("JSON parse hatasi: gecersiz escape.");
            }
        }

        throw std::runtime_error("JSON parse hatasi: string kapanmadi.");
    }

    double parseNumber() {
        const std::size_t begin = index_;
        if (peekIf('-')) {
            ++index_;
        }
        consumeDigits();
        if (peekIf('.')) {
            ++index_;
            consumeDigits();
        }
        if (peekIf('e') || peekIf('E')) {
            ++index_;
            if (peekIf('+') || peekIf('-')) {
                ++index_;
            }
            consumeDigits();
        }

        const auto raw = std::string(text_.substr(begin, index_ - begin));
        try {
            return std::stod(raw);
        } catch (...) {
            throw std::runtime_error("JSON parse hatasi: sayi gecersiz.");
        }
    }

    void consumeDigits() {
        if (eof() || std::isdigit(static_cast<unsigned char>(peek())) == 0) {
            throw std::runtime_error("JSON parse hatasi: rakam bekleniyordu.");
        }
        while (!eof() && std::isdigit(static_cast<unsigned char>(peek())) != 0) {
            ++index_;
        }
    }

    void consumeLiteral(std::string_view literal) {
        for (char ch : literal) {
            if (eof() || get() != ch) {
                throw std::runtime_error("JSON parse hatasi: literal uyusmadi.");
            }
        }
    }

    void consume(char expected) {
        if (eof() || get() != expected) {
            throw std::runtime_error("JSON parse hatasi: beklenen karakter bulunamadi.");
        }
    }

    bool peekIf(char expected) const {
        return !eof() && text_[index_] == expected;
    }

    char peek() const {
        return text_[index_];
    }

    char get() {
        return text_[index_++];
    }

    bool eof() const {
        return index_ >= text_.size();
    }

    void skipWhitespace() {
        while (!eof() && std::isspace(static_cast<unsigned char>(text_[index_])) != 0) {
            ++index_;
        }
    }

    std::string_view text_;
    std::size_t index_ = 0;
};

std::string escapeString(std::string_view value) {
    std::ostringstream out;
    for (char ch : value) {
        switch (ch) {
            case '"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            case '\b':
                out << "\\b";
                break;
            case '\f':
                out << "\\f";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (static_cast<unsigned char>(ch) < 32U) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(ch))
                        << std::dec << std::setfill(' ');
                } else {
                    out << ch;
                }
                break;
        }
    }
    return out.str();
}

void dumpValue(const JsonValue& value, std::ostringstream& out, int indent, int depth) {
    const auto pad = [&](int extra = 0) {
        for (int index = 0; index < depth + extra; ++index) {
            out.put(' ');
        }
    };

    switch (value.type()) {
        case JsonValue::Type::Null:
            out << "null";
            break;
        case JsonValue::Type::Bool:
            out << (value.asBool() ? "true" : "false");
            break;
        case JsonValue::Type::Number:
            out << value.asNumber();
            break;
        case JsonValue::Type::String:
            out << '"' << escapeString(value.asString()) << '"';
            break;
        case JsonValue::Type::Array: {
            out << '[';
            const auto& array = value.asArray();
            if (!array.empty()) {
                const bool pretty = indent > 0;
                for (std::size_t index = 0; index < array.size(); ++index) {
                    if (pretty) {
                        out << '\n';
                        pad(indent);
                    }
                    dumpValue(array[index], out, indent, depth + indent);
                    if (index + 1 < array.size()) {
                        out << ',';
                    }
                }
                if (pretty) {
                    out << '\n';
                    pad();
                }
            }
            out << ']';
            break;
        }
        case JsonValue::Type::Object: {
            out << '{';
            const auto& object = value.asObject();
            if (!object.empty()) {
                const bool pretty = indent > 0;
                std::size_t index = 0;
                for (const auto& [key, item] : object) {
                    if (pretty) {
                        out << '\n';
                        pad(indent);
                    }
                    out << '"' << escapeString(key) << '"' << ':';
                    if (pretty) {
                        out << ' ';
                    }
                    dumpValue(item, out, indent, depth + indent);
                    if (++index < object.size()) {
                        out << ',';
                    }
                }
                if (pretty) {
                    out << '\n';
                    pad();
                }
            }
            out << '}';
            break;
        }
    }
}

}  // namespace

JsonValue::JsonValue() : data_(nullptr) {}
JsonValue::JsonValue(std::nullptr_t) : data_(nullptr) {}
JsonValue::JsonValue(bool value) : data_(value) {}
JsonValue::JsonValue(double value) : data_(value) {}
JsonValue::JsonValue(int value) : data_(static_cast<double>(value)) {}
JsonValue::JsonValue(std::string value) : data_(std::move(value)) {}
JsonValue::JsonValue(const char* value) : data_(std::string(value == nullptr ? "" : value)) {}
JsonValue::JsonValue(Array value) : data_(std::move(value)) {}
JsonValue::JsonValue(Object value) : data_(std::move(value)) {}

JsonValue::Type JsonValue::type() const {
    if (std::holds_alternative<std::nullptr_t>(data_)) {
        return Type::Null;
    }
    if (std::holds_alternative<bool>(data_)) {
        return Type::Bool;
    }
    if (std::holds_alternative<double>(data_)) {
        return Type::Number;
    }
    if (std::holds_alternative<std::string>(data_)) {
        return Type::String;
    }
    if (std::holds_alternative<Array>(data_)) {
        return Type::Array;
    }
    return Type::Object;
}

bool JsonValue::isNull() const { return type() == Type::Null; }
bool JsonValue::isBool() const { return type() == Type::Bool; }
bool JsonValue::isNumber() const { return type() == Type::Number; }
bool JsonValue::isString() const { return type() == Type::String; }
bool JsonValue::isArray() const { return type() == Type::Array; }
bool JsonValue::isObject() const { return type() == Type::Object; }

bool JsonValue::asBool() const { return std::get<bool>(data_); }
double JsonValue::asNumber() const { return std::get<double>(data_); }
const std::string& JsonValue::asString() const { return std::get<std::string>(data_); }
const JsonValue::Array& JsonValue::asArray() const { return std::get<Array>(data_); }
const JsonValue::Object& JsonValue::asObject() const { return std::get<Object>(data_); }
JsonValue::Array& JsonValue::asArray() { return std::get<Array>(data_); }
JsonValue::Object& JsonValue::asObject() { return std::get<Object>(data_); }

const JsonValue* JsonValue::find(std::string_view key) const {
    if (!isObject()) {
        return nullptr;
    }
    const auto& object = asObject();
    const auto it = object.find(std::string(key));
    return it == object.end() ? nullptr : &it->second;
}

JsonValue* JsonValue::find(std::string_view key) {
    if (!isObject()) {
        return nullptr;
    }
    auto& object = asObject();
    const auto it = object.find(std::string(key));
    return it == object.end() ? nullptr : &it->second;
}

std::string JsonValue::dump(int indent) const {
    std::ostringstream out;
    dumpValue(*this, out, indent, 0);
    return out.str();
}

JsonValue JsonValue::parse(std::string_view text) {
    return Parser(text).parse();
}

}  // namespace jarvis::util
