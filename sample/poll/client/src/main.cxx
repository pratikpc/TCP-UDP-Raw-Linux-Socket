#include <iostream>

#include <pc/network/TCP.hpp>
#include <pc/network/ip.hpp>

#include <pc/deadliner/deadline.hpp>

#include <cstdlib>
#include <ctime>
#include <sstream>

#include <pc/lexical_cast.hpp>

void* func(void* clientIndex)
{
   pc::network::IP ip(SOCK_STREAM);
   ip.load("127.0.0.1", "9900");

   std::string ipstr = ip;
   std::cout << "\nIP = " << ipstr;
   pc::network::TCP tcp(ip.connect());
   std::cout << "\nHostname = " << pc::network::IP::hostName();
   tcp.keepAlive();
   pc::Deadline deadline(25 - 1);
   tcp.send("ACK-ACK");
   pc::network::buffer recv = tcp.recv(8);
   if (strncmp(recv.data(), "ACK-SYN", 7) != 0)
   {
      throw std::runtime_error("ACK-SYN not received. Protocol violated");
   }
   tcp.send("CLIENT-" + pc::lexical_cast(*((int*)clientIndex)));
   recv = tcp.recv(1000);
   if (strncmp(recv.data(), "JOIN", 4) != 0)
   {
      throw std::runtime_error("JOIN not received. Protocol violated");
   }
   std::cout << "\nJoined Server as "
             << "CLIENT-" << pc::lexical_cast(*((int*)clientIndex));

   for (std::size_t i = 0; true; i++)
   {
      if (!deadline)
      {
         std::cout << std::endl
                   << "Message sending " << i << " at CLIENT-" << *((int*)clientIndex);
         tcp.send("ALIVE-ALIVE");
         pc::network::buffer recv = tcp.recv(100);
         if (recv.empty())
         {
            std::cout << "\nData not found";
            break;
         }
         std::cout << std::endl
                   << "Server says: " << recv.size() << " : " << (const char*)recv.data()
                   << " at CLIENT-" << *((int*)clientIndex);
         ++deadline;
      }
      else
      {
         std::cout << "\nMessage not sent";
      }
      // usleep(10 * 1000 * 1000 / 25);
      sleep(32);
   }
   return NULL;
}

#include <pc/thread/Thread.hpp>
int main()
{
   std::vector<int> count(5);
   for (std::size_t i = 0; i < count.size(); ++i)
   {
      count[i] = i;
      pc::threads::Thread(func, &count[i]).detach();
   }
   std::cout << "Hi\n";
   for (std::size_t i = 0; i < 100; ++i)
   {
      sleep(UINT32_MAX);
   }
}