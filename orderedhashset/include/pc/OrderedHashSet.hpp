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
      typedef std::list<std::pair<Key, Value> /* */> List;
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

      bool contains(Key const key) const
      {
         return mapper.find(key) != mapper.end();
      }

      std::size_t size() const
      {
         return list.size();
      }

      void insert(Key const key, Value const& value)
      {
         // If already present
         // Remove
         if (contains(key))
         {
            typename List::iterator removeExistingIt = mapper[key];
            list.erase(removeExistingIt);
         }
         list.push_back(std::make_pair(key, value));
         iterator newItemIt = list.end();
         std::advance(newItemIt, -1);
         mapper[key] = newItemIt;
      }

      Value const& operator[](Key const key) const
      {
         return *mapper[key];
      }
      Value& operator[](Key const key)
      {
         return *mapper[key];
      }

      iterator removeAndIterate(iterator it)
      {
         typename Map::iterator removeExistingMapperIt = mapper.find(it->first);
         if (removeExistingMapperIt != mapper.end())
            mapper.erase(removeExistingMapperIt);
         it = list.erase(it);
         return it;
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
