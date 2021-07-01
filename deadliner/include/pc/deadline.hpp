#pragma once

#include <cstddef>
#include <ctime>

namespace pc
{
   class Deadline
   {
      std::ptrdiff_t maxCount;
      std::ptrdiff_t maxTime;

      bool kill;

      std::vector<std::ptrdiff_t> queue;

      std::ptrdiff_t front;
      std::ptrdiff_t rear;
      timespec       specTime;

    public:
      Deadline(std::size_t maxCount = 25, std::ptrdiff_t maxTime = 10 * 1000) :
          maxCount(maxCount), maxTime(maxTime), kill(false), queue(maxCount), front(-1),
          rear(-1)
      {
      }

      operator bool() const
      {
         return kill;
      }
      std::ptrdiff_t getCurrentTimeMs()
      {
         clock_gettime(CLOCK_MONOTONIC, &specTime);
         return (std::ptrdiff_t)(((std::ptrdiff_t)specTime.tv_sec * 1000) +
                                 specTime.tv_nsec / 1.0e6);
      }

      friend std::ostream& operator<<(std::ostream& os, Deadline& deadline)
      {
         if (deadline.front == -1)
            return os;
         // Function to display status of Circular Queue
         std::ptrdiff_t i = 0;
         for (i = deadline.front; i != deadline.rear; i = (i + 1) % deadline.maxCount)
            os << (deadline.queue[i] - deadline.queue[deadline.front]) << ", ";
         os << (deadline.queue[i] - deadline.queue[deadline.front]);
         return os;
      }
      void IgnoreLargeDifferences()
      { // Optimize
         // Ignore the very large differences
         // Move ahead of differences we may
         // not be interested in
         // If the deadline is greater now
         // The deadline would certainly be greater later
         // So these parts can be easily ignored
         while (true)
         {
            if (front == -1)
               break;
            if (front == rear)
               break;
            // If greater, we can ignore this value
            // And move ahead
            if ((queue[rear] - queue[(front + 1) % maxCount]) > maxTime)
            {
#ifdef DEBUG
               std::cout << std::endl
                         << "In loop " << front << " f : r " << rear << " : "
                         << (queue[rear] - queue[front] - maxTime) << " : " << *this;
#endif
               front = (front + 1) % maxCount;
            }
            else
               break;
         }
      }
      Deadline& increment()
      {
         std::ptrdiff_t curTime = getCurrentTimeMs();
#ifdef DEBUG
         std::cout << std::endl
                   << "operator " << front << " f : r " << rear << " : "
                   << (curTime - queue[front] - maxTime) << " : " << *this;

#endif
         if ((front == 0 && rear == maxCount - 1) || (front == rear + 1))
         {
            if ((curTime - queue[front]) <= maxTime)
            {
#ifdef DEBUG
               std::cout << std::endl
                         << "Killed now " << front << " f : r " << rear << " : "
                         << (curTime - queue[front] - maxTime) << " : " << *this;

#endif
               kill = true;
               return *this;
            }
         }
         if (front == -1)
            front = 0;
         rear        = (rear + 1) % maxCount;
         queue[rear] = curTime;
         IgnoreLargeDifferences();
         return *this;
      }
      Deadline& operator++()
      {
         return increment();
      }

      Deadline& MaxCount(std::size_t MaxCount)
      {
         maxCount = MaxCount;
         return *this;
      }
      std::size_t MaxCount()
      {
         return maxCount;
      }
   };
} // namespace pc