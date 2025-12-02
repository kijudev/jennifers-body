#include <iostream>

#include "sfmt.hpp"

int main() {
    sfmt::Object sexpr = sfmt::Object::table(
        {{"key1", sfmt::Object::value("value1")},
         {"key2",
          sfmt::Object::list({sfmt::Object::value("value2"), sfmt::Object::value("value3")})}});

    std::cout << sexpr.fmt() << std::endl;
    std::cout << "\n";
    std::cout << sexpr.fmt_pretty() << std::endl;

    return 0;
}
