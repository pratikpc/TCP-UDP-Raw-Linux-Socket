#include <iostream>

#include <pc/network/ip.hpp>
#include <pc/network/tcp/TCP.hpp>

#include <thread>

int main()
{
   pc::network::IP ip(SOCK_STREAM);
   ip.load("127.0.0.1", "9900");

   std::string ipstr = ip;
   std::cout << "IP = " << ipstr;
   std::cout << "\nHostname = " << pc::network::IP::hostName();
   pc::network::TCP tcp{ip.connect()};
   while (true)
   {
      {
         char* output = (char*)tcp.recv(1000);
         if (output == nullptr)
            break;
         output[1000 - 1] = '\0';
         std::cout << "Server said : " << output << "\n";
      }
      std::string message;
      if (std::getline(std::cin, message))
      {
         tcp.send((const void*)message.data(), message.size());
         std::cout << "Message sent\n";
      }
   }
   return EXIT_SUCCESS;
}
