#include <memory>
#include <unistd.h>
#include <vector>

#include <remus/remus.h>

#include "cloudlab.h"
#include "nodes.h"
#include "helper.h"

int main (int argc, char **argv) {

    using set_t = LazyListSet;

    // initialize remus
    remus::INIT(); 

    // configure and parse arguments
    auto args = std::make_shared<remus::ArgMap>(); 
    args->import(remus::ARGS);
    args->import(DS_EXP_ARGS);
    args->parse(argc, argv);

    // extract args needed in every node 
    uint64_t id = args->uget(remus::NODE_ID);
    uint64_t m0 = args->uget(remus::FIRST_MN_ID); 
    uint64_t mn = args->uget(remus::LAST_MN_ID);
    uint64_t c0 = args->uget(remus::FIRST_CN_ID);
    uint64_t cn = args->uget(remus::LAST_CN_ID);

    // prepare network info about this machine and about memory nodes
    remus::MachineInfo self(id, id_to_dns_name(id)); 
    std::vector<remus::MachineInfo> memnodes; 
    for (uint64_t i = m0; i <= mn; ++i) {
        memnodes.emplace_back(i, id_to_dns_name(i)); 
    }

    // info for memory node
    std::unique_ptr<remus::MemoryNode> memory_node; 

    // info for compute node
    std::shared_ptr<remus::ComputeNode> compute_node;

    // if compute node, configure to be compute node
    if (id >= m0 && id <= mn) {
        // make the pools, await connections 
        memory_node.reset(new remus::MemoryNode(self, args));
    }

    // if memory node, wait till receive all connections from CNs
    //      then spin until control channel in each segment becomes 1 ?? 
    //      then shutdown memory node
    if (id >= c0 && id <= cn) {
        compute_node.reset(new remus::ComputeNode(self, args)); 
        // if this CN is also a Mn, then need to pass the rkeys to the local MN
        //  there's no harm in doing them first
        if (memory_node.get() != nullptr) {
            auto rkeys = memory_node->get_local_rkeys(); 
            compute_node->connect_local(memnodes, rkeys);
        }
        compute_node->connect_remote(memnodes);
    }

    // if compute node, create threads and run experiment 
    if (id >= c0 && id <= cn) {
        // create ComputeThread contexts
        std::vector<std::shared_ptr<remus::ComputeThread>> compute_threads; 
        uint64_t total_threads = (cn - c0 + 1) * args->uget(remus::CN_THREADS); 
        for (uint64_t i = 0; i < args->uget(remus::CN_THREADS); ++i) {
            compute_threads.push_back(std::make_shared<remus::ComputeThread>(id, compute_node, args)); 
        }

        // CN 0 will construct the data structure and save it in root
        if (id == c0) {
            auto set_ptr = set_t::New(compute_threads[0]);
            compute_threads[0]->set_root(set_ptr); 
        }

        // make threads and start them
        std::vector<std::thread> worker_threads; 
        for (uint64_t i = 0; i < args->uget(remus::CN_THREADS); i++) {
            worker_threads.push_back(std::thread(
                [&](uint64_t i) {
                    auto &ct = compute_threads[i]; 
                    // wait for all threads to be created
                    ct->arrive_control_barrier(total_threads);

                    // get the root, make a local reference to it
                    auto set_ptr = ct->get_root<LazyListSet>();
                    // call constructor for LazyListSet
                    LazyListSet set_handler(set_ptr);

                    // the workload part but i don't have a workload... 
                
                    ct->arrive_control_barrier(total_threads); 

                    
                    // reclaim memory from prior phase
                    ct->ReclaimDeferred(); 
                },
            i));
        }
        for (auto &t : worker_threads) {
            t.join(); 
        }
    } else if (id >= m0 && id <= mn) {
        // if only memory node, wait a bit before shutting down
        pause(); 
    }
    return 0; 

}