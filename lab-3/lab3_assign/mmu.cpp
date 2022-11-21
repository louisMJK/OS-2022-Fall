#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <vector>
#include <string>
#include <deque>


using namespace std;


const int MAX_VPAGES = 64;
const int MAX_FRAMES = 128;


struct pte_t 
{
    unsigned int present:1;
    unsigned int referenced:1;
    unsigned int modified:1;
    unsigned int write_protect:1;
    unsigned int paged_out:1;
    unsigned int frame_index:7;
    unsigned int file_mapped:1;
};


struct frame_t 
{
    int allocated;
    int pid;
    int vpage;
    unsigned int age:32;
    int time_last_use;
};


frame_t *frame_table = new frame_t[MAX_FRAMES]();


struct instruction
{
    char op;
    int val;
    instruction(char instruction, int value) {
        op = instruction;
        val = value;
    }
};


struct VMA
{
    int start_page;
    int end_page;
    int write_protected;
    int file_mapped;
    VMA(int start, int end, int writeProteced, int fileMapped) {
        start_page = start;
        end_page = end;
        write_protected = writeProteced;
        file_mapped = fileMapped;
    };
};


class Process
{
public:
    int pid;
    int numVMA;
    vector<VMA> VMAList;
    pte_t *pageTable;

    Process(int id, vector<VMA> vmaList, pte_t *page_table) {
        pid = id;
        VMAList = vmaList;
        numVMA = VMAList.size();
        pageTable = page_table;
    };

    Process() {};
    ~Process() {};
};

vector<Process *> process_list;


class Pager
{
public:
    char algo;
    int FT_size;
    int instr_curr;
    int instr_prev;
    virtual int select_victim_frame_index() {return -1;};
    virtual void add_frame(int frame_idx) {};
    Pager() {};
    ~Pager() {};
};

class Pager_FIFO: public Pager
{
public:
    deque<int> q;
    int idx;

    int select_victim_frame_index() {
        if (q.empty()) {
            return -1;
        }
        int victimIdx = q[idx];
        idx = (idx + 1) % FT_size;
        return victimIdx;
    };

    void add_frame(int frame_idx) {
        if (q.size() < FT_size) {
            q.push_back(frame_idx);
        }
    };

    Pager_FIFO(int num_frames) {
        idx = 0;
        FT_size = num_frames;
    };

    ~Pager_FIFO() {};
};

class Pager_Clock: public Pager
{
public:
    deque<int> q;
    int idx;

    int select_victim_frame_index() {
        if (q.empty()) {
            return -1;
        }
        frame_t *frame = &frame_table[q[idx]];
        pte_t *pte = &process_list[frame->pid]->pageTable[frame->vpage];
        while (pte->referenced) {
            pte->referenced = 0;
            idx = (idx + 1) % q.size();
            frame = &frame_table[q[idx]];
            pte = &process_list[frame->pid]->pageTable[frame->vpage];
        }
        int victimIdx = q[idx];
        idx = (idx + 1) % q.size();
        return victimIdx;
    }

    void add_frame(int frame_idx) {
        if (q.size() < FT_size) {
            q.push_back(frame_idx);
        }
    }

    Pager_Clock(int num_frames) {
        idx = 0;
        FT_size = num_frames;
    };

    ~Pager_Clock() {};
};

class Pager_Random: public Pager
{
public:
    deque<int> q;
    int idx;
    int *randvals;

    int select_victim_frame_index() {
        if (q.empty()) {
            return -1;
        }
        int randIdx = randvals[idx] % q.size();
        idx++;
        return q[randIdx];
    };

    void add_frame(int frame_idx) {
        if (q.size() < FT_size) {
            q.push_back(frame_idx);
        }
    };

    Pager_Random(int num_frames, string path) {
        idx = 0;
        FT_size = num_frames;
        ifstream input;
        string line;
        input.open(path);
        getline(input, line);
        int size = stoi(line);
        randvals = new int[size];
        for (int i = 0; i < size; i++) {
            getline(input, line);
            randvals[i] = stoi(line);
        }
        input.close();
    };

    ~Pager_Random() {};
};


bool ESC_reset = false;

class Pager_ESC: public Pager
{
public:
    deque<int> q;
    int idx;

