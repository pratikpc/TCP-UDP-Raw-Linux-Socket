#include <iostream>

#include <pc/network/TCP.hpp>
#include <pc/network/TCPPoll.hpp>
#include <pc/network/ip.hpp>

#include <cstdlib>

#include <pc/thread/Thread.hpp>

#include <sys/sysinfo.h>

void pollCallback(pollfd const& poll)
{
   //   std::cout << "Poll called type"
   //             << " revents: " << poll.revents << "\n"
   //             << ((poll.revents & POLLIN) ? "POLLIN\n" : "")
   //             << ((poll.revents & POLLPRI) ? "POLLPRI\n" : "")
   //             << ((poll.revents & POLLHUP) ? "POLLHUP\n" : "")
   //             << ((poll.revents & POLLERR) ? "POLLERR\n" : "")
   //             << ((poll.revents & POLLNVAL) ? "POLLNVAL\n" : "");

   std::cout << "Poll called " << poll.fd << "\n";
   if (poll.revents & POLLIN)
   {
      pc::network::memory::unique_arr<char> data(1000);
      pc::network::TCP::recvRaw(poll.fd, data.get(), data.size);
      if (!data)
      {
         std::cout << "Data not found\n";
         close(poll.fd);
         return;
      }
      std::cout << "\nReceived data " << data.get();
      std::string message = "Server says hi to ";
      pc::network::TCP::sendRaw(poll.fd, (const char*)message.data(), message.size());
   }
}

void* execTcp(void* arg)
{
   pc::network::TCPPoll<>* poll = ((pc::network::TCPPoll<>*)arg);
   while (true)
   {
      std::cout << "\nExec poll start";
      poll->exec(5);
      std::cout << "\nExec poll end";
   }
   return NULL;
}

int main()
{
   pc::network::IP ip(SOCK_STREAM);
   ip.hints.ai_flags = AI_PASSIVE; // Use current IP
   ip.load("", "9900");

   std::string ipstr = ip;
   std::cout << "IP = " << ipstr;
   std::cout << "\n Hostname = " << ip.hostName();
   pc::network::TCP tcp(ip.bind());
   tcp.setReusable();
   tcp.listen();
   std::vector<pc::network::TCPPoll<> /* */> polls;
   polls.resize(get_nprocs());

   for (std::vector<pc::network::TCPPoll<> /* */>::iterator it = polls.begin();
        it != polls.end();
        ++it)
      pc::threads::Thread(&execTcp, &(*it)).detach();

   int threadCurrentAlloc = 0;
   while (true)
   {
      pc::network::TCP child(tcp.accept());
      if (child.invalid())
         continue;
      polls[threadCurrentAlloc].PollThis(child.socket, pollCallback);
      std::cout << "Connected to " << child.socket << " on " << threadCurrentAlloc
                << " thread"
                << "\n";
      threadCurrentAlloc = (threadCurrentAlloc + 1) % polls.size();
      child.invalidate();
   }
   return EXIT_SUCCESS;
}
