#pragma once

#include <cerrno>
#include <sys/epoll.h>
#include <unistd.h>

namespace pc
{
   namespace poll
   {
      class EPoll
      {
       protected:
         int const fd;

       public:
         EPoll(int const flags = 0) : fd(::epoll_create1(flags)) {}
         ~EPoll()
         {
            if (!*this)
               ::close(fd);
         }
         operator bool() const
         {
            return fd != -1;
         }
         bool Add(::epoll_event& event) const
         {
            int const result = ::epoll_ctl(fd, EPOLL_CTL_ADD, event.data.fd, &event);
            if (result != -1)
               return true;
            if (errno == EEXIST)
               return true;
            return false;
         }
         bool Add(int const socket, int const events) const
         {
            ::epoll_event event;
            event.data.fd = socket;
            event.events  = events;
            return Add(event);
         }
         template <typename Events>
         int Wait(Events& events, int timeoutS) const
         {
            int const res = epoll_wait(fd, events.data(), events.size(), timeoutS * 1000);
            return res;
         }
      };
   } // namespace poll
} // namespace pc
