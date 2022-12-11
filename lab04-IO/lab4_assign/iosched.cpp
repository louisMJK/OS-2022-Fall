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
    double total_turnaround;
    double total_waittime;
    int max_waittime;
    double avg_turnaround;
    double avg_waittime;
};
stats *stat = new stats();


int time_curr = 0;
int head = 0;
int dir = 1;


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

class Scheduler_SSTF: public Scheduler
{
public:
    deque<request *> q;

    void add_request(request *req) {
        if (q.empty()) {
            q.push_back(req);
        }
        else {
            deque<request *>::iterator it = q.begin();
            while (it != q.end() && (*it)->track <= req->track) {
                it++;
            }
            q.insert(it, req);
        }
    }

    request* get_request() {
        if (q.empty()) {
            return  nullptr;
        }
        request *req;
        deque<request *>::iterator it = q.begin();
        while (it != q.end() && (*it)->track < head) {
            it++;
        }

        if (it == q.begin()) {
            req = *it;
        }
        else if (it == q.end()) {
            it--;
            int track_next = (*it)->track;
            while (it != q.begin() && (*it)->track == track_next) {
                it--;
            }
            if (it != q.begin() || (*it)->track != track_next) {
                it++;
            }
            req = *it;
        }
        else {
            int leftDist = head - (*(it - 1))->track;
            int rightDist = (*it)->track - head;
            if (leftDist > rightDist) {
                req = *it;
            }
            else if (leftDist < rightDist) {
                it--;
                int track_next = (*it)->track;
                while (it != q.begin() && (*it)->track == track_next) {
                    it--;
                }
                if (it != q.begin() || (*it)->track != track_next) {
                    it++;
                }
                req = *it;
            }
            else {
                deque<request *>::iterator it_left = it - 1;
                int track_left = (*it_left)->track;
                while (it_left != q.begin() && (*it_left)->track == track_left) {
                    it_left--;
                }
                if (it_left != q.begin() || (*it_left)->track != track_left) {
                    it_left++;
                }
                if ((*it_left)->arrival_time < (*it)->arrival_time) {
                    it = it_left;
                }
                req = *it;
            }
        }
        q.erase(it);
        return req;
    }

    bool empty() {
        return q.empty();
    }

    Scheduler_SSTF() {};
};

class Scheduler_LOOK: public Scheduler
{
public:
    deque<request *> q;

    void add_request(request *req) {
        if (q.empty()) {
            q.push_back(req);
        }
        else {
            deque<request *>::iterator it = q.begin();
            while (it != q.end() && (*it)->track <= req->track) {
                it++;
            }
            q.insert(it, req);
        }
    }

    request* get_request() {
        if (q.empty()) {
            return nullptr;
        }
        request *req;
        deque<request *>::iterator it = q.begin();
        while (it != q.end() && (*it)->track < head) {
            it++;
        }
        if (it != q.end() && (*it)->track == head) {
            req = *it;
            q.erase(it);
            return req;
        }
        if (dir == 1) {
            if (head <= q.back()->track) {
                req = *it;
            }
            else {
                dir = -1;
                it = q.end() - 1;
                while (it != q.begin() && (*it)->track == (*(it - 1))->track) {
                    it--;
                }
                req = *it;
            }
        }
        else if (dir == -1) {
            if (head >= q.front()->track) {
                it--;
                while (it != q.begin() && (*it)->track == (*(it - 1))->track) {
                    it--;
                }
                req = *it;
            }
            else {
                dir = 1;
                it = q.begin();
                req = *it;
            }
        }
        q.erase(it);
        return req;
    }

    bool empty() {
        return q.empty();
    }

    Scheduler_LOOK() {};
};


void simulation(Scheduler *sched) {
    int reqIdx = 0;
    int numOps = 0;
    bool active = false;
    request *req_curr = nullptr;
    request *req_new = reqList[reqIdx];

    while(true) {
        // if a new I/O arrived at the system at this current time
        if (reqIdx < reqList.size() && time_curr == req_new->arrival_time) {
            sched->add_request(req_new);
            reqIdx++;
            req_new = reqList[reqIdx];
            // cout << "T:" << time_curr << " Added:" << reqIdx - 1 << endl;
        }

        // if an IO is active and completed at this time
        if (active && head == req_curr->track) {
            req_curr->end_time = time_curr;
            stat->total_turnaround += time_curr - req_curr->arrival_time;

            // printf("%5d: %5d %5d %5d, head = %d\n", time_curr, req_curr->arrival_time, req_curr->start_time, req_curr->end_time, head);

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
                int waittime = time_curr - req_curr->arrival_time;
                stat->total_waittime += waittime;
                stat->max_waittime = max(stat->max_waittime, waittime);
                if (req_curr->track == head) {
                    continue;
                }
                else {
                    dir = (req_curr->track > head) ? 1 : -1;
                }
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

    // create scheduler
    Scheduler *scheduler;
    switch (algo[0])
    {
    case 'i':
        scheduler = new Scheduler_FIFO();
        break;
    case 'j':
        scheduler = new Scheduler_SSTF();
        break;
    case 's':
        scheduler = new Scheduler_LOOK();
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
        reqList.push_back(req);
    }
    input.close();

    // simulation
    simulation(scheduler);

    // print stats
    for (int i = 0; i < reqList.size(); i++) {
        request *req = reqList[i];
        printf("%5d: %5d %5d %5d\n", i, req->arrival_time, req->start_time, req->end_time);
    }
    printf("SUM: %d %d %.2lf %.2lf %d\n", 
        stat->total_time, stat->total_movement, stat->avg_turnaround, stat->avg_waittime, stat->max_waittime);

    return 0;
}

