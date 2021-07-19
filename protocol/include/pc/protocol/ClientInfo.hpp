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
namespace pc
{
   namespace protocol
   {
      class ClientInfo
      {
         typedef pc::threads::Atomic<bool> AtomicBool;

         int const socket;

       public:
         std::string             clientId;
         pc::deadliner::Deadline deadline;
         AtomicBool              terminateOnNextCycle;
         AtomicBool              terminateNow;

       public:
         ClientInfo(int const         socket,
                    std::size_t const DeadlineMaxCount = DEADLINE_MAX_COUNT_DEFAULT) :
             socket(socket),
             deadline(DeadlineMaxCount), terminateOnNextCycle(false), terminateNow(false)
         {
            ++deadline;
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
            network::Socket::close(socket);
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
      }; // namespace protocol
   }     // namespace protocol
} // namespace pc
