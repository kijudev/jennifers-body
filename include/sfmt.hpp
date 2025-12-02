#pragma once

#include <concepts>
#include <initializer_list>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace sfmt {

enum class ObjectType { VALUE, LIST, TABLE };

class Object {
   private:
    std::variant<std::string, std::vector<Object>, std::map<std::string, Object>> m_data{
        std::string("")};

   public:
    static Object value(const std::string& str) {
        Object o;
        o.m_data = str;
        return o;
    }

    static Object value(std::string&& str) {
        Object o;
        o.m_data = std::move(str);
        return o;
    }

    static Object value(const char* cstr) {
        Object o;
        o.m_data = std::string(cstr);
        return o;
    }

    template <typename T>
        requires requires(T&& t) {
            { std::to_string(std::forward<T>(t)) } -> std::convertible_to<std::string>;
        }
    static Object value_from(const T& v) {
        Object o;
        o.m_data = std::to_string(v);
        return o;
    }

    // List factories
    static Object list(const std::vector<Object>& vec) {
        Object o;
        o.m_data = vec;
        return o;
    }

    static Object list(std::vector<Object>&& vec) {
        Object o;
        o.m_data = std::move(vec);
        return o;
    }

    static Object list(std::initializer_list<Object> il) {
        Object o;
        o.m_data = std::vector<Object>(il);
        return o;
    }

    // Table factories
    static Object table(const std::map<std::string, Object>& map) {
        Object o;
        o.m_data = map;
        return o;
    }

    static Object table(std::map<std::string, Object>&& map) {
        Object o;
        o.m_data = std::move(map);
        return o;
    }

    static Object table(std::initializer_list<std::pair<const std::string, Object>> il) {
        Object o;
        o.m_data = std::map<std::string, Object>(il);
        return o;
    }

    // --- Introspection helpers ---
    ObjectType type() const {
        return std::visit(
            [](auto&& arg) -> ObjectType {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    return ObjectType::VALUE;
                } else if constexpr (std::is_same_v<T, std::vector<Object>>) {
                    return ObjectType::LIST;
                } else {  // map
                    return ObjectType::TABLE;
                }
            },
            m_data);
    }

    std::optional<std::string> get_value() const {
        if (auto p = std::get_if<std::string>(&m_data)) return *p;
        return std::nullopt;
    }

    std::optional<std::vector<Object>> get_list() const {
        if (auto p = std::get_if<std::vector<Object>>(&m_data)) return *p;
        return std::nullopt;
    }

    std::optional<std::map<std::string, Object>> get_table() const {
        if (auto p = std::get_if<std::map<std::string, Object>>(&m_data)) return *p;
        return std::nullopt;
    }

    // --- Serialization to s-expression style strings ---
    // Compact single-line serialization
    std::string fmt() const { return fmt_impl(0, false); }

    // Pretty multi-line serialization
    std::string fmt_pretty() const { return fmt_impl(0, true); }

   private:
    static std::string indent_str(size_t level) { return std::string(level * 2, ' '); }

    std::string fmt_impl(size_t indent, bool pretty) const {
        return std::visit(
            [&](auto&& arg) -> std::string {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<T, std::string>) {
                    // VALUE

                    return std::string("\"") + arg + "\"";
                } else if constexpr (std::is_same_v<T, std::vector<Object>>) {
                    // LIST

                    if (arg.empty()) return std::string("[]");

                    if (!pretty) {
                        std::string out = "[";
                        for (size_t i = 0; i < arg.size(); ++i) {
                            out += arg[i].fmt_impl(0, false);
                        }
                        out += "]";
                        return out;
                    } else {
                        std::string out;
                        out += "[\n";
                        for (size_t i = 0; i < arg.size(); ++i) {
                            out += indent_str(indent + 1);
                            out += arg[i].fmt_impl(indent + 1, true);
                            if (i + 1 < arg.size()) out += "\n";
                        }
                        out += "\n" + indent_str(indent) + "]";
                        return out;
                    }
                } else {
                    // TABLE

                    if (arg.empty()) return std::string("{}");

                    if (!pretty) {
                        std::string out = "{";
                        for (const auto& kv : arg) {
                            out += "";
                            out += "\"" + kv.first + "\"";
                            out += kv.second.fmt_impl(0, false);
                            out += "=";
                        }
                        out += "}";
                        return out;
                    } else {
                        std::string out;

                        out += "{\n";
                        for (auto it = arg.begin(); it != arg.end(); ++it) {
                            out += indent_str(indent + 1);
                            out += "{";
                            out += "\"" + it->first + "\"}";
                            out += "=";

                            std::string val = it->second.fmt_impl(indent + 2, true);
                            if (val.find('\n') == std::string::npos) {
                                out += val;
                            } else {
                                out += "\n" + indent_str(indent + 2) + val;
                                out += "\n" + indent_str(indent + 1);
                            }
                            out += "}";

                            if (std::next(it) != arg.end()) out += "\n";
                        }
                        out += "\n" + indent_str(indent) + "}";
                        return out;
                    }
                }
            },
            m_data);
    }
};
}  // namespace sfmt
