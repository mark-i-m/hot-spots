
// run using command :
// make eval.bmk
// input bulk_load_limit, R, W, N, X

// Features for future additions :
// - think time
// - bulk_insert

#include "btree-base.h"
#include "btree-bytereorder.h"
#include "btree-hybrid.h"
#include "btreeolc.h"
#include "pinning.h"
#include <sched.h>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <random>
#include <thread>
#include <vector>
#include <stdio.h>
#include <unistd.h>
#include <fstream>
#include <mutex>

using namespace std;
// get number of CPUs
int get_nprocs(void);
int get_nprocs_conf(void);

// Create a Btreetype
enum class BTreeType {
    BTreeOLC = 1,
    BTreeHybrid = 2,
    BTreeByteReorder = 3,
};

// for timer
uint64_t rdtsc() {
    unsigned int lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

// global variables
// counter is for knowing what has to be inserted next sequentially
std::atomic_ullong counter = {0};
std::atomic_ullong cpu = {0};
// are all threads ready and does parent thread know that
std::atomic<bool> ready = {false};
// are all threads ready
vector<bool> tready;
// for locking
std::mutex m;

unsigned long long int get_counter() { return ++counter; }
int get_cpu() { return ++cpu; }

//function to print average time in std::out (console)
/* void report_average_time(unsigned long long int X, unsigned long long int ops) {
    unsigned long long int w = 0;
    size_t i;
    for (i = 0; i < w_timer.size() - 1; i++) {
        w += w_timer[i];
    }
    cout << "Average write time for " << X << " operations is : " << w / X
         << endl;
    w = w + w_timer[i];
    cout << "Average write time for 1 operation is : " << w / ops << endl;
    uint64_t r = 0;
    for (i = 0; i < r_timer.size() - 1; i++) {
        r += r_timer[i];
    }
    cout << "Average read time for " << X << " operations is : " << r / X
         << endl;
    r = r + r_timer[i];
    cout << "Average read time for 1 operation is : " << r / ops << endl;
}
*/

void reader_child(int thread_id, unsigned long long int ops,
                  common::BTreeBase<unsigned long long int, unsigned long long int> *btree, unsigned long long int X, string path) {
    // set cpu
    set_cpu(get_cpu());
    m.lock();
    // announce that it's ready
    tready[thread_id] = true;
    m.unlock();
    // wait till all threads have spawned
    while (!ready.load(std::memory_order_relaxed)) {
    };
    // to store time taken in x operations
    // each entry represents time of x operations except last entry. last entry
    // represents time of last few ops%X operations.
    vector<uint64_t> r_timer;
    unsigned long long int read_value;
    std::random_device rd;  // Get a random seed from the OS entropy device, or whatever
    std::mt19937_64 eng(rd());  // Use the 64-bit Mersenne Twister 19937 generator
    // and seed it with entropy.
    std::uniform_int_distribution<unsigned long long> distr;
    
    unsigned long long int x = 0;
    uint64_t time_x = 0;
    
    while (ops) {
        // generate a random number (key) of uint_64 to read
        read_value = distr(eng);
        // the number can only be as big as the counter
        read_value = read_value % (counter.load(std::memory_order_relaxed));
        std::pair<unsigned long long int, unsigned long long int> read;
        read.first = read_value;
        read.second = 0;
        // Measure time taken for operation and add it to time taken for x operations variable
        uint64_t tick = rdtsc();
        btree->lookup(read.first, read.second);
        time_x += rdtsc() - tick;
        x++;
        ops--;
	// if x opeations are done, so time has been measured, so push it and start again
        if (x == X || ops == 0) {
            r_timer.push_back(time_x);
            x = 0;
            time_x = 0;
        }
    }
    // after all operations are done, store the vector containing times in a file
    ofstream myfile;
    string file_name = path+"Read_"+std::to_string(thread_id);
    myfile.open (file_name);
    for(size_t i=0; i<r_timer.size(); i++)
    {
        myfile<<r_timer[i]<<endl;
    }
    myfile.close();
    return;
}

void writer_child(int thread_id, unsigned long long int ops,
                  common::BTreeBase<unsigned long long int, unsigned long long int> *btree, unsigned long long int X, string path) {
    // set cpu
    set_cpu(get_cpu());
    
    m.lock();
    // announce that it's ready
    tready[thread_id] = true;
    m.unlock();
    // wait till all threads have spawned
    while (!ready.load(std::memory_order_relaxed)) {
    }
    
    // store time of every x operations in a vector
    vector<uint64_t> w_timer;
    unsigned long long int write_value;
    unsigned long long int x = 0;
    uint64_t time_x = 0;

    while (ops) {
        // generate a value for sequential write
        // write_value must be greater than counter
        std::pair<unsigned long long int, unsigned long long int> write;
        write_value = get_counter();
        write.first = write_value;
        write.second = rand();
        // calculate time for operation
        uint64_t tick = rdtsc();
        btree->insert(write);
        time_x += rdtsc() - tick;
        x++;
	ops--;
	// if x operations are done, store the time taken in the vector
        if (x == X || ops == 0) {
            w_timer.push_back(time_x);
            x = 0;
            time_x = 0;
        }
    }
    // after all operations are done, store the time vector in a file
    ofstream myfile;
    string file_name = path+"Write_"+std::to_string(thread_id);
    myfile.open (file_name);
    for(size_t i=0; i<w_timer.size(); i++)
    {
        myfile<<w_timer[i]<<endl;
    }
    myfile.close();

    return;
}

// function to check if all the values in a boolean vector are true
// used by parent thread to decide if all threads it spawned are ready
bool check_all_true(vector<bool> &arr) {
    m.lock();
    for (size_t i = 0; i < arr.size(); i++) {
        if (arr[i] == false) {
            m.unlock();
	    return false;
	}
    }
    m.unlock();
    return true;
}

void test(int R, int W, unsigned long long int N, common::BTreeBase<unsigned long long int, unsigned long long int> *btree,
          unsigned long long int X, string path) {
 
    // spawn reader threads : carefully handle the case of R=0 or W=0
    if (W == 0 && R != 0) {
        std::thread readers[R];
        for (int i = 0; i < R; i++)
            readers[i] = std::thread(reader_child, i, N, btree, X, path);
	// wait for all children threads to be ready
        while (!check_all_true(tready)) {
        }
        ready = true;

        for (int i = 0; i < R; i++) readers[i].join();
    } else if (R == 0 && W != 0) {
        std::thread writers[W];
        for (int i = 0; i < W; i++)
            writers[i] = std::thread(writer_child, i, N, btree, X, path);
	// wait for all children threads to be ready
        while (!check_all_true(tready)) {
        }
        ready = true;
        for (int i = 0; i < W; i++) writers[i].join();
    } else if (R != 0 && W != 0) {
        std::thread readers[R];
        for (int i = 0; i < R; i++)
            readers[i] = std::thread(reader_child, i, N, btree, X, path);

        std::thread writers[W];
        for (int i = 0; i < W; i++)
            writers[i] = std::thread(writer_child, i + R, N, btree, X, path);
        // wait for all children threads to be ready
        while (!check_all_true(tready)) {
        }
        cout << "Now ready to go" << endl;
	// ready = true is what all spawned threads are waiting at. Now they begin
        ready = true;
	
        for (int i = 0; i < R; i++) readers[i].join();
        for (int i = 0; i < W; i++) writers[i].join();
    }
    return;
}

int main(int argc, char** argv) {
    set_cpu(0);
    // bulk_load_limit initializes the btree with some pre-existing values before evaluation starts
    unsigned long long int bulk_load_limit = stoull(argv[2]);
    // treetype tells whether we are evaluating OLC, hybrid or bytereorder implementation
    int treetype = atoi(argv[1]);
    BTreeType type = BTreeType::BTreeOLC;

        if (treetype == 1) {
            std::cout << "Testing OLC" << std::endl;
            type = BTreeType::BTreeOLC;
        } else if (treetype == 2) {
            std::cout << "Testing Hybrid" << std::endl;
            type = BTreeType::BTreeHybrid;
        } else if (treetype == 3) {
            std::cout << "Testing Byte Reordering" << std::endl;
            type = BTreeType::BTreeByteReorder;
        }
    
    // Construct the btree implementation we want to test.

    using Key = unsigned long long int;
    using Value = unsigned long long int;
    auto new_btree_fn = [type]() -> common::BTreeBase<Key, Value> * {
        switch (type) {
	    case BTreeType::BTreeOLC:
                return new btreeolc::BTree<Key, Value>();
            case BTreeType::BTreeHybrid:
                return new btree_hybrid::BTree<Key, Value>();
            case BTreeType::BTreeByteReorder:
                return new btree_bytereorder::BTree<Key, Value>();
            default:
                // should never happen
                assert(false);
                return nullptr;
        }
    };
    // insert the keys from 1 to bulk_load_limit in the btree
    common::BTreeBase<Key, Value> *btree = new_btree_fn();
    unsigned long long int insert = 1;
    srand(time(NULL));
    while (insert <= bulk_load_limit) {
        int value = rand();
        btree->insert(std::pair<unsigned long long int, unsigned long long int>(insert, value));
        if (insert%10000000==0)
		cout << "Key : " << insert << " Value : " << value << endl;
        insert++;
    }
    // R is no. of reader threads, W is number of writer threads, N is number of
    // operations each thread is supposed to do X is the no. of operations after
    // which we measure time taken.
    int R = atoi(argv[3]);
    int W = atoi(argv[4]);
    unsigned long long int N = stoull(argv[5]);
    unsigned long long int X = stoull(argv[6]);
    
    tready.resize(R + W);
    for (int i = 0; i < R + W; i++) {
        tready[i] = false;
    }
    // counter stores bulk_load_limit, next insertions will be ahead of this sequentially
    counter.store(bulk_load_limit, std::memory_order_relaxed);
    // path where files with results will be saved
    string path = argv[7];
    // call the function that spawns threads
    test(R, W, N, btree, X, path);
    //  report_average_time(X, N);
    return 0;
}
