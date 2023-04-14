#include <cache/L1_cache.h>

L1Cache::L1Cache() {
    int i=0;
    sets.resize(no_sets);

    for(i; i < no_sets; i++){
        sets[i] = new Set(no_ways);
    }

    misses = 0;
    upgrade_misses = 0;
    accesses = 0;
    ott = new OTT();
    buffer = new bufferl1();
}

void L1Cache::lookup(Block *_block) {
    for (int i = 0; i < no_ways; i++) {
        if (sets[_block->index]->blocks[i].valid && sets[_block->index]->blocks[i].tag == _block->tag) {
            _block->way = i;
            // CHANGED IN COPENHEGAN 
            _block->block_state = sets[_block->index]->blocks[i].block_state;
            //
            _block->valid = true;
            return;
        }
    }
}

void L1Cache::invalidate(Block *_block) {
    sets[_block->index]->invalid_ways.insert(_block->way);
    sets[_block->index]->blocks[_block->way].valid = false;
    // CHANGED IN COPENHEGAN 
    sets[_block->index]->blocks[_block->way].block_state = INVALID; 
    //
    sets[_block->index]->blocks[_block->way].tag = 0;
}

void L1Cache::update_repl_params(int index, int way) {
    
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

int L1Cache::invoke_repl_policy(int index) {
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
    
    // Copenhegan
    victim->block_state  = _tmp.block_state;
    //
    return _victim_way;
}

void L1Cache::copy(Block *_block) {

    _block->way = get_target_way(_block->index);

    if (_block->way < 0) {
        _block->way = invoke_repl_policy(_block->index);

    }

    sets[_block->index]->invalid_ways.erase(_block->way);
    _block->valid = 1; // to be sure
    sets[_block->index]->blocks[_block->way] = *_block;

}

int L1Cache::get_target_way(int index) {

    if (sets[index]->invalid_ways.empty())
        return -1;

    return *sets[index]->invalid_ways.begin();

}

void L1Cache::get_block(unsigned long long addr,
            Block *dst) {
    dst->addr = addr;  
    addr = addr >> no_block_bits;
    dst->index = addr & mask;
    dst->tag = addr >> no_index_bits;

}

bool L1Cache::empty_msg_queue() {
    return msgs.empty();
}

bool L1Cache::empty_trace_queue() {
    return trace_input.empty();
}

void L1Cache::set_block_state(int index, int way, state new_state) {
    sets[index]->blocks[way].block_state = new_state;
}

void L1Cache::queue_msg(Msg *_msg) {
    msgs.push(_msg);
}