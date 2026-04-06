#include <memory>
#include <unistd.h>
#include <vector>

#include <remus/remus.h>

#include "cloudlab.h"
#include "nodes.h"
#include "helper.h"

int main (int argc, char **argv) {

    using set_t = LazyListSet;   // this LazyListSet doesn't accept a type, it's set to int

    // initialize remus
    remus::INIT(); 

    // configure and parse arguments
    auto args = std::make_shared<remus::ArgMap>(); 
    args->remus::import(remus::ARGS);
    args->remus::import(DS_EXP_ARGS);
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

    // if memory node, configure to be memory node
    if (id >= m0 && id <= mn) {
        // make the pools, await connections 
        memory_node.reset(new remus::MemoryNode(self, args));
    }

    // if compute node
    if (id >= c0 && id <= cn) {
        compute_node.reset(new remus::ComputeNode(self, args)); 
        // if this CN is also a MN, then need to pass the rkeys to the local MN
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
                    // each node has its own compute thread context 
                    auto &ct = compute_threads[i]; 
                    // wait for all threads to be created across all nodes
                    ct->arrive_control_barrier(total_threads);

                    // get the root, make a local reference to it
                    auto set_ptr = ct->get_root<LazyListSet>();
                    // call constructor for LazyListSet
                    LazyListSet set_handler(set_ptr);

        // workload test

                    uint64_t numOps = 25;
                    uint64_t keyRange = 25; 

                    // thread-local random number generator -- seed with thread id to ensure uniqueness
                    std::mt19937_64 rng(i + 42); 
                    std::uniform_int_distribution<uint64_t> key_dist(1, keyRange); 
                    std::uniform_int_distribution<int> op_dist(1, 100); 

                    // to track success
                    uint64_t successIns = 0; 
                    uint64_t successRem = 0; 

                    // for each thread, workload
                    for (uint64_t op = 0; op < numOps; op++) {
                        uint64_t key = key_dist(rng); 
                        uint64_t op_type = op_dist(rng); 

                        // try 20% insert, 10% remove, 70% contains
                        if (op_type <= 20) {
                            if (set_handler.insert(key, ct)) {
                                successIns++; 
                            }
                        } else if (op_type <= 30) {
                            if (set_handler.remove(key, ct)) {
                                successRem++; 
                            } 
                        } else {
                            set_handler.contains(key, ct); 
                        }
                    }

                    // results 
                    std::cout << "Thread " << i << " finished -- " << successIns << " inserts, " << successRem << " removes" << endl; 

        // end of workload test
                
                    // wait till all threads finish workload test
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
