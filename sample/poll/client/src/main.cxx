#include <iostream>

#include <pc/network/TCP.hpp>
#include <pc/network/ip.hpp>

#include <cstdlib>

int main()
{
   pc::network::IP ip(SOCK_STREAM);
   ip.load("127.0.0.1", "9900");

   std::string ipstr = ip;
   std::cout << "IP = " << ipstr;
   std::cout << "\nHostname = " << pc::network::IP::hostName();
   pc::network::TCP tcp(ip.connect());
   for (std::size_t i = 0; true; i++)
   {
      // {
      //    pc::network::memory::unique_arr<char> recv = tcp.recv(1000);
      //    if (!recv)
      //       // Gracefull disconnection
      //       break;
      //    recv.get()[1000 - 1] = '\0';
      //    std::cout << "\nServer said : " << (const char*) recv.get();
      // }
      // std::cout << "\nSay: ";
      std::string message = "Message ";
      // if (std::getline(std::cin, message))
      // {
      tcp.send((const char*)message.data(), message.size());
      std::cout << "\nMessage sent";
      sleep(3);
         // pc::network::memory::unique_arr<char> recv(1000);
         // tcp.recv(recv.size, recv.get());
      // if (!recv)
      // {
      //    std::cout << "\nData not found";
      //    break;
      // }
      // std::cout << "\nServer says: " << (const char*)recv.get();
      // }
   }
   return EXIT_SUCCESS;
}
