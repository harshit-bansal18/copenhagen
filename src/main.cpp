#include<iostream>
#include<simulator.h>

using namespace std;
ofstream debug;

void handle_signal(int sig) {
    cout << "received signal " << sig << endl;
    debug.flush();
    debug.close();
    exit(1);
}

int main(int argc, char* argv[]){
    if(argc != 3){
        printf("Usage: ./init5 <trace filename> <debug file>\n");
        exit(-1);
    }
    for(int i=0;i<19;i++)
        signal(i, handle_signal);
    
    debug.open(argv[2]);

    Simulator simulator(argv[1]);

    simulator.start_simulator();

    debug.flush();
    debug.close();
    return 0;
}