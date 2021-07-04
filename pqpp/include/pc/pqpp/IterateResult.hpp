#pragma once

#include <pc/pqpp/Result.hpp>
#include <postgresql/libpq-fe.h>
#include <string>

#include <cassert>

namespace pc
{
   namespace pqpp
   {
      class IterateResult : pc::pqpp::Result
      {
         int currentTuple;

       public:
         IterateResult(PGresult* res = NULL) : pc::pqpp::Result(res), currentTuple(0) {}
         operator bool() const
         {
            return PQresultStatus(res) == PGRES_TUPLES_OK && currentTuple < rowCount();
         }
         friend IterateResult& operator++(IterateResult& res)
         {
            ++res.currentTuple;
            return res;
         }
         int rowCount() const
         {
            return PQntuples(res);
         }
         int fieldCount() const
         {
            return PQnfields(res);
         }
         std::string operator[](int const field) const
         {
            assert(field < fieldCount());
            if (PQgetisnull(res, currentTuple, field))
               return "";
            return std::string(PQgetvalue(res, currentTuple, field));
         }
      };
   } // namespace pqpp
} // namespace pc
