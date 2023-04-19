#ifndef __TRACE_H__
#define __TRACE_H__

#include<iostream>

#define READ 'r'
#define WRITE 'w'

class Trace {
public:
    int thread_id;
    char request;
    unsigned long long address;
    long long global_id;
    
    Trace();
};

#endif