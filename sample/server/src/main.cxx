#include <iostream>

#include <pc/network/ip.hpp>
#include <pc/network/tcp/TCP.hpp>

#include <thread>

int main()
{
   pc::network::IP ip(SOCK_STREAM);
   ip.hints.ai_flags = AI_PASSIVE; // Use current IP
   ip.load("", "9900");

   std::string ipstr = ip;
   std::cout << "Hey "
             << " IP = " << ipstr;
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
          [child=std::move(child)]()
          {
             std::cout << "Accepted\n";
             std::string sendStr("Hiii brother");

             //  std::this_thread::sleep_for(std::chrono::seconds(10));
             std::cout << "Sent\n";
             std::cout << "Bytes sent : "
                       << child.send((void*)sendStr.data(), sendStr.size());
             std::cout << " \nSupposed to send " << sendStr.size() << "\n";
             std::cout << "Recv Child socket " << child.socket << "\n";
          })
          .detach();
   }
   return EXIT_SUCCESS;
}
