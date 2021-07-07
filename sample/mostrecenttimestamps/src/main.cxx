#include <pc/deadliner/MostRecentTimestamps.hpp>
#include <vector>

#include <iostream>

int main()
{
   using namespace pc::deadliner;
   std::vector<std::size_t> indexes(10);

   MostRecentTimestamps timestamps;
   for (std::vector<std::size_t>::iterator it = indexes.begin(); it != indexes.end();
        ++it)
   {
      *it = (it - indexes.begin()) * 24;
      timestamps.updateFor(it - indexes.begin());
   }
   timestamps.updateFor(5);
   for (MostRecentTimestamps::iterator it = timestamps.begin();
        it != timestamps.end();
        ++it)
   {
      std::cout << "========\n"
                << "Socket : " << it->first << "\nTimestamp : " << it->second
                << "\n========\n"
                << std::endl;
   }
}
