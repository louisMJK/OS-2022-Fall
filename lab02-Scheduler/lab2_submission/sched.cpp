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
    int state_TS;
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
        state_TS = arrival_time;
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
    process_transition_t transition;

    Event(int ts, Process * proc, process_state_t state_from, process_transition_t trans) {
        timestamp = ts;
        process = proc;
        state_prev = state_from;
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
    int quantum;
    bool preempt;
    virtual void add_process(Process * proc) {};
    virtual Process * get_next_process() {return NULL;};
    Scheduler() {};
    ~Scheduler() {};
};

class FCFS: public Scheduler
{
private:
    list<Process *> que;

public:
    void add_process(Process * p) {
        p->state = STATE_READY;
        que.push_back(p);
    }

    Process * get_next_process() {
        if (que.empty()) {
            return nullptr;
        }
        else {
            Process * p = que.front();
            que.pop_front();
            return p;
        }
    }

    FCFS() {
        scheduler_type = "FCFS";
        quantum = 10000;
        preempt = false;
    }
};

class LCFS: public Scheduler
{
private:
    list<Process *> que;
public:
    void add_process(Process * p) {
        p->state = STATE_READY;
        que.push_back(p);
    }

    Process * get_next_process() {
        if (que.empty()) {
            return nullptr;
        }
        else {
            Process * p = que.back();
            que.pop_back();
            return p;
        }
    }

    LCFS() {
        scheduler_type = "LCFS";
        quantum = 10000;
        preempt = false;
    }
};

class SRTF: public Scheduler 
{
private:
	list<Process *> que;

public:
	void add_process(Process * p) {
		p->state = STATE_READY;
		list<Process *>::iterator iter;
		for(iter = que.begin(); iter != que.end(); iter++) {
			if((*iter)->time_cpu_remain > p->time_cpu_remain) {
				que.insert(iter, p);
				break;
			}
		}
		if(iter == que.end()) {
			que.push_back(p);
		}
	};

	Process * get_next_process()  {
		if(que.empty()) {
			return nullptr;
		}
		else {
			Process * p = que.front();
			que.pop_front();
			return p;
		}
	};

	SRTF() {
		scheduler_type = "SRTF";
        quantum = 10000;
		preempt = false;
	};
};

class RR: public Scheduler
{
private:
    list<Process *> que;

public:
    void add_process(Process * p) {
        p->state = STATE_READY;
        que.push_back(p);
    }

    Process * get_next_process() {
        if (que.empty()) {
            return nullptr;
        }
        else {
            Process * p = que.front();
            que.pop_front();
            return p;
        }
    }

    RR(int quant) {
        scheduler_type = "RR " + to_string(quant);
        quantum = quant;
        preempt = false;
    }
};

class PRIO: public Scheduler
{
private:
    list<Process *> activeQ;
    list<Process *> expiredQ;

public:
    void add_process(Process * p) {
        if (p->state == STATE_RUNNING) {
            p->priority_d -= 1;
        }
        else {
            p->priority_d = p->priority - 1;
        }
        p->state = STATE_READY;
        list<Process *>::iterator iter;
        if (p->priority_d >= 0) {
            for (iter = activeQ.begin(); iter != activeQ.end(); iter++) {
                if (p->priority_d > (*iter)->priority_d) {
                    activeQ.insert(iter, p);
                    break;
                }
            }
            if (iter == activeQ.end()) {
                activeQ.push_back(p);
            }
        }
        else {
            p->priority_d = p->priority - 1;
            for (iter = expiredQ.begin(); iter != expiredQ.end(); iter++) {
                if (p->priority_d > (*iter)->priority_d) {
                    expiredQ.insert(iter, p);
                    break;
                }
            }
            if (iter == expiredQ.end()) {
                expiredQ.push_back(p);
            }
        }
    }

    Process * get_next_process() {
        if (activeQ.empty()) {
            activeQ.swap(expiredQ);
        }
        if (activeQ.empty()) {
            return nullptr;
        }
        else {
            Process * p = activeQ.front();
            activeQ.pop_front();
            return p;
        }
    }

    PRIO(int quant) {
        scheduler_type = "PRIO " + to_string(quant);
        quantum = quant;
        preempt = false;
    }
};

class PREPRIO: public Scheduler
{
private:
    list<Process *> activeQ;
    list<Process *> expiredQ;

public:
    void add_process(Process * p) {
        if (p->state == STATE_RUNNING) {
            p->priority_d -= 1;
        }
        else {
            p->priority_d = p->priority - 1;
        }
        p->state = STATE_READY;
        list<Process *>::iterator iter;
        if (p->priority_d >= 0) {
            for (iter = activeQ.begin(); iter != activeQ.end(); iter++) {
                if (p->priority_d > (*iter)->priority_d) {
                    activeQ.insert(iter, p);
                    break;
                }
            }
            if (iter == activeQ.end()) {
                activeQ.push_back(p);
            }
        }
        else {
            p->priority_d = p->priority - 1;
            for (iter = expiredQ.begin(); iter != expiredQ.end(); iter++) {
                if (p->priority_d > (*iter)->priority_d) {
                    expiredQ.insert(iter, p);
                    break;
                }
            }
            if (iter == expiredQ.end()) {
                expiredQ.push_back(p);
            }
        }
    }

