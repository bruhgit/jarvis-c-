#pragma once

#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace jarvis::util {

class JsonValue {
public:
    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;

    enum class Type {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object,
    };

    JsonValue();
    JsonValue(std::nullptr_t);
    JsonValue(bool value);
    JsonValue(double value);
    JsonValue(int value);
    JsonValue(std::string value);
    JsonValue(const char* value);
    JsonValue(Array value);
    JsonValue(Object value);

    Type type() const;
    bool isNull() const;
    bool isBool() const;
    bool isNumber() const;
    bool isString() const;
    bool isArray() const;
    bool isObject() const;

    bool asBool() const;
    double asNumber() const;
    const std::string& asString() const;
    const Array& asArray() const;
    const Object& asObject() const;
    Array& asArray();
    Object& asObject();

    const JsonValue* find(std::string_view key) const;
    JsonValue* find(std::string_view key);

    std::string dump(int indent = 0) const;

    static JsonValue parse(std::string_view text);

private:
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> data_;
};

}  // namespace jarvis::util
