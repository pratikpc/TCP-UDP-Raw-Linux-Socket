#include <cstdlib>
#include <iostream>

#include <pc/pqpp/Connection.hpp>

int main()
{
   pc::pqpp::Connection connection;
   if (!connection)
   {
      std::cerr << "Connection failure\n";
      return EXIT_FAILURE;
   }
   {
      pc::pqpp::Result res = connection.exec(
          "CREATE TABLE IF NOT EXISTS priority_table ("
          "clientId varchar(45) NOT NULL,"
          "priority smallint NOT NULL,"
          "PRIMARY KEY(clientId)) ");
      if (!res)
      {
         std::cerr << "Failed to create table\n";
         return EXIT_FAILURE;
      }
      std::cout << "\nTable Created";
   }
   {
      std::vector<const char*> params(2);
      params[0]            = "CLIENT-1";
      params[1]            = "21";
      pc::pqpp::Result res = connection.exec(
          "INSERT INTO priority_table(clientId, priority) VALUES($1,$2) ON "
          "CONFLICT (clientId) DO UPDATE "
          "SET priority = cast($2 as SMALLINT);",
          params);
      if (!res)
      {
         std::cerr << "Failed to insert data\n";
         return EXIT_FAILURE;
      }
      std::cout << "\nData inserted";
   }
   {
      pc::pqpp::IterateResult res =
          connection.iterate("SELECT clientId, priority FROM priority_table");
      while (res)
      {
         std::cout << "Processing table\n";
         std::cout << "Client ID = " << res[0] << " : "
                   << "priority = " << res[1] << "\n";
         ++res;
      }
   }
   return EXIT_SUCCESS;
}
