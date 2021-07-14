#pragma once

#include <pc/balancer/priority.hpp>
#include <pc/deadliner/IfNotWithin.hpp>
#include <pc/lexical_cast.hpp>
#include <pc/pqpp/Connection.hpp>
#include <pc/thread/Mutex.hpp>
#include <pc/thread/MutexGuard.hpp>

#include <string>

namespace pc
{
   namespace protocol
   {
      struct Config
      {
         typedef void(DownCallback)(std::size_t const, std::size_t const);
         typedef balancer::priority balancerT;

#ifndef PC_DISABLE_DATABASE_SUPPORT
         typedef pqpp::Connection DBConnection;

         DBConnection connection;
#endif
         balancerT*    balancer;
         DownCallback* downCallback;

         Config(std::string   connectionString,
                balancerT&    balancer,
                DownCallback* downCallback) :
#ifndef PC_DISABLE_DATABASE_SUPPORT
             connection(connectionString),
#endif
             balancer(&balancer), downCallback(downCallback)
         {
         }

         std::size_t ExtractDeadlineMaxCountFromDatabase(std::string const clientId)
         {
#ifndef PC_DISABLE_DATABASE_SUPPORT
            std::vector<const char*> params(2);
            params[0] = clientId.c_str();
            params[1] = "45"; // DEFAULT

            static pc::threads::Mutex mutex;
            pc::threads::MutexGuard   guard(mutex);
            pc::pqpp::IterateResult   res = connection.iterate(
                "SELECT coalesce(MAX(priority),$2) AS priority FROM priority_table WHERE "
                "clientId=$1",
                params);
            if (!res)
            {
               throw std::runtime_error(
                   "Unable to extract deadline max count from database");
            }
            std::ptrdiff_t newDeadlineMaxCount =
                pc::lexical_cast<std::string, std::ptrdiff_t>(res[0]);
            return newDeadlineMaxCount;
#else
            return 45;
#endif
         }
      };
   } // namespace protocol
} // namespace pc
