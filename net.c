#include <arpa/inet.h>
#include "net.h"

uint64_t __my_be64toh(uint64_t big_endian) {
  union bit64 in;
  union bit64 res;
  
  in.six=big_endian;
  res.thir[0]=ntohl(in.thir[1]);
  res.thir[1]=ntohl(in.thir[0]);
  
  return res.six;
}

uint64_t __my_htobe64(uint64_t little_endian) {
  union bit64 in;
  union bit64 res;
  
  in.six=little_endian;
  res.thir[0]=htonl(in.thir[1]);
  res.thir[1]=htonl(in.thir[0]);
  
  return res.six;
}

uint64_t my_htobe64(uint64_t x) {
  if( 1 == htonl(1))
    return x;
  else
    return __my_be64toh(x);
}

uint64_t my_be64toh(uint64_t x) {
  if( 1 == htonl(1))
    return x;
  else
    return __my_be64toh(x);
}