#pragma once

#include <pc/memory/ExtendableNoCopyArr.hpp>
#include <pc/poll/EPoll.hpp>
#include <pc/protocol/ClientInfo.hpp>
#include <pc/protocol/ClientPollResult.hpp>
#include <tr1/unordered_map>
#include <vector>

#include <pc/thread/RWLock.hpp>
#include <pc/thread/RWReadGuard.hpp>
#include <pc/thread/RWWriteGuard.hpp>

#ifdef PC_SEPARATE_POLL_EXEC_WRITE
#   include <pc/thread/ConditionVariable.hpp>
#endif
namespace pc
{
   namespace protocol
   {
      class ClientInfos
      {
         typedef std::tr1::unordered_map<int /*Socket*/, ClientInfo> ClientInfoMap;

         typedef ClientInfoMap::iterator                  iterator;
         typedef ClientInfoMap::const_iterator            const_iterator;
         typedef memory::ExtendableNoCopyArr<epoll_event> PollEvents;

         ClientInfoMap clientInfos;

         bool        updateIssued;
         poll::EPoll ePollIn;
         PollEvents  pollInEvents;

         mutable threads::RWLock lock;

         PacketList packetsToRead;
         PacketList packetsToWrite;

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
         typedef profiler::Analyzer<double> Analyzer;

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
         ClientInfos() : updateIssued(false), ePollIn(), pollInEvents(5) {}
         template <typename UniqueSockets, typename Balancer>
         void close(UniqueSockets&    socketsToRemove,
                    Balancer&         balancer,
                    std::size_t const balancerIndex)
         {
            threads::RWWriteGuard guard(lock);
            for (typename UniqueSockets::iterator it = socketsToRemove.begin();
                 it != socketsToRemove.end();)
            {
               int const      socket = *it;
               const_iterator infoIt = clientInfos.find(socket);
               // Check if the socket exists first of all
               if (infoIt != clientInfos.end())
               {
                  balancer.decPriority(balancerIndex, infoIt->second.DeadlineMaxCount());
                  clientInfos.erase(infoIt);
                  ::close(socket);
                  ++it;
               }
               // If it does not
               // Remove from Sockets to remove list
               else
                  it = socketsToRemove.erase(it);
            }
#ifdef PC_PROFILE
            if (socketsToRemove.empty())
               return;
            std::cout << "Average exec time= " << averageExecuteTime << std::endl;
            std::cout << "Average rdit time= " << averageSingleReadIterTime << std::endl;
            std::cout << "Average intr time= " << averageIntraProcessingTime << std::endl;
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
         void insert(ClientInfo const& clientInfo)
         {
            threads::RWWriteGuard guard(lock);
            int const             socket = clientInfo.socket;
            clientInfos.insert(std::make_pair(socket, clientInfo));
            ePollIn.Add(socket, EPOLLIN | EPOLLRDHUP);
            updateIssued = true;
         }
         void insert(int const         socket,
                     std::size_t const DeadlineMaxCount = DEADLINE_MAX_COUNT_DEFAULT)
         {
            return insert(ClientInfo(socket, DeadlineMaxCount));
         }
         template <typename UniqueSockets>
         void FilterSocketsToTerminate(UniqueSockets& socketsSelected)
         {
            if (socketsSelected.empty())
               return;
            PacketList warningMessageList;
            for (typename UniqueSockets::const_iterator it = socketsSelected.begin();
                 it != socketsSelected.end();)
            {
               int const socket       = *it;
               bool      terminateNow = false;
               {
                  threads::RWReadGuard guard(lock);
                  iterator             clientIt = clientInfos.find(socket);
                  if (clientIt != clientInfos.end())
                     terminateNow = clientIt->second.TerminateThisCycleOrNext();
                  else
                     // By terminating now
                     // We effectively ignore this
                     terminateNow = true;
               }
               // If this is not the cycle to terminate
               if (!terminateNow)
               {
                  // Remove from the list
                  // Because all selected sockets will be terminated
                  // And we are giving this a warning
                  it = socketsSelected.erase(it);
                  warningMessageList.push_back(
                      NetworkPacket(socket, Commands::HeartBeat));
                  // Send heartbeat
               }
               else
                  ++it;
            }
            if (!warningMessageList.empty())
            {
               LockGuard guard(writeMutex);
               // Add heartbeat packet to write queue
               packetsToWrite.splice(packetsToWrite.end(), warningMessageList);
            }
         }
         void Execute(ClientResponseCallback& callback)
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

            threads::RWReadGuard guard(lock);
            if (clientInfos.empty())
               return;
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
               ClientInfoMap::const_iterator it = clientInfos.find(packetIt->socket);
               if (it == clientInfos.end())
                  continue;

#ifdef PC_PROFILE
               timespec const executeStart = timer::now();
#endif
               // Item to write added to packetIt itself
               // So the read array will now have the items to write
               callback(*packetIt, it->second);
#ifdef PC_PROFILE
               packetIt->executeTimeDiff = timer::now() - executeStart;
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

         void Update()
         {
            // Take Read guard first and check
            // Multiple reads are allowed from these objects
            // Only multiple writes are an issue
            // And given there is a high chance that no write will happen
            // Hence no need to update will take place
            {
               threads::RWReadGuard guard(lock);
               if (!updateIssued)
                  return;
            }
            threads::RWWriteGuard guard(lock);
            pollInEvents.ExtendTo(clientInfos.size());
            updateIssued = false;
         }

         bool empty() const
         {
            threads::RWReadGuard guard(lock);
            return clientInfos.empty();
         }

         ClientInfo RemoveOldestClientAndReturn()
         {
            threads::RWWriteGuard guard(lock);

            iterator oldestCient = clientInfos.begin();
            if (oldestCient == clientInfos.end())
               return ClientInfo(-1);
            ClientInfo const clientInfo = oldestCient->second;
            clientInfos.erase(oldestCient);
            return clientInfo;
         }
         int OldestSocket() const
         {
            threads::RWReadGuard guard(lock);
            const_iterator       oldestCient = clientInfos.begin();
            return oldestCient->first /*socket*/;
         }

         void Write()
         {
            PacketList tempWriteList;
            {
               LockGuard guard(writeMutex);
               if (packetsToWrite.empty())
                  return;
               tempWriteList.splice(tempWriteList.end(), packetsToWrite);
            }
            for (PacketList::const_iterator writePacketIt = tempWriteList.begin();
                 writePacketIt != tempWriteList.end();
                 ++writePacketIt)
            {
               network::Result const result = writePacketIt->Write();
               if (result.SocketClosed)
               {
                  network::Socket::close(writePacketIt->socket);
                  return;
               }
#ifdef PC_PROFILE
               averageWriteTime += writePacketIt->writeTimeDiff;
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
            }
         }

         std::size_t size() const
         {
            threads::RWReadGuard guard(lock);
            return clientInfos.size();
         }

         template <typename Buffer>
         ClientPollResult ReadForSingleSocket(int const   socket,
                                              ClientInfo& clientInfo,
                                              PacketList& readList,
                                              Buffer&     buffer)
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
               network::Socket::close(socket);
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
                     network::Socket::close(socket);
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
               ++clientInfo.deadline;
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
                  network::Socket::close(socket);
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
                     network::Socket::close(socket);
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

               NetworkPacket const packet(socket,
                                          buffer,
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
                  clientInfo.terminateOnNextCycle = false;
               }
               else if (packet.command == Commands::Setup::Ack)
               {
                  LockGuard guard(writeMutex);
                  packetsToWrite.push_back(NetworkPacket(socket, Commands::Setup::Syn));
               }
               else if (packet.command == Commands::Setup::ClientID)
               {
                  clientInfo.clientId = packet.data;
                  LockGuard guard(writeMutex);
                  packetsToWrite.push_back(NetworkPacket(socket, Commands::Setup::Join));
               }
               else if (packet.command == Commands::Send)
               {
                  clientInfo.terminateOnNextCycle = false;
                  readList.push_back(packet);
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
            ClientPollResult result;
            result.read = true;
            return result;
         }

         template <typename Buffer, typename UniqueSockets>
         void OnReadPoll(UniqueSockets& socketsWeReadAt,
                         UniqueSockets& socketsToTerminate,
                         Buffer&        buffer,
                         int const      timeoutS)
         {
            PacketList readList;
            int const  countPolls = ePollIn.Wait(pollInEvents, timeoutS);
            if (countPolls < 1)
               return;
            {
               threads::RWReadGuard guard(lock);
               for (std::size_t i = 0;
                    (i < (std::size_t)(countPolls)) && i < pollInEvents.size();
                    ++i)
               {
                  // This is going to exist till the end of this function
                  // Avoid unncessary copies
                  ::epoll_event const& poll   = pollInEvents[i];
                  int const            socket = poll.data.fd;

                  // Deal with socket closure
                  // No need to extract clientInfo in such a scenario
                  if ((poll.events & EPOLLHUP) || (poll.events & EPOLLRDHUP) ||
                      (poll.events & EPOLLERR))
                  {
                     socketsToTerminate.insert(socket);
                     continue;
                  }

                  ClientInfoMap::iterator clientIt = clientInfos.find(socket);
                  if (clientIt == clientInfos.end())
                  {
                     socketsToTerminate.insert(socket);
                     continue;
                  }
                  ClientInfo& clientInfo = clientIt->second;
                  // Check if this is the socket to terminate
                  if (clientInfo.terminateNow || clientInfo.deadlineBreach())
                     socketsToTerminate.insert(socket);
                  else if (poll.events & EPOLLIN)
                  {
                     ClientPollResult result =
                         this->ReadForSingleSocket(socket, clientInfo, readList, buffer);
                     if (result.terminate)
                        socketsToTerminate.insert(socket);
                     else if (result.read)
                        socketsWeReadAt.insert(socket);
                  }
               }
            }
            if (!readList.empty())
            {
               LockGuard guard(readMutex);
               packetsToRead.splice(packetsToRead.end(), readList);
#ifdef PC_SEPARATE_POLL_EXEC_WRITE
               onReadAvailableCond.Signal();
#endif
            }
         }
      };
   } // namespace protocol
} // namespace pc
