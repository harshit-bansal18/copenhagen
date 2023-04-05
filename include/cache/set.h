#include <vector>
#include <replacement.h>

using namespace std;

class Block {
    public:
    unsigned long tag;
    unsigned long index;
    unsigned int way;
    bool  valid;
    // field for state bit
};

class L2Block: public Block{
    //hold directory here for each block
};

// Contain the blocks in one set
// LRU information for the set
class Set {
    public:
    vector<Block> blocks;
    vector <int> invalid_ways;
    struct access_list repl_list;

    Set(int _ways);

};