#pragma once
#include<simulator.h>

Simulator::Simulator(string f_name){

    for(int i = 0; i < THREAD_COUNT; i++){
        string new_f_name = f_name + "." + to_string(i);
        f_traces[i].open(new_f_name.c_str(), ios::in);
    }

    l1_caches.resize(CORES);
    for (int i = 0; i < CORES; i++)
        l1_caches[i] = new L1Cache();

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
}   

bool Simulator::end_condition(bool started){
    bool l1_trace_empty = true;
    bool l1_msg_empty = true;
    for(int i = 0; i < THREAD_COUNT; i++){
        if(!l1_caches[i]->empty_trace_queue()){
            l1_trace_empty = false;
            break;
        } 
        if(!l1_caches[i]->empty_msg_queue()){
            l1_msg_empty = false;
            break;
        }
    }

    return started && l1_trace_empty && l1_msg_empty && l2_cache->empty_msg_queues();
}

// TODO
Trace* process_trace_line(string trace_line){
    //to complete;
}

void Simulator::execute_part2(){
    if(tmp_trace != nullptr)
        mesi->process_trace(tmp_trace);

    for(int i = 0 ; i < THREAD_COUNT; i++){
        if(tmp_msg1_queue[i] != nullptr) 
            mesi->process_l1_msg(tmp_msg1_queue[i]);
        if(tmp_msg2_queue[i] != nullptr) 
            mesi->process_l2_msg(tmp_msg2_queue[i], i);
    }
    
}

void Simulator::start_simulator(){
    bool started = false;
    while(1){
        if(end_condition(started)) break;
        //posting trace entries to queue and find one trace to process;
        for(int i = 0; i < THREAD_COUNT; i++){
            tmp_trace = nullptr;
            queue<Trace *>& ti = l1_caches[i]->trace_input;
            auto change_trace = [&](){
                if(!ti.empty() && ti.front()->global_id == global_counter_to_process){
                    tmp_trace = ti.front();
                    ti.pop();
                }
            };
            if(ti.size() >= MAX_TRACE_QUEUE_SIZE){
                change_trace();
                continue;
            };
            string trace_line;
            if(!getline(f_traces[i], trace_line)){
                change_trace();
                continue;
            };    
            Trace* trace = process_trace_line(trace_line);
            ti.push(trace);
            change_trace();
        }

        //finding head of all input queues to l1 and l2
        for(int i = 0; i < THREAD_COUNT; i++){
            auto& l1_msgs = l1_caches[i]->msgs;
            auto& l2_msgs = l2_cache->msg_queues[i];
            if(!l1_msgs.empty()){
                tmp_msg1_queue[i] = l1_msgs.front();
                l1_msgs.pop();
            }
            else tmp_msg1_queue[i] = nullptr;
            if(l2_msgs.empty()){
                tmp_msg2_queue[i] = l2_msgs.front();
                l2_msgs.pop();
            }
            else tmp_msg2_queue[i] = nullptr;
        }

        //PART 2
        execute_part2();

        global_counter_to_process++;
        free(tmp_trace);
    }
}

