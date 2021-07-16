#pragma once

#include <list>
#include <pc/deadliner/Deadline.hpp>
#include <pc/protocol/ClientPollResult.hpp>
#include <pc/protocol/Packet.hpp>
#include <pc/protocol/types.hpp>
#include <pc/thread/Atomic.hpp>
#include <string>

#ifdef PC_PROFILE
#   include <pc/timer/timer.hpp>
#endif

#ifdef PC_PROFILE
#   include <pc/opt/Averager.hpp>
#endif

#ifndef PC_USE_SPINLOCKS
#   include <pc/thread/Mutex.hpp>
#   include <pc/thread/MutexGuard.hpp>
#else
#   include <pc/thread/spin/SpinGuard.hpp>
#   include <pc/thread/spin/SpinLock.hpp>
#endif

namespace pc
{
   namespace protocol
   {
      class ClientInfo
      {
         typedef std::list<NetworkPacket> PacketList;
#ifdef PC_PROFILE
         typedef pc::opt::Averager<double> Averager;
#endif
         typedef pc::threads::Atomic<bool> AtomicBool;

         int socket;

       public:
         std::string clientId;

       private:
         pc::deadliner::Deadline deadline;
         ClientResponseCallback& callback;
         AtomicBool              terminateOnNextCycle;
         AtomicBool              terminateNow;
         PacketList              packetsToRead;
#ifndef PC_USE_SPINLOCKS
         mutable threads::Mutex      readMutex;
         mutable threads::Mutex      writeMutex;
         typedef threads::MutexGuard LockGuard;
#else
         mutable threads::SpinLock  readMutex;
         mutable threads::SpinLock  writeMutex;
         typedef threads::SpinGuard LockGuard;
#endif
         PacketList packetsToWrite;

#ifdef PC_PROFILE
         Averager averageIntraProcessingTime;
         Averager averageReadTime;
         Averager averageWriteTime;
         Averager averageExecuteTime;
         Averager averageBufferCopyTime;
         Averager averageAccumulatedTime;
         Averager averageSingleReadIterTime;

#   ifdef PC_OPTIMIZE_READ_MULTIPLE_SINGLE_SHOT
         Averager averageReadCountPerIt;
#   endif
#endif

       public:
         ClientInfo(int const               socket,
                    ClientResponseCallback& callback,
                    std::size_t const DeadlineMaxCount = DEADLINE_MAX_COUNT_DEFAULT) :
             socket(socket),
             deadline(DeadlineMaxCount), callback(callback), terminateOnNextCycle(),
             terminateNow()
#ifdef PC_PROFILE
             ,
             averageIntraProcessingTime(), averageReadTime(), averageWriteTime(),
             averageExecuteTime(), averageBufferCopyTime()
#endif
         {
            ++deadline;
         }

         void executeCallbacks()
         {
            PacketList tempList;
            {
               // Add to a temporary vector for writes
               LockGuard guard(readMutex);
               if (packetsToRead.empty())
                  return;
               tempList.splice(tempList.end(), packetsToRead);
            }
            // What we will do here
            // We will iterate over the read list
            // Then we will assign the value to write
            // Within this list itself as an optimization
            // To reduce copies
            // Then we will copy these values to the write List
            for (PacketList::iterator packetIt = tempList.begin();
                 packetIt != tempList.end();)
            {
               // At the start, packetIt will have the item to read
#ifdef PC_PROFILE
               timespec const readPacketTimeDiff = packetIt->readTimeDiff;
               timespec const readPacketIntraProcessingTimeStart =
                   packetIt->intraProcessingTimeStart;
               timespec const executeStart = timer::now();
#endif
               // Item to write added to packetIt itself
               // So the read array will now have the items to write
               *packetIt = callback(*packetIt, *this);
#ifdef PC_PROFILE
               packetIt->executeTimeDiff          = timer::now() - executeStart;
               packetIt->readTimeDiff             = readPacketTimeDiff;
               packetIt->intraProcessingTimeStart = readPacketIntraProcessingTimeStart;
#endif
               // We only write if send
               if (packetIt->command == Commands::Send)
                  ++packetIt;
               // If not send, remove this packet
               else
                  packetIt = tempList.erase(packetIt);
            }
            // If nothing available to write
            // Return
            // Remember that tempList now will contain
            // All output of callbacks
            // And hence it is now the write array
            if (tempList.empty())
               return;
            {
               // Add to write vector
               LockGuard guard(writeMutex);
               packetsToWrite.splice(packetsToWrite.end(), tempList);
            }
         }

