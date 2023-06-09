#include <simulator.h>

map<msg_type, string> msg_names = {{GET, "GET"},
                                   {GETX, "GETX"},
                                   {PUT, "PUT"},
                                   {PUTX, "PUTX"},
                                   {WB, "WB"},
                                   {WB_ACK, "WB_ACK"},
                                   {NACK, "NACK"},
                                   {NACKE, "NACKE"},
                                   {INV, "INV"},
                                   {SWB, "SWB"},
                                   {INV_ACK, "INV_ACK"},
                                   {UPGR, "UPGR"},
                                   {UPGR_ACK, "UPGR_ACK"},
                                   {PUTE, "PUTE"},
                                   {NUM_MSG_TYPES, "NUM_MSG_TYPES"}};

map<state, string> state_names = {{SHARED, "SHARED"},
                                  {MODIFIED, "MODIFIED"},
                                  {PSH, "PSH"},
                                  {PDEX, "PDEX"},
                                  {INVALID, "INVALID"},
                                  {UNOWNED, "UNOWNED"},
                                  {EXCLUSIVE, "EXCLUSIVE"}};

static inline int get_home_node(unsigned long long addr, L2Cache *l2_cache)
{
    int node;
    node = addr & l2_cache->bank_mask;
    return node;
}

Simulator::Simulator(string f_name)
{
    log("Simulator construct starting...");
    f_traces.resize(CORES);
    for (int i = 0; i < THREAD_COUNT; i++)
    {
        string new_f_name = f_name + "." + to_string(i);
        log("processing file " << new_f_name);
        f_traces[i].open(new_f_name.c_str(), ios::in);
    }
    log("trace files opened");
    l1_caches.resize(CORES);
    for (int i = 0; i < CORES; i++)
        l1_caches[i] = new L1Cache(i);

    log("L1 cache initialized");
    l2_cache = new L2Cache();

    l1_block = new Block();
    l2_block = new Block();

    mesi = new Mesi();
    mesi->l1_caches = l1_caches;
    mesi->l2_cache = l2_cache;

    cycle_counter = 0;
    global_counter_to_process = 0;

    tmp_msg1_queue.resize(THREAD_COUNT);
    tmp_msg2_queue.resize(THREAD_COUNT);
    log("simulator constructor done");
    print_specs();
}

//TODO: update it to include pending msgs and ott check
bool Simulator::end_condition()
{
    bool l1_trace_empty = true;
    bool l1_msg_empty = true;
    for (int i = 0; i < THREAD_COUNT; i++)
    {
        if (!l1_caches[i]->empty_trace_queue())
        {
            l1_trace_empty = false;
        }
        if (!l1_caches[i]->empty_msg_queue())
        {
            l1_msg_empty = false;
        }
    }
//    log(l1_trace_empty);
//    log(l1_msg_empty);
//    log(l2_cache->empty_msg_queues());
    return l1_trace_empty && l1_msg_empty && l2_cache->empty_msg_queues();
}

// TODO
Trace *process_trace_line(string trace_line)
{
    //to complete;
    if(trace_line == "") return nullptr;
    Trace *trace = new Trace();
    stringstream trace_line_ss(trace_line);
    trace_line_ss >> trace->thread_id >> trace->address >> trace->request >> trace->global_id;
    trace->address >>= BLOCK_BITS;
//    log("tid: " << trace->thread_id << ", global_id: " << trace->global_id << ", address: " << trace->address << ", type: " << trace->request);
    return trace;
}

void Simulator::fill_traces() {
    string trace_line;
    Trace *_tmp_trace;
    for(int i = 0; i < THREAD_COUNT; i++) {
        while(getline(f_traces[i], trace_line)) {
            _tmp_trace = process_trace_line(trace_line);
            if (_tmp_trace != nullptr)
                l1_caches[i]->trace_input.push(_tmp_trace);

        }
        cout << "Core " << i << ": Trace input size: " << l1_caches[i]->trace_input.size() << "\n";
    }

}

void Simulator::execute_part2(bool &executed_something)
{
    if (tmp_trace != nullptr) {
        executed_something = true;
        mesi->process_trace(tmp_trace);
    }
    for (int i = 0; i < THREAD_COUNT; i++)
    {
        if (tmp_msg1_queue[i] != nullptr){
            executed_something = true;
            mesi->process_l1_msg(tmp_msg1_queue[i], i);
        }

        if (tmp_msg2_queue[i] != nullptr) {
            executed_something = true;
            mesi->process_l2_msg(tmp_msg2_queue[i], i);
        }
    }
}

