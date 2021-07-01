#pragma once

#include <postgresql/libpq-fe.h>
#include <string>

#include <pc/pqpp/IterateResult.hpp>
#include <pc/pqpp/Result.hpp>
#include <vector>

namespace pc
{
   namespace pqpp
   {
      class Connection
      {
         PGconn* conn;

       public:
         Connection()
         {
            conn = PQconnectdb("postgresql://postgres@localhost:5432/");
         }
         ~Connection()
         {
            if (*this)
               PQfinish(conn);
         }
         operator bool() const
         {
            return PQstatus(conn) == CONNECTION_OK;
         }
         Result exec(std::string exec)
         {
            return (PQexec(conn, exec.c_str()));
         }
         Result exec(std::string exec, std::vector<const char*> const& params)
         {
            return (PQexecParams(conn,
                                 exec.c_str(),
                                 params.size(), /* one param */
                                 NULL,          /* let the backend deduce param type */
                                 params.data(),
                                 NULL, /* don't need param lengths since text */
                                 NULL, /* default to all text params */
                                 0 /* ask for binary results */));
         }
         IterateResult iterate(std::string exec)
         {
            return (PQexec(conn, exec.c_str()));
         }
         IterateResult iterate(std::string exec, std::vector<const char*> const& params)
         {
            return (PQexecParams(conn,
                                 exec.c_str(),
                                 params.size(), /* one param */
                                 NULL,          /* let the backend deduce param type */
                                 params.data(),
                                 NULL, /* don't need param lengths since text */
                                 NULL, /* default to all text params */
                                 0 /* ask for binary results */));
         }

         int socket() const
         {
            return PQsocket(conn);
         }
      };
   } // namespace pqpp
} // namespace pc