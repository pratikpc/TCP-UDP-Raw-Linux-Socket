#include <iostream>

#include <pc/network/ip.hpp>
#include <pc/network/tcp/TCP.hpp>

#include <thread>

int main()
{
   pc::network::IP ip(SOCK_STREAM);
   ip.load("127.0.0.1", "9900");

   std::string ipstr = ip;
   std::cout << "Hey "
             << " IP = " << ipstr;
   std::cout << "\n Hostname = " << ip.hostName();
   pc::network::TCP tcp{ip.connect()};

   // while (true)
   // {
   char* output = (char*)tcp.recv(1000);
   // if (output == nullptr)
   // {
   //    std::this_thread::sleep_for(std::chrono::seconds(1));
   //    continue;
   // }
   if (output != nullptr)
      output[1000 - 1] = '\0';
   std::cout << "\n" << output << "recv";
   // break;
   // }
   return EXIT_SUCCESS;
}
