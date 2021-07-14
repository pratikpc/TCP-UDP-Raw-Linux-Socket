#pragma once

#include <pc/deadliner/Deadline.hpp>
#include <pc/protocol/ClientPollResult.hpp>
#include <pc/protocol/Packet.hpp>
#include <pc/protocol/types.hpp>
#include <pc/thread/Atomic.hpp>
#include <queue>
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
         typedef std::queue<NetworkPacket> PacketVec;
#ifdef PC_PROFILE
         typedef std::queue<timespec>      TimeVec;
         typedef pc::opt::Averager<double> Averager;
#endif
         typedef pc::threads::Atomic<bool> AtomicBool;

         int socket;

       public:
         std::string clientId;

       private:
         pc::deadliner::Deadline deadline;
         ClientResponseCallback  callback;
         AtomicBool              terminateOnNextCycle;
         AtomicBool              terminateNow;
         PacketVec               packetsToRead;
#ifndef PC_USE_SPINLOCKS
         mutable threads::Mutex      readMutex;
         mutable threads::Mutex      writeMutex;
         typedef threads::MutexGuard LockGuard;
#else
         mutable threads::SpinLock  readMutex;
         mutable threads::SpinLock  writeMutex;
         typedef threads::SpinGuard LockGuard;
#endif
         PacketVec packetsToWrite;

#ifdef PC_PROFILE
         Averager averageIntraProcessingTime;
         Averager averageReadTime;
         Averager averageWriteTime;
         Averager averageExecuteTime;
         Averager averageBufferCopyTime;
         Averager averageAccumulatedTime;
#endif

       public:
         ClientInfo(int                    socket,
                    ClientResponseCallback callback,
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
            if (packetsToRead.empty())
               return;
            PacketVec tempWriteVec;
            {
               // Add to a temporary vector for writes
               LockGuard guard(readMutex);
               while (!packetsToRead.empty())
               {
                  NetworkPacket const readPacket = packetsToRead.front();
#ifdef PC_PROFILE
                  timespec const executeStart = timer::now();
#endif
                  NetworkSendPacket writePacket = callback(readPacket, *this);
#ifdef PC_PROFILE
                  writePacket.executeTimeDiff = timer::now() - executeStart;
                  writePacket.readTimeDiff    = readPacket.readTimeDiff;
                  writePacket.intraProcessingTimeStart =
                      readPacket.intraProcessingTimeStart;
#endif
                  packetsToRead.pop();
                  if (writePacket.command == Commands::Send)
                     tempWriteVec.push(writePacket);
               }
            }
            {
               // Add to write vector
               LockGuard guard(writeMutex);
               // If Write Vector is empty, simply copy
               if (packetsToWrite.empty())
                  packetsToWrite = tempWriteVec;
               else
                  // Else, add all elems from tempWriteVec to end of write vector
                  while (!tempWriteVec.empty())
                  {
                     NetworkPacket writePacket = tempWriteVec.front();
                     tempWriteVec.pop();
                     packetsToWrite.push(writePacket);
                  }
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
               packetsToWrite.push(NetworkPacket(Commands::Setup::Syn));
            }
            else if (packet.command == Commands::Setup::ClientID)
            {
               clientId = packet.data;
               LockGuard guard(writeMutex);
               packetsToWrite.push(NetworkPacket(Commands::Setup::Join));
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
               packetsToRead.push(packet);
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
               while (!packetsToWrite.empty())
               {
                  NetworkPacket const&  writePacket = packetsToWrite.front();
                  network::Result const result      = writePacket.Write(socket, timeout);
                  if (result.SocketClosed)
                     return Terminate();
#ifdef PC_PROFILE
                  // Only send commands have readTimeTaken and readWriteTime defined
                  if (writePacket.command == Commands::Send)
                  {
                     averageExecuteTime += writePacket.executeTimeDiff;
                     averageWriteTime += writePacket.writeTimeDiff;
                     averageIntraProcessingTime += writePacket.intraProcessingTimeDiff;
                     averageAccumulatedTime +=
                         (writePacket.intraProcessingTimeDiff +
                          writePacket.executeTimeDiff + writePacket.writeTimeDiff +
                          writePacket.readTimeDiff);
                  }
#endif
                  timeout = 0;
                  packetsToWrite.pop();
               }
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
               packetsToWrite.push(NetworkPacket(Commands::HeartBeat));
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
