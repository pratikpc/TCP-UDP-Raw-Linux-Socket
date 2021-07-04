#pragma once

#include <vector>

namespace pc
{
   template <typename T>
   class DataQueue
   {
    public:
      typedef std::vector<std::size_t> RemoveIndexes;
      typedef std::vector<T>      QueueVec;

    private:
      bool          updateIssued;
      RemoveIndexes removeIndexes;
      QueueVec      in;

    public:
      QueueVec out;

    private:
      void IssueUpdate()
      {
         updateIssued = true;
      }

    public:
      DataQueue& operator+=(T const& val)
      {
         in.push_back(val);
         IssueUpdate();
         return *this;
      }

      DataQueue& operator-=(std::size_t index)
      {
         removeIndexes.push_back(index);
         IssueUpdate();
         return *this;
      }
      void PerformUpdate()
      {
         if (!updateIssued)
            return;
         std::size_t noOfDeleted = 0;
         if (!removeIndexes.empty())
         {
            for (RemoveIndexes::iterator it = removeIndexes.begin();
                 it != removeIndexes.end();
                 ++it, ++noOfDeleted)
            {
               std::size_t indexErase = *it - noOfDeleted;
               in.erase(in.begin() + indexErase);
            }
            // Clear indices
            removeIndexes.clear();
         }
         out = in;

         updateIssued = false;
      }
      std::size_t size() const
      {
         return in.size();
      }
      T* data()
      {
         return out.data();
      }
      typename QueueVec::const_reference front() const
      {
         return in.front();
      }
   };
} // namespace pc
