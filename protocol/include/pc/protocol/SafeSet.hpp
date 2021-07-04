#pragma once

#include <set>

#include <pc/thread/Mutex.hpp>
#include <pc/thread/MutexGuard.hpp>

namespace pc
{
   template <typename T>
   class SafeSet
   {
      typedef std::set<T>    setT;
      typedef std::ptrdiff_t size_type;

    public:
      setT items;

      mutable threads::Mutex mutex;

      operator bool() const
      {
         // If Full
         return !items.empty();
      }

      setT* operator->()
      {
         return &items;
      }
      const setT* operator->() const
      {
         return &items;
      }
      SafeSet& operator-=(T const val)
      {
         threads::MutexGuard guard(mutex);
         items.erase(val);
         return *this;
      }
      SafeSet& operator+=(T const val)
      {
         threads::MutexGuard guard(mutex);
         items.insert(val);
         return *this;
      }
      bool contains(T const val) const
      {
         return items.find(val) != items.end();
      }
   };
} // namespace pc