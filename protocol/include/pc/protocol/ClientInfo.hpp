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
#   include <pc/profiler/Analyzer.hpp>
#endif

// Poll Exec Write separated approach uses condition variables for signalling
// Which rely on mutexes and not spinlocks
#if !defined(PC_USE_SPINLOCKS) || defined(PC_SEPARATE_POLL_EXEC_WRITE)
#   include <pc/thread/Mutex.hpp>
#   include <pc/thread/MutexGuard.hpp>
#else
#   include <pc/thread/spin/SpinGuard.hpp>
#   include <pc/thread/spin/SpinLock.hpp>
#endif

#ifdef PC_SEPARATE_POLL_EXEC_WRITE
#   include <pc/thread/ConditionVariable.hpp>
#endif

namespace pc
{
   namespace protocol
   {
      class ClientInfo
      {
#ifdef PC_PROFILE
         typedef profiler::Analyzer<double> Analyzer;
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
         PacketList              packetsToWrite;

// Poll Exec Write separated approach uses condition variables for signalling
// Which rely on mutexes and not spinlocks
#if !defined(PC_USE_SPINLOCKS) || defined(PC_SEPARATE_POLL_EXEC_WRITE)
         mutable threads::Mutex      readMutex;
         mutable threads::Mutex      writeMutex;
         typedef threads::MutexGuard LockGuard;
#else
         mutable threads::SpinLock  readMutex;
         mutable threads::SpinLock  writeMutex;
         typedef threads::SpinGuard LockGuard;
#endif

#ifdef PC_SEPARATE_POLL_EXEC_WRITE
         threads::ConditionVariable onReadAvailableCond;
#endif

#ifdef PC_PROFILE
         Analyzer averageIntraProcessingTime;
         Analyzer averageReadTime;
         Analyzer averageWriteTime;
         Analyzer averageExecuteTime;
         Analyzer averageBufferCopyTime;
         Analyzer averageAccumulatedTime;
         Analyzer averageSingleReadIterTime;

#   ifdef PC_SEPARATE_POLL_EXEC_WRITE
         Analyzer averageCondWaitDuration;
#   endif
#   ifdef PC_OPTIMIZE_READ_MULTIPLE_SINGLE_SHOT
         Analyzer averageReadCountPerIt;
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
#ifdef PC_SEPARATE_POLL_EXEC_WRITE
#   ifdef PC_PROFILE
               timespec const startPollExecWrite = timer::now();
#   endif
               timespec waitDuration = timer::now(CLOCK_REALTIME);
               waitDuration.tv_nsec += 0;
               waitDuration.tv_sec += 5;
               onReadAvailableCond.Wait(readMutex, waitDuration);
#   ifdef PC_PROFILE
               averageCondWaitDuration += (timer::now() - startPollExecWrite);
#   endif
#endif
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
                 packetIt != tempList.end();
                 ++packetIt)
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
               callback(*packetIt, *this);
#ifdef PC_PROFILE
               packetIt->executeTimeDiff          = timer::now() - executeStart;
               packetIt->readTimeDiff             = readPacketTimeDiff;
               packetIt->intraProcessingTimeStart = readPacketIntraProcessingTimeStart;
#endif
            }
            // If nothing available to write
            // Return
            // Remember that tempList now will contain
            // All output of callbacks
            // And hence it is now the write array
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
#   ifdef PC_SEPARATE_POLL_EXEC_WRITE
               std::cout << "Average cond wait= " << averageCondWaitDuration << std::endl;
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
            network::Result recvResult;
            buffer.Offset(0);
#ifndef PC_OPTIMIZE_READ_MULTIPLE_SINGLE_SHOT

