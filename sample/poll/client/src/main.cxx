#include <iostream>

#include <pc/network/TCP.hpp>
#include <pc/network/ip.hpp>

#include <pc/deadliner/deadline.hpp>

#include <cstdlib>
#include <ctime>
#include <sstream>

#include <pc/lexical_cast.hpp>
{
   pc::network::IP ip(SOCK_STREAM);
   ip.load("127.0.0.1", "9900");

   std::string ipstr = ip;
   std::cout << "IP = " << ipstr;
   pc::network::TCP tcp(ip.connect());
   std::cout << "\nHostname = " << pc::network::IP::hostName();

   pc::Deadline deadline(25 - 1);
   std::string  message = "ACK-ACK";
   tcp.send((const char*)message.data(), message.size());
   std::vector<char> recv = tcp.recv(1000);
   if (strncmp(recv.data(), "ACK-SYN", 7) != 0)
   {
      throw std::runtime_error("ACK-SYN not received. Protocol violated");
   }
   message = "CLIENT-" + pc::lexical_cast(i);
   tcp.send((const char*)message.data(), message.size());
   recv = tcp.recv(1000);
   if (strncmp(recv.data(), "JOIN", 4) != 0)
   {
      throw std::runtime_error("JOIN not received. Protocol violated");
   }
   std::cout << "\nJoined Server as " << message;

   for (std::size_t i = 0; true; i++)
   {
      // {
      // recv = tcp.recv(1000);
      //    if (!recv)
      //       // Gracefull disconnection
      //       break;
      //    recv.get()[1000 - 1] = '\0';
      //    std::cout << "\nServer said : " << (const char*) recv.get();
      // }
      // std::cout << "\nSay: ";
      message = "ALIVE-ALIVE";
      // if (std::getline(std::cin, message))
      // {
      if (!deadline)
      {
         std::cout << std::endl << "Message sending " << i;
         tcp.send((const char*)message.data(), message.size());
         recv = tcp.recv(1000);
         if (recv.empty())
         {
            std::cout << "\nData not found";
            break;
         }
         std::cout << "\nServer says: " << recv.size() << " : "
                   << (const char*)recv.data();
         ++deadline;
      }
      else
      {
         std::cout << "\nMessage not sent";
      }
      std::cout << std::endl << "Sleep start " << i;
      usleep(8 * 1000 * 1000 / 25);
      std::cout << std::endl << "Sleep end " << i;
   }
   return EXIT_SUCCESS;
}
