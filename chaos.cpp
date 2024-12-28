#include <iostream>
#include <ctime>
#include <queue>
#include <vector>
#include <sstream>
#include <csignal>
#include "pthread.h"
#include "unistd.h"
#include "semaphore.h"

#define BAKER_COUNT 3          // number of baker threads
#define OVEN_BAKING_TIME 2     // seconds
#define MAX_CUSTOMER_BREADS 15 // Max number of breads a customer can order

using namespace std;

const int OVEN_MAX_CAPACITY = BAKER_COUNT * 10;
static int clockSec = 0;

struct Request
{
    int bakerIndex;
    vector<pair<string, int>> requests;
};

struct ChaosRequest
{
    int bakerIndex;
    pair<string, int> request;
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

static bool customerFinished[BAKER_COUNT];
static bool bakerFinished[BAKER_COUNT];
static bool ovenFinished; // There is only one oven!

pthread_mutex_t sharedSpaceLock = PTHREAD_MUTEX_INITIALIZER; // There is only one shared space!
pthread_mutex_t requestOrderLocks[BAKER_COUNT];
pthread_mutex_t ovenLock = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t sharedSpaceLockCondition[BAKER_COUNT];
pthread_cond_t requestOrderLockConditions[BAKER_COUNT];

queue<Bread> ovenBreadQueue;
queue<Order> requestQueues[BAKER_COUNT];
queue<Order> deliveryQueues[BAKER_COUNT];

sem_t ovenEmptySlots;
sem_t ovenFullSlots;

void mySigHandler(int signo)
{
    printf("Time elapsed: #%d seconds\n", ++clockSec);
}

bool isAllBakersFinished()
{
    for (int i = 0; i < BAKER_COUNT; i++)
    {
        if (!bakerFinished[i])
        {
            return false;
        }
    }
    return true;
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

void getInput(vector<int> &breadCounts, vector<string> &names, const int &queueNumber)
{
    cout << "Bakery Queue number #" << queueNumber;
    string input, breadCountsInput;
    cout << "\tInput customer names: " << endl;
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

void createRequest(Request &request, const int &queueNumber)
{
    vector<string> names;
    vector<int> breadCounts;

    getInput(breadCounts, names, queueNumber);

    if (breadCounts.size() != names.size())
    {
        cerr << "invalid input. exiting...\n";
        exit(EXIT_FAILURE);
    }
    for (size_t i = 0; i < breadCounts.size(); i++)
    {
        request.requests.push_back(std::make_pair(names[i], breadCounts[i]));
    }
}

void *customer(void *req)
{
    auto *request = (ChaosRequest *)(req);
    int bakerIndex = request->bakerIndex;
    string customerQueueName = "Customer_" + request->request.first + to_string(bakerIndex) + ' ';
    printf("%s THREAD started.\n", customerQueueName.c_str());

    queue<Order> *requestQueue = &requestQueues[bakerIndex];
    queue<Order> *deliveryQueue = &deliveryQueues[bakerIndex];
    pthread_mutex_t *requestOrderLock = &requestOrderLocks[bakerIndex];
    pthread_cond_t *requestOrderLockCondition = &requestOrderLockConditions[bakerIndex];

    Order order;
    order.breadCount = request->request.second;
    order.customerName = request->request.first;

    // ------ Sending order --------
    pthread_mutex_lock(requestOrderLock);
    requestQueue->push(order);
    printf("Customer %s is ordering %d breads to baker #%d \n", order.customerName.c_str(), order.breadCount, bakerIndex);
    pthread_cond_signal(requestOrderLockCondition);
    pthread_mutex_unlock(requestOrderLock);
    // ------ End Sending order --------

    // ------ Receiving Bread --------
    pthread_mutex_lock(&sharedSpaceLock);
    while (deliveryQueue->empty())
    {
        pthread_cond_wait(&sharedSpaceLockCondition[bakerIndex], &sharedSpaceLock);
    }
    auto response = deliveryQueue->front();
    deliveryQueue->pop();
    cout << "Customer: " << response.customerName << " Received " << response.breadCount << " breads and is leaving...\n\n";
    pthread_mutex_unlock(&sharedSpaceLock);
    // ------ End Receiving Bread --------

    customerFinished[bakerIndex] = true;
    printf("%s thread ended.\n", customerQueueName.c_str());
    pthread_exit(nullptr);
}

void *baker(void *arg)
{
    int bakerIndex = *(int *)arg;
    string bakerName = "Baker_" + to_string(bakerIndex) + ' ';
    cout << bakerName << "thread starting...\n\n";

    queue<Order> *requestQueue = &requestQueues[bakerIndex];
    queue<Order> *deliveryQueue = &deliveryQueues[bakerIndex];
    pthread_mutex_t *requestOrderLock = &requestOrderLocks[bakerIndex];
    pthread_cond_t *requestOrderLockCondition = &requestOrderLockConditions[bakerIndex];

    while (!customerFinished[bakerIndex])
    {
        // ------ Receive order --------
        pthread_mutex_lock(requestOrderLock);
        while (requestQueue->empty() && !customerFinished[bakerIndex])
        {
            pthread_cond_wait(requestOrderLockCondition, requestOrderLock);
        }
        if (customerFinished[bakerIndex])
        {
            break;
        }
        auto req = requestQueue->front();
        requestQueue->pop();
        pthread_mutex_unlock(requestOrderLock);
        // ------ End Receive order --------

        // ------ Baking on the oven --------
        for (int i = 0; i < req.breadCount; i++)
        {
            Bread bread;
            bread.bakingStartTime = clockSec;
            bread.customerName = req.customerName;
            bread.index = i;
            sem_wait(&ovenEmptySlots);
            pthread_mutex_lock(&ovenLock);
            // printf("%s : creating bread %s_%d\n", bakerName.c_str(), bread.customerName.c_str(), bread.index);
            ovenBreadQueue.push(bread);
            pthread_mutex_unlock(&ovenLock);
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
        deliveryQueue->push(req);
        pthread_cond_signal(&sharedSpaceLockCondition[bakerIndex]);
        pthread_mutex_unlock(&sharedSpaceLock);
        sleep(1);
        // ------ End Delivery to customer --------
    }

    printf("%s thread ending...\n", bakerName.c_str());
    bakerFinished[bakerIndex] = true;
    pthread_exit(nullptr);
}

void *oven(void *arg)
{
    cout << "\nOven thread starting...\n\n";
    while (true)
    {
        int capacity;
        sem_getvalue(&ovenEmptySlots, &capacity);

        if (isAllBakersFinished() && capacity == OVEN_MAX_CAPACITY)
        {
            break;
        }
        if (ovenBreadQueue.empty())
        {
            continue;
        }

        auto bread = ovenBreadQueue.front();
        if (clockSec - bread.bakingStartTime >= OVEN_BAKING_TIME)
        {
            // cout << "Oven: time to put " << bread.customerName << "_" << bread.index << " out!!\n";
            sem_wait(&ovenFullSlots);
            pthread_mutex_lock(&ovenLock);
            ovenBreadQueue.pop();
            pthread_mutex_unlock(&ovenLock);
            sem_post(&ovenEmptySlots);
        }
    }

    ovenFinished = true;
    cout << "Oven thread ending...\n";
    pthread_exit(nullptr);
}

int main(int argc, char *argv[])
{
    //////////////// input and init threads, clock, locks ////////////////
    sem_init(&ovenEmptySlots, 0, OVEN_MAX_CAPACITY);
    sem_init(&ovenFullSlots, 0, 0);
    clockInit();

    vector<Request> reqs(BAKER_COUNT);
    int customerCount = 0;
    for (int i = 0; i < BAKER_COUNT; i++)
    {
        bakerFinished[i] = false;
        customerFinished[i] = false;
        pthread_mutex_init(&requestOrderLocks[i], nullptr);
        pthread_cond_init(&requestOrderLockConditions[i], nullptr);
        pthread_cond_init(&sharedSpaceLockCondition[i], nullptr);

        Request request;
        createRequest(request, i);
        customerCount += request.requests.size();
        request.bakerIndex = i;
        reqs[i] = request;
    }
    pthread_t timer_handler, customer_handler[customerCount], baker_handler[BAKER_COUNT], oven_handler;

    /////////////////////////////////////////////////////////////////////////

    cout << "\n\n**** Starting program **** \n\n";
    time_t progStart = time(nullptr);

    //////////////// create threads ////////////////
    pthread_create(&timer_handler, nullptr, &timer_thread, nullptr);
    int count = 0;
    for (int i = 0; i < BAKER_COUNT; i++)
    {
        int *bakerIndex = (int *)malloc(sizeof(int));
        *bakerIndex = i;
        pthread_create(&baker_handler[i], nullptr, &baker, bakerIndex);

        for (int j = 0; j < reqs[i].requests.size(); j++)
        {
            auto* newReq = (ChaosRequest*) malloc(sizeof(ChaosRequest));
            newReq->bakerIndex = i;
            newReq->request = reqs[i].requests[j];
            pthread_create(&customer_handler[count++], nullptr, &customer, newReq);
        }
    }
    pthread_create(&oven_handler, nullptr, oven, nullptr);
    ////////////////////////////////////////////////

    //////////////// join threads ////////////////
    for (int i = 0; i < customerCount; i++)
    {
        pthread_join(customer_handler[i], nullptr);
    }

    for (int i = 0; i < BAKER_COUNT; i++)
    {
        pthread_join(baker_handler[i], nullptr);
    }
    pthread_join(oven_handler, nullptr);
    pthread_join(timer_handler, nullptr);
    //////////////////////////////////////////////

    ////////////////// destroy locks and conditions ////////////////
    pthread_mutex_destroy(&sharedSpaceLock);
    for (int i = 0; i < BAKER_COUNT; i++)
    {
        pthread_mutex_destroy(&requestOrderLocks[i]);
        pthread_cond_destroy(&requestOrderLockConditions[i]);
        pthread_cond_destroy(&sharedSpaceLockCondition[i]);
    }
    pthread_mutex_destroy(&ovenLock);
    sem_destroy(&ovenEmptySlots);
    sem_destroy(&ovenFullSlots);
    ////////////////////////////////////////////////////////////////

    cout << "\n\n**** Ending program **** \n\n";
    cout << "Total Execution time: " << time(nullptr) - progStart << " Seconds.\n";

    return 0;
}
