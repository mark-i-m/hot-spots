
//this is a test file

// to dos :
// include btree file in this to access that code
// connect with  btree : create an object of btree. use bulk insert, insert and read functions of btree
// measure times after every x operations and at the end for each thread
// store times in an array
// right now this is just one number - we have to make this into a pair of key value

#include<iostream>
#include<atomic>
#include<thread>
#include<random>
using namespace std;

//global variables
std::atomic<std::uint64_t> counter = {0};
bool ready=false;
vector <bool> tready;
int get_counter ()
{
    return ++counter;
}
void reader_child (int thread_id, int ops)
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
        cout<<"Thread "<<thread_id <<" reading "<<read_value<<endl;
        //
        // to do call read function to read value
        //
        ops--;
    }
    return;
}

void writer_child(int thread_id, int ops)
{
    tready[thread_id] = true;
    //wait till all threads have spawned
    while (!ready) {}

    uint64_t write_value;
    while (ops)
    {
        //generate a value for sequential write
        //write_value must be greater than counter
        write_value = get_counter();
        cout<<"Thread "<<thread_id <<" writing "<<write_value<<endl;
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

void test (int R, int W, int N)
{
    // ready variables to start the threads at the same time

    
    //spawn reader threads
    if (W==0 && R!=0)
    {
        std::thread readers [R];
        for (int i=0; i<R; i++)
            readers[i] = std::thread(reader_child, i, N);
        while (!check_all_true(tready)) {}
        ready = true;
        
        for (int i=0; i<R; i++)
            readers[i].join();
    }
    else if (R==0 && W!=0)
    {
        std::thread writers [W];
        for (int i=0; i<W; i++)
            writers[i] = std::thread(writer_child, i, N);
        while (!check_all_true(tready)) {}
        ready = true;
        for (int i=0; i<W; i++)
            writers[i].join();
    }
    else if (R!=0 && W!=0)
    {
        std::thread readers [R];
        for (int i=0; i<R; i++)
            readers[i] = std::thread(reader_child, i, N);
        
        std::thread writers [W];
        for (int i=0; i<W; i++)
            writers[i] = std::thread(writer_child, i+R, N);
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
    int bulk_load_limit=0;
    cout<<"Enter bulk load value"<<endl;
    cin>>bulk_load_limit;
    
    //call the bulk_load function to load integers from 1 to bulk_load_limit into a tree
    //
    // to do
    //
    
    
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
    test (R, W, N);
    return 0;
}

