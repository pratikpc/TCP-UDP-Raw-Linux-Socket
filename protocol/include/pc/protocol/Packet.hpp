#pragma once

#include <cassert>
#include <climits>
#include <pc/network/TCPPoll.hpp>
#include <pc/protocol/Commands.hpp>

namespace pc
{
   namespace protocol
   {
      template <std::size_t N>
      class RawPacket
      {
       public:
         // Command always of size 4
         std::string command;
         std::string data;

         static std::size_t const SizeBytes = N;

       private:
         std::size_t size() const
         {
            return command.size() + data.size();
         }
         RawPacket(network::buffer const& buffer, network::TCPResult recvData)
         {
            if (recvData.IsFailure())
            {
               if (recvData.PollFailure)
                  command = Commands::Empty;
               // Anything other than Poll Failure
               else
                  command = Commands::MajorErrors::SocketClosed;
               return;
            }
            command.resize(4);
            assert(recvData.NoOfBytes >= command.size());
            std::copy(buffer.begin(), buffer.begin() + command.size(), command.begin());
            if (recvData.NoOfBytes > command.size())
            {
               data.resize(recvData.NoOfBytes - command.size());
               std::copy(buffer.begin() + command.size(),
                         buffer.begin() + recvData.NoOfBytes,
                         data.begin());
            }
         }

       public:
         RawPacket(std::string const& command, std::string const& data = "") :
             command(command), data(data)
         {
         }
         static RawPacket<N>
             Read(::pollfd const poll, network::buffer& buffer, std::size_t const timeout)
         {
            return RawPacket<N>::Read(poll.fd, buffer, timeout);
         }
         static RawPacket<N>
             Read(int const socket, network::buffer& buffer, std::size_t const timeout)
         {
            network::TCPResult recvData =
                network::TCPPoll::readOnly(socket, buffer, N, timeout);
            if (!recvData.IsFailure())
            {
               // buffer[0] << 0 + buffer[1] << CHAR_BIT
               // Convert char array to integer
               std::size_t bytesToRead = 0;
               for (std::size_t i = 0; i < N; ++i)
                  bytesToRead |= (((unsigned char)buffer[i]) << (CHAR_BIT * i));
               assert(buffer.size() > bytesToRead);

               recvData =
                   network::TCPPoll::readOnly(socket, buffer, bytesToRead, timeout);
               if (!recvData.IsFailure())
               {
                  assert(recvData.NoOfBytes == bytesToRead);
               }
            }
            return RawPacket(buffer, recvData);
         }
         network::TCPResult Write(::pollfd poll, std::size_t timeout) const
         {
            return Write(poll.fd, timeout);
         }
         network::TCPResult Write(int const socket, std::size_t timeout) const
         {
            std::size_t const packetSize = size();
            // Convert packet to string array
            {
               // Convert size to buffer
               std::string sizeBuffer;
               sizeBuffer.resize(N);
               for (size_t i = 0; i < N; ++i)
               {
                  unsigned char value =
                      ((unsigned char)packetSize >> (CHAR_BIT * (i))) & UCHAR_MAX;
                  sizeBuffer[i] = value;
               }
               return network::TCPPoll::write(
                   socket, sizeBuffer + command + data, timeout);
            }
         }
      };
      template <std::size_t N>
      struct RawSendPacket : RawPacket<N>
      {
         RawSendPacket(std::string data) : RawPacket<N>(Commands::Send, data) {}
      };
   } // namespace protocol
} // namespace pc
