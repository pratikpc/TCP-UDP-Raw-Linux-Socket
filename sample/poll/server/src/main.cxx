#include <iostream>

#include <pc/network/TCP.hpp>
#include <pc/network/TCPPoll.hpp>
#include <pc/network/ip.hpp>

#include <pc/memory/unique_ptr.hpp>

#include <pc/thread/Thread.hpp>

#include <pc/balancer/priority.hpp>

#include <cstdlib>

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

   // std::cout << "Poll called " << poll.fd << "\n";
   if (poll.revents & POLLIN)
   {
      pc::memory::unique_arr<char> data(1000);
      pc::network::TCP::recvRaw(poll.fd, data.get(), data.size);
      // std::cout << "\nReceived data " << data.get();
      std::string message = "Server says hi ";
      pc::network::TCP::sendRaw(poll.fd, (const char*)message.data(), message.size());
   }
}

void* execTcp(void* arg)
{
   pc::network::TCPPoll<>& poll = *((pc::network::TCPPoll<>*)arg);
   while (true)
   {
      poll.exec(10);
   }
   return NULL;
}

void downCallback(pc::balancer::priority& balencer, std::size_t const idx)
{
   balencer.decPriority(idx);
   std::cout << std::endl << "One client went down";
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
   std::vector<pc::network::TCPPoll<> /* */> polls(get_nprocs());

   pc::balancer::priority balancer(polls.size());

   for (std::vector<pc::network::TCPPoll<> /* */>::iterator it = polls.begin();
        it != polls.end();
        ++it)
   {
      it->balancerIndex = (it - polls.begin());
      it->downCallback  = &downCallback;
      it->balancer      = &balancer;
      pc::threads::Thread(&execTcp, &(*it)).detach();
   }

   while (true)
   {
      std::cout << std::endl << "Try to connect";
      pc::network::TCP child(tcp.accept());
      if (child.invalid())
         continue;
      std::size_t currentBalance = *balancer;
      ++balancer;
      polls[currentBalance].PollThis(child.socket, pollCallback);
      std::cout << std::endl
                << "Connected to " << child.socket << " on " << currentBalance
                << " thread : Balancer" << balancer;
      child.invalidate();
   }
   return EXIT_SUCCESS;
}
