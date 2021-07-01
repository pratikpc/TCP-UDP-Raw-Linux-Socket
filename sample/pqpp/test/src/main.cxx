#include <cstdlib>
#include <iostream>

#include <pc/pqpp/Connection.hpp>

int main()
{
   pc::pqpp::Connection connection("postgresql://postgres@localhost:5432/");
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
   }
   {
      std::vector<const char*> params(2);
      params[0]            = "CLIENT-2";
      params[1]            = "98";
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
      std::cout << "Data inserted\n";
   }
   {
      pc::pqpp::IterateResult res =
          connection.iterate("SELECT clientId, priority FROM priority_table");
      std::cout << "Processing table\n";
      while (res)
      {
         std::cout << "Client ID = " << res[0] << " : "
                   << "priority = " << res[1] << "\n";
         ++res;
      }
   }
   // Select single client which exists
   {
      std::vector<const char*> params(2);
      params[0]                   = "CLIENT-1";
      params[1]                   = "45"; // DEFAULT VALUE. UNUSED
      pc::pqpp::IterateResult res = connection.iterate(
          "SELECT coalesce(MAX(priority),$2) AS priority FROM priority_table WHERE "
          "clientId=$1",
          params);
      std::cout << "Client exists so existing value found" << std::endl;
      std::cout << "Client ID = " << params[0] << " : "
                << "priority = " << res[0] << "\n";
   }
   // Select single client which does not exist
   {
      std::cout << "Client does not exist so default value returned\n";
      std::vector<const char*> params(2);
      params[0] = "CLIENT-3";
      params[1] = "45"; // DEFAULT

      pc::pqpp::IterateResult res = connection.iterate(
          "SELECT coalesce(MAX(priority),$2) AS priority FROM priority_table WHERE "
          "clientId=$1",
          params);
      std::cout << "Client ID = " << params[0] << " : "
                << "priority = " << res[0] << "\n";
   }
   return EXIT_SUCCESS;
}