    int select_victim_frame_index() {
        if (q.empty()) {
            return -1;
        }
        int frame_class[4] = {-1, -1, -1, -1};
        int victimIdx;
        frame_t *frame;
        pte_t *pte;
        for (int i = 0; i < q.size(); i++) {
            frame = &frame_table[q[idx]];
            pte = &process_list[frame->pid]->pageTable[frame->vpage];
            int class_index = 2 * pte->referenced + pte->modified;
            if (frame_class[class_index] == -1) {
                frame_class[class_index] = idx;
            }
            if (ESC_reset) {
                pte->referenced = 0;
            }
            idx = (idx + 1) % q.size();
        }
        for (int i = 0; i < 4; i++) {
            if (frame_class[i] != -1) {
                victimIdx = q[frame_class[i]];
                idx = (victimIdx + 1) % q.size();
                break;
            }
        }
        if (ESC_reset) {
            instr_prev = instr_curr;
        }
        return victimIdx;
    };

    void add_frame(int frame_idx) {
        if (q.size() < FT_size) {
            q.push_back(frame_idx);
        }
    };

    Pager_ESC(int num_frames) {
        idx = 0;
        FT_size = num_frames;
        instr_curr = 0;
        instr_prev = -1;
    };

    ~Pager_ESC() {};
};

class Pager_Aging: public Pager
{
public:
    deque<int> q;
    int idx;

    int select_victim_frame_index() {
        if (q.empty()) {
            return -1;
        }
        int victimIdx;
        unsigned long minAge = ULONG_MAX;
        frame_t *frame;
        pte_t *pte;
        for (int i = 0; i < FT_size; i++) {
            frame = &frame_table[q[idx]];
            pte = &process_list[frame->pid]->pageTable[frame->vpage];
            frame->age = frame->age >> 1;
            if (pte->referenced) {
                frame->age = (frame->age | 0x80000000);
            }
            pte->referenced = 0;
            if (frame->age < minAge) {
                minAge = frame->age;
                victimIdx = q[idx];
            }
            idx = (idx + 1) % FT_size;
        }
        idx = (victimIdx + 1) % FT_size;
        return victimIdx;
    };

    void add_frame(int frame_idx) {
        if (q.size() < FT_size) {
            q.push_back(frame_idx);
        }
    };

    Pager_Aging(int num_frames) {
        idx = 0;
        FT_size = num_frames;
    };

    ~Pager_Aging() {};
};

class Pager_WorkingSet: public Pager
{
public:
    deque<int> q;
    int idx;

    int select_victim_frame_index() {
        if (q.empty()) {
            return -1;
        }
        int vicitimIdx = idx;
        int time_min = instr_curr + 1;
        frame_t *frame;
        pte_t *pte;
        for (int i = 0; i < FT_size; i++) {
            frame = &frame_table[q[idx]];
            pte = &process_list[frame->pid]->pageTable[frame->vpage];
            if (pte->referenced) {
                frame->time_last_use = instr_curr;
                pte->referenced = 0;
            }
            else {
                int age = instr_curr - frame->time_last_use;
                if (age >= 50) {
                    vicitimIdx = q[idx];
                    break;
                }
                else if (frame->time_last_use < time_min) {
                    time_min = frame->time_last_use;
                    vicitimIdx = q[idx];
                }
            }
            idx = (idx + 1) % FT_size;
        }
        idx = (vicitimIdx + 1) % FT_size;
        return vicitimIdx;
    };

    void add_frame(int frame_idx) {
        if (q.size() < FT_size) {
            q.push_back(frame_idx);
        }
    };

    Pager_WorkingSet(int num_frames) {
        idx = 0;
        FT_size = num_frames;
    };

    ~Pager_WorkingSet() {};
};


struct Stats
{
    unsigned long inst_count;
    unsigned long ctx_switches;
    unsigned long process_exits;
    unsigned long long cost;
    int num_frames;
};

struct PStat
{
    unsigned long unmaps;
    unsigned long maps;
    unsigned long ins;
    unsigned long outs;
    unsigned long fins;
    unsigned long fouts;
    unsigned long zeros;
    unsigned long segv;
    unsigned long segprot;
};

Stats *stats = new Stats();
PStat *pstats;