         // TODO
         // {
         //    std::size_t               newDeadlineMaxCount =
         //        config->ExtractDeadlineMaxCountFromDatabase(clientInfo.clientId);
         //    // As the balancer element is common
         //    // and shared across protocols
         //    // Make the balancer updation guard
         //    // static
         //    static pc::threads::Mutex balancerPriorityUpdationMutex;
         //    LockGuard   guard(balancerPriorityUpdationMutex);
         //    config->balancer->setPriority(balancerIndex,
         //                                  // Update priority for given element
         //                                  (*config->balancer)[balancerIndex] -
         //                                      clientInfo.deadline.MaxCount() +
         //                                      newDeadlineMaxCount);
         //    clientInfo.changeMaxCount(newDeadlineMaxCount);
         // }
         //    return result;
         // }
         void Terminate()
         {
            if (socket != -1)
            {
               terminateNow = true;
               ::shutdown(socket, SHUT_RDWR);
               socket = -1;
#ifdef PC_PROFILE
               std::cout << "For Client ID " << clientId << std::endl;
               std::cout << "Average exec time= " << averageExecuteTime << std::endl;
               std::cout << "Average rdit time= " << averageSingleReadIterTime
                         << std::endl;
               std::cout << "Average intr time= " << averageIntraProcessingTime
                         << std::endl;
               std::cout << "Average read time= " << averageReadTime << std::endl;
               std::cout << "Average writ time= " << averageWriteTime << std::endl;
               std::cout << "Average bfcp time= " << averageBufferCopyTime << std::endl;
               std::cout << "Average sumt time= " << averageAccumulatedTime << std::endl;
#   ifdef PC_OPTIMIZE_READ_MULTIPLE_SINGLE_SHOT
               // Average Read count per iteration
               std::cout << "Average reco prit= " << averageReadCountPerIt << std::endl;
#   endif
               std::cout << "=================" << std::endl;
#endif
            }
         }

         template <typename Buffer>
         ClientPollResult ReadPacket(Buffer& buffer)
         {
#ifdef PC_PROFILE
            timespec const readPacketItStart = timer::now();
#endif
            buffer.Offset(0);
#ifdef PC_OPTIMIZE_READ_MULTIPLE_SINGLE_SHOT
            network::Result recvResult =
                network::TCP::recvAsManyAsPossibleAsync(socket, buffer);
#else
            if (!network::TCP::containsDataToRead(socket))
            {
               Terminate();
               ClientPollResult result;
               result.terminate = true;
               return result;
            }
            network::Result recvResult = network::TCP::recvFixedBytes(
                socket, buffer, NetworkPacket::SizeBytes, MSG_DONTWAIT);
#endif
            if (recvResult.IsFailure())
            {
               Terminate();
               ClientPollResult result;
               result.terminate = true;
               return result;
            }
            ClientPollResult result;
            result.read = true;
#ifdef PC_OPTIMIZE_READ_MULTIPLE_SINGLE_SHOT
            std::size_t bytesToRead = recvResult.NoOfBytes;
#endif
#ifdef PC_OPTIMIZE_READ_MULTIPLE_SINGLE_SHOT
#   ifdef PC_PROFILE
            std::size_t readCountThisIt = 0;
#   endif
#endif
            PacketList tempReadList;
#ifdef PC_OPTIMIZE_READ_MULTIPLE_SINGLE_SHOT
            while (bytesToRead > NetworkPacket::SizeBytes)
            {
#endif
               ++deadline;
               // We know at least one packet is available
               std::size_t packetSize =
                   NetworkPacket::ExtractPacketSizeFromBuffer(buffer);
#ifdef PC_OPTIMIZE_READ_MULTIPLE_SINGLE_SHOT
               bytesToRead -= NetworkPacket::SizeBytes;
               // Move buffer ahead
               buffer.OffsetBy(NetworkPacket::SizeBytes);
#endif

#ifndef PC_OPTIMIZE_READ_MULTIPLE_SINGLE_SHOT
               recvResult =
                   network::TCP::recvFixedBytes(socket, buffer, packetSize, MSG_DONTWAIT);
               if (recvResult.IsFailure())
               {
                  Terminate();
                  ClientPollResult result;
                  result.terminate = true;
                  return result;
               }
#endif
               assert(packetSize < buffer.sizeIgnoreOffset());
               if (packetSize >= buffer.sizeIgnoreOffset())
                  throw std::invalid_argument("Packet is very very large");

#ifdef PC_OPTIMIZE_READ_MULTIPLE_SINGLE_SHOT
#   ifdef PC_PROFILE
               ++readCountThisIt;
#   endif
#endif
#ifdef PC_OPTIMIZE_READ_MULTIPLE_SINGLE_SHOT
               if (packetSize > bytesToRead)
               {

                  std::size_t const extraElements = packetSize - bytesToRead;
                  // Move all elements to start
                  // And set 0 offset
                  buffer.MoveToStart();
                  buffer.Offset(bytesToRead);
                  // Read only the extra elements
                  recvResult = pc::network::TCP::recvFixedBytes(
                      socket, buffer, extraElements, MSG_DONTWAIT);
                  // Terrminate if extras read fails
                  if (recvResult.IsFailure())
                  {
                     Terminate();
                     ClientPollResult result;
                     result.terminate = true;
                     return result;
                  }
                  // Add to bytesToRead
                  bytesToRead += recvResult.NoOfBytes;
                  // Set buffer offset to start
                  buffer.Offset(0);
               }
#endif

               NetworkPacket const packet(buffer,
                                          packetSize
#ifdef PC_PROFILE
                                          ,
                                          recvResult.duration
#endif
               );

               // Ignore current packet
               buffer.OffsetBy(packetSize);
#ifdef PC_OPTIMIZE_READ_MULTIPLE_SINGLE_SHOT
               bytesToRead -= packetSize;
#endif

#ifdef PC_PROFILE
               averageReadTime += packet.readTimeDiff;
#endif
               if (packet.command == Commands::Blank)
               {
                  terminateOnNextCycle = false;
               }
               else if (packet.command == Commands::Setup::Ack)
               {
                  LockGuard guard(writeMutex);
                  packetsToWrite.push_back(NetworkPacket(Commands::Setup::Syn));
               }
               else if (packet.command == Commands::Setup::ClientID)
               {
                  clientId = packet.data;
                  LockGuard guard(writeMutex);
                  packetsToWrite.push_back(NetworkPacket(Commands::Setup::Join));
               }
               else if (packet.command == Commands::MajorErrors::SocketClosed)
               {
                  Terminate();
                  ClientPollResult result;
                  result.terminate = true;
                  return result;
               }

               else if (packet.command == Commands::Send)
               {
                  terminateOnNextCycle = false;
                  tempReadList.push_back(packet);
               }
#ifdef PC_PROFILE
               averageSingleReadIterTime += (timer::now() - readPacketItStart);
               averageBufferCopyTime += packet.bufferCopyTimeDiff;
#endif

#ifdef PC_OPTIMIZE_READ_MULTIPLE_SINGLE_SHOT
            }
#endif
#ifdef PC_OPTIMIZE_READ_MULTIPLE_SINGLE_SHOT
#   ifdef PC_PROFILE
            averageReadCountPerIt += readCountThisIt;
#   endif
#endif
            if (!tempReadList.empty())
            {
               LockGuard guard(readMutex);
               packetsToRead.splice(packetsToRead.end(), tempReadList);
            }

            return result;
         }

