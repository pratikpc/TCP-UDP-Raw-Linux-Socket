#include <iostream>

#include <pc/network/UDP.hpp>
#include <pc/network/ip.hpp>

#include <cstdlib>

int main()
{
   pc::network::IP ip(SOCK_DGRAM);
   ip.hints.ai_flags = AI_PASSIVE; // Use current IP
   ip.load("", "9900");

   std::string ipstr = ip;
   std::cout << "IP = " << ipstr;
   std::cout << "\n Hostname = " << ip.hostName();
   pc::network::UDP udp(ip.bind());

   while (1)
   {
      pc::network::memory::unique_arr<char> recv;
      udp.recv(recv.size, recv.get());
      if (!recv)
         break;
      recv.get()[1000 - 1] = '\0';
      std::cout << "Client said : " << recv.get() << "\n";
   }
   return EXIT_SUCCESS;
}
