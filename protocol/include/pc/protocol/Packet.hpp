#pragma once

#include <cassert>
#include <climits>
#include <pc/network/TCPPoll.hpp>
#include <pc/protocol/Commands.hpp>

namespace pc
{
   namespace protocol
   {
      namespace
      {
         template <std::size_t N>
         network::TCPResult countPacketBytesToRead(pollfd&           poll,
                                                  network::buffer&  buffer,
                                                  std::size_t const timeout)
         {
            std::size_t       bytesToRead = 0;
            network::TCPResult recvData =
                network::TCPPoll::readOnly(poll, buffer, N, timeout);
            if (recvData.IsFailure())
               return recvData;

            // buffer[0] << 0 + buffer[1] << CHAR_BIT
            // Convert char array to integer
            for (std::size_t i = 0; i < N; ++i)
               bytesToRead |= (((std::size_t)buffer[i]) << (CHAR_BIT * i));
            assert(buffer.size() > bytesToRead);

            recvData = network::TCPPoll::readOnly(poll, buffer, bytesToRead, timeout);
            assert(recvData.NoOfBytes == bytesToRead);
            return recvData;
         }
      } // namespace
      template <std::size_t N>
      class RawPacket
      {
       public:
         // Command always of size 4
         std::string command;
         std::string data;

         static std::size_t const SizeBytes = N;

       private:
         operator std::size_t() const
         {
            return command.size() + data.size();
         }
         RawPacket(network::buffer const& buffer, network::TCPResult recvData)
         {
            if (recvData.IsFailure())
            {
               // If polling failed
               if (recvData.PollFailure)
               {
                  command = Commands::Empty;
                  return;
               }
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
             Read(pollfd poll, network::buffer& buffer, std::size_t const timeout)
         {
            network::TCPResult const recvData =
                countPacketBytesToRead<N>(poll, buffer, timeout);
            return RawPacket(buffer, recvData);
         }

         void Write(pollfd poll, std::size_t timeout) const
         {
            std::size_t const packetSize = *this;
            // Convert packet to string array
            {
               // Convert size to buffer
               std::string sizeBuffer;
               sizeBuffer.resize(N);
               for (size_t i = 0; i < N; ++i)
               {
                  unsigned char value =
                      ((std::size_t)packetSize >> (CHAR_BIT * (i))) & UCHAR_MAX;
                  sizeBuffer[i] = value;
               }
               network::TCPPoll::write(poll, sizeBuffer + command + data, timeout);
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
