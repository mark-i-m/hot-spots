
//this is a test file

// run using command :
// g++ -std=c++11 -pthread eval.cpp -o eval.out
// input bulk_load_limit, R, W. N

// to dos :
//
// measure times after every x operations and at the end for each thread
// store times in an array
// right now this is just one number - we have to make this into a pair of key value

// Features to add in later :
// think time
// bulk_insert

#include<iostream>
#include<atomic>
#include<thread>
#include<random>
#include<vector>
#include<cstdlib>
#include<cassert>

#include "../btrees/btree-base.h"
#include "../btrees/btreeolc.h"
#include "../btrees/btree-hybrid.h"
#include "../btrees/btree-bytereorder.h"

using namespace std;

enum class BTreeType {
    BTreeOLC = 1,
    BTreeHybrid = 2,
    BTreeByteReorder = 3,
};

//global variables
std::atomic<std::uint64_t> counter = {0};
bool ready=false;
vector <bool> tready;
int get_counter ()
{
    return ++counter;
}
void reader_child (int thread_id, int ops, common::BTreeBase<uint64_t, uint64_t> *btree)
{
    tready[thread_id] = true;
    //wait till all threads have spawned
    while (!ready) {};
    uint64_t read_value;
    std::random_device rd;     //Get a random seed from the OS entropy device, or whatever
    std::mt19937_64 eng(rd()); //Use the 64-bit Mersenne Twister 19937 generator
    //and seed it with entropy.
    std::uniform_int_distribution<unsigned long long> distr;
    while (ops)
    {
        //generate a random number of uint_64 to read
        read_value = distr(eng);
        // the number can only be as big as the counter
        cout<<"random is : "<<read_value<<endl;
        read_value = read_value % (counter.load(std::memory_order_relaxed));
        std::pair<uint64_t, uint64_t> read;
        read.first = read_value;
        read.second = 0;
        cout<<"Thread "<<thread_id <<" reading Key "<<read_value<<endl;
        btree->lookup(read.first, read.second);
        cout<<"Value read is "<<read.second<<endl;
        //
        // to do call read function to read value
        //
        ops--;
    }
    return;
}

void writer_child(int thread_id, int ops, common::BTreeBase<uint64_t, uint64_t> *btree)
{
    tready[thread_id] = true;
    //wait till all threads have spawned
    while (!ready) {}

    uint64_t write_value;
    while (ops)
    {
        //generate a value for sequential write
        //write_value must be greater than counter
        std::pair<uint64_t, uint64_t> write;
        write_value = get_counter();
        write.first = write_value;
        write.second = rand();
        cout<<"Thread "<<thread_id <<" writing "<<write.first<<" "<<write.second<<endl;
        btree->insert(write);
        ops--;
    }

    //
    // to do : call insert function to write this value
    //
    return;
}

bool check_all_true (vector<bool> &arr)
{
    for (int i=0; i<arr.size(); i++)
    {
        if (arr[i]==false)
            return false;
    }
    return true;
}

void test (int R, int W, int N, common::BTreeBase<uint64_t, uint64_t> *btree)
{
    // ready variables to start the threads at the same time


    //spawn reader threads
    if (W==0 && R!=0)
    {
        std::thread readers [R];
        for (int i=0; i<R; i++)
            readers[i] = std::thread(reader_child, i, N, btree);
        while (!check_all_true(tready)) {}
        ready = true;

        for (int i=0; i<R; i++)
            readers[i].join();
    }
    else if (R==0 && W!=0)
    {
        std::thread writers [W];
        for (int i=0; i<W; i++)
            writers[i] = std::thread(writer_child, i, N, btree);
        while (!check_all_true(tready)) {}
        ready = true;
        for (int i=0; i<W; i++)
            writers[i].join();
    }
    else if (R!=0 && W!=0)
    {
        std::thread readers [R];
        for (int i=0; i<R; i++)
            readers[i] = std::thread(reader_child, i, N, btree);

        std::thread writers [W];
        for (int i=0; i<W; i++)
            writers[i] = std::thread(writer_child, i+R, N, btree);
        cout<<"Checking if ready"<<ready<<endl;
        while (!check_all_true(tready)) {cout<<"Wait"<<endl;}
        cout<<"Now ready to go"<<endl;
        ready = true;

        for (int i=0; i<R; i++)
           readers[i].join();
        for (int i=0; i<W; i++)
           writers[i].join();
    }
    return;
}


int main()
{
    uint64_t bulk_load_limit=0;
    cout<<"Evaluate which BTree implementation? \n(1) : OLC \n(2) : Hybrid \n(3) : ByteReorder\n";
    int treetype;
    BTreeType type = BTreeType::BTreeOLC;
    do {
    cin>>treetype;

    if (treetype == 1 )
    {
      std::cout << "Testing OLC" << std::endl;
      type = BTreeType::BTreeOLC;
    }
    else if (treetype == 2)
    {
      std::cout << "Testing Hybrid" << std::endl;
      type = BTreeType::BTreeHybrid;
    }
    else if (treetype == 3)
    {
      std::cout << "Testing Byte Reordering" << std::endl;
      type = BTreeType::BTreeByteReorder;
    }
    else
      cout<<"Wrong input. Please enter option 1, 2 or 3\n";
    }while (treetype!=1 && treetype!=2 && treetype!=3);
    cout<<"Enter bulk load value"<<endl;
    cin>>bulk_load_limit;

    //call the bulk_load function to load integers from 1 to bulk_load_limit into a tree
    //
    // to do
    //
    // Construct the btree implementation we want to test.

    using Key = uint64_t;
    using Value = uint64_t;
    auto new_btree_fn = [type]() -> common::BTreeBase<Key, Value> * {
        switch (type) {
            case BTreeType::BTreeOLC:
                return new btreeolc::BTree<Key, Value>();
            case BTreeType::BTreeHybrid:
                return new btree_hybrid::BTree<Key, Value>();
            case BTreeType::BTreeByteReorder:
                return new btree_bytereorder::BTree<Key, Value>();
        }
    };
    
    common::BTreeBase<Key, Value> *btree  = new_btree_fn();
    int insert = 1;
    srand ( time(NULL) );
    while (insert<=bulk_load_limit)
    {
        int value = rand();
        btree->insert(std::pair<uint64_t, uint64_t> (insert, value));
        cout<<"Key : "<<insert<<" Value : "<<value<<endl;
        insert++;
    }
    // R is no. of reader threads, W is number of writer threads, N is number of operations each thread is supposed to do
    int R, W, N;
    cin>>R>>W>>N;
    tready.resize(R+W);
    for (int i=0; i<R+W; i++)
    {
        tready[i] = false;
    }
    counter.store(bulk_load_limit,std::memory_order_relaxed);
    cout<<"Initial counter is "<<counter<<endl;
    // to do timing???
    test (R, W, N, btree);
    return 0;
}
