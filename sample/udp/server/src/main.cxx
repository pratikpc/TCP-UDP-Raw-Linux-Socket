#include <iostream>

#include <pc/memory/Buffer.hpp>
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
   pc::network::UDP    udp(ip.bind());
   pc::memory::Buffer<char> recv(100);
   while (1)
   {
      pc::network::Result result = udp.recv(recv);
      if (result.IsFailure())
         break;
      std::cout << "Client said : " << recv.data() << "\n";
   }
   return EXIT_SUCCESS;
}
