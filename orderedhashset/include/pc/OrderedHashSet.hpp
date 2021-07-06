#pragma once

#include <cassert>
#include <list>
#include <tr1/unordered_map>

// Based on https://stackoverflow.com/a/42072390
namespace pc
{
   template <typename Key, typename Value>
   class OrderedHashSet
   {

    public:
      typedef std::list<std::pair<Key, Value> /* */> Iterator;
      typedef typename Iterator::iterator            iterator;
      typedef typename Iterator::const_iterator      const_iterator;
      typedef std::tr1::unordered_map<Key, iterator> Mapper;

    private:
      Iterator iterate;
      Mapper   mapper;

    public:
      iterator begin()
      {
         return iterate.begin();
      }
      const_iterator begin() const
      {
         return iterate.begin();
      }
      const_iterator end()
      {
         return iterate.end();
      }
      const_iterator end() const
      {
         return iterate.end();
      }

      bool contains(Key const key) const
      {
         return mapper.find(key) != mapper.end();
      }

      void insert(Key const key, Value const value)
      {
         // If already present
         // Remove
         if (contains(key))
         {
            typename Iterator::iterator removeExistingIt = mapper[key];
            iterate.erase(removeExistingIt);
         }
         iterate.push_back(std::make_pair(key, value));
         iterator newItemIt = iterate.end();
         std::advance(newItemIt, -1);
         mapper[key] = newItemIt;
      }

      void remove(Key const key)
      {
         assert(contains(key));
         typename Iterator::iterator removeExistingIt = mapper[key];
         iterate.erase(removeExistingIt);
         typename Mapper::iterator removeExistingMapperIt = mapper.find(key);
         mapper.erase(removeExistingMapperIt);
      }
   };
} // namespace pc
