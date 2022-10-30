#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <list>
#include <string>
#include <vector>


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
    int time_cpu_remain_prev;
    int cpu_burst_prev;
    
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
        time_cpu_remain_prev = 0;
        cpu_burst_prev = 0;
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

    bool isEmpty() {
        return que.empty();
    }

    int next_event_time() {
        if (que.empty()) {
            return -1;
        }
        else {
            return que.front()->timestamp;
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
        int randint = 1 + (randvals[ofs] % burst);
        ofs++;
        return randint;
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


struct stat 
{
    int PID;
    int AT;
    int TC;
    int CB;
    int IO;
    int PRIO;
    int FT;
    int TT;
    int IT;
    int CW;
    stat(int pid, int at, int tc, int cb, int io, int prio) {
        PID = pid;
        AT = at;
        TC = tc;
        CB = cb;
        IO = io;
        PRIO = prio;
        FT = 0;
        TT = 0;
        IT = 0;
        CW = 0;
    }
};

class Stats
{
public:
    int time_finished;
    int CPU_UTIL;
    int IO_UTIL;
    int time_io_finished;
    vector<stat> process_stats;

    void print_scheduler(string scheduler_type) {
        cout << scheduler_type << "\n";
        int N = process_stats.size();
        double TT = 0.0;
        double CW = 0.0;
        for(int i = 0; i < N; i++) {
            printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n",
                    process_stats[i].PID,
                    process_stats[i].AT, process_stats[i].TC, process_stats[i].CB, process_stats[i].IO, process_stats[i].PRIO,
                    process_stats[i].FT,
                    process_stats[i].FT - process_stats[i].AT,
                    process_stats[i].IT,
                    process_stats[i].CW);
            TT += process_stats[i].FT - process_stats[i].AT;
            CW += process_stats[i].CW;
        }
        double CPU_UTIL_P = 100.0 * CPU_UTIL / time_finished;
        double IO_UTIL_P = 100.0 * IO_UTIL / time_finished;
        double TT_AVG = TT / N;
        double CW_AVG = CW / N;
        double throughput = 100.0 * N / time_finished;
        printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n",
                time_finished,
                CPU_UTIL_P,
                IO_UTIL_P,
                TT_AVG,
                CW_AVG,
                throughput);
    }

    Stats() {
        time_finished = 0;
        CPU_UTIL = 0;
        IO_UTIL = 0;
        time_io_finished = 0;
    };

    ~Stats() {};
};


