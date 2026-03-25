#pragma once

#include <memory>
#include <remus/remus.h>

// lazy list implementation with rdma 
//     sorted linked-list with locks and wait-free contains
//
/// @param K    the type for keys stored in the map
template <typename K> class LazyListSet {

    // define type for nodes
    using CT = std::shared_pointer<remus::ComputeThread>; 

    // node struct for the linked list
    struct Node {
        Atomic<K> key;              // the key stored in this node 
        Atomic<Node*> next;         // pointer to next node
        Atomic<boolean> marked;     // boolean for logical removes
        Atomic<uint64_t> lock;      // lock
    }

public: 

    // sentinel pointers -- head and tail
    Atomic<Node*> head; 
    Atomic<Node*> tail; 

    // This pointer for data access ... not understanding this? 
    LazyListSet *This; 





    // contains
    bool contains(const K &key) {
        
    }


}