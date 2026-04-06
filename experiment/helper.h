#pragma once

#include <cstdint>
#include <string>
#include <atomic>
#include <csignal>
#include <random>
#include <fstream>

#include <remus/remus.h>


// i already have this in cloudlab.h ...
// std::string id_to_dns_name(uint64_t id) {
//   return std::string("node") + std::to_string(id);
// }

// command-list options for integer set microbenchmarks 
constexpr const char *NUM_OPS = "--num-ops"; 
constexpr const char *PREFILL = "--prefill";
constexpr const char *INSERT = "--insert";
constexpr const char *REMOVE = "--remove";
constexpr const char *KEY_LB = "--key-lb";
constexpr const char *KEY_UB = "--key-ub";

// an ARGS object for integer set microbenchmarks
auto DS_EXP_ARGS = {
    U64_ARG_OPT(NUM_OPS, "Number of operations to run per thread", 65536),
    U64_ARG_OPT(PREFILL, "Percent of elements to prefill the data structure", 50),
    U64_ARG_OPT(INSERT, "Percent of operations that should be inserts", 50),
    U64_ARG_OPT(REMOVE, "Percent of operations that should be removes", 50),
    U64_ARG_OPT(KEY_LB, "Lower bound of the key range", 0),
    U64_ARG_OPT(KEY_UB, "Upper bound of the key range", 4096),
};
