#pragma once

#include <cstddef>
#include <vector>

namespace pc
{
   template <typename T>
   class queue
   {
      typedef std::vector<T> queueT;
      typedef std::ptrdiff_t size_type;

    public:
      queueT    items;
      size_type front;
      size_type rear;
      size_type maxCount;

      queue(size_type size) : items(size), front(-1), rear(-1), maxCount(size) {}

      operator bool() const
      {
         // If Full
         return ((front == 0 && rear == maxCount - 1) || (front == rear + 1));
      }

      T First() const
      {
         assert(front != -1);
         return items[front];
      }
      T Last() const
      {
         assert(rear != -1);
         return items[rear];
      }
      queue& Add(T const value)
      {
         // If queue is full
         if (*this)
         {
            // Deque current value
            front = (front + 1) % maxCount;
         }
         if (front == -1)
            front = 0;
         rear        = (rear + 1) % maxCount;
         items[rear] = value;
         return *this;
      }

      queue& MaxCount(std::ptrdiff_t newMaxCount)
      {
         // Check if Queue needs to be expanded
         if (maxCount < newMaxCount)
         {
            if (front > rear)
            {
               // Copy from start to finish to temporary
               queueT temp(newMaxCount);
               for (std::ptrdiff_t i = front, j = 0; i != rear;
                    i = (i + 1) % maxCount, ++j)
                  temp[j] = items[i];
               // Set front and rear to 0 and new end
               front = 0;
               rear  = maxCount;
               // Clear queue
               items.clear();
               // Move temp to queue
               items = temp;
            }
            // If the array is not yet circular
            // Simply resize
            else
               items.resize(newMaxCount);
         }
         maxCount = newMaxCount;
         return *this;
      }
   };
} // namespace pc