         void WritePackets(std::time_t timeout)
         {
#ifdef PC_PROFILE
            {
#endif
               LockGuard guard(writeMutex);
               if (packetsToWrite.empty())
                  return;
               for (PacketList::const_iterator writePacketIt = packetsToWrite.begin();
                    writePacketIt != packetsToWrite.end();
                    ++writePacketIt)
               {
                  network::Result const result = writePacketIt->Write(socket, timeout);
                  if (result.SocketClosed)
                     return Terminate();
#ifdef PC_PROFILE
                  averageWriteTime += writePacketIt->writeTimeDiff;
                  if (writePacketIt->command == Commands::Send)
                  {
                     averageExecuteTime += writePacketIt->executeTimeDiff;
                     averageIntraProcessingTime += writePacketIt->intraProcessingTimeDiff;
                     averageAccumulatedTime +=
                         (writePacketIt->intraProcessingTimeDiff +
                          writePacketIt->bufferCopyTimeDiff +
                          writePacketIt->executeTimeDiff + writePacketIt->writeTimeDiff +
                          writePacketIt->readTimeDiff);
                  }
#endif
                  timeout = 0;
               }
               packetsToWrite.clear();
#ifdef PC_PROFILE
            }
#endif
         }

         bool CheckSocket(::pollfd const& poll)
         {
            return poll.revents & POLLHUP || poll.revents & POLLNVAL || deadlineBreach();
         }

         template <typename Buffer>
         ClientPollResult OnReadPoll(::pollfd const& poll, Buffer& buffer)
         {
            ClientPollResult result;
            if (terminateNow)
            {
               result.terminate = true;
               return result;
            }
            if (CheckSocket(poll))
            {
               Terminate();
               result.terminate = true;
               return result;
            }
            if (poll.revents & POLLIN)
               return this->ReadPacket(buffer);
            return result;
         }

         bool TerminateThisCycleOrNext()
         {
            // The next cycle
            // When this was to be terminated
            // Just happened
            if (terminateOnNextCycle || terminateNow)
            {
               // Terminate now
               Terminate();
               return true;
            }

            // Schedule Termination on Next cycle
            terminateOnNextCycle = true;

            // Send heartbeat
            {
               LockGuard guard(writeMutex);
               // Add heartbeat packet to write queue
               packetsToWrite.push_back(NetworkPacket(Commands::HeartBeat));
            }

            // Terminate on next cycle
            return false;
         }

         bool deadlineBreach() const
         {
            return deadline;
         }

         ClientInfo& changeMaxCount(std::size_t maxCount)
         {
            deadline.MaxCount(maxCount);
            return *this;
         }

         std::ptrdiff_t DeadlineMaxCount() const
         {
            return deadline.MaxCount();
         }
      };
   } // namespace protocol
} // namespace pc
