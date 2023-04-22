#include <simulator.h>

extern map<msg_type, string> msg_names;
extern map<state, string> state_names;

L2Cache::L2Cache() {
    misses = 0;
    msg_queues.resize(BANKS);
    sets.resize(this->no_sets);
    for(int i=0; i < this->no_sets; i++)
        sets[i] = new Set(no_ways);
    
    victim = NULL;
    num_msgs.resize(BANKS);
    for (int i=0; i < BANKS; i++)
        num_msgs[i].resize(NUM_MSG_TYPES+1);

    eb_buffer = new evicted_blocks();
    eb_buffer->buffer.clear();

}

void L2Cache::lookup(Block *_block) {
    for (int i = 0; i < no_ways; i++) {
        if (sets[_block->index]->blocks[i].valid && sets[_block->index]->blocks[i].tag == _block->tag) {
            _block->way = i;
            // CHANGED IN COPENHEGAN
            _block->dir_entry = sets[_block->index]->blocks[i].dir_entry;
            //
            _block->valid = true;
            return;
        }
    }
    _block->valid=false;
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
    // change here to check for victim way to have state psh or pdex, avoid those

    struct list_item *curr_elem = sets[index]->repl_list.head->prev;
    struct list_item *_victim;
    bool all_busy = true;
    do {
        if (sets[index]->blocks[curr_elem->way].block_state != PSH && sets[index]->blocks[curr_elem->way].block_state != PDEX)
        {
            _victim = curr_elem;
            all_busy = false;
            break;
        }
        curr_elem = curr_elem->prev;
    } while(curr_elem != sets[index]->repl_list.head);

    if(all_busy){
        _victim = sets[index]->repl_list.head->prev;
    }


    _victim_way = _victim->way;

    if (_victim_way == -1) {
        throw_error("error in replacment list of index %d\n", index);
    }

    _tmp = sets[index]->blocks[_victim_way];

    
    victim = new Block(_tmp.index, _tmp.way, _tmp.tag, _tmp.addr);
    
    // Copenhegan
    victim->dir_entry = _tmp.dir_entry;
    // 
    return _victim_way;
}

void L2Cache::copy(Block *_block) {

    _block->way = get_target_way(_block->index);
    log("target way: " << _block->way);
    if (_block->way < 0) {
        _block->way = invoke_repl_policy(_block->index);

    }

//    sets[_block->index]->invalid_ways.erase(_block->way);
    _block->valid = 1; // to be sure

    // Copenhegan. Now dir_entry will also be copied
    sets[_block->index]->blocks[_block->way] = *_block;
    log("L2 addr: " << sets[_block->index]->blocks[_block->way].addr << ": dir state: " << sets[_block->index]->blocks[_block->way].dir_entry.curr_state);
    //
}

int L2Cache::get_target_way(int index) {
    auto &_set = sets[index]->blocks;
    for (int i =0; i < no_ways; i++){
        if (!_set[i].valid){
            return i;
        }
    }
    return -1;
}

void L2Cache::get_block(unsigned long long addr,
            Block *dst) {
    
    
    // addr = addr >> no_block_bits;
    ///#############
    dst->addr = addr;
    ///##############
    dst->index = addr & mask;
    dst->tag = addr >> no_index_bits;
}

bool L2Cache::empty_msg_queues() {
    bool ret = true;
    for(auto &q: msg_queues)
        ret = ret && q.empty();

    return ret;
}

void L2Cache::queue_msg(Msg *_msg, int bank) {
    log("bank: " << bank << " msg: " << msg_names[_msg->type] << " global_id: " << _msg->global_id);
    msg_queues[bank].push(_msg);
}

void L2Cache::set_directory_state(state dir_state, int index, int way) {
    auto &dir = sets[index]->blocks[way].dir_entry;

    log("addr: " << sets[index]->blocks[way].addr << ": prev state: " << state_names[dir.curr_state]);

    sets[index]->blocks[way].dir_entry.curr_state = dir_state;
    log("addr: " << sets[index]->blocks[way].addr << ": new state: " << state_names[dir.curr_state]);

}

void L2Cache::set_sharers(vector<int> &sharers, int index, int way) {
    auto &bitvec =  sets[index]->blocks[way].dir_entry.sharer;
    bitvec.reset();
    for(auto i : sharers)
        bitvec[i] = true;

    log( "addr: " << sets[index]->blocks[way].addr  << " dir state: " << sets[index]->blocks[way].dir_entry.curr_state);

}

void L2Cache::set_owner(int owner, int index, int way) {
    auto &bitvec =  sets[index]->blocks[way].dir_entry.sharer;
    int mask = 1;
    bitvec.reset();
    for (int i = 0; i < 8; i++, mask *=2)
        bitvec[i] = owner & mask;

    log( "addr: " << sets[index]->blocks[way].addr << " owner: " << bitvec.to_ulong()  << " dir state: " << sets[index]->blocks[way].dir_entry.curr_state);
}

void L2Cache::add_sharer(int _sharer, int index, int way) {
    sets[index]->blocks[way].dir_entry.sharer[_sharer] = true;
}

void L2Cache::lookup_evicted_blocks(Block *_block) {
    if (eb_buffer->buffer.find(_block->addr) == eb_buffer->buffer.end()) {
        return;
    }
    _block->valid = true;
    _block->dir_entry = eb_buffer->buffer[_block->addr]->_block.dir_entry;
}

void L2Cache::drop_evicted_block(Block *_block) {
    log("Dropping block addr: " << _block->addr);
    if (eb_buffer->buffer.find(_block->addr) == eb_buffer->buffer.end())
        throw_error("%s: evicted block not found\n", __func__);

    free(eb_buffer->buffer[_block->addr]);
    eb_buffer->buffer.erase(_block->addr);
}

int L2Cache::dec_pending_inv(Block *_block) {
    int ret;
    if (eb_buffer->buffer.find(_block->addr) == eb_buffer->buffer.end())
        throw_error("%s: evicted block not found\n", __func__);
    
    return (--eb_buffer->buffer[_block->addr]->pending_invals);
}

void L2Cache::insert_evicted_block(Block *_block, int num_inval) {
    log("inserting block to evicted block l2 addr: " << _block->addr);
    eb_entry *new_entry;
    if (eb_buffer->buffer.find(_block->addr) != eb_buffer->buffer.end())
        throw_error("%s: evicted block already exists\n", __func__);

    new_entry = new eb_entry();
    new_entry->pending_invals = num_inval;
    new_entry->_block =  *_block;
    eb_buffer->buffer[_block->addr] = new_entry;
}

