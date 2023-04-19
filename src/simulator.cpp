#include <simulator.h>


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
    Trace *trace = new Trace();
    stringstream trace_line_ss(trace_line);
    trace_line_ss >> trace->thread_id >> trace->address >> trace->request >> trace->global_id;
    trace->address >>= BLOCK_BITS;

    return trace;
}

void Simulator::execute_part2()
{
    if (tmp_trace != nullptr)
        mesi->process_trace(tmp_trace);

    for (int i = 0; i < THREAD_COUNT; i++)
    {
        if (tmp_msg1_queue[i] != nullptr)
            mesi->process_l1_msg(tmp_msg1_queue[i], i);
        if (tmp_msg2_queue[i] != nullptr)
            mesi->process_l2_msg(tmp_msg2_queue[i], i);
    }
}
void Simulator::start_simulator() {
    bool started = false;
    int selected_core = -1;
    string trace_line;
    do {
        // Cycle counter increment
        cycle_counter++;
        selected_core = -1;
        // NACK timer handling
        for(int i=0; i < THREAD_COUNT; i++) {
            l1_caches[i]->ott->decrement_timer();
            vector<Ott_entry *> &reprocess_msgs = l1_caches[i]->ott->nackTimer.front();
            for(Ott_entry *entry: reprocess_msgs) {
                Msg *new_msg = new Msg(entry->_msg);
                entry->invalid = false;
                l2_cache->queue_msg(new_msg, get_home_node(new_msg->addr, l2_cache));
            }
        }

        // select trace entry to process
        for(int i =0; i < THREAD_COUNT; i++){
            tmp_trace = nullptr;
            queue<Trace*> &ti = l1_caches[i]->trace_input;
            if(getline(f_traces[i], trace_line)){
                tmp_trace = process_trace_line(trace_line);
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
        }
        else
            tmp_trace = nullptr;

        for (int i = 0; i < THREAD_COUNT; i++)
        {
            auto &l1_msgs = l1_caches[i]->msgs;
            auto &l2_msgs = l2_cache->msg_queues[i];
            if (!l1_msgs.empty())
            {
                tmp_msg1_queue[i] = l1_msgs.front();
                l1_msgs.pop();
            }
            else
                tmp_msg1_queue[i] = nullptr;
            if (l2_msgs.empty())
            {
                tmp_msg2_queue[i] = l2_msgs.front();
                l2_msgs.pop();
            }
            else
                tmp_msg2_queue[i] = nullptr;
        }

        execute_part2();
        log(cycle_counter);
        global_counter_to_process++;

    } while(!end_condition());

}


//void Simulator::start_simulator()
//{
//    log("Starting the simulator...");
//    bool started = false;
//    while (1)
//    {
//        // nack handling done
//        // TODO: handle pending msgs over here
//        for (int i = 0; i < THREAD_COUNT; i++)
//        {
//            l1_caches[i]->ott->decrement_timer();
//            vector<Ott_entry *> &reprocess_traces = l1_caches[i]->ott->nackTimer.front();
//            for (Ott_entry *ott_entry : reprocess_traces)
//            {
//                Msg *new_msg = new Msg(ott_entry->_msg);
//                ott_entry->invalid = false;
//                l2_cache->queue_msg(new_msg, get_home_node(new_msg->addr, l2_cache));
//            }
//        }
//
////        log("post nack");
//
//        if (end_condition(started))
//            break;
//
//        started = true;
//        //posting trace entries to queue and find one trace to process;
//        for (int i = 0; i < THREAD_COUNT; i++)
//        {
//            tmp_trace = nullptr;
//            queue<Trace *> &ti = l1_caches[i]->trace_input;
//            auto change_trace = [&]()
//            {
//                if (!ti.empty() && ti.front()->global_id == global_counter_to_process)
//                {
//                    tmp_trace = ti.front();
//                    ti.pop();
//                    mesi->process_trace(tmp_trace);
//                }
//            };
//            if (ti.size() >= MAX_TRACE_QUEUE_SIZE)
//            {
//                change_trace();
//                break;
//            };
//            string trace_line;
//            if (!getline(f_traces[i], trace_line))
//            {
//                change_trace();
//                break;
//            };
//            Trace *trace = process_trace_line(trace_line);
//            ti.push(trace);
//            change_trace();
//        }
////        log("trace done");
//        //finding head of all input queues to l1 and l2
//        for (int i = 0; i < THREAD_COUNT; i++)
//        {
//            auto &l1_msgs = l1_caches[i]->msgs;
//            auto &l2_msgs = l2_cache->msg_queues[i];
//            if (!l1_msgs.empty())
//            {
//                tmp_msg1_queue[i] = l1_msgs.front();
//                l1_msgs.pop();
//            }
//            else
//                tmp_msg1_queue[i] = nullptr;
//            if (l2_msgs.empty())
//            {
//                tmp_msg2_queue[i] = l2_msgs.front();
//                l2_msgs.pop();
//            }
//            else
//                tmp_msg2_queue[i] = nullptr;
//        }
//
//        //PART 2
//        execute_part2();
//
//        global_counter_to_process++;
////        free(tmp_trace);
//
//    }
//}
