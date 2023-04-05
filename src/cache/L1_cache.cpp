#include <L1_cache.h>

L1Cache::L1Cache() {
    int i=0;
    sets.resize(no_sets);

    // I have created a single copy of empty set and then copied it to all the sets
    // This should be faster than initializing all the sets individually
    for(i; i < no_sets; i++){
        sets[i] = new Set(no_ways);
    }
    
    tag_index.clear();
}