#pragma once

#include <string>

namespace options
{
    // ordering in this block is somewhat delicate

    // allow std variants of this (for basic types) to work in this namespace
    using std::to_string;

    namespace details
    {
        template<class>
        struct sfinae_true : std::true_type{};

        template<class T>
        static auto test_has_str(int)
            -> sfinae_true<decltype(std::declval<T>().str())>;
        template<class>
        static auto test_has_str(long) -> std::false_type;
    }

    // type trait: does it have a method ::str()?
    // literal `0` is type int, this is to force a resolution ordering
    template<class T>
    struct Has_str : decltype(details::test_has_str<T>(0)){};

    template<typename T,
        typename = typename std::enable_if<!Has_str<T>::value>::type>
    std::string to_string(const T &e) = delete; // missing options::to_string specialization

    // if T implements a T::str() function, try to use it
    template<typename T,
        typename std::enable_if<Has_str<T>::value, bool>::type = true>
    std::string to_string(const T &e)
    {
        return e.str();
    }

    // not a template specialization, but participates in overload resolution
    std::string to_string(const std::string &e);
}