void simulation (vector<instruction> instr_list, Pager *pager) {
    deque<int> free_frame_list;
    int pid;
    int vpage;
    Process *proc_curr;
    vector<VMA> VMAList;

    for (int i = 0; i < pager->FT_size; i++) {
        free_frame_list.push_back(i);
    }

    // instructions
    for (int i = 0; i < instr_list.size(); i++) {
        char op = instr_list[i].op;
        int val = instr_list[i].val;

        cout << i << ": ==> " << op << " " << val << endl;

        // context switch
        if (op == 'c') {
            pid = val;
            proc_curr = process_list[pid];
            stats->ctx_switches++;
            stats->cost += 130;
            continue;
        }
        // exit
        if (op == 'e') {
            pid = proc_curr->pid;
            cout << "EXIT current process " << pid << endl;
            stats->process_exits++;
            stats->cost += 1250;
            // page table of current process
            pte_t *page_table = proc_curr->pageTable;
            for (int page = 0; page < MAX_VPAGES; page++) {
                pte_t *pte = &page_table[page];
                pte->paged_out = 0;
                if (!pte->present) {
                    continue;
                }
                // Unmap PTE
                pte->present = 0;
                printf(" UNMAP %d:%d\n", pid, page);
                pstats[pid].unmaps++;
                stats->cost += 400;
                // FOUT
                if (pte->modified && pte->file_mapped) {
                    cout << " FOUT" << endl;
                    pstats[pid].fouts++;
                    stats->cost += 2400;
                }
                // return frame to free_frame_list
                int frameIdx = pte->frame_index;
                frame_t *frame = &frame_table[frameIdx];
                frame->allocated = 0;
                free_frame_list.push_back(frameIdx);
            }
            continue;
        }

        // read / write
        stats->cost += 1;
        vpage = val;
        pte_t *pte = &proc_curr->pageTable[vpage];

        if (pager->algo == 'e') {
            ESC_reset = (i - pager->instr_prev >= 50) ? true : false;
        }
        pager->instr_curr = i;

        // page fault exception
        if (!pte->present) {
            // verify this is actually a valid page in a vma, if not raise error and next inst
            bool isValid = false;
            int write_protected = 0;
            int file_mapped = 0;
            VMAList = proc_curr->VMAList;
            for (int j = 0; j < VMAList.size(); j++) {
                VMA vma = VMAList[j];
                if (vpage >= vma.start_page && vpage <= vma.end_page) {
                    isValid = true;
                    write_protected = vma.write_protected;
                    file_mapped = vma.file_mapped;
                    break;
                }
            }
            if (!isValid) {
                cout << " SEGV" << endl;
                pstats[proc_curr->pid].segv++;
                stats->cost += 340;
                continue;
            }
            pte->write_protect = write_protected;
            pte->file_mapped = file_mapped;

            // vpage valid -> get new frame
            int new_frame_idx;
            if (!free_frame_list.empty()) {
                new_frame_idx = free_frame_list[0];
                free_frame_list.pop_front();
                pager->add_frame(new_frame_idx);    // add new frame to pager
            }
            else {
                new_frame_idx = pager->select_victim_frame_index();
            }
            frame_t *frame_new = &frame_table[new_frame_idx];

            // update PTE -> new frame
            pte->present = 1;
            pte->frame_index = new_frame_idx;

            // frame is mapped -> Unmap
            if (frame_new->allocated) {
                int pid_prev = frame_new->pid;
                int vpage_prev = frame_new->vpage;
                Process *proc_prev = process_list[pid_prev];
                pte_t *pte_prev = &proc_prev->pageTable[vpage_prev];

                printf(" UNMAP %d:%d\n", pid_prev, vpage_prev);
                pstats[pid_prev].unmaps++;
                stats->cost += 400;

                if (pte_prev->modified) {
                    if (!pte_prev->file_mapped) {
                        cout << " OUT" << endl;
                        pstats[pid_prev].outs++;
                        stats->cost += 2700;
                    }
                    else {
                        cout << " FOUT" << endl;
                        pstats[pid_prev].fouts++;
                        stats->cost += 2400;
                    }
                }

                // update old PTE
                pte_prev->present = 0;
                if (pte_prev->modified) {
                    pte_prev->modified = 0;
                    if (!pte_prev->file_mapped) {
                        pte_prev->paged_out = 1;
                    }
                }
                pte_prev->referenced = 0;
                pte_prev->file_mapped = 0;
                pte_prev->write_protect = 0;
            }

            // update frame
            frame_new->allocated = 1;
            frame_new->pid = proc_curr->pid;
            frame_new->vpage = vpage;
            frame_new->time_last_use = i;
            // reset age for the Aging algo
            if (pager->algo == 'a') {
                frame_new->age = 0;
            }

            if (pte->paged_out && !pte->file_mapped) {
                cout << " IN" << endl;
                pstats[proc_curr->pid].ins++;
                stats->cost += 3100;
            }

            if (pte->file_mapped) {
                cout << " FIN" << endl;
                pstats[proc_curr->pid].fins++;
                stats->cost += 2800;
            }

            if (!pte->paged_out && !pte->file_mapped) {
                cout << " ZERO" << endl;
                pstats[proc_curr->pid].zeros++;
                stats->cost += 140;
            }

            cout << " MAP " << pte->frame_index << endl;
            pstats[proc_curr->pid].maps++;
            stats->cost += 300;
        }

        // now the page is definitely present
        // simulate instruction execution by hardware by updating the R/M PTE bits
        if (op == 'w') {
            pte->referenced = 1;
            if (pte->write_protect) {
                cout << " SEGPROT" << endl;
                pstats[proc_curr->pid].segprot++;
                stats->cost += 420;
            }
            else {
                pte->modified = 1;
            }
        }

        if (op == 'r') {
            pte->referenced = 1;
        }
    }
}


