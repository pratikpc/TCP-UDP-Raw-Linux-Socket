#pragma once

#include <cassert>
#include <list>
#include <tr1/unordered_map>

// Based on https://stackoverflow.com/a/42072390
namespace pc
{
   template <typename T1, typename T2>
   struct Pair
   {
      T1 const first;
      T2 const second;

      Pair(T1 first, T2 second) : first(first), second(second) {}
   };
   template <typename Key, typename Value>
   class OrderedHashSet
   {

    public:
      typedef Pair<Key, Value>                       KVPair;
      typedef std::list<KVPair>                      List;
      typedef typename List::iterator                iterator;
      typedef typename List::const_iterator          const_iterator;
      typedef std::tr1::unordered_map<Key, iterator> Map;

    private:
      List list;
      Map  mapper;

    public:
      iterator begin()
      {
         return list.begin();
      }
      const_iterator begin() const
      {
         return list.begin();
      }
      const_iterator end()
      {
         return list.end();
      }
      const_iterator end() const
      {
         return list.end();
      }

      std::size_t size() const
      {
         return list.size();
      }

      void insert(Key const key, Value const& value)
      {
         // If already present
         // Remove
         typename Map::iterator existingMapIt = mapper.find(key);
         list.push_back(KVPair(key, value));
         iterator newItemIt = list.end();
         std::advance(newItemIt, -1);
         if (existingMapIt != mapper.end())
         {
            list.erase(existingMapIt->second);
            existingMapIt->second = newItemIt;
         }
      }

      void remove(Key const key)
      {
         typename Map::iterator mapperIt = mapper.find(key);
         if (mapperIt == mapper.end())
            return;
         list.erase(mapperIt->second);
         mapper.erase(mapperIt);
      }
   };
} // namespace pc
