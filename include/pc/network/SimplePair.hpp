namespace pc
{
   namespace network
   {
      template <typename T1, typename T2>
      struct SimplePair
      {
         T1 first;
         T2 second;

#if __cplusplus <= 199711L
         SimplePair(T1 const& first, T2 const& second) : first(first), second(second) {}
      };
#endif
   } // namespace network
} // namespace pc