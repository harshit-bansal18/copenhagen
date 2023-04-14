#pragma once

#include <vector>
#include <replacement.h>
#include <protocol/mesi.h>
using namespace std;

class Block {
    public:
    unsigned long long addr;
    unsigned long tag;
    unsigned long index;
    unsigned int way;
    bool  valid;
    Directory_entry dir_entry; // for L2 cache
    state block_state; // for L1 cache
    // field for state bit
    Block();
    Block(unsigned long _index, unsigned int _way, unsigned long _tag, unsigned long long _addr);

};

// Contain the blocks in one set
// LRU information for the set
class Set {
    public:
    vector<Block> blocks;
    set<int> invalid_ways;
    struct access_list repl_list;

    Set(int _ways);

};