            recvResult = network::TCP::recvFixedBytes(
                socket,
                buffer,
                NetworkPacket::SizeBytes /*Just read the Packet size*/,
                // Other processing happens in the loop
                // Loop exists only when we set PC_OPTIMIZE_READ_MULTIPLE_SINGLE_SHOT
                MSG_DONTWAIT);
            if (recvResult.IsFailure())
            {
               Terminate();
               ClientPollResult result;
               result.terminate = true;
               return result;
            }
#endif
#ifdef PC_OPTIMIZE_READ_MULTIPLE_SINGLE_SHOT
            // No reads performed yet
            // Perform the reads within the loop
            std::size_t bytesToRead = 0;
#   ifdef PC_PROFILE
            std::size_t readCountThisIt = 0;
#   endif
#endif
            PacketList tempReadList;
            // Only loop when we are interested in multiple values
#ifdef PC_OPTIMIZE_READ_MULTIPLE_SINGLE_SHOT
            // Do not check condition just yet
            do
            {
#endif
#ifdef PC_OPTIMIZE_READ_MULTIPLE_SINGLE_SHOT
               if (bytesToRead < NetworkPacket::SizeBytes)
               {
                  // As we can see
                  // One packet is available
                  // Because we have only partially read the size
                  // So let us perform a big read again
                  // Till we have data

                  // Move buffer ahead
                  // Read the given bytes
                  buffer.OffsetBy(bytesToRead);
                  recvResult = network::TCP::recvAsManyAsPossibleAsync(socket, buffer);
                  if (recvResult.IsFailure())
                  {
                     Terminate();
                     ClientPollResult result;
                     result.terminate = true;
                     return result;
                  }
                  // Now that we moved buffer ahead
                  // Move it back
                  // So that the data we currently were looking at
                  // Gets considered as well
                  buffer.OffsetBack(bytesToRead);

                  // Update Bytes to read
                  // With total number of bytes read in this iteration
                  bytesToRead += recvResult.NoOfBytes;

#   ifdef PC_PROFILE
                  averageReadTime += recvResult.duration;
#   endif
               }
#endif
               ++deadline;
               // We know at least one packet is available
               std::size_t packetSize =
                   NetworkPacket::ExtractPacketSizeFromBuffer(buffer);
// For single shot, the previous was the first read
// For multi shot, the first read occured before
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
#   ifdef PC_PROFILE
               averageReadTime += recvResult.duration;
#   endif
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
#   ifdef PC_PROFILE
                  averageReadTime += recvResult.duration;
#   endif
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
               averageBufferCopyTime += packet.bufferCopyTimeDiff;
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
               else if (packet.command == Commands::Send)
               {
                  terminateOnNextCycle = false;
                  tempReadList.push_back(packet);
               }
#ifdef PC_PROFILE
               averageSingleReadIterTime += (timer::now() - readPacketItStart);
#endif

#ifdef PC_OPTIMIZE_READ_MULTIPLE_SINGLE_SHOT
            } while (bytesToRead > 0);
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
#ifdef PC_SEPARATE_POLL_EXEC_WRITE
               onReadAvailableCond.Signal();
#endif
            }
            ClientPollResult result;
            result.read = true;
            return result;
         }

         void WritePackets(std::time_t timeout)
         {
            std::string writeMarshalled;
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
                  writeMarshalled += writePacketIt->Marshall();
#ifdef PC_PROFILE
                  if (writePacketIt->command == Commands::Send)
                  {
                     averageExecuteTime += writePacketIt->executeTimeDiff;
                     averageIntraProcessingTime += writePacketIt->intraProcessingTimeDiff;
                     averageAccumulatedTime +=
                         (writePacketIt->intraProcessingTimeDiff +
                          writePacketIt->bufferCopyTimeDiff +
                          writePacketIt->executeTimeDiff + writePacketIt->readTimeDiff);
                  }
#endif
                  timeout = 0;
               }
               packetsToWrite.clear();
#ifdef PC_PROFILE
            }
#endif
            network::Result const result =
                network::TCPPoll::write(socket, writeMarshalled, timeout);
            if (result.SocketClosed)
               return Terminate();
#ifdef PC_PROFILE
            averageWriteTime += result.duration;
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
