#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <vector>
#include <deque>


using namespace std;


struct request
{
    int arrival_time;
    int track;
    int start_time;
    int end_time;
    request (int timeIdx, int trackIdx) {
        arrival_time = timeIdx;
        track = trackIdx;
        start_time = -1;
        end_time = -1;
    }
};
vector<request *> reqList;


struct stats
{
    int total_time;
    int total_movement;
    float total_turnaround;
    float total_waittime;
    int max_waittime;
    float avg_turnaround;
    float avg_waittime;
};
stats *stat = new stats();


class Scheduler
{
public:
    virtual void add_request(request *req) {};
    virtual request* get_request() {return nullptr;};
    virtual bool empty() {return true;};
    Scheduler() {};
    ~Scheduler() {};
};

class Scheduler_FIFO: public Scheduler
{
public:
    deque<request *> q;

    void add_request(request *req) {
        q.push_back(req);
    }

    request* get_request() {
        if (q.empty()) {
            return nullptr;
        }
        request *req = q.front();
        q.pop_front();
        return req;
    }

    bool empty() {
        return q.empty();
    }

    Scheduler_FIFO() {};
};


void simulation(Scheduler *sched) {
    int time_curr = 0;
    int reqIdx = 0;
    int numOps = 0;
    int head = 0;
    int dir = 1;
    bool active = false;
    request *req_curr = nullptr;
    request *req_new = reqList[reqIdx];

    while(true) {
        // if a new I/O arrived at the system at this current time
        if (reqIdx < reqList.size() && time_curr == req_new->arrival_time) {
            sched->add_request(req_new);
            req_new = reqList[++reqIdx];
            // cout << "T" << time_curr << " Added:" << reqIdx - 1 << endl;
        }

        // if an IO is active and completed at this time
        if (active && head == req_curr->track) {
            req_curr->end_time = time_curr;
            stat->total_turnaround += time_curr - req_curr->arrival_time;
            printf("%5d: %5d %5d %5d\n", numOps, req_curr->arrival_time, req_curr->start_time, req_curr->end_time);
            numOps++;
            active = false;
            req_curr = nullptr;
        }

        // if no IO request active now
        if (!active) {
            if (!sched->empty()) {
                // Fetch the next request from IO-queue and start the new IO.
                req_curr = sched->get_request();
                active = true;
                req_curr->start_time = time_curr;
                dir = (req_curr->track > head) ? 1 : -1;
                int waittime = time_curr - req_curr->arrival_time;
                stat->total_waittime += waittime;
                stat->max_waittime = max(stat->max_waittime, waittime);
            }
            else if (numOps == reqList.size()) {
                stat->total_time = time_curr;
                stat->avg_turnaround = stat->total_turnaround / numOps;
                stat->avg_waittime = stat->total_waittime / numOps;
                break;
            }
        }

        // if an IO is active
        if (active) {
            head += dir;
            stat->total_movement++;
        }

        time_curr++;
    }

}


int main(int argc, char **argv) {
    // get arguments
    int c;
    string algo;
    while ((c = getopt(argc, argv, "s:")) != -1) {
        switch (c)
        {
        case 's':
            algo = optarg;
            break;
        default:
            break;
        }
    }

    // create Pager
    Scheduler *scheduler;
    switch (algo[0])
    {
    case 'i':
        scheduler = new Scheduler_FIFO();
        break;
    default:
        break;
    }

    // read input
    string path_input = argv[optind];
    ifstream input;
    string line;
    int timeIdx;
    int trackIdx;
    input.open(path_input);
    while (!input.eof()) {
        getline(input, line);
        if (input.eof()) {
            break;
        }
        if (line[0] == '#') {
            continue;
        }
        size_t pos = line.find(" ");
        timeIdx = stoi(line.substr(0, pos));
        trackIdx = stoi(line.substr(pos + 1));
        request *req = new request(timeIdx, trackIdx);
        // cout << req->arrival_time << endl;
        reqList.push_back(req);
    }
    input.close();

    // simulation
    simulation(scheduler);

    // summary
    printf("SUM: %d %d %.2lf %.2lf %d\n", 
        stat->total_time, stat->total_movement, stat->avg_turnaround, stat->avg_waittime, stat->max_waittime);

    return 0;
}

