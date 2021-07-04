#pragma once

#include <postgresql/libpq-fe.h>
#include <string>

namespace pc
{
   namespace pqpp
   {
      class Result
      {
       protected:
         PGresult* res;

       public:
         Result(PGresult* res = NULL) : res(res) {}
         ~Result()
         {
            PQclear(res);
         }
         operator bool() const
         {
            return PQresultStatus(res) == PGRES_COMMAND_OK;
         }
      };
   } // namespace pqpp
} // namespace pc
