#pragma once

#include <pc/deadliner/Deadline.hpp>
#include <pc/protocol/ClientPollResult.hpp>
#include <pc/protocol/Packet.hpp>
#include <pc/protocol/types.hpp>
#include <pc/thread/Atomic.hpp>
#include <pc/thread/Mutex.hpp>
#include <pc/thread/MutexGuard.hpp>
#include <queue>
#include <string>

#ifdef PC_PROFILE
#   include <pc/timer/timer.hpp>
#endif

#ifdef PC_PROFILE
#   include <pc/opt/Averager.hpp>
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
         pc::threads::Mutex      readMutex;
         PacketVec               packetsToWrite;
         pc::threads::Mutex      writeMutex;

#ifdef PC_PROFILE
         Averager averageReadWriteTimeNs;
         Averager averageReadTimeNs;
         Averager averageWriteTimeNs;
#endif

       public:
         ClientInfo(int socket, ClientResponseCallback callback) :
             socket(socket), callback(callback), terminateOnNextCycle(), terminateNow()
#ifdef PC_PROFILE
             ,
             averageReadWriteTimeNs(), averageReadTimeNs(), averageWriteTimeNs()
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
               pc::threads::MutexGuard guard(readMutex);
               while (!packetsToRead.empty())
               {
                  NetworkPacket const readPacket  = packetsToRead.front();
                  NetworkSendPacket   writePacket = callback(readPacket, *this);
#ifdef PC_PROFILE
                  writePacket.readTimeDiff  = readPacket.readTimeDiff;
                  writePacket.readTimeStart = readPacket.readTimeStart;
#endif
                  packetsToRead.pop();
                  if (writePacket.command == Commands::Send)
                     tempWriteVec.push(writePacket);
               }
            }
            {
               // Add to write vector
               pc::threads::MutexGuard guard(writeMutex);
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
         network::Result setupConnection(std::time_t timeout)
         {
            network::Result result;
            network::buffer data(40);
            NetworkPacket   ackAck = NetworkPacket::Read(socket, data, timeout);
            if (ackAck.command != Commands::Setup::Ack)
            {
               result.SocketClosed = true;
               return result;
            }
            network::Result const ackSynResult =
                NetworkPacket(Commands::Setup::Syn).Write(socket, timeout);
            if (ackSynResult.IsFailure())
               return ackSynResult;

            NetworkPacket const clientId = NetworkPacket::Read(socket, data, timeout);
            if (clientId.command != Commands::Setup::ClientID)
            {
               network::Result result;
               result.SocketClosed = true;
               return result;
            }
            this->clientId = std::string(clientId.data);

            network::Result const joinResult =
                NetworkPacket(Commands::Setup::Join).Write(socket, timeout);
            if (joinResult.IsFailure())
               return joinResult;

            // TODO
            // {
            //    // As the balancer element is common
            //    // and shared across protocols
            //    // Make the balancer updation guard
            //    // static
            //    static pc::threads::Mutex balancerPriorityUpdationMutex;
            //    pc::threads::MutexGuard   guard(balancerPriorityUpdationMutex);
            //    std::size_t               newDeadlineMaxCount =
            //        config->ExtractDeadlineMaxCountFromDatabase(clientInfo.clientId);
            //    config->balancer->setPriority(balancerIndex,
            //                                  // Update priority for given element
            //                                  (*config->balancer)[balancerIndex] -
            //                                      clientInfo.deadline.MaxCount() +
            //                                      newDeadlineMaxCount);
            //    clientInfo.changeMaxCount(newDeadlineMaxCount);
            // }
            return result;
         }
         void Terminate()
         {
            if (socket != -1)
            {
               terminateNow = true;
               ::close(socket);
               socket = -1;
            }
         }
         void ReadPacket(std::time_t timeout)
         {
            if (!pc::network::TCP::containsDataToRead(socket))
               return Terminate();
            ++deadline;
            if (clientId.empty())
            {
               setupConnection(timeout);
               return;
            }
            network::buffer buffer(UINT16_MAX);

            NetworkPacket packet = NetworkPacket::Read(socket, buffer, 0);
            if (packet.command == Commands::Blank)
            {
               terminateOnNextCycle = false;
               return;
            }

            else if (packet.command == Commands::MajorErrors::SocketClosed)
               return Terminate();

            else if (packet.command == Commands::Send)
            {
               terminateOnNextCycle = false;
               pc::threads::MutexGuard guard(readMutex);
               packetsToRead.push(packet);
            }
         }

         void WritePackets(std::time_t timeout)
         {
            pc::threads::MutexGuard guard(writeMutex);
            if (packetsToWrite.empty())
               return;
            std::vector<timespec> differences;
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
                  timespec const readTimeTaken  = writePacket.readTimeDiff;
                  timespec const writeTimeTaken = writePacket.writeTimeDiff;
                  timespec const readWriteTime  = writePacket.readWriteDiff;
                  differences.push_back(readTimeTaken);
                  differences.push_back(writeTimeTaken);
                  differences.push_back(readWriteTime);
               }
#endif
               packetsToWrite.pop();
            }
#ifdef PC_PROFILE
            if (!differences.empty())
            {
               for (std::vector<timespec>::const_iterator it = differences.begin();
                    it != differences.end();)
               {
                  // std::cout << *it << " read" << std::endl;
                  averageReadTimeNs += (it->tv_sec * 1.e6 + it->tv_nsec / 1.e3);
                  ++it;
                  // std::cout << *it << " write" << std::endl;
                  averageWriteTimeNs += (it->tv_sec * 1.e6 + it->tv_nsec / 1.e3);
                  ++it;
                  // std::cout << *it << " read+write" << std::endl;
                  averageReadWriteTimeNs += (it->tv_sec * 1.e6 + it->tv_nsec / 1.e3);
                  ++it;
                  // std::cout << "=================" << std::endl;
               }
               std::cout << "Average read+write size= " << averageReadWriteTimeNs.val()
                         << "us" << std::endl;
               std::cout << "Average read size= " << averageReadTimeNs.val() << "us"
                         << std::endl;
               std::cout << "Average write size= " << averageWriteTimeNs.val() << "us"
                         << std::endl;
               std::cout << "=================" << std::endl;
            }
#endif
         }

         ClientPollResult OnPoll(::pollfd const& poll, std::time_t timeout)
         {
            ClientPollResult result;
            if (terminateNow)
            {
               result.terminate = true;
               return result;
            }
            if (poll.revents & POLLHUP || poll.revents & POLLNVAL || deadlineBreach())
            {
               // Terminate on next health check
               Terminate();
               result.terminate = true;
               return result;
            }
            if (poll.revents & POLLOUT)
            {
               this->WritePackets(0);
               result.write = true;
            }
            if (poll.revents & POLLIN)
            {
               this->ReadPacket(timeout);
               result.read = true;
            }
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
               pc::threads::MutexGuard guard(writeMutex);
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
