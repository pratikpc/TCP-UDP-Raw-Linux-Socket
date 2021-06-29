#include <iostream>

#include <pc/network/TCP.hpp>
#include <pc/network/TCPPoll.hpp>
#include <pc/network/ip.hpp>

#include <thread>

void pollCallback(pollfd const& poll)
{
   //   std::cout << "Poll called type"
   //             << " revents: " << poll.revents << "\n"
   //             << ((poll.revents & POLLIN) ? "POLLIN\n" : "")
   //             << ((poll.revents & POLLPRI) ? "POLLPRI\n" : "")
   //             << ((poll.revents & POLLHUP) ? "POLLHUP\n" : "")
   //             << ((poll.revents & POLLERR) ? "POLLERR\n" : "")
   //             << ((poll.revents & POLLNVAL) ? "POLLNVAL\n" : "");

   std::cout << "Poll called " << poll.fd << "\n";
   if (poll.revents & POLLIN)
   {
      auto data = pc::network::TCP::recvRaw(poll.fd, 1000);
      if (!data)
      {
         std::cout << "Data not found\n";
         close(poll.fd);
         return;
      }
      std::cout << "Received data " << (const char*)data.get() << "\n";
      std::string message = "Server says hi to " + std::to_string(poll.fd);
      pc::network::TCP::sendRaw(
          poll.fd, (const std::uint8_t*)message.data(), message.size());
   }
}

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
   pc::network::TCPPoll poll;

   std::thread(
       [&poll]()
       {
          while (true)
          {
             std::cout << "Exec poll start\n";
             poll.exec(std::chrono::seconds(5));
             std::cout << "Exec poll end\n";
          }
       })
       .detach();

   while (true)
   {
      pc::network::TCP child = tcp.accept();
      if (child.invalid())
         continue;
      poll.PollThis(child.socket, pollCallback);
      std::cout << "Connected to " << child.socket << "\n";
      child.invalidate();
   }
   return EXIT_SUCCESS;
}
