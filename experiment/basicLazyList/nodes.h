#pragma once

#include <mutex>
#include <iostream> 
#include <atomic>

//** using pointers everywhere bc mutex sucks without pointers 

// lazy list implementation 
//     sorted linked-list with locks and wait-free contains
class LazyListSet {

private: 
    // node struct for the linked list
    struct Node {
        int key;                      // the key stored in this node 
        std::atomic<Node*> next;      // pointer to next node
        std::atomic<bool> marked;     // boolean for logical removes
        std::mutex lock;              // lock -- using std::mutex instead of uint64_t bc using lock not bit logic
    
        // ** since using atomic variables, need to use .load() and .store() to safely read and write 

        // constructor
        Node (int k) {
            key = k; next.store(nullptr); marked.store(false); 
        }
    };

    // sentinel pointers -- head and tail
    Node* head; 
    Node* tail;
    
    // validate helper method
    bool validate(Node* pred, Node* curr) {
        return !pred->marked.load() && !curr->marked.load() && pred->next.load() == curr; 
    }

public: 

    LazyListSet() {
        // initialize sentinel nodes (head and tail) 
        head = new Node(0);
        tail = new Node(100);
        head->next.store(tail); 
    }

    // contains
    bool contains(const int key) {
        Node* curr = head; 
        while (curr->key < key) {
                // traverse (no locks) till first curr.key >= key
            curr = curr->next.load(); 
        }
        // check that curr.key == key and curr not removed -- key is contained in list
        return curr->key == key && !curr->marked.load();
    }

    // insert
    bool insert(const int key) {
        while (true) {                  // use while true so that if validation fails, try again 
            Node* pred = head; 
            Node* curr = head->next.load(); 
            while (curr->key < key) {
                    // traverse list (no locks) till first curr.key >= key 
                pred = curr; 
                curr = curr->next.load(); 
            }

            // note can use unique_lock for these but lock_guard has less overhead and don't need anything overly effective here
            std::lock_guard<std::mutex> predLock(pred->lock);       // lock pred
            std::lock_guard<std::mutex> currLock(curr->lock);       // lock curr

            if (validate(pred, curr)) {            // validate pred and curr after locking
                if (curr->key == key) {         // key already exists - no insert
                    return false; 
                } else {
                    Node* node = new Node(key);      // create new node for key
                    node->next.store(curr);          // set new node's next to curr
                    pred->next.store(node);          // set pred to point to new node
                    return true; 
                }
            }
        }
        // lock_guard will automatically release at end of scope 
    }

    bool remove(const int key) {
        while (true) {
            Node* pred = head; 
            Node* curr = head->next.load(); 
            while (curr->key < key) {
                    // traverse list (lock free) till first curr.key >= key
                pred = curr; 
                curr = curr->next.load(); 
            }

            std::lock_guard<std::mutex> predLock(pred->lock);   // lock pred
            std::lock_guard<std::mutex> currLock(curr->lock);   // lock curr 

            if (validate(pred, curr)) {
                if (curr->key != key) {         // if current key not seek key, cannot remove it
                    return false; 
                } else {
                    curr->marked.store(true);               // logically remove
                    pred->next.store(curr->next.load());    // physical remove
                    return true; 
                }
            }
        }
    }

};