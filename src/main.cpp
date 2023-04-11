#include<iostream>
#include<simulator.h>

using namespace std;


int main(int argc, char* argv[]){
    if(argc != 2){
        printf("Usage: ./init5 <trace filename>");
        exit(-1);
    }
    Simulator simulator(argv[1]);

    simulator.start_simulator();

    return 0;
}