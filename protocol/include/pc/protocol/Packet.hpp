#pragma once

#include <cassert>
#include <climits>
#include <pc/network/TCPPoll.hpp>
#include <pc/protocol/Commands.hpp>

#ifdef PC_PROFILE
#   include <pc/timer/timer.hpp>
#endif

#include <pc/stl/numeric.hpp>

#if defined(PC_NETWORK_MOCK)
#   ifndef PC_IGNORE
#      define PC_IGNORE(x) (void)x
#   endif
#endif

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

       public:
         template <typename Buffer>
         RawPacket(Buffer const&    buffer,
                   PacketSize const bytesToRead
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
            command.resize(4);
            this->ExtractDataFromBuffer(buffer, bytesToRead);
#ifdef PC_PROFILE
            intraProcessingTimeStart = timer::now();
#endif
         }
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
         static PacketSize ExtractPacketSizeFromBuffer(Buffer const& buffer)
         {
// buffer[0] << 0 + buffer[1] << CHAR_BIT
// Convert char array to integer
#ifndef PC_NETWORK_MOCK
            PacketSize bytesToRead = 0;
            for (std::size_t i = 0; i < N; ++i)
               bytesToRead |= (((unsigned char)buffer[i]) << (CHAR_BIT * i));
            bytesToRead = NetworkToHost(bytesToRead);
            return bytesToRead;
#else
            PC_IGNORE(buffer);
            return 20;
#endif
         }
         template <typename Buffer>
         void ExtractDataFromBuffer(Buffer const& buffer, PacketSize const bytesToRead)
         {
#ifdef PC_PROFILE
            timespec const bufferCopyTimeStart = timer::now();
#endif
            assert(bytesToRead >= (PacketSize)command.size());
            std::copy(buffer.begin(), buffer.begin() + command.size(), command.begin());
            if (bytesToRead > (PacketSize)command.size())
            {
               data.resize(bytesToRead - command.size());
               std::copy(buffer.begin() + command.size(),
                         buffer.begin() + bytesToRead,
                         data.begin());
            }
#ifdef PC_PROFILE
            bufferCopyTimeDiff = timer::now() - bufferCopyTimeStart;
#endif
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
            if (recvData.IsFailure())
            {
               if (recvData.PollFailure)
                  return RawPacket(Commands::Empty);
               // Anything other than Poll Failure
               else
                  return RawPacket(Commands::MajorErrors::SocketClosed);
            }
            PacketSize const bytesToRead =
                RawPacket<N>::ExtractPacketSizeFromBuffer(buffer);
            assert((PacketSize)buffer.size() > bytesToRead);
            recvData =
                network::TCPPoll::recvFixedBytes(socket, buffer, bytesToRead, timeout);
#ifdef PC_PROFILE
            readTimeDiff = readTimeDiff + recvData.duration;
#endif
            if (recvData.IsFailure())
            {
               if (recvData.PollFailure)
                  return RawPacket(Commands::Empty);
               // Anything other than Poll Failure
               else
                  return RawPacket(Commands::MajorErrors::SocketClosed);
            }
            assert((PacketSize)recvData.NoOfBytes == bytesToRead);
            return RawPacket(buffer,
                             bytesToRead
#ifdef PC_PROFILE
                             ,
                             readTimeDiff
#endif
            );
         }
         network::Result Write(::pollfd const& poll, std::size_t timeout) const
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

#undef PC_IGNORE
