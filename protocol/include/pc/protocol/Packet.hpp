#pragma once

#include <cassert>
#include <climits>
#include <pc/network/TCPPoll.hpp>
#include <pc/protocol/Commands.hpp>

#ifdef PC_PROFILE
#   include <pc/timer/timer.hpp>
#endif

#include <pc/stl/numeric.hpp>

namespace pc
{
   namespace protocol
   {
      numeric::Integral<2>::type NetworkToHost(numeric::Integral<2>::type value)
      {
         return ntohs(value);
      }
      numeric::Integral<4>::type NetworkToHost(numeric::Integral<4>::type value)
      {
         return ntohl(value);
      }
      numeric::Integral<2>::type HostToNetwork(numeric::Integral<2>::type value)
      {
         return htons(value);
      }
      numeric::Integral<4>::type HostToNetwork(numeric::Integral<4>::type value)
      {
         return htonl(value);
      }

      template <std::size_t N>
      class RawPacket
      {
         typedef typename numeric::Integral<N>::type PacketSize;

       public:
         // Command always of size 4
         std::string command;
         std::string data;
#ifdef PC_PROFILE
       public:
         mutable timespec readTimeDiff;
         mutable timespec intraProcessingTimeStart;
         mutable timespec intraProcessingTimeDiff;
         mutable timespec writeTimeDiff;
         mutable timespec executeTimeDiff;
         mutable timespec bufferCopyTimeDiff;
#endif
         static std::size_t const SizeBytes = N;

       private:
         PacketSize size() const
         {
            return command.size() + data.size();
         }
         template <typename Buffer>
         RawPacket(Buffer const&   buffer,
                   network::Result recvData
#ifdef PC_PROFILE
                   ,
                   timespec const& readTimeDiff
#endif
                   )
#ifdef PC_PROFILE
             :
             readTimeDiff(readTimeDiff)
#endif
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
#ifdef PC_PROFILE
            timespec const bufferCopyTimeStart = timer::now();
#endif
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
#ifdef PC_PROFILE
            timespec nowTime         = timer::now();
            intraProcessingTimeStart = nowTime;
            bufferCopyTimeDiff       = nowTime - bufferCopyTimeStart;
#endif
         }

       public:
         RawPacket(std::string const& command, std::string const& data = "") :
             command(command), data(data)
#ifdef PC_PROFILE
             ,
             readTimeDiff(), intraProcessingTimeStart(), intraProcessingTimeDiff(),
             writeTimeDiff(), executeTimeDiff(), bufferCopyTimeDiff()
#endif
         {
         }
         template <typename Buffer>
         static RawPacket<N>
             Read(::pollfd const poll, Buffer& buffer, std::size_t const timeout)
         {
            return RawPacket<N>::Read(poll.fd, buffer, timeout);
         }
         template <typename Buffer>
         static RawPacket<N>
             Read(int const socket, Buffer& buffer, std::size_t const timeout)
         {
            network::Result recvData =
                network::TCPPoll::recvFixedBytes(socket, buffer, N, timeout);
#ifdef PC_PROFILE
            timespec readTimeDiff = recvData.duration;
#endif
            if (recvData.IsSuccess())
            {
               // buffer[0] << 0 + buffer[1] << CHAR_BIT
               // Convert char array to integer
#ifndef PC_NETWORK_MOCK
               PacketSize bytesToRead = 0;
               for (std::size_t i = 0; i < N; ++i)
                  bytesToRead |= (((unsigned char)buffer[i]) << (CHAR_BIT * i));
               bytesToRead = NetworkToHost(bytesToRead);
#else
               // Assume buffer contains 20 bytes at least
               // When mocking
               PacketSize bytesToRead = 20;
#endif
               assert(buffer.size() > bytesToRead);
               recvData =
                   network::TCPPoll::recvFixedBytes(socket, buffer, bytesToRead, timeout);
#ifdef PC_PROFILE
               readTimeDiff = readTimeDiff + recvData.duration;
#endif
               if (recvData.IsSuccess())
               {
                  assert(recvData.NoOfBytes == bytesToRead);
               }
            }
            return RawPacket(buffer,
                             recvData
#ifdef PC_PROFILE
                             ,
                             readTimeDiff
#endif
            );
         }
         network::Result Write(::pollfd poll, std::size_t timeout) const
         {
            return Write(poll.fd, timeout);
         }

         std::string Marshall() const
         {
            PacketSize const packetSize = HostToNetwork(size());
            // Convert packet to string array
            // Convert size to buffer
            std::string sizeBuffer;
            sizeBuffer.resize(N);
            for (size_t i = 0; i < N; ++i)
            {
               unsigned char value =
                   ((unsigned char)((packetSize >> (CHAR_BIT * i))) & UCHAR_MAX);
               sizeBuffer[i] = value;
            }
            return sizeBuffer + command + data;
         }
         network::Result Write(int const socket, std::size_t timeout) const
         {
#ifdef PC_PROFILE
            intraProcessingTimeDiff =
                timer::now() - intraProcessingTimeStart - executeTimeDiff;
#endif
            network::Result const result =
                network::TCPPoll::write(socket, Marshall(), timeout);
#ifdef PC_PROFILE
            writeTimeDiff = result.duration;
#endif
            return result;
         }
      };
      template <std::size_t N>
      struct RawSendPacket : RawPacket<N>
      {
         RawSendPacket(std::string data) : RawPacket<N>(Commands::Send, data) {}
      };
   } // namespace protocol
} // namespace pc
