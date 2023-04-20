#include <simulator.h>

Block::Block() {
    index = -1;
    way = -1;
    valid = false;
    block_state = INVALID;
}

Block::Block(unsigned long _index, unsigned int _way, unsigned long _tag, unsigned long long _addr){
    this->index = _index;
    this->way = _way;
    this->tag = _tag;
    this->addr = _addr;
    this->valid = false;
}

Set::Set(int _ways) {
    blocks.resize(_ways);
    invalid_ways.clear();
    repl_list.item_index.assign(_ways, NULL);
    for(int i=0; i < _ways; i++){
        blocks[i].way = i;
        blocks[i].valid = false;
    }
}