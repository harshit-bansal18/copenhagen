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
    log("tid: " << trace->thread_id << ", global_id: " << trace->global_id << ", address: " << trace->address << ", type: " << trace->request);
    return trace;
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
    do {
        executed_something = false;
        // Cycle counter increment
//        log("executed_something: " << executed_something);
        cycle_counter++;
        selected_core = -1;
        // NACK timer handling
        for (int i = 0; i < THREAD_COUNT; i++) {
            l1_caches[i]->ott->decrement_timer();
            vector<Ott_entry *> &reprocess_msgs = l1_caches[i]->ott->nackTimer.front();
            // Retry all the requests
            for (Ott_entry *entry: reprocess_msgs) {
                Msg *new_msg = new Msg(entry->_msg);
                entry->invalid = false;
                l2_cache->queue_msg(new_msg, get_home_node(new_msg->addr, l2_cache));
                executed_something = true;
            }
        }

        // select trace entry to process
        for (int i = 0; i < THREAD_COUNT; i++) {
            tmp_trace = nullptr;
            queue<Trace *> &ti = l1_caches[i]->trace_input;
            if (getline(f_traces[i], trace_line)) {
                log("read trace_line: " << trace_line);
                tmp_trace = process_trace_line(trace_line);
                if(tmp_trace != nullptr)
                    ti.push(tmp_trace);
//                log("push done");
            }

            if (ti.empty())
                continue;

            tmp_trace = ti.front();
            if (tmp_trace->global_id == global_counter_to_process) {
                selected_core = i;
            }
        }
        if (selected_core != -1) {
            tmp_trace = l1_caches[selected_core]->trace_input.front();
            l1_caches[selected_core]->trace_input.pop();
        } else
            tmp_trace = nullptr;

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

        execute_part2(executed_something);
        log("cycle counter: " << cycle_counter);
        global_counter_to_process++;
//        if (!executed_something){
//            break;
//        }
    } while (!end_condition());

}

void Simulator::print_stats() {
    int bank_id = 0;
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

    for (auto &vec:l2_cache->num_msgs){
        cout << "\t------ BANK" << bank_id++ << " --------\n";
//        cout << "\tAccesses: " << cache->accesses;
//        cout << "\tMisses: " << cache->misses;
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
//        int leftotts = 1;
//        if(temp_l1_cache->ott->table.size() != 0){
//
//            cout << "-------------- OTT LEFT 1 ----------------"
//        }
        //check for trace queue
        cout << "\tTrace queue empty: " << (temp_l1_cache->empty_trace_queue() ? "True" : "False") << '\n';
        cout << "\tMsg queue empty: " << (temp_l1_cache->empty_msg_queue() ? "True" : "False") << '\n';
        cout << "\tOTT table trace buffer empty: " << (temp_l1_cache->miss_trace_buffer->empty_trace_buffer() ? "True" : "False") << '\n';
        cout << "\tOTT table pending msgs empty: " << (temp_l1_cache->pending_msgs_buffer->empty_pending_msgs() ? "True" : "False") << '\n';
    }
    cout << '\n';
    cout << "-------L2 Cache--------\n";
    cout << "\tMsg queue empty: " << (l2_cache->empty_msg_queues() ? "True" : "False") << '\n';
}