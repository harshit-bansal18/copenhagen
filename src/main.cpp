#include<iostream>
#include<simulator.h>

using namespace std;

int handle_signal(int sig) {
    cout << "received signal " << sig << endl;
    exit(1);
}

int main(int argc, char* argv[]){
    if(argc != 2){
        printf("Usage: ./init5 <trace filename>");
        exit(-1);
    }
    for(int i=0;i<19;i++)
        signal(i, handle_signal);
    Simulator simulator(argv[1]);

    simulator.start_simulator();

    return 0;
}