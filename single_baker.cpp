#include <iostream>
#include <ctime>
#include <queue>
#include <vector>
#include <sstream>
#include <csignal>
#include "pthread.h"
#include "unistd.h"
#include "semaphore.h"

#define BAKER_COUNT 1          // number of baker threads
#define OVEN_BAKING_TIME 5     // seconds
#define MAX_CUSTOMER_BREADS 15 // Max number of breads a customer can order

using namespace std;

const int OVEN_MAX_CAPACITY = BAKER_COUNT * 10;
static int clockSec = 0;

struct Request
{
    vector<pair<string, int>> requests;
};

struct Order
{
    string customerName;
    int breadCount;
};

struct Bread
{
    int bakingStartTime;
    int index;
    string customerName;
};

static bool customerFinished = false;
static bool bakerFinished = false;
static bool ovenFinished = false;

pthread_mutex_t sharedSpaceLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t sharedSpaceLockCondition = PTHREAD_COND_INITIALIZER;
queue<Order *> requestQueue;

pthread_mutex_t requestOrderLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t requestOrderLockCondition = PTHREAD_COND_INITIALIZER;
queue<Order> deliveryQueue;

sem_t ovenEmptySlots;
sem_t ovenFullSlots;
pthread_mutex_t ovenLock = PTHREAD_MUTEX_INITIALIZER;
queue<Bread> ovenBreadQueue;

void mySigHandler(int signo)
{
    pid_t threadId = gettid();
    // cout << "With thread " << threadId << " : ";
    printf("Time elapsed: #%d seconds\n", ++clockSec);
}

void *timer_thread(void *arg)
{
    struct sigaction signalAction{};
    signalAction.sa_flags = 0;
    signalAction.sa_handler = mySigHandler;
    sigemptyset(&signalAction.sa_mask);

    int signo = SIGRTMIN;

    if (sigaction(signo, &signalAction, nullptr) == -1)
    {
        perror("sigaction");
        pthread_exit(nullptr);
    }

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, signo);
    pthread_sigmask(SIG_UNBLOCK, &mask, nullptr);

    timer_t timer_id;
    struct sigevent sev{};
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = signo;
    sev.sigev_value.sival_ptr = &timer_id;

    if (timer_create(CLOCK_REALTIME, &sev, &timer_id) == -1)
    {
        perror("timer_create");
        pthread_exit(nullptr);
    }

    struct itimerspec timerSpec{};
    timerSpec.it_value.tv_sec = 1;
    timerSpec.it_value.tv_nsec = 0;
    timerSpec.it_interval.tv_sec = 1;
    timerSpec.it_interval.tv_nsec = 0;

    if (timer_settime(timer_id, 0, &timerSpec, nullptr) == -1)
    {
        perror("timer_settime");
        pthread_exit(nullptr);
    }

    while (!ovenFinished)
    {
        pause();
    }

    pthread_exit(nullptr);
}

void clockInit()
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGRTMIN);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);
}

void getInput(vector<int> &breadCounts, vector<string> &names)
{
    string input, breadCountsInput;
    cout << "Input customer names: " << endl;
    getline(cin, input);
    cout << "Input each customer order: " << endl;
    getline(cin, breadCountsInput);

    istringstream breadSS(breadCountsInput);
    istringstream iss(input);
    string word, breadCount;

    while (iss >> word)
    {
        names.push_back(word);
    }

    while (breadSS >> breadCount)
    {
        int count = stoi(breadCount);
        if (count > MAX_CUSTOMER_BREADS || count <= 0)
        {
            cerr << "You can't order more than " << MAX_CUSTOMER_BREADS << " or less than one! Exiting...\n";
            exit(1);
        }
        breadCounts.push_back(count);
    }
}

void *customer(void *req)
{
    cout << "customer thread starting..." << endl;
    auto *request = (Request *)(req);

    for (size_t i = 0; i < request->requests.size(); i++)
    {
        // ------ Sending order --------
        pthread_mutex_lock(&requestOrderLock);
        Order order;
        order.breadCount = request->requests[i].second;
        order.customerName = request->requests[i].first;
        requestQueue.push(&order);
        cout << "Customer: " << order.customerName << " is ordering " << order.breadCount << " breads.\n";
        pthread_cond_signal(&requestOrderLockCondition);
        pthread_mutex_unlock(&requestOrderLock);
        sleep(1);
        // ------ End Sending order --------

        // ------ Receiving Bread --------
        pthread_mutex_lock(&sharedSpaceLock);
        while (deliveryQueue.empty())
        {
            pthread_cond_wait(&sharedSpaceLockCondition, &sharedSpaceLock);
        }
        auto response = deliveryQueue.front();
        deliveryQueue.pop();
        cout << "Customer:" << response.customerName << " Received " << response.breadCount << " breads and is leaving...\n\n";
        pthread_mutex_unlock(&sharedSpaceLock);
        // ------ End Receiving Bread --------
    }

    cout << "customer thread ending..." << endl;
    customerFinished = true;
    pthread_exit(nullptr);
}

void createRequest(Request *request)
{
    vector<string> names;
    vector<int> breadCounts;

    getInput(breadCounts, names);

    if (breadCounts.size() != names.size())
    {
        cerr << "invalid input. exiting...\n";
        exit(EXIT_FAILURE);
    }
    for (size_t i = 0; i < breadCounts.size(); i++)
    {
        request->requests.push_back(std::make_pair(names[i], breadCounts[i]));
    }
}

