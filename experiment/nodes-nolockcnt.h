#pragma once

#include <memory>
#include <remus/remus.h>

using namespace remus; 

/**
  same as nodes.h, but was the original version before i started counting
    how many attempts at lock acquires
*/

/// lazy list implementation of a link list (sorted list), with wait-free contains 
class LazyListSet {

using CT = std::shared_ptr<remus::ComputeThread>; 

private:
    // node struct
    struct Node {
        remus::Atomic<int> key;
        remus::Atomic<Node*> next;
        remus::Atomic<uint64_t> lock; 
        remus::Atomic<bool> marked;
    

    /// initialize node
    /// 
    /// @param k    key to store in node
    /// @param ct   compute thread
    void init(const int &k, CT &ct) {
        key.store(k, ct);
        next.store(nullptr, ct);
        lock.store(false, ct);
        marked.store(false, ct);
    }

    /// acquire lock -- tas operation
    ///
    /// @param ct   compute thread
    void acquire(CT &ct) {
        while(true) {           // loop to keep trying
            if (lock.compare_exchange_weak(0, 1, ct)) {     // equivalent to tas, check if value of lock is 0 and set to 1
                break;                                          // if old value is 0, break
            } 
            while (lock.load(ct) == 1) {}                   // spin until lock is 0
        }
    }

    /// release lock 
    ///
    /// @param ct   compute thread
    void release(CT &ct) {
        lock.store(0, ct);
    }

    };

    /// validate helper method 
    bool validate(Node* pred, Node* curr, CT &ct) {
        return !pred->marked.load(ct) && !curr->marked.load(ct) && pred->next.load(ct) == curr; 
    }

public: 

    // sentinel pointers -- head and tail
    remus::Atomic<Node *> head; 
    remus::Atomic<Node *> tail; 

    // 'This' to access consistent RDMA memory location, the shared data object address
    //      whereas 'this' is local or to the LazyListSet object
    LazyListSet *This; 

    /// to allocate a LazyList in *remote* memory, initialize it
    ///
    /// @param ct   the compute thread
    /// @return     rdma_ptr to newly allocated and initialized empty list
    static remus::rdma_ptr<LazyListSet> New(CT &ct) {
       // rdma_ptr is a "smart pointer" to memory on another machine
       
       auto tail = ct->New<Node>(); 
       tail->init(3000, ct);

       auto head = ct->New<Node>(); 
       head->init(0, ct); 
       head->next.store(tail, ct); 

       auto list = ct->New<LazyListSet>(); 
       list->head.store(head, ct); 
       list->tail.store(tail, ct); 

       return remus::rdma_ptr<LazyListSet>((uintptr_t)list); 
    }

    /// construct a LazyListSet, setting its 'This' pointer to a remote memory location
    ///         every thread in the program could have a unique LazyListSet, but if they all use the same
    ///             'This', they'll all access the same remote memory 
    ///
    /// @param This 
    LazyListSet(const remus::rdma_ptr<LazyListSet> &This) 
        : This((LazyListSet *)((uintptr_t)This)) {}
        

    /// wait-free contains
    ///
    /// @param key  the key to search for
    /// @param ct   compute thread
    /// @return true if found, false otherwise 
    bool contains (const int key, CT &ct) {
        Node* head = This->head.load(ct); 
        Node* tail = This->tail.load(ct); 
        Node* curr = head->next.load(ct); 
        while (curr->key.load(ct) < key && curr != tail) {
                    // traverse (no locks) till first curr.key >= key
            curr = curr->next.load(ct); 
        }
        // check that curr.key == key and curr not removed -- key is contained in list
        return curr->key.load(ct) == key && !curr->marked.load(ct); 
    }

    

    /// insert 
    ///
    /// @param key  key to be inserted
    /// @param ct   compute thread
    /// @return true if success, false otherwise 
    bool insert (const int key, CT &ct) {
        Node *head = This->head.load(ct);
        Node *tail = This->tail.load(ct);
        Node *curr, *pred; 
        while (true) {              // keep trying
            pred = head; 
            curr = pred->next.load(ct); 
            while (curr->key.load(ct) < key && curr != tail) {
                    // traverse list (no locks) till first curr.key >= key
                pred = curr; 
                curr = curr->next.load(ct); 
            }

            // need to lock both curr and pred
            pred->acquire(ct); 
            curr->acquire(ct); 

            if (validate(pred, curr, ct)) {     // validate pred and curr after locking
                if (curr->key.load(ct) == key) {    // key already exists - no insert
                    curr->release(ct);
                    pred->release(ct); 
                    return false; 
                } else {
                    Node* node = ct->New<Node>();   // create new node for key 
                    node->init(key, ct);            // initialize new node 
                    node->next.store(curr, ct);     // set new node's next to curr
                    pred->next.store(node, ct);     // set pred to point to new node

                    break;
                }
            }
            // validation failed -- release locks and try again 
            curr->release(ct); 
            pred->release(ct); 
        }

        // release locks on pred and curr
        curr->release(ct);
        pred->release(ct); 
        return true; 
    }

    
    /// remove
    ///
    /// @param key  key to remove
    /// @param ct   compute thread
    /// @return true if successful, false otherwise 
    bool remove(const int key, CT& ct) {
        Node *tail = This->tail.load(ct); 
        Node *pred, *curr; 
        while (true) {              // keep trying
            pred = This->head.load(ct); 
            curr = pred->next.load(ct); 
            while (curr->key.load(ct) < key && curr != tail) {
                    // traverse list (lock free) till first curr.key >= key
                pred = curr; 
                curr = curr->next.load(ct); 
            }

            // lock pred and curr
            pred->acquire(ct); 
            curr->acquire(ct); 

            if (validate(pred, curr, ct)) {
                if (curr->key.load(ct) != key || curr == tail) {        // if curr key not seek key, can't remove it
                    pred->release(ct);
                    curr->release(ct);
                    return false; 
                } else {      
                    curr->marked.store(true, ct);               // logically remove
                    pred->next.store(curr->next.load(ct), ct);  // physically remove
                    ct->SchedReclaim(curr);
                    break;
                }
            }
            // validate failed -- release locks and try again
            pred->release(ct); 
            curr->release(ct); 
        }

        pred->release(ct);
        curr->release(ct);
        return true; 
    }


    /// destructor of sorts -- reclaim all nodes, reclaim 'This'
    ///
    /// @param ct   compute thread
    void destroy(CT &ct) {
        Node *curr = This->head.load(ct);
        while (curr) {
            Node *next = curr->next.load(ct);
            ct->Delete(curr);
            curr = next;
        }
        ct->Delete(This);
    }
    
};