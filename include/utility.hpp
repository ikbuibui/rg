#pragma once
#include <iostream>
#include <string>

namespace rg
{
    template<typename T>
    void display_type()
    {
        std::string func_name(__PRETTY_FUNCTION__);
        std::string tmp = func_name.substr(func_name.find_first_of("[") + 1);
        std::string type = "type" + tmp.substr(1, tmp.size() - 2);
        std::cout << type << std::endl;
    }
} // namespace rg