    Process * get_next_process() {
        if (activeQ.empty()) {
            activeQ.swap(expiredQ);
        }
        if (activeQ.empty()) {
            return nullptr;
        }
        else {
            Process * p = activeQ.front();
            activeQ.pop_front();
            return p;
        }
    }

    PREPRIO(int quant) {
        scheduler_type = "PREPRIO " + to_string(quant);
        quantum = quant;
        preempt = true;
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
    int FT;
    int FT_IO;
    int CPU_TIME;
    int IO_TIME;
    vector<stat> process_stats;

    void print_scheduler(Scheduler * scheduler) {
        string scheduler_type = scheduler->scheduler_type;
        cout << scheduler_type << endl;
        int N = process_stats.size();
        double TT = 0.0;
        double CW = 0.0;
        for(int i = 0; i < N; i++) {
            printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n",
                    process_stats[i].PID,
                    process_stats[i].AT, 
                    process_stats[i].TC, 
                    process_stats[i].CB, 
                    process_stats[i].IO, 
                    process_stats[i].PRIO,
                    process_stats[i].FT,
                    process_stats[i].FT - process_stats[i].AT,
                    process_stats[i].IT,
                    process_stats[i].CW);
            TT += process_stats[i].FT - process_stats[i].AT;
            CW += process_stats[i].CW;
        }
        double CPU_UTIL = 100.0 * CPU_TIME / FT;
        double IO_UTIL = 100.0 * IO_TIME / FT;
        double TT_AVG = TT / N;
        double CW_AVG = CW / N;
        double throughput = 100.0 * N / FT;
        printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n",
                FT,
                CPU_UTIL,
                IO_UTIL,
                TT_AVG,
                CW_AVG,
                throughput);
    }

    Stats() {
        FT = 0;
        FT_IO = 0;
        CPU_TIME = 0;
        IO_TIME = 0;
    };

    ~Stats() {};
};


