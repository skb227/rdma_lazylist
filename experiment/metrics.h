#pragma once

#include <cstdint>

#include <remus/remus.h>

using namespace remus; 

// create a struct, Metrics, to track the execution variables
struct Metrics {
    size_t con_t = 0;           // successful contains
    size_t con_f = 0;           // failed contains
    size_t ins_t = 0;           // successful insert
    size_t ins_f = 0;           // failed insert
    size_t rem_t = 0;           // successful remove
    size_t rem_f = 0;           // failed remove
    size_t op_count = 0;        // total num of lazy list operations
    size_t write_ops = 0;       // total number of RDMA writes          -- tracked by remus -- struct_
    size_t write_bytes = 0;     // total bytes written over RDMA        -- tracked by remus -- struct_
    size_t read_ops = 0;        // total number of RDMA reads           -- tracked by remus -- struct_
    size_t read_bytes = 0;      // total bytes read over RDMA           -- tracked by remus -- struct_
    size_t faa_ops = 0;         // total num of RDMA faa                -- tracked by remus -- struct_
    size_t cas_ops = 0;         // total num of RDMA cas                -- tracked by remus -- struct_
    size_t lock_t = 0;          // successful lock acquire
    size_t lock_f = 0;          // successful lock fail 

    // (taken from tutorial) 
    /// write Metrics object to file (metrics.txt)
    ///
    /// @param duration    the duration of the experiment (microseconds) 
    /// @param ct          compute thread for additional metrics 
    void to_file(double duration, std::shared_ptr<ComputeThread> ct) {
        std::ofstream file("metrics.txt", std::ios::out); 
        file << "duration: " << duration << std::endl; 
        file << "con_t: " << con_t << std::endl; 
        file << "con_f: " << con_f << std::endl; 
        file << "ins_t: " << ins_t << std::endl; 
        file << "ins_f: " << ins_f << std::endl; 
        file << "rem_t: " << rem_t << std::endl; 
        file << "rem_f: " << rem_f << std::endl; 
        file << "op_count: " << op_count << std::endl; 
        // do these metrics even aggregate??? 
        file << "writes: " << ct->metrics_.write.ops << std::endl; 
        file << "write_bytes: " << ct->metrics_.write.bytes << std::endl; 
        file << "reads: " << ct->metrics_.read.ops << std::endl; 
        file << "read_bytes: " << ct->metrics_.read.bytes << std::endl; 
        file << "faa: " << ct->metrics_.faa << std::endl; 
        file << "cas: " << ct->metrics_.cas << std::endl; 
        file << "lock_t: " << lock_t << std::endl; 
        file << "lock_f: " << lock_f << std::endl; 
    }
};