void simulation (Scheduler * scheduler, EventQueue * eventQue, int quantum, Random &rand, Stats &stats) {
    Event * event;
    int time_current = 0;
    bool call_scheduler = false;
    Process * process_running = nullptr;

    while ((event = eventQue->get_event())) {
        Process * proc = event->process;
        time_current = event->timestamp;
        proc->time_in_prev_state = time_current - proc->timestamp_prev;
        proc->timestamp_prev = time_current;

        switch (event->transition)
        {
        case TRANS_TO_READY: {
            // must come from BLOCKED or CREATED
            // add to run queue, no event created
            if (scheduler->preempt && process_running) {
                int timestamp = 0;
                // pq fix
                list<Event *>::iterator iter;
                for(iter = eventQue->que.begin(); iter != eventQue->que.end(); iter++) {
                    if ((*iter)->process->pid == process_running->pid) {
                        timestamp = (*iter)->timestamp;
                        break;
                    }
                }
                if (timestamp > 0 && timestamp != time_current && (proc->priority - 1) > process_running->priority_d) {
                    process_running->time_cpu_remain = (*iter)->process->time_cpu_remain_prev - (time_current - process_running->timestamp_prev);
                    process_running->cpu_burst = (*iter)->process->cpu_burst_prev - (time_current - process_running->timestamp_prev);
                    eventQue->que.erase(iter);
                    Event * newEvent = new Event(time_current, process_running, STATE_RUNNING, STATE_READY, TRANS_TO_PREEMPT);
                    eventQue->add_event(newEvent);
                }

            }
            scheduler->add_process(proc);
            call_scheduler = true;
            break;
        }

        case TRANS_TO_RUNNING: {
            // create event for either preemption or blocking
            Event * newEvent;
            proc->state = STATE_RUNNING;
            process_running = proc;

            if (proc->cpu_burst == 0) {
                proc->cpu_burst = min(rand.randomInt(proc->max_cpu_burst), proc->time_cpu_remain);
            }

            stats.process_stats[proc->pid].CW += proc->time_in_prev_state;

            proc->time_cpu_remain_prev = proc->time_cpu_remain;
            proc->cpu_burst_prev = proc->cpu_burst;

            if (proc->cpu_burst > quantum) {
                // Preempt
                if (proc->time_cpu_remain > quantum) {
                    int time = time_current + quantum;
                    proc->time_cpu_remain -= quantum;
                    proc->cpu_burst -= quantum;
                    newEvent = new Event(time, proc, STATE_RUNNING, STATE_READY, TRANS_TO_PREEMPT);
                }
                // Done
                else {
                    int time = time_current + proc->time_cpu_remain;
                    proc->time_cpu_remain = 0;
                    newEvent = new Event(time, proc, STATE_RUNNING, STATE_DONE, TRANS_TO_DONE);
                }
            }
            else {
                // Blocked
                if (proc->time_cpu_remain > proc->cpu_burst) {
                    int time = time_current + proc->cpu_burst;
                    proc->time_cpu_remain -= proc->cpu_burst;
                    proc->cpu_burst = 0;
                    newEvent = new Event(time, proc, STATE_RUNNING, STATE_BLOCKED, TRANS_TO_BLOCKED);
                }
                // Done
                else {
                    int time = time_current + proc->time_cpu_remain;
                    proc->time_cpu_remain = 0;
                    newEvent = new Event(time, proc, STATE_RUNNING, STATE_DONE, TRANS_TO_DONE);
                }
            }
            eventQue->add_event(newEvent);
            break;
        }

        case TRANS_TO_BLOCKED: {
            //create an event for when process becomes READY again
            Event * newEvent;
            proc->state = STATE_BLOCKED;
            proc->io_burst = rand.randomInt(proc->max_io_burst);

            stats.CPU_UTIL += proc->time_in_prev_state;

            // IO
            if (time_current > stats.time_io_finished) {
                stats.IO_UTIL += proc->io_burst;
                stats.time_io_finished = time_current + proc->io_burst;
            }
            else if (time_current + proc->io_burst > stats.time_io_finished) {
                stats.IO_UTIL += (time_current + proc->io_burst - stats.time_io_finished);
                stats.time_io_finished = time_current + proc->io_burst;
            }

            stats.process_stats[proc->pid].IT += proc->io_burst;

            // Ready
            int time = time_current + proc->io_burst;
            newEvent = new Event(time, proc, STATE_BLOCKED, STATE_READY, TRANS_TO_READY);
            eventQue->add_event(newEvent);
            call_scheduler = true;
            process_running = nullptr;
            break;
        }

        case TRANS_TO_PREEMPT: {
            // must come from RUNNING (preemption)
            // add to runqueue (no event is generated)
            stats.CPU_UTIL += proc->time_in_prev_state;
            scheduler->add_process(proc);
            call_scheduler = true;
            process_running = nullptr;
            break;
        }

        case TRANS_TO_DONE: {
            proc->state = STATE_DONE;
            stats.process_stats[proc->pid].FT = time_current;
            stats.CPU_UTIL += proc->time_in_prev_state;
            call_scheduler = true;
            process_running = nullptr;
            break;
        }

        default:
            break;
        }

        delete event;
        event = nullptr;

        if (call_scheduler) {
            if (eventQue->next_event_time() == time_current) {
                continue;
            }

            call_scheduler = false;

            if (process_running == nullptr) {
                process_running = scheduler->get_next_process();
                if (process_running) {
                    Event * newEvent = new Event(time_current, process_running, STATE_READY, STATE_RUNNING, TRANS_TO_RUNNING);
                    eventQue->add_event(newEvent);
                }
            }
        }
    }
    stats.time_finished = time_current;
}



int main (int argc, char ** argv) {
    int verbose = 0;
    int quantum = 10000;
    int maxPrio = 4;    // default max priority
    int c;
    string scheduler_type;
    Stats stats = Stats();

    // read arguments
    while ((c = getopt(argc, argv, "s:v")) != -1) {
        switch (c)
        {
        case 's':
            scheduler_type = optarg;
            break;
        case 'v':
            verbose = 1;
            break;
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
    EventQueue * event_que = new EventQueue();

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
    int PRIO;

    while (!input.eof()) {
        input >> AT;
        if (input.eof()) {
            break;
        }
        input >> TC;
        input >> CB;
        input >> IO;
        PRIO = rand.randomInt(maxPrio);
        Process * process = new Process(pid, AT, TC, CB, IO, PRIO);
        Event * event = new Event(AT, process, STATE_CREATED, STATE_READY, TRANS_TO_READY);
        event_que->add_event(event);
        stat procStat(pid, AT, TC, CB, IO, PRIO);
        stats.process_stats.push_back(procStat);
        pid++;
    }
    input.close();

    simulation(scheduler, event_que, quantum, rand, stats);

    stats.print_scheduler(scheduler->scheduler_type);

    return 0;
}

