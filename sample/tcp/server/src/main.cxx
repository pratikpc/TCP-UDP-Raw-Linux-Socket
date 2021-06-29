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
   while (1)
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
             for (int i = 0;; ++i)
             {
                {
                   std::string message;
                   if (std::getline(std::cin, message))
                   {
                      child.send((const void*)message.data(), message.size());
                      std::cout << "Message sent\n";
                   }
                   else
                   {
                      continue;
                   }
                }
                {
                   auto  recv   = child.recv(1000);
                   char* output = (char*)recv.get();
                   if (output == nullptr)
                      break;
                   output[1000 - 1] = '\0';
                   std::cout << "Client said : " << output << "\n";
                }
             }
          })
          .detach();
   }
   return EXIT_SUCCESS;
}