void print_stats() {
    // page tables
    for (int i = 0; i < process_list.size(); i++) {
        Process *p = process_list[i];
        pte_t *pt = p->pageTable;
        printf("PT[%d]:", i);
        for (int j = 0; j < MAX_VPAGES; j++) {
            if (!pt[j].present) {
                if (pt[j].paged_out) {
                    cout << " #";
                }
                else {
                    cout << " *";
                }
                continue;
            }
            printf(" %d:", j);
            if (pt[j].referenced) {
                cout << "R";
            } else {
                cout << "-";
            }
            if (pt[j].modified) {
                cout << "M";
            } else {
                cout << "-";
            }
            if (pt[j].paged_out) {
                cout << "S";
            } else {
                cout << "-";
            }
        }
        cout << endl;
    }
    // frame table
    cout << "FT:";
    for (int i = 0; i < stats->num_frames; i++) {
        if (frame_table[i].allocated == 0) {
            cout << " *";
        }
        else {
            printf(" %d:%d", frame_table[i].pid, frame_table[i].vpage);
        }
    }
    cout << endl;
    // processes
    for (int i = 0; i < process_list.size(); i++) {
        printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
                i,
                pstats[i].unmaps, pstats[i].maps, pstats[i].ins, pstats[i].outs,
                pstats[i].fins, pstats[i].fouts, pstats[i].zeros,
                pstats[i].segv, pstats[i].segprot);
    }
    // summary
    printf("TOTALCOST %lu %lu %lu %llu %lu\n", 
            stats->inst_count, stats->ctx_switches, stats->process_exits, stats->cost, sizeof(pte_t));
}


int main (int argc, char **argv) {
    // get arguments
    int c;
    int num_frames;
    string algo;
    string options;
    while ((c = getopt(argc, argv, "f:a:o:")) != -1) {
        switch (c)
        {
        case 'f':
            num_frames = atoi(optarg);
            break;
        case 'a':
            algo = optarg;
            break;
        case 'o':
            options = optarg;
        default:
            break;
        }
    }
    string path_input = argv[optind];
    string path_rand = argv[optind + 1];

    // create Pager
    Pager *pager;
    switch (algo[0])
    {
    case 'f':
        pager = new Pager_FIFO(num_frames);
        pager->algo = 'f';
        break;
    case 'c':
        pager = new Pager_Clock(num_frames);
        pager->algo = 'c';
        break;
    case 'r':
        pager = new Pager_Random(num_frames, path_rand);
        pager->algo = 'r';
        break;
    case 'e':
        pager = new Pager_ESC(num_frames);
        pager->algo = 'e';
        break;
    case 'a':
        pager = new Pager_Aging(num_frames);
        pager->algo = 'a';
        break;
    case 'w':
        pager = new Pager_WorkingSet(num_frames);
        pager->algo = 'w';
        break;
    default:
        break;
    }

    // creat Processes(VMA, page_table) and Instructions
    ifstream input;
    string line;
    int N;
    int index = 0;
    int pid = 0;
    vector<instruction> instructionList;

    input.open(path_input);
    while (!input.eof()) {
        getline(input, line);
        if (input.eof()) {
            break;
        }
        if (line[0] == '#' || line.length() == 0) {
            continue;
        }

        if (index == 0) {
            // # of processes
            N = stoi(line);
        }
        else if (line.length() == 1) {
            vector<VMA> VMAList;
            pte_t *page_table = new pte_t[MAX_VPAGES]();    // PTEs initialzed to 0 (default)
            int numVMA = stoi(line);
            
            int start_page;
            int end_page;
            int write_protected;
            int file_mapped;
            for (int i = 0; i < numVMA; i++) {
                input >> start_page >> end_page >> write_protected >> file_mapped;
                VMA vma(start_page, end_page, write_protected, file_mapped);
                VMAList.push_back(vma);
            }
            Process *proc = new Process(pid, VMAList, page_table);
            process_list.push_back(proc);
            pid++;
        }
        else {
            // read instructions
            char op = line[0];
            int val = stoi(line.substr(2));
            instruction ins(op, val);
            instructionList.push_back(ins);
        }
        index++;
    }
    input.close();

    stats->inst_count = instructionList.size();
    stats->num_frames = num_frames;
    // PStat pstats[process_list.size()];
    pstats = new PStat[process_list.size()]();

    // simulation
    simulation(instructionList, pager);

    // print info
    print_stats();

    return 0;
}

