#pragma once

#include <tr1/unordered_set>
#include <vector>

namespace pc
{
   template <typename T>
   class DataQueue
   {
    public:
      typedef std::tr1::unordered_set<std::size_t> RemoveIndexes;
      typedef std::vector<T>                       QueueVec;

      typedef typename QueueVec::iterator       iterator;
      typedef typename QueueVec::const_iterator const_iterator;

    private:
      bool          updateIssued;
      RemoveIndexes removeIndexes;
      QueueVec      in;
      QueueVec      out;

    private:
      void IssueUpdate()
      {
         updateIssued = true;
      }

    public:
      DataQueue& Add(T const& val)
      {
         in.push_back(val);
         IssueUpdate();
         return *this;
      }

      DataQueue& RemoveAtIndex(std::size_t index)
      {
         removeIndexes.insert(index);
         IssueUpdate();
         return *this;
      }
      void PerformUpdate()
      {
         if (!updateIssued)
            return;
         if (!removeIndexes.empty())
         {
            out.clear();
            for (std::size_t i = 0; i < in.size(); ++i)
            {
               if (removeIndexes.find(i) == removeIndexes.end())
                  out.push_back(in[i]);
            }
            // Clear indices
            removeIndexes.clear();
            in = out;
         }
         else
         {
            out = in;
         }
         updateIssued = false;
      }
      std::size_t size() const
      {
         return out.size();
      }
      T* data()
      {
         return out.data();
      }

      const_iterator begin() const
      {
         return out.begin();
      }
      iterator begin()
      {
         return out.begin();
      }
      const_iterator end() const
      {
         return out.end();
      }
      iterator end()
      {
         return out.end();
      }
   };
} // namespace pc
