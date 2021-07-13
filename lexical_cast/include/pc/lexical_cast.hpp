#pragma once

#include <cstdlib>
#include <sstream>
#include <string>

#include <pc/stl/templates.hpp>

namespace pc
{
   template <typename Param, typename ReturnV>
   ReturnV lexical_cast(Param)
   {
   }

   template <>
   int lexical_cast<std::string, int>(std::string value)
   {
      return std::atoi(value.c_str());
   }
   template <>
   long lexical_cast<std::string, long>(std::string value)
   {
      return std::atol(value.c_str());
   }

   template <typename T>
   typename stl::enable_if<!stl::is_same<T, std::string>::value, std::string>::type
       lexical_cast(T value)
   {
      std::ostringstream stream;
      stream << value;
      return stream.str();
   }
} // namespace pc
