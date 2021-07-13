#pragma once

#if __cplusplus >= 201103L
#   include <cstdint>
#else
#   include <cstddef>
#endif

namespace pc
{
   namespace numeric
   {
      template <std::size_t N>
      struct Integral;

      template <>
      struct Integral<1>
      {
#if __cplusplus >= 201103L
         typedef std::int8_t type;
#else
#   ifdef __GNUG__
         typedef __INT8_TYPE__   type;
#   endif
#endif
      };

      template <>
      struct Integral<2>
      {
#if __cplusplus >= 201103L
         typedef std::int16_t type;
#else
#   ifdef __GNUG__
         typedef __INT16_TYPE__  type;
#   endif
#endif
      };

      template <>
      struct Integral<4>
      {
#if __cplusplus >= 201103L
         typedef std::int32_t type;
#else
#   ifdef __GNUG__
         typedef __INT32_TYPE__  type;
#   endif
#endif
      };

      template <std::size_t N>
      struct UnsignedIntegral;

      template <>
      struct UnsignedIntegral<1>
      {
#if __cplusplus >= 201103L
         typedef std::uint8_t type;
#else
#   ifdef __GNUG__
         typedef __UINT8_TYPE__  type;
#   endif
#endif
      };

      template <>
      struct UnsignedIntegral<2>
      {
#if __cplusplus >= 201103L
         typedef std::uint16_t type;
#else
#   ifdef __GNUG__
         typedef __UINT16_TYPE__ type;
#   endif
#endif
      };

      template <>
      struct UnsignedIntegral<4>
      {
#if __cplusplus >= 201103L
         typedef std::uint32_t type;
#else
#   ifdef __GNUG__
         typedef __UINT32_TYPE__ type;
#   endif
#endif
      };
      typedef Integral<2>::type         Int16;
      typedef Integral<4>::type         Int32;
      typedef UnsignedIntegral<2>::type UInt16;
      typedef UnsignedIntegral<4>::type UInt32;
   } // namespace numeric
} // namespace pc
