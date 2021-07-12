#include <iostream>

#include <pc/network/TCP.hpp>
#include <pc/network/types.hpp>
#include <pc/network/ip.hpp>

#include <cstdlib>

int main()
{
   pc::network::IP ip(SOCK_STREAM);
   ip.load("127.0.0.1", "9900");

   std::string ipstr = ip;
   std::cout << "IP = " << ipstr;
   std::cout << "\nHostname = " << pc::network::IP::hostName();
   pc::network::TCP    tcp(ip.connect());
   pc::network::buffer recv(100);
   while (true)
   {
      pc::network::Result result = tcp.recv(recv);
      if (result.SocketClosed)
         break;
      std::cout << "\nServer said : " << recv.data();
      std::cout << "\nSay: ";
      std::string message;
      if (std::getline(std::cin, message))
      {
         tcp.send(message);
         std::cout << "\nMessage sent";
      }
   }
   return EXIT_SUCCESS;
}
