#pragma once

namespace pc
{
   template <bool B, class T, class F>
   struct conditional
   {
      typedef T type;
   };

   template <class T, class F>
   struct conditional<false, T, F>
   {
      typedef F type;
   };

   template <class T, class U>
   struct is_same
   {
      static const bool value = false;
   };

   template <class T>
   struct is_same<T, T>
   {
      static const bool value = true;
   };

   template <bool B, class T = void>
   struct enable_if
   {
   };

   template <class T>
   struct enable_if<true, T>
   {
      typedef T type;
   };
} // namespace pc
