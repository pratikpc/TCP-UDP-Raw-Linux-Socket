#include <iostream>

#include <pc/network/TCP.hpp>
#include <pc/network/TCPPoll.hpp>
#include <pc/network/ip.hpp>

#include <pc/memory/unique_ptr.hpp>

#include <pc/thread/Thread.hpp>

#include <pc/balancer/priority.hpp>

#include <cstdlib>

#include <sys/sysinfo.h>

#include <pc/pqpp/Connection.hpp>

struct Config
{
   pc::pqpp::Connection connection;
   Config(std::string connectionString) : connection(connectionString) {}
};

void pollCallback(pollfd const&            poll,
                  pc::network::ClientInfo& clientInfo,
                  void*                    configParam)
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
         clientInfo.deadline.incrementMaxCount();
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
      {
         Config* config = (Config*)configParam;

         std::vector<const char*> params(2);
         params[0] = clientInfo.clientId.c_str();
         params[1] = "45"; // DEFAULT

         pc::pqpp::IterateResult res = config->connection.iterate(
             "SELECT coalesce(MAX(priority),$2) AS priority FROM priority_table WHERE "
             "clientId=$1",
             params);
         if (!res)
         {
            std::cerr << "\nUnable to connect to database";
            return;
         }
         std::ptrdiff_t newDeadlineMaxCount = std::atoll(res[0].c_str());
         clientInfo.changeMaxCount(newDeadlineMaxCount);
         std::cout << "\nNew deadlien count for "<< clientInfo.clientId << " is " << newDeadlineMaxCount;
      }
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

   Config config("postgresql://postgres@localhost:5432/");
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

   for (std::vector<pc::network::TCPPoll<> /* */>::iterator it = polls.begin();
        it != polls.end();
        ++it)
   {
      it->balancerIndex  = (it - polls.begin());
      it->downCallback   = &downCallback;
      it->balancer       = &balancer;
      it->callbackConfig = &config;
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
