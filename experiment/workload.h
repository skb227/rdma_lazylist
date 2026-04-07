#pragma once

#include <atomic> 
#include <csignal> 
#include <random> 

#include <remus/remus.h>

using std::uniform_int_distribution; 
using namespace remus; 

// create a struct, Metrics, to track the execution variables
struct Metrics {
    size_t con_s = 0;           // successful contains
    size_t con_f = 0;           // failed contains
    size_t ins_s = 0;           // successful insert
    size_t ins_f = 0;           // failed insert
    size_t rem_s = 0;           // successful remove
    size_t rem_f = 0;           // failed remove
    size_t op_count = 0;        // total num of lazy list operations
    size_t write_ops = 0;       // total number of RDMA writes          -- tracked by remus -- struct_
    size_t write_bytes = 0;     // total bytes written over RDMA        -- tracked by remus -- struct_
    size_t read_ops = 0;        // total number of RDMA reads           -- tracked by remus -- struct_
    size_t read_bytes = 0;      // total bytes read over RDMA           -- tracked by remus -- struct_
    size_t faa_ops = 0;         // total num of RDMA faa                -- tracked by remus -- struct_
    size_t cas_ops = 0;         // total num of RDMA cas                -- tracked by remus -- struct_
    size_t lock_s = 0;          // successful lock acquire
    size_t lock_f = 0;          // successful lock fail 

    // (taken from tutorial) 
    /// write Metrics object to file (metrics.txt)
    ///
    /// @param duration    the duration of the experiment (microseconds) 
    /// @param ct          compute thread for additional metrics 
    void to_file(double duration, std::shared_ptr<ComputeThread> ct) {
        std::ofstream file("metrics.txt", std::ios::out); 
        file << "duration: " << duration << std::endl; 
        file << "con_s: " << con_s << std::endl; 
        file << "con_f: " << con_f << std::endl; 
        file << "ins_s: " << ins_s << std::endl; 
        file << "ins_f: " << ins_f << std::endl; 
        file << "rem_s: " << rem_s << std::endl; 
        file << "rem_f: " << rem_f << std::endl; 
        file << "op_count: " << op_count << std::endl; 
        // do these metrics even aggregate??? 
        file << "writes: " << ct->metrics_.write.ops << std::endl; 
        file << "write_bytes: " << ct->metrics_.write.bytes << std::endl; 
        file << "reads: " << ct->metrics_.read.ops << std::endl; 
        file << "read_bytes: " << ct->metrics_.read.bytes << std::endl; 
        file << "faa: " << ct->metrics_.faa << std::endl; 
        file << "cas: " << ct->metrics_.cas << std::endl; 
        file << "lock_s: " << lock_s << std::endl; 
        file << "lock_f: " << lock_f << std::endl; 
    }
};

struct test {
    using CT = std::shared_ptr<ComputeThread>; 
    using AM = std::shared_ptr<ArgMap>; 

    // hold this thread's metrics object
    Metrics metrics; 
    // reference to lazy list set
    LazyListSet &set;
    // hold this thread's ID
    uint64_t thread_id;       
    // hold id of compute node thread exists on 
    uint64_t node_id; 
    
    /// construct a test object 
    ///
    /// @param thread_id
    /// @param node_id
    test(LazyListSet &set, uint64_t thread_id, uint64_t node_id) 
        : set(set), thread_id(thread_id), node_id(node_id) {}

    /// perform a distributed prefill of the data structure
    ///     s.t. removes and contains don't fail, reduce lock contention on constant inserts 
    ///
    /// @param ct     calling thread's Remus context
    /// @param params arguments to the program 
    void prefill(CT &ct, AM &params) {
        // calculate the per-thread range of keys 
        auto total_threads = params->uget(CN_THREADS) * (params->uget(LAST_CN_ID) - params->uget(FIRST_CN_ID) + 1); 
        
        // set parameters (hardcoded rather than args) 
        auto key_lb = params->uget(KEY_LB); 
        auto key_ub = params->uget(KEY_UB); 
        auto fill = params->uget(PREFILL); 

        // per-thread range of keys 
        auto range_length = (key_ub - key_lb + 1) / total_threads; 

        // how many keys to insert in that range
        auto num_keys = (key_ub - key_lb + 1) * fill / 100 / total_threads;   
        // find start and end points for this thread
        auto start_key = key_lb + thread_id * range_length; 
        auto end_key = start_key + range_length; 
        // the step gap between inserts 
        auto step = (end_key - start_key) / num_keys;
                
        // now actuall do the insert
        for (auto key = start_key; key < end_key; key += step) {
            int key_tmp = static_cast<int>(key); 
            set.insert(key_tmp, ct); 
        }
    }