void Simulator::start_simulator() {
    bool started = false;
    int selected_core = -1;
    string trace_line;
//    int executed_something = 6;
    bool executed_something = false;
    bool nack_exec = false;
    //will give error in prog1
//    fill_traces();
    do {
        log("cycle started");
        unsigned long long min_global_id = LONG_LONG_MAX;
        tmp_trace = nullptr;
        nack_exec = false;
        executed_something = false;
        selected_core = -1;
        // NACK timer handling
        for (int i = 0; i < THREAD_COUNT; i++) {
            l1_caches[i]->ott->decrement_timer();
            log("inside nack handling");
            vector<Ott_entry *> &reprocess_msgs = l1_caches[i]->ott->nackTimer.front();
            // Retry all the requests
            log("before reprocessing trace");
            for (Ott_entry *entry: reprocess_msgs) {
                assert(entry != nullptr);
//                log("reprocessing traces");
                Msg *new_msg = new Msg(entry->_msg);
                entry->invalid = false;
                l2_cache->queue_msg(new_msg, get_home_node(new_msg->addr, l2_cache));
                executed_something = true;
                nack_exec = true;
//                if(entry->_block.addr == 638588){
//                    log("Nack exiting for addr: 638588");
//                    exit(1);
//                }
            }
        }
        log("before collecting trace");
        if (!nack_exec) {
            // select trace entry to process
            for (int i = 0; i < THREAD_COUNT; i++) {
                Trace *_t2;
                _t2 = nullptr;
                queue<Trace *> &ti = l1_caches[i]->trace_input;

                //load new traces
                if (ti.empty() && getline(f_traces[i], trace_line)) {
                    Trace *_tmp_trace = process_trace_line(trace_line);
                    if (_tmp_trace != nullptr)
                        l1_caches[i]->trace_input.push(_tmp_trace);
                }

                if (ti.empty())
                    continue;

                _t2 = ti.front();log("core " << i << ": head id: " << _t2->global_id);
                if (_t2->global_id < min_global_id) {
                    min_global_id = _t2->global_id;
                    selected_core = i;
                }
            }
            if (min_global_id == global_counter_to_process) { log("selected global id: " << min_global_id);
                global_counter_to_process++;
                tmp_trace = l1_caches[selected_core]->trace_input.front();
                l1_caches[selected_core]->trace_input.pop();

            }
        }
        log("before collecting msgs");
        for (int i = 0; i < THREAD_COUNT; i++) {
            auto &l1_msgs = l1_caches[i]->msgs;
            auto &l2_msgs = l2_cache->msg_queues[i];
            if (!l1_msgs.empty()) {
                tmp_msg1_queue[i] = l1_msgs.front();
                l1_msgs.pop();
            } else
                tmp_msg1_queue[i] = nullptr;
            if (!l2_msgs.empty()) {
                tmp_msg2_queue[i] = l2_msgs.front();
                l2_msgs.pop();
            } else
                tmp_msg2_queue[i] = nullptr;
        }
        log("Before part2");
        execute_part2(executed_something);
        log("cycle counter: " << cycle_counter);
        cycle_counter++;
//        if (!executed_something){
//            break;
//        }
    } while (!end_condition());

}

void Simulator::print_stats() {
    //maybe beautify this

    int bank_id = 0;
    cout << "Cycles: " << cycle_counter << "\n";
    cout << "------L1 STATS-------\n";
    for (auto cache:l1_caches){
        cout << "\t------ CORE " << cache->ID << " --------\n";
        cout << "\tAccesses: " << cache->accesses;
        cout << "\tMisses: " << cache->misses;
        cout << "\tUpgrade Misses: " << cache->upgrade_misses << "\n";
        for (auto it: msg_names) {
            cout << "\t" << it.second << ": " << cache->num_msgs[it.first] << "\n";
        }
    }
    cout << "------L2 STATS-------\n";
    cout << "\t Misses: " << l2_cache->misses << '\n';
    for (auto &vec:l2_cache->num_msgs){
        cout << "\t------ BANK" << bank_id++ << " --------\n";
//        cout << "\tAccesses: " << cache->accesses;
        //cout << "\tMisses: " << cache->misses;
//        cout << "\tUpgrade Misses: " << ;
        for (auto it: msg_names) {
            cout << "\t" << it.second << ": " << vec[it.first] << "\n";
        }
    }
}

void Simulator::print_specs() {
    cout << "--------L1 Cache-------\n";
    cout << "Ways: " << l1_caches[0]->no_ways << "\n";
    cout << "Sets: " << l1_caches[0]->no_sets << "\n";
    cout << "--------L2 Cache-------\n";
    cout << "Ways: " << l2_cache->no_ways << "\n";
    cout << "Sets: " << l2_cache->no_sets << "\n";
}

void Simulator::print_end_states(){
    cout << "---------------------Ending condition-------------------------\n";
    for(int core = 0; core < CORES; core++){
        cout << "------- L1 core " << core << " ----------\n";
        L1Cache* temp_l1_cache = l1_caches[core];
        //check for OTT entry
        cout << "\tOTT table size: " << temp_l1_cache->ott->table.size() << '\n';

        int leftotts = 1;
//        if(temp_l1_cache->ott->table.size() != 0){
//
//            for (auto &[pending_addr,_entry]: temp_l1_cache->ott->table){
//                cout << "addr: " << pending_addr << " global_id: " << _entry->_msg.global_id << " msg_type: "
//                     << msg_names[_entry->_msg.type] << " ott invalid: " << (_entry->invalid ? "true" : "false") << '\n';
//
//            }
//
//        }

        //check for trace queue
        cout << "\tTrace queue empty: " << (temp_l1_cache->empty_trace_queue() ? "True" : "False") << '\n';
//        if(temp_l1_cache->empty_trace_queue() == false){
//            cout << "\t Trace queue size: " << temp_l1_cache->trace_input.size() << '\n';
//            queue<Trace*>& tr_queue = temp_l1_cache->trace_input;
//            while(!tr_queue.empty()){
//                Trace* curr_trace = tr_queue.front();
//                cout << '\n';
//                cout << "Global_id: " << curr_trace->global_id << " addr: " << curr_trace->address << " type: " << curr_trace->request << "\n";
//                cout << '\n';
//                tr_queue.pop();
//            }
//        }
        cout << "\tMsg queue empty: " << (temp_l1_cache->msgs.empty() ? "True" : "False") << '\n';
        cout << "\tOTT table trace buffer empty: " << (temp_l1_cache->miss_trace_buffer->empty_trace_buffer() ? "True" : "False") << '\n';
        cout << "\tOTT table pending msgs empty: " << (temp_l1_cache->pending_msgs_buffer->empty_pending_msgs() ? "True" : "False") << '\n';
    }
    cout << '\n';
    cout << "-------L2 Cache--------\n";
    cout << "\tMsg queue empty: " << (l2_cache->empty_msg_queues() ? "True" : "False") << '\n';
}
