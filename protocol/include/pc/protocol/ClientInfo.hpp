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

namespace pc
{
   namespace protocol
   {
      class ClientInfo
      {

         typedef std::queue<NetworkPacket> ReadPacketVec;
         typedef std::queue<NetworkPacket> WritePacketVec;
         typedef pc::threads::Atomic<bool> AtomicBool;

         int socket;

       public:
         std::string clientId;

       private:
         pc::deadliner::Deadline deadline;
         ClientResponseCallback  callback;
         AtomicBool              terminateOnNextCycle;
         AtomicBool              terminateNow;
         ReadPacketVec           packetsToRead;
         pc::threads::Mutex      packetsToReadMutex;
         WritePacketVec          packetsToWrite;
         pc::threads::Mutex      packetsToWriteMutex;

       public:
         ClientInfo(int socket, ClientResponseCallback callback) :
             socket(socket), callback(callback)
         {
            ++deadline;
         }

         void executeCallbacks()
         {
            if (packetsToRead.empty())
               return;
            WritePacketVec tempWriteVec;
            {
               // Add to a temporary vector for writes
               pc::threads::MutexGuard guard(packetsToReadMutex);
               while (!packetsToRead.empty())
               {
                  NetworkSendPacket writePacket = callback(packetsToRead.front(), *this);
                  packetsToRead.pop();
                  if (writePacket.command == Commands::Send)
                     tempWriteVec.push(writePacket);
               }
            }
            {
               // Add to write vector
               pc::threads::MutexGuard guard(packetsToWriteMutex);
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
               pc::threads::MutexGuard guard(packetsToReadMutex);
               packetsToRead.push(packet);
            }
         }

         void WritePackets(std::time_t timeout)
         {
            pc::threads::MutexGuard guard(packetsToWriteMutex);
            while (!packetsToWrite.empty())
            {
               NetworkPacket   writePacket = packetsToWrite.front();
               network::Result result      = writePacket.Write(socket, timeout);
               if (result.SocketClosed)
                  return Terminate();
               packetsToWrite.pop();
            }
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
            if (poll.revents & POLLIN)
            {
               this->ReadPacket(timeout);
               result.read = true;
            }
            if (poll.revents & POLLOUT)
            {
               this->WritePackets(0);
               result.write = true;
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
               pc::threads::MutexGuard guard(packetsToWriteMutex);
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
