#include <iostream>

#include <pc/network/UDP.hpp>
#include <pc/network/ip.hpp>

#include <thread>

int main()
{
   pc::network::IP ip(SOCK_DGRAM);
   ip.hints.ai_flags = AI_PASSIVE; // Use current IP
   ip.load("", "9900");

   std::string ipstr = ip;
   std::cout << "IP = " << ipstr;
   std::cout << "\n Hostname = " << ip.hostName();
   pc::network::UDP udp{ip.bind()};

   while (1)
   {
      auto  outputPtr = udp.recv(1000).first;

      char* output    = (char*)outputPtr.get();
      if (output == nullptr)
         break;
      output[1000 - 1] = '\0';
      std::cout << "Client said : " << output << "\n";
   }
   return EXIT_SUCCESS;
}
