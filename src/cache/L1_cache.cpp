#include <simulator.h>

extern map<msg_type, string> msg_names;

L1Cache::L1Cache(int id) {
    int i=0;
    ID = id;
    sets.resize(no_sets);

    for(i; i < no_sets; i++){
        sets[i] = new Set(no_ways);
    }

    misses = 0;
    upgrade_misses = 0;
    accesses = 0;
    num_msgs.resize(NUM_MSG_TYPES + 1);
    victim = NULL;
    ott = new OTT();

    miss_trace_buffer = new trace_buffer();
    pending_msgs_buffer = new pending_msgs();
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
    _block->valid = false;
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
    struct list_item *curr_elem = sets[index]->repl_list.head->prev;
    struct list_item *_victim;
    bool all_busy = true;
    do {
        if (!ott->check_entry(sets[index]->blocks[curr_elem->way].addr))
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
    victim->block_state  = _tmp.block_state;
    //
    return _victim_way;
}

void L1Cache::copy(Block *_block) {
    log("Copy to L1 core " << ID << " for block addr: " << _block->addr);
    _block->way = get_target_way(_block->index);

    if (_block->way < 0) {
        log("no way found, start replacement");
        _block->way = invoke_repl_policy(_block->index);

    }
//    sets[_block->index]->invalid_ways.erase(_block->way);
    _block->valid = 1; // to be sure
    sets[_block->index]->blocks[_block->way] = *_block;
}

int L1Cache::get_target_way(int index) {
    for (int i =0; i < no_ways; i++){
        if (!sets[index]->blocks[i].valid){
            log("way = " << i);
            return i;
        }
    }
    return -1;
}

void L1Cache::get_block(unsigned long long addr,
            Block *dst) {
    // addr = addr >> no_block_bits;
    dst->addr = addr;
    dst->index = addr & mask;
    dst->tag = addr >> no_index_bits;

}

bool L1Cache::empty_msg_queue() {
    return (msgs.empty() && ott->table.empty());
}

bool L1Cache::empty_trace_queue() {
    return trace_input.empty();
}

void L1Cache::set_block_state(int index, int way, state new_state) {
    sets[index]->blocks[way].block_state = new_state;
}

void L1Cache::queue_msg(Msg *_msg) {
//    log("core: " << ID << " msg: " << msg_names[_msg->type] << " global_id: " << _msg->global_id);
    msgs.push(_msg);
}

void L1Cache::insert_to_pending(Msg *_msg){
    log("core: " << ID << " addr: " << _msg->addr);
    pending_msgs_buffer->buffer[_msg->addr].push(*_msg);
}

Ott_entry* L1Cache::find_ott_entry(unsigned long long addr){
    if(ott->check_entry(addr)){
        return ott->table[addr];
    }
    return nullptr;
}