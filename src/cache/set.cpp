#include <set.h>

Set::Set(int _ways) {
    blocks.resize(_ways);
    invalid_ways.clear();

    for(int i=0; i < _ways; i++){
        blocks[i].way = i;
        blocks[i].valid = false;
    }
}