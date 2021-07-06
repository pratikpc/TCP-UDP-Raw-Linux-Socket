#include <pc/deadliner/MostRecentTimestamps.hpp>
#include <vector>

#include <iostream>

int main()
{
   using namespace pc::deadliner;
   std::vector<std::size_t> indexes(10);

   MostRecentTimestamps<std::vector<std::size_t> /* */> timestamps;
   for (std::vector<std::size_t>::iterator it = indexes.begin(); it != indexes.end();
        ++it)
   {
      *it = (it - indexes.begin()) * 24;
      timestamps.insert(it - indexes.begin(), it);
   }
   timestamps.insert(5, indexes.begin());
   for (MostRecentTimestamps<std::vector<std::size_t> /* */>::iterator it =
            timestamps.begin();
        it != timestamps.end();
        ++it)
   {
      std::cout << "========\n"
                << "Socket : " << it->first << "\nTimestamp : " << it->second.first
                << "\nIterator pointed to value : " << *it->second.second << "\n========\n"
                << std::endl;
   }
}