    /// aggregate this thread's metrics into a global remote memory metrics object
    ///
    /// @param ct        calling thread's remus context
    /// @param gMetrics  global metrics object 
    void collect(CT &ct, rdma_ptr<Metrics> gMetrics) {
        auto base = gMetrics.raw(); 
        // whole shit ton of faas to update counts of global metrics obj 
        ct->FetchAndAdd(rdma_ptr<uint64_t>(base + offsetof(Metrics, con_s)), metrics.con_s);
        ct->FetchAndAdd(rdma_ptr<uint64_t>(base + offsetof(Metrics, con_f)), metrics.con_f); 
        ct->FetchAndAdd(rdma_ptr<uint64_t>(base + offsetof(Metrics, ins_s)), metrics.ins_s); 
        ct->FetchAndAdd(rdma_ptr<uint64_t>(base + offsetof(Metrics, ins_f)), metrics.ins_f); 
        ct->FetchAndAdd(rdma_ptr<uint64_t>(base + offsetof(Metrics, rem_s)), metrics.rem_s); 
        ct->FetchAndAdd(rdma_ptr<uint64_t>(base + offsetof(Metrics, rem_f)), metrics.rem_f); 
        ct->FetchAndAdd(rdma_ptr<uint64_t>(base + offsetof(Metrics, op_count)), metrics.op_count); 
        ct->FetchAndAdd(rdma_ptr<uint64_t>(base + offsetof(Metrics, write_ops)), metrics.write_ops); 
        ct->FetchAndAdd(rdma_ptr<uint64_t>(base + offsetof(Metrics, read_ops)), metrics.read_ops); 
        ct->FetchAndAdd(rdma_ptr<uint64_t>(base + offsetof(Metrics, faa_ops)), metrics.faa_ops); 
        ct->FetchAndAdd(rdma_ptr<uint64_t>(base + offsetof(Metrics, cas_ops)), metrics.cas_ops); 
        ct->FetchAndAdd(rdma_ptr<uint64_t>(base + offsetof(Metrics, lock_s)), metrics.lock_s); 
        ct->FetchAndAdd(rdma_ptr<uint64_t>(base + offsetof(Metrics, lock_f)), metrics.lock_f); 
    }

    /// run experiment in this thread
    ///
    /// @param ct       calling thread's remus context
    /// @param params   arguments to program 
    void run(CT &ct, AM &params) {
        // set up random num gen for the thread, and a few distributions
        using std::uniform_int_distribution; 
        uniform_int_distribution<size_t> key_dist(params->uget(KEY_LB), params->uget(KEY_UB)); 
        uniform_int_distribution<size_t> action_dist(0, 100); 
        std::mt19937 gen(std::random_device{}());

        // get target operation ratios from ARGS
        auto insert_ratio = params->uget(INSERT); 
        auto remove_ratio = params->uget(REMOVE); 
        auto contains_ratio = 100 - insert_ratio - remove_ratio; 
        uint64_t num_ops = params->uget(NUM_OPS); 

        // fixed number of operations per thread
        for (uint64_t i = 0; i < num_ops; ++i) {
            size_t key = key_dist(gen); 
            size_t action = action_dist(gen); 
            
            // if in contains_ratio  
            if (action <= contains_ratio) {
                int key_tmp = static_cast<int>(key); 
                if (set.contains(key_tmp, ct)) {
                    ++metrics.con_s; 
                } else {
                    ++metrics.con_f; 
                }
            // if in insert_ratio but no contains_ratio
            } else if (action < (contains_ratio + insert_ratio)) {
                int key_tmp = static_cast<int>(key); 
                if (set.insert(key_tmp, ct)) {
                    ++metrics.ins_s; 
                } else {
                    ++metrics.ins_f; 
                }
            // if in remove_ratio
            } else {
                int key_tmp = static_cast<int>(key); 
                if (set.remove(key_tmp, ct)) {
                    ++metrics.rem_s; 
                } else {
                    ++metrics.rem_f; 
                }
            }
            ++metrics.op_count; 
        }
    }
};