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
            PacketList tempWriteVec;
            {
               // Add to a temporary vector for writes
               LockGuard guard(readMutex);
               if (packetsToRead.empty())
                  return;
               for (PacketList::const_iterator readPacketIt = packetsToRead.begin();
                    readPacketIt != packetsToRead.end();
                    ++readPacketIt)
               {
#ifdef PC_PROFILE
                  timespec const executeStart = timer::now();
#endif
                  NetworkPacket writePacket = callback(*readPacketIt, *this);
#ifdef PC_PROFILE
                  writePacket.executeTimeDiff = timer::now() - executeStart;
                  writePacket.readTimeDiff    = readPacketIt->readTimeDiff;
                  writePacket.intraProcessingTimeStart =
                      readPacketIt->intraProcessingTimeStart;
#endif
                  if (writePacket.command == Commands::Send)
                     tempWriteVec.push_back(writePacket);
               }
               packetsToRead.clear();
            }
            if (!tempWriteVec.empty())
            {
               // Add to write vector
               LockGuard guard(writeMutex);
               // If Write Vector is empty, simply copy
               packetsToWrite.splice(packetsToWrite.end(), tempWriteVec);
            }
         }

         // TODO
         // {
         //    // As the balancer element is common
         //    // and shared across protocols
         //    // Make the balancer updation guard
         //    // static
         //    static pc::threads::Mutex balancerPriorityUpdationMutex;
         //    LockGuard   guard(balancerPriorityUpdationMutex);
         //    std::size_t               newDeadlineMaxCount =
         //        config->ExtractDeadlineMaxCountFromDatabase(clientInfo.clientId);
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
               ::close(socket);
               socket = -1;
#ifdef PC_PROFILE
               std::cout << "For Client ID " << clientId << std::endl;
               std::cout << "Average exec time= " << averageExecuteTime << std::endl;
               std::cout << "Average intr time= " << averageIntraProcessingTime
                         << std::endl;
               std::cout << "Average read time= " << averageReadTime << std::endl;
               std::cout << "Average writ time= " << averageWriteTime << std::endl;
               std::cout << "Average bfcp time= " << averageBufferCopyTime << std::endl;
               std::cout << "Average sumt time= " << averageAccumulatedTime << std::endl;
               std::cout << "=================" << std::endl;
#endif
            }
         }

         template <typename Buffer>
         ClientPollResult ReadPacket(Buffer& buffer)
         {
            if (!pc::network::TCP::containsDataToRead(socket))
            {
               Terminate();
               ClientPollResult result;
               result.terminate = true;
               return result;
            }
            ++deadline;
            ClientPollResult result;
            result.read = true;

            NetworkPacket packet = NetworkPacket::Read(socket, buffer, 0);
#ifdef PC_PROFILE
            averageReadTime += packet.readTimeDiff;
            // std::cout << packet.readTimeDiff << " read" << std::endl;
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
               LockGuard guard(readMutex);
               packetsToRead.push_back(packet);
            }
#ifdef PC_PROFILE
            averageBufferCopyTime += packet.bufferCopyTimeDiff;
#endif
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
                  // Only send commands have readTimeTaken and readWriteTime defined
                  if (writePacketIt->command == Commands::Send)
                  {
                     averageExecuteTime += writePacketIt->executeTimeDiff;
                     averageWriteTime += writePacketIt->writeTimeDiff;
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
