#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#ifdef __cplusplus
extern "C" {
#endif


void hash_insert( unsigned long key, size_t val);
size_t hash_find( unsigned long key);
void hash_delete( unsigned long key);
size_t find_hash_total();

#ifdef __cplusplus
};
#endif


