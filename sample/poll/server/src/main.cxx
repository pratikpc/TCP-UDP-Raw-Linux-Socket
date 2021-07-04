#include <cstdlib>
#include <iostream>
#include <sstream>

#include <pc/network/TCP.hpp>
#include <pc/network/ip.hpp>
#include <pc/protocol/LearnProtocol.hpp>

#include <pc/thread/Thread.hpp>

#include <pc/balancer/priority.hpp>

#include <sys/sysinfo.h>

std::string repeat(std::string value, std::size_t times)
{
   std::ostringstream stream;
   for (std::size_t i = 0; i < times; ++i)
      stream << value;
   return stream.str();
}

void pollCallback(pollfd const& poll, pc::protocol::ClientInfo& clientInfo)
{
   //   std::cout << "\nPoll called type"
   //             << " revents: " << poll.revents << "\n"
   //             << ((poll.revents & POLLIN) ? "POLLIN\n" : "")
   //             << ((poll.revents & POLLPRI) ? "POLLPRI\n" : "")
   //             << ((poll.revents & POLLHUP) ? "POLLHUP\n" : "")
   //             << ((poll.revents & POLLERR) ? "POLLERR\n" : "")
   //             << ((poll.revents & POLLNVAL) ? "POLLNVAL\n" : "");

   if (!(poll.revents & POLLIN))
      return;
   pc::network::buffer data(1000);
   pc::network::TCP::recv(poll.fd, data);
   if (!data)
   {
      return;
   }

   if (strncmp(data->data(), "DEAD-INC", 8) == 0)
      clientInfo.deadline.incrementMaxCount();
   const std::string message = repeat("Server says hi " + clientInfo.clientId, 1);
   pc::network::TCP::sendRaw(poll.fd, message);
}

void* execTcp(void* arg)
{
   pc::protocol::LearnProtocol& poll = *((pc::protocol::LearnProtocol*)arg);
   while (true)
   {
      poll.pollExec();
      poll.execHealthChecks();
   }
   return NULL;
}

void downCallback(std::size_t const idx)
{
   std::cout << std::endl << "One client went down at " << idx << " balancer";
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
   std::vector<pc::protocol::LearnProtocol> protocols(get_nprocs());

   pc::balancer::priority balancer(protocols.size());

   pc::protocol::Config config("postgresql://postgres@localhost:5432/");
   config.downCallback = &downCallback;
   config.balancer     = &balancer;

   {
      pc::pqpp::Result res = config.connection.exec(
          "CREATE TABLE IF NOT EXISTS priority_table ("
          "clientId varchar(45) NOT NULL,"
          "priority smallint NOT NULL,"
          "PRIMARY KEY(clientId)) ");
      if (!res)
      {
         std::cerr << "Failed to create table\n";
         return EXIT_FAILURE;
      }
      std::cout << "\nTable Created";
   }

   for (std::vector<pc::protocol::LearnProtocol>::iterator it = protocols.begin();
        it != protocols.end();
        ++it)
   {
      it->config        = &config;
      it->balancerIndex = (it - protocols.begin());
      it->timeout       = 10;
      pc::threads::Thread(&execTcp, &(*it)).detach();
   }

   while (true)
   {
      std::cout << std::endl << "Try to connect";
      pc::network::TCP child(tcp.accept());
      if (child.invalid())
         continue;
      child.keepAlive();
      std::size_t currentBalance = *balancer;
      protocols[currentBalance].Add(child.socket, pollCallback);
      std::cout << std::endl
                << "Connected to " << child.socket << " on " << currentBalance
                << " thread : Balancer" << balancer;
      child.invalidate();
   }
   return EXIT_SUCCESS;
}
