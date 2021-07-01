#include <iostream>

#include <pc/network/TCP.hpp>
#include <pc/network/ip.hpp>

#include <pc/deadline.hpp>

#include <pc/memory/unique_ptr.hpp>

#include <cstdlib>
#include <ctime>

int main()
{
   pc::network::IP ip(SOCK_STREAM);
   ip.load("127.0.0.1", "9900");

   std::string ipstr = ip;
   std::cout << "IP = " << ipstr;
   pc::network::TCP tcp(ip.connect());
   std::cout << "\nHostname = " << pc::network::IP::hostName();

   pc::Deadline deadline(25 - 1);
   try
   {
      for (std::size_t i = 0; true; i++)
      {
         // {
         //    pc::memory::unique_arr<char> recv = tcp.recv(1000);
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
         std::cout << "\nMessage sending " << i;
         if (!deadline)
         {
            tcp.send((const char*)message.data(), message.size());
            ++deadline;
            pc::memory::unique_arr<char> recv(1000);
            tcp.recv(recv.size, recv.get());
            if (!recv)
            {
               std::cout << "\nData not found";
               break;
            }
            std::cout << "\nServer says: " << (const char*)recv.get();
         }
         else
         {
            std::cout << "\nMessage not sent";
         }
         usleep(8 * 1000 * 1000 / 25);
      }
   }
   catch (std::exception const&)
   {
   }
   return EXIT_SUCCESS;
}
