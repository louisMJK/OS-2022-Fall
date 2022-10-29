#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <list>
#include <string>


using namespace std;


enum process_state_t {
	STATE_CREATED,
	STATE_READY,
	STATE_RUNNING,
	STATE_BLOCKED,
	STATE_DONE
};

enum process_transition_t {
    TRANS_TO_READY,
	TRANS_TO_RUNNING,
	TRANS_TO_BLOCKED,
	TRANS_TO_PREEMPT,
	TRANS_TO_DONE
};


class Process
{
public:
    int pid;
    int arrival_time;
    int total_cpu_time;
    int max_cpu_burst;
    int max_io_burst;
    int priority;
    process_state_t state;
    int timestamp_prev;
    int priority_d;
    int time_cpu_remain;
    int time_in_prev_state;
    int cpu_burst;
    int io_burst;
    
    Process(int id, int at, int tc, int cb, int io, int prio) {
        pid = id;
        arrival_time = at;
        total_cpu_time = tc;
        max_cpu_burst = cb;
        max_io_burst = io;
        priority = prio;
        priority_d = prio - 1;
        state = STATE_CREATED;
        timestamp_prev = arrival_time;
        time_cpu_remain = total_cpu_time;
        time_in_prev_state = 0;
        cpu_burst = 0;
        io_burst = 0;
    }

    ~Process() {};
};


class Event
{
public:
    int timestamp;
    Process * process;
    process_state_t state_prev;
    process_state_t state_next;
    process_transition_t transition;

    Event(int ts, Process * proc, process_state_t prevState, process_state_t nextState, process_transition_t trans) {
        timestamp = ts;
        process = proc;
        state_prev = prevState;
        state_next = nextState;
        transition = trans;
    }

    ~Event() {};
};


class EventQueue
{
public:
    list<Event *> que;

    Event * get_event() {
        if (que.empty()) {
            return nullptr;
        }
        else {
            Event * event = que.front();
            que.pop_front();
            return event;
        }
    }

    void add_event(Event * event) {
        if (que.empty()) {
            que.push_back(event);
        }
        else {
            list<Event *>::iterator iter;
            for (iter = que.begin(); iter != que.end(); iter++) {
                if ((*iter)->timestamp > event->timestamp) {
                    que.insert(iter, event);
                    break;
                }
            }
            if (iter == que.end()) {
                que.push_back(event);
            }
        }
    }

    EventQueue() {};

    ~EventQueue() {};
};


class Scheduler
{
public:
    string scheduler_type;
    bool preempt;
    virtual void add_process(Process * proc) {};
    virtual Process * get_next_process() {return NULL;};
    Scheduler() {};
    ~Scheduler() {};
};

class FCFS: public Scheduler
{
private:
    list<Process * > que;
public:
    void add_process(Process * proc) {
        proc->state = STATE_READY;
        que.push_back(proc);
    }

    Process * get_next_process() {
        if (que.empty()) {
            return nullptr;
        }
        else {
            Process * proc = que.front();
            que.pop_front();
            return proc;
        }
    }

    FCFS() {
        scheduler_type = "FCFS";
        preempt = false;
    }
};


class Random
{
private:
    int * randvals;
    int size;
    int ofs;
public:
    int randomInt(int burst) {
        ofs = ofs % size;
        ofs++;
        return 1 + (randvals[ofs] % burst);
    };

    Random(char * path) {
        ofs = 0;
        ifstream input;
        input.open(path);
        if (!input.is_open()) {
            cout << "Unable to open: " << path << endl;
            exit(0);
        }
        string line;
        getline(input, line);
        size = stoi(line);
        randvals = new int[size];
        for (int i = 0; i < size; i++) {
            getline(input, line);
            randvals[i] = stoi(line);
        }
        input.close();
    };

    ~Random() {};
};


void simulation (Scheduler * scheduler, EventQueue * que, int quantum) {
    Event * event;
    int time_current = 0;
    bool call_scheduler = false;
    Process * process_running;

    while ((event = que->get_event())) {
        Process * proc = event->process;
        time_current = event->timestamp;
        proc->time_in_prev_state = time_current - proc->timestamp_prev;

        int transition = event->transition;

        switch (transition)
        {
        case TRANS_TO_READY:
            /* code */
            break;
        
        case TRANS_TO_RUNNING:
            /* code */
            break;

        case TRANS_TO_BLOCKED:
            /* code */
            break;

        case TRANS_TO_PREEMPT:
            /* code */
            break;

        case TRANS_TO_DONE:
            /* code */
            break;

        default:
            break;
        }

        delete event;
        event = nullptr;

        if (call_scheduler) {

        }
    }
}



int main (int argc, char ** argv) {
    int verbose = 0;
    int quantum = 10;
    int maxPrio = 4;    // default max priority
    int c;
    string scheduler_type;

    // read arguments
    while ((c = getopt(argc, argv, "vs:")) != -1) {
        switch (c)
        {
        case 'v':
            verbose = 1;
            break;
        case 's':
            scheduler_type = optarg;
        default:
            break;
        }
    }

    // create scheduler
    Scheduler * scheduler;
    switch (scheduler_type[0])
    {
    case 'F':
        scheduler = new FCFS();
        break;
    // case 'L':
	// 	scheduler = new LCFS();
	// 	break;
	// case 'S':
	// 	scheduler = new SRTF();
	// 	break;
    default:
        break;
    }

    // create event queue
    EventQueue * que = new EventQueue();

    // read input file
    char * path_input = argv[optind];
    char * path_rand = argv[optind + 1];
    Random rand(path_rand);
    ifstream input;
    input.open(path_input);
    if (!input.is_open()) {
        cout << "Unable to open: " << path_input << endl;
        exit(0);
    }

    int pid = 0;
    int AT;
    int TC;
    int CB;
    int IO;
    int priority;

    while (!input.eof()) {
        input >> AT;
        if (input.eof()) {
            break;
        }
        input >> TC;
        input >> CB;
        input >> IO;
        priority = rand.randomInt(maxPrio);
        Process * process = new Process(pid, AT, TC, CB, IO, priority);
        Event * event = new Event(AT, process, STATE_CREATED, STATE_READY, TRANS_TO_READY);
        que->add_event(event);
        pid++;

        printf("%d --- %d, %d, %d, %d \n", pid, AT, TC, CB, IO);
    }
    input.close();

    // simulation(scheduler, que, quantum);

    return 0;
}

