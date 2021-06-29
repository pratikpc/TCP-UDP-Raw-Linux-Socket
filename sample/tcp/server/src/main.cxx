#include <cstdlib>
#include <iostream>

#include <pc/network/TCP.hpp>
#include <pc/network/ip.hpp>

#include <pc/network/Thread.hpp>

void* childSocketExec(void* arg)
{
   pc::network::TCP* child = (pc::network::TCP*)arg;
   std::cout << "Accepted " << child->socket << "\n";
   while (true)
   {
      {
         std::cout << "\nMessage: ";
         std::string message = "nokia tyre " + child->socket;
         //  if (std::getline(std::cin, message))
         //  {
         sleep(3);
         child->send((const char*)message.data(), message.size());
         //  }
         //     else
         //     {
         //        continue;
         //     }
      }
      {
         pc::network::memory::unique_arr<char> recv;
         child->recv(recv.size, recv.get());
         if (!recv)
            break;
         recv.get()[1000 - 1] = '\0';
         std::cout << "Client said : " << recv.get() << "\n";
      }
   }
   std::cout << "Server disconnected : " << child->socket << "\n";

   delete child;
   return NULL;
}

int main()
{
   pc::network::IP ip(SOCK_STREAM);
   ip.hints.ai_flags = AI_PASSIVE; // Use current IP
   ip.load("", "9900");

   std::string ipstr = ip;
   std::cout << "IP = " << ipstr;
   std::cout << "\n Hostname = " << ip.hostName();
   pc::network::TCP tcp(ip.bind());
   tcp.setReusable();
   tcp.listen();
   while (true)
   {
      std::cout << "Waiting for next value\n";
      pc::network::TCP* child = new pc::network::TCP(tcp.accept());
      if (child->invalid())
         continue;
      pc::network::Thread(&childSocketExec, child).detach();
   }
   return EXIT_SUCCESS;
}
