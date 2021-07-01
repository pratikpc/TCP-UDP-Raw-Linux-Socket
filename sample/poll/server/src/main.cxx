#include <iostream>

#include <pc/network/TCP.hpp>
#include <pc/network/TCPPoll.hpp>
#include <pc/network/ip.hpp>

#include <pc/memory/unique_ptr.hpp>

#include <pc/thread/Thread.hpp>

#include <pc/balancer/priority.hpp>

#include <cstdlib>

#include <sys/sysinfo.h>

void pollCallback(pollfd const& poll, pc::network::ClientInfo& clientInfo)
{
   //   std::cout << "Poll called type"
   //             << " revents: " << poll.revents << "\n"
   //             << ((poll.revents & POLLIN) ? "POLLIN\n" : "")
   //             << ((poll.revents & POLLPRI) ? "POLLPRI\n" : "")
   //             << ((poll.revents & POLLHUP) ? "POLLHUP\n" : "")
   //             << ((poll.revents & POLLERR) ? "POLLERR\n" : "")
   //             << ((poll.revents & POLLNVAL) ? "POLLNVAL\n" : "");

   // std::cout << "Poll called " << poll.fd << "\n";
   if (!(poll.revents & POLLIN))
      return;
   pc::memory::unique_arr<char> data(1000);
   pc::network::TCP::recvRaw(poll.fd, data.get(), data.size);

   if (clientInfo.hasClientId())
   {
      if (strncmp(data.get(), "DEAD-INC", 8) == 0)
         clientInfo.deadline.increment();
      // std::cout << "\nReceived data " << data.get();
      std::string message = "Server says hi ";
      pc::network::TCP::sendRaw(poll.fd, (const char*)message.data(), message.size());
   }
   else
   {
      if (strncmp(data.get(), "ACK-ACK", 7) != 0)
      {
         throw std::runtime_error("ACK-ACK not received. Protocol violated");
      }
      std::string message = "ACK-SYN";
      pc::network::TCP::sendRaw(poll.fd, (const char*)message.data(), message.size());
      pc::network::TCP::recvRaw(poll.fd, data.get(), data.size);
      message = "JOIN";
      pc::network::TCP::sendRaw(poll.fd, (const char*)message.data(), message.size());
      clientInfo.clientId = std::string(data.get());
      std::cout << "\nNew Client ID joined " << clientInfo.clientId;
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
      polls[currentBalance].Add(child.socket, pollCallback);
      std::cout << std::endl
                << "Connected to " << child.socket << " on " << currentBalance
                << " thread : Balancer" << balancer;
      child.invalidate();
   }
   return EXIT_SUCCESS;
}
