#include <cstdlib>
#include <iostream>

#include <pc/network/TCP.hpp>
#include <pc/network/ip.hpp>

#include <pc/thread/Thread.hpp>

void* childSocketExec(void* arg)
{
   pc::network::TCP* child = (pc::network::TCP*)arg;
   std::cout << "Accepted " << child->socket << "\n";
   while (true)
   {
      {
         std::cout << "\nMessage: ";
         //  if (std::getline(std::cin, message))
         //  {
         sleep(3);
         child->send("nokia tyre");
         //  }
         //     else
         //     {
         //        continue;
         //     }
      }
      {
         pc::network::buffer recv = child->recv(1000);
         if (recv.empty())
            break;
         recv[1000 - 1] = '\0';
         std::cout << "Client said : " << recv.data() << "\n";
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
      pc::threads::Thread(&childSocketExec, child).detach();
   }
   return EXIT_SUCCESS;
}
