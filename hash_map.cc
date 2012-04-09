// map::find
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <map>
#include "hash_map.h"

using namespace std;
map<unsigned long,size_t> mymap;
map<unsigned long,size_t>::iterator it;

void hash_insert( unsigned long key, size_t val )
{
	it =  mymap.find( key );
	if ( it != mymap.end()) 
	{
	  mymap[key] = val;
	}else {
	   mymap[key] = val;
	 }
}

size_t hash_find( unsigned long key) {

	 size_t val =0;

	 it =  mymap.find(key );
	if ( it != mymap.end())
	{
	  val =  it->second;
	}else {
	   val = 0;
	}
	return val ;
}


void hash_delete( unsigned long key) {
 
	 it =  mymap.find(key );
	if ( it != mymap.end())
	{
		mymap.erase (it);

	}
}

size_t find_hash_total() {

	size_t total_val =0;

	for( it =  mymap.begin(); it != mymap.end(); it++){
		total_val += it->second;			
	}

	return total_val;
}


/*int main() {
 char str[6] = "hello";
 hash_insert( str );
 hash_insert( str );
}*/
/*----------------------------*/
