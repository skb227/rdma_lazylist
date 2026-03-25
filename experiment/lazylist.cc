#include <memory>
#include <unistd.h>
#include <vector>

#include <remus/remus.h>

#include "cloudlab.h"
#include "nodes.h"

int main (int argc, char **argv) {

    // initialize remus
    remus::INIT(); 

    // configure and parse arguments

    // extract args needed in every node 

    // prepare network info about this machine and about memory nodes

    // info for memory node

    // info for compute node

    // if compute node, configure to be compute node

    // if memory node, wait till receive all connections from CNs
    //      then spin until control channel in each segment becomes 1 ?? 
    //      then shutdown memory node

    // if compute node, create threads and run experiment 

}