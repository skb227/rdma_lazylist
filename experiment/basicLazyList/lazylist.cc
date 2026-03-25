#include <iostream>
#include <thread>
#include <vector>

#include "nodes.h"

// global instance of lazy list
LazyListSet set; 

void worker(int threadID) {
    // each thread inserts a few random numbers (well not random...) 
    int num1 = threadID * 10; 
    int num2 = threadID * 10 + 2; 

    // insert values into lazy list set (check insert functionality)
    set.insert(num1); set.insert(num2); 

    // check contains functionality 
    if (set.contains(num1)) {
        // maybe set a bool here? 
    }

    // check remove functionality 
    set.remove(num2); 
}

int main() {
    std::cout << "testing lazy list" << std::endl; 

    // create thread vector
    std::vector<std::thread> threads; 

    // create five threads
    std::cout << "creating threads" << std::endl;
    for (int i = 0; i < 5; i++) {
        threads.push_back(std::thread(worker, (i+1)));
    }

    // call join to wait for all threads to finish 
    for (auto& t : threads) {
        t.join(); 
    }

    // verify values that should still exist
    std::cout << "lazy list contains 10? " << (set.contains(10) ? "yes!" : "no...") << std::endl; 
    std::cout << "lazy list contains 11? " << (set.contains(11) ? "yes..." : "no!") << std::endl; 

    return 0;
}