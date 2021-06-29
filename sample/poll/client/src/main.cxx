#include <iostream>

#include <pc/network/TCP.hpp>
#include <pc/network/ip.hpp>

#include <thread>

int main()
{
   pc::network::IP ip(SOCK_STREAM);
   ip.load("127.0.0.1", "9900");

   std::string ipstr = ip;
   std::cout << "IP = " << ipstr;
   std::cout << "\nHostname = " << pc::network::IP::hostName();
   pc::network::TCP tcp{ip.connect()};
   for (std::size_t i = 0; true; i++)
   {
      // {
      //    auto recv = tcp.recv(1000);
      //    if (!recv)
      //       // Gracefull disconnection
      //       break;
      //    recv[1000 - 1] = '\0';
      //    std::cout << "\nServer said : " << (const char*) recv.get();
      // }
      // std::cout << "\nSay: ";
      std::string message = "Message " + std::to_string(i);
      // if (std::getline(std::cin, message))
      // {
      tcp.send((const std::uint8_t*)message.data(), message.size());
      std::cout << "\nMessage sent";
      auto data = tcp.recv(1000);
      if (!data)
      {
         std::cout << "\nData not found";
         break;
      }
      std::cout << "\nServer says: " << (const char*)data.get();

      std::this_thread::sleep_for(std::chrono::seconds(3));

      // }
   }
   return EXIT_SUCCESS;
}
