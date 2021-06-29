#include <iostream>

#include <pc/network/TCP.hpp>
#include <pc/network/ip.hpp>

#include <thread>

int main()
{
   pc::network::IP ip(SOCK_STREAM);
   ip.hints.ai_flags = AI_PASSIVE; // Use current IP
   ip.load("", "9900");

   std::string ipstr = ip;
   std::cout << "IP = " << ipstr;
   std::cout << "\n Hostname = " << ip.hostName();
   pc::network::TCP tcp{ip.bind()};
   tcp.setReusable();
   tcp.listen();
   while (true)
   {
      std::cout << "Waiting for next value\n";
      pc::network::TCP child = tcp.accept();
      if (child.invalid())
         break;
      std::cout << "Child socket " << child.socket << "\n";
      std::thread(
          [child = std::move(child)]()
          {
             std::cout << "Accepted\n";
             while (true)
             {
                {
                   std::cout << "\nMessage: ";
                   std::string message;
                   if (std::getline(std::cin, message))
                   {
                      child.send((const void*)message.data(), message.size());
                   }
                   else
                   {
                      continue;
                   }
                }
                {
                   auto recv = child.recv(1000);
                   if (!recv)
                      break;
                   recv[1000 - 1] = '\0';
                   std::cout << "Client said : " << recv.get() << "\n";
                }
             }
             std::cout << "Server disconnected\n";
          })
          .detach();
   }
   return EXIT_SUCCESS;
}
