union bit64 {
  uint64_t six;
  uint32_t thir[2];
}; 

uint64_t my_htobe64(uint64_t);
uint64_t my_be64toh(uint64_t);