void simulation (Scheduler * scheduler, EventQueue * eventQue, Random &rand, Stats &stats) {
    Event * event;
    int TIME_CURRENT = 0;
    int quantum = scheduler->quantum;
    bool CALL_SCHEDULER = false;
    Process * process_running = nullptr;

    while ((event = eventQue->get_event())) {
        Process * proc = event->process;
        TIME_CURRENT = event->timestamp;
        proc->time_in_prev_state = TIME_CURRENT - proc->state_TS;
        proc->state_TS = TIME_CURRENT;

        switch (event->transition)
        {
        case TRANS_TO_READY: {
            // must come from BLOCKED or CREATED

            // PREPRIO
            if ((proc->state == STATE_BLOCKED || proc->state == STATE_CREATED) && scheduler->preempt && process_running) {
                int timestamp = 0;
                list<Event *>::iterator iter;
                for(iter = eventQue->que.begin(); iter != eventQue->que.end(); iter++) {
                    if ((*iter)->process->pid == process_running->pid) {
                        timestamp = (*iter)->timestamp;
                        break;
                    }
                }
                if (timestamp > 0 && timestamp != TIME_CURRENT && (proc->priority - 1) > process_running->priority_d) {
                    process_running->time_cpu_remain = process_running->time_cpu_remain_prev - (TIME_CURRENT - process_running->state_TS);
                    process_running->cpu_burst = process_running->cpu_burst_prev - (TIME_CURRENT - process_running->state_TS);
                    eventQue->que.erase(iter);
                    Event * newEvent = new Event(TIME_CURRENT, process_running, STATE_RUNNING, TRANS_TO_PREEMPT);
                    eventQue->add_event(newEvent);
                }
            }

            if (proc->state == STATE_BLOCKED) {
                stats.process_stats[proc->pid].IT += proc->time_in_prev_state;
            }

            proc->state = STATE_READY;
            scheduler->add_process(proc);
            CALL_SCHEDULER = true;
            break;
        }

        case TRANS_TO_RUNNING: {
            // create event for either PREEMPT or BLOCKED
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
                // PREEMPT
                if (proc->time_cpu_remain > quantum) {
                    int time = TIME_CURRENT + quantum;
                    proc->time_cpu_remain -= quantum;
                    proc->cpu_burst -= quantum;
                    newEvent = new Event(time, proc, STATE_RUNNING, TRANS_TO_PREEMPT);
                }
                // DONE
                else {
                    int time = TIME_CURRENT + proc->time_cpu_remain;
                    proc->time_cpu_remain = 0;
                    newEvent = new Event(time, proc, STATE_RUNNING, TRANS_TO_DONE);
                }
            }
            else {
                // BLOCKED
                if (proc->time_cpu_remain > proc->cpu_burst) {
                    int time = TIME_CURRENT + proc->cpu_burst;
                    proc->time_cpu_remain -= proc->cpu_burst;
                    proc->cpu_burst = 0;
                    newEvent = new Event(time, proc, STATE_RUNNING, TRANS_TO_BLOCKED);
                }
                // DONE
                else {
                    int time = TIME_CURRENT + proc->time_cpu_remain;
                    proc->time_cpu_remain = 0;
                    newEvent = new Event(time, proc, STATE_RUNNING, TRANS_TO_DONE);
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

            // CPU busy time in READY
            stats.CPU_TIME += proc->time_in_prev_state;

            // IO
            if (TIME_CURRENT > stats.FT_IO) {
                stats.IO_TIME += proc->io_burst;
                stats.FT_IO = TIME_CURRENT + proc->io_burst;
            }
            else if (TIME_CURRENT + proc->io_burst > stats.FT_IO) {
                stats.IO_TIME += (TIME_CURRENT + proc->io_burst - stats.FT_IO);
                stats.FT_IO = TIME_CURRENT + proc->io_burst;
            }

            // BLOCKED -> READY
            int time = TIME_CURRENT + proc->io_burst;
            newEvent = new Event(time, proc, STATE_BLOCKED, TRANS_TO_READY);
            eventQue->add_event(newEvent);
            CALL_SCHEDULER = true;
            process_running = nullptr;
            break;
        }

        case TRANS_TO_PREEMPT: {
            // must come from RUNNING (preemption)
            // add to run queue (no event is generated)
            stats.CPU_TIME += proc->time_in_prev_state;
            scheduler->add_process(proc);
            CALL_SCHEDULER = true;
            process_running = nullptr;
            break;
        }

        case TRANS_TO_DONE: {
            // must from RUNNING
            proc->state = STATE_DONE;
            process_running = nullptr;
            proc->time_cpu_remain = 0;
            CALL_SCHEDULER = true;
            stats.process_stats[proc->pid].FT = TIME_CURRENT;
            stats.CPU_TIME += proc->time_in_prev_state;
            break;
        }

        default:
            break;
        }

        delete event;
        event = nullptr;

        if (CALL_SCHEDULER) {
            if (eventQue->next_event_time() == TIME_CURRENT) {
                continue;
            }
            CALL_SCHEDULER = false;
            if (process_running == nullptr) {
                process_running = scheduler->get_next_process();
                if (process_running == nullptr) {
                    continue;
                }
                Event * newEvent = new Event(TIME_CURRENT, process_running, STATE_READY, TRANS_TO_RUNNING);
                eventQue->add_event(newEvent);
            }
        }
    }
    stats.FT = TIME_CURRENT;
}



int main (int argc, char ** argv) {
    int verbose = 0;
    int quantum = 10000;
    int maxPrio = 4;        // default max priority
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
    case 'F': {
        scheduler = new FCFS();
        break;
    }
    case 'L': {
		scheduler = new LCFS();
		break;
    }
	case 'S': {
		scheduler = new SRTF();
		break;
    }
    case 'R': {
        quantum = stoi(scheduler_type.substr(1, string::npos));
        scheduler = new RR(quantum);
        break;
    }
    case 'P': {
        int split = scheduler_type.find(":");
        if (split != string::npos) {
            quantum = stoi(scheduler_type.substr(1, split));
            maxPrio = stoi(scheduler_type.substr(split + 1, string::npos));
        }
        else {
            quantum = stoi(scheduler_type.substr(1, string::npos));
        }
        scheduler = new PRIO(quantum);
        break;
    }
    case 'E': {
        int split = scheduler_type.find(":");
        if (split != string::npos) {
            quantum = stoi(scheduler_type.substr(1, split));
            maxPrio = stoi(scheduler_type.substr(split + 1, string::npos));
        }
        else {
            quantum = stoi(scheduler_type.substr(1, string::npos));
        }
        scheduler = new PREPRIO(quantum);
        break;
    }
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

    int pid;
    int AT;
    int TC;
    int CB;
    int IO;
    int PRIO;

    pid = 0;

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
        Event * event = new Event(AT, process, STATE_CREATED, TRANS_TO_READY);
        event_que->add_event(event);
        stat procStat(pid, AT, TC, CB, IO, PRIO);
        stats.process_stats.push_back(procStat);
        pid++;
    }
    input.close();

    simulation(scheduler, event_que, rand, stats);

    stats.print_scheduler(scheduler);

    return 0;
}

