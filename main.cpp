#include "topology.h"
#include "workload.h"
#include "simulator.h"

using namespace std;

Topology* topology = nullptr;
Workload* workload = nullptr;
Simulator* simulator = nullptr;

int main(int argc, char** argv){
    
    // srand(time(nullptr));
    srand(0);

    // get current time
    // auto start = chrono::high_resolution_clock::now();

    cout << "--------------------------" << endl;
    topology = new Topology();
    // topology->generateFattree(8, 1, 1);
    // topology->generateOneBigSwitch(8, 1); // capacity * factor
    topology->generateOneBigSwitch(16*8*8, 400.0*1000000000/8); // capacity * factor
    // topology->print();
    cout << "--------------------------" << endl;
    
    workload = new Workload(16,      // PP
                            8,      // DP      
                            8,      // TP 
                            192,      // microbatches   
                            0.005782,    // fwdCompTime * factor
                            0.015002,    // bwdCompTime * factor
                            1056964608,    // fwdTPSize
                            1056964608,    // bwdTPSize
                            11796480,    // fwdPPSize
                            11796480,    // bwdPPSize
                            5121446400     // dpSize
                        );
    // workload = new Workload(2,      // PP
    //                         2,      // DP      
    //                         2,      // TP 
    //                         5,      // microbatches   
    //                         0.1,    // fwdCompTime * factor
    //                         0.1,    // bwdCompTime * factor
    //                         1.0,    // fwdTPSize
    //                         1.0,    // bwdTPSize
    //                         1.0,    // fwdPPSize
    //                         1.0,    // bwdPPSize
    //                         1.0     // dpSize
    //                     );
    workload->topology = topology;
    workload->configureParallelism();   // 1F1B now
    workload->placement();
    workload->routing();
    // workload->print();
    // return 0;
    cout << "--------------------------" << endl;
    simulator = new Simulator();
    simulator->workload = workload;
    simulator->topology = topology;
    simulator->initialize();
    // simulator->print();    
    cout << "--------------------------" << endl;
    simulator->run();
    cout << "--------------------------" << endl;

    return 0;
}