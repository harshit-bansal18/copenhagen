#include <cache/L2_cache.h>


L2Cache::L2Cache() {
    misses = 0;
    msg_queues.resize(BANKS);
    sets.resize(this->no_sets);
    for(int i=0; i < this->no_sets; i++)
        sets[i] = new Set(no_ways);
}

void L2Cache::lookup(Block *_block) {
    for (int i = 0; i < no_ways; i++) {
        if (sets[_block->index]->blocks[i].valid && sets[_block->index]->blocks[i].tag == _block->tag) {
            _block->way = i;
            // CHANGED IN COPENHEGAN
            // Here dir_entry should be in sync with all the changes. Hence direct pointer is given
            _block->dir_entry = sets[_block->index]->blocks[i].dir_entry;
            //
            _block->valid = true;
            return;
        }
    }
}

void L2Cache::invalidate(Block *_block) {
    sets[_block->index]->invalid_ways.insert(_block->way);
    sets[_block->index]->blocks[_block->way].valid = false;
    sets[_block->index]->blocks[_block->way].tag = 0;
}

void L2Cache::update_repl_params(int index, int way) {
    
    struct list_item *_item = sets[index]->repl_list.find_item(way);
    if (is_null(_item)) {
        _item = (struct list_item*)malloc(sizeof(struct list_item));
        _item->way = way;
        sets[index]->repl_list.add_item(_item);
        return;
    }
    
    // If way is previously accessed, move it to the head of list
    sets[index]->repl_list.move_to_front(_item);
}

int L2Cache::invoke_repl_policy(int index) {
    int _victim_way = -1;
    Block _tmp;
    // it should set the victim cache block
    // Victim will be the tail of list
    struct list_item *_victim = sets[index]->repl_list.head->prev;
    _victim_way = _victim->way;

    if (_victim_way == -1) {
        throw_error("error in replacment list of index %d\n", index);
    }

    _tmp = sets[index]->blocks[_victim_way];

    
    victim = new Block(_tmp.index, _tmp.way, _tmp.tag, _tmp.addr);
    
    return _victim_way;
}

void L2Cache::copy(Block *_block) {

    _block->way = get_target_way(_block->index);

    if (_block->way < 0) {
        _block->way = invoke_repl_policy(_block->index);

    }

    sets[_block->index]->invalid_ways.erase(_block->way);
    _block->valid = 1; // to be sure
    sets[_block->index]->blocks[_block->way] = *_block;

}

int L2Cache::get_target_way(int index) {

    if (sets[index]->invalid_ways.empty())
        return -1;

    return *sets[index]->invalid_ways.begin();

}

void L2Cache::get_block(unsigned long long addr,
            Block *dst) {
    
    ///#############
    dst->addr = addr;
    ///##############
    
    addr = addr >> no_block_bits;
    dst->index = addr & mask;
    dst->tag = addr >> no_index_bits;
}

bool L2Cache::empty_msg_queues() {
    bool ret = true;
    for(auto q: msg_queues)
        ret = ret && q.empty();

    return ret;
}