void *baker(void *arg)
{
    cout << "Baker thread starting...\n\n";
    while (!customerFinished)
    {
        // ------ Receive order --------
        pthread_mutex_lock(&requestOrderLock);
        while (requestQueue.empty())
        {
            pthread_cond_wait(&requestOrderLockCondition, &requestOrderLock);
        }
        auto req = requestQueue.front();
        requestQueue.pop();
        pthread_mutex_unlock(&requestOrderLock);
        // ------ End Receive order --------

        // ------ Baking on the oven --------
        cout << "Baker: Putting order of customer into oven, count:" << req->breadCount << " , name: " << req->customerName << endl;
        for (int i = 0; i < req->breadCount; i++)
        {
            Bread bread;
            bread.bakingStartTime = clockSec;
            bread.customerName = req->customerName;
            bread.index = i;
            sem_wait(&ovenEmptySlots);
            pthread_mutex_lock(&ovenLock);
            cout << "Baker: creating bread " << bread.customerName << "_" << bread.index << endl;
            ovenBreadQueue.push(bread);
            pthread_mutex_unlock(&ovenLock);
            cout << "Baker: putting the bread into oven...\n\n";
            sem_post(&ovenFullSlots);
        }
        // ------ End baking on the oven --------

        // ------ Waiting for the oven to bake. --------
        while (!ovenBreadQueue.empty())
        {
            sleep(1);
        }
        // ------ End Waiting for the oven to bake. --------

        // ------ Delivery to customer --------
        pthread_mutex_lock(&sharedSpaceLock);
        deliveryQueue.push(*req);
        pthread_cond_signal(&sharedSpaceLockCondition);
        cout << "\n Baker: Delivery Signal sent. unlocking...\n";
        pthread_mutex_unlock(&sharedSpaceLock);
        sleep(1);
        // ------ End Delivery to customer --------
    }
    cout << "Baker thread ending...\n";
    bakerFinished = true;
    pthread_exit(nullptr);
}

void *oven(void *arg)
{
    cout << "\nOven thread starting...\n\n";
    while (true)
    {
        int capacity;
        sem_getvalue(&ovenEmptySlots, &capacity);
        if (bakerFinished && capacity == OVEN_MAX_CAPACITY)
        {
            cout << "\nOven: It's done!! \n";
            break;
        }
        if (ovenBreadQueue.empty())
        {
            continue;
        }
        auto bread = ovenBreadQueue.front();
        if (clockSec - bread.bakingStartTime >= OVEN_BAKING_TIME)
        {
            // cout << "Oven: current bread: " << bread.customerName << "_" << bread.index << " at " << bread.bakingStartTime << "s\n";
            cout << "Oven: time to put " << bread.customerName << "_" << bread.index << " out!!\n";
            sem_wait(&ovenFullSlots);
            pthread_mutex_lock(&ovenLock);
            ovenBreadQueue.pop();
            pthread_mutex_unlock(&ovenLock);
            sem_post(&ovenEmptySlots);
            // cout << "Oven: bread " << bread.customerName << "_" << bread.index << " is fully out.\n";

            sem_getvalue(&ovenEmptySlots, &capacity);
            // printf("Oven: current capacity: %d \n", capacity);
        }
        // printf("Oven: current capacity: %d \n", capacity);
    }
    cout << "Oven thread ending...\n";
    ovenFinished = true;
    pthread_exit(nullptr);
}

int main(int argc, char *argv[])
{
    // init threads, clock, and locks
    pthread_t timer_handler, customer_handler, baker_handler[BAKER_COUNT], oven_handler;
    sem_init(&ovenEmptySlots, 0, OVEN_MAX_CAPACITY);
    sem_init(&ovenFullSlots, 0, 0);
    clockInit();

    // get input
    auto *request = (Request *)malloc(sizeof(Request));
    createRequest(request);

    cout << "Starting program..." << endl;
    time_t progStart = time(nullptr);

    // create threads
    pthread_create(&timer_handler, nullptr, &timer_thread, nullptr);
    pthread_create(&customer_handler, nullptr, &customer, request);
    pthread_create(&oven_handler, nullptr, oven, nullptr);
    for (unsigned long &i : baker_handler)
    {
        pthread_create(&i, nullptr, baker, nullptr);
    }

    // join threads
    pthread_join(customer_handler, nullptr);
    for (unsigned long i : baker_handler)
    {
        pthread_join(i, nullptr);
    }
    pthread_join(oven_handler, nullptr);
    pthread_join(timer_handler, nullptr);

    // destroy locks and conditions
    pthread_mutex_destroy(&sharedSpaceLock);
    pthread_mutex_destroy(&requestOrderLock);
    pthread_mutex_destroy(&ovenLock);

    pthread_cond_destroy(&sharedSpaceLockCondition);
    pthread_cond_destroy(&requestOrderLockCondition);

    sem_destroy(&ovenEmptySlots);
    sem_destroy(&ovenFullSlots);
    free(request);

    cout << "Ending program. Total time: " << time(nullptr) - progStart << " Seconds.\n";
    return 0;
}
