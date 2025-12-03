#include <string>
namespace serr {
enum class Unit {
    UINT,
    INT,
    FLOAT,
    DOUBLE,
    BOOL,
    STRING,
    ATOM,
    TABLE,
    ARRAY,
};

class Object {
    public:
        virtual Type type() const = 0;
        virtual std::string fmt() const = 0;
        virtual std::string fmt_pretty() const = 0;
        virtual std::string fmt_type() const = 0;
        virtual std::string fmt_type_pretty() const = 0;
};
}  // namespace serr
