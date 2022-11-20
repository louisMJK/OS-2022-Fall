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
int MAX_FRAMES = 128;


struct pte_t 
{
    // initialized to 0
    unsigned int present:1;
    unsigned int referenced:1;
    unsigned int modified:1;
    unsigned int write_protect:1;
    unsigned int paged_out:1;
    unsigned int frame_index:7;

    unsigned int file_mapped:1;
    // valid addresses?
};


struct frame_t 
{
    // frame -> PTE
    int allocated;
    int pid;
    int vpage;
};


frame_t *frame_table;


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


// Each process has a page_table (64 PTEs)
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
        int frameIdx = q[idx];
        idx = (idx + 1) % MAX_FRAMES;
        return frameIdx;
    }

    void add_frame(int frame_idx) {
        if (q.size() < MAX_FRAMES) {
            q.push_back(frame_idx);
        }
    }

    Pager_FIFO() {idx = 0;};
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
        int frameIdx = q[idx];
        idx = (idx + 1) % q.size();
        return frameIdx;
    }

    void add_frame(int frame_idx) {
        if (q.size() < MAX_FRAMES) {
            q.push_back(frame_idx);
        }
    }

    Pager_Clock() {idx = 0;};
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
        int frameIdx = q[randIdx];
        idx++;
        return frameIdx;
    };

    void add_frame(int frame_idx) {
        if (q.size() < MAX_FRAMES) {
            q.push_back(frame_idx);
        }
    };

    Pager_Random(string path) {
        idx = 0;
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
                instr_prev = instr_curr;
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
        return victimIdx;
    };

    void add_frame(int frame_idx) {
        if (q.size() < MAX_FRAMES) {
            q.push_back(frame_idx);
        }
    };

    Pager_ESC() {
        idx = 0;
        instr_curr = 0;
        instr_prev = -1;
    };

    ~Pager_ESC() {};
};


class Stats
{
public:
    unsigned long inst_count;
    unsigned long ctx_switches;
    unsigned long process_exits;
    unsigned long long cost;

    Stats() {
        inst_count = 0;
        ctx_switches = 0;
        process_exits = 0;
        cost = 0;
    };

    ~Stats() {};
};


class PStat
{
public:
    unsigned long unmaps;
    unsigned long maps;
    unsigned long ins;
    unsigned long outs;
    unsigned long fins;
    unsigned long fouts;
    unsigned long zeros;
    unsigned long segv;
    unsigned long segprot;
    PStat() {
        unmaps = 0;
        maps = 0;
        ins = 0;
        outs = 0;
        fins = 0;
        fouts = 0;
        zeros = 0;
        segv = 0;
        segprot = 0;
    };
    ~PStat() {};
};


void simulation (vector<instruction> instr_list, Pager *pager, Stats *stats, PStat *pstats) {
    deque<int> free_frame_list;
    int pid;
    int vpage;
    Process *proc_curr;
    vector<VMA> VMAList;

    for (int i = 0; i < MAX_FRAMES; i++) {
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
            stats->process_exits++;
            stats->cost += 1250;
            continue;
        }
        stats->cost += 1;

        vpage = val;
        pte_t *pte = &proc_curr->pageTable[vpage];

        if (pager->algo == 'e') {
            ESC_reset = (i - pager->instr_prev >= 50) ? true : false;
            pager->instr_curr = i;
        }

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
            }
            else {
                new_frame_idx = pager->select_victim_frame_index();
            }
            frame_t *frame_new = &frame_table[new_frame_idx];

            // add new frame to pager
            pager->add_frame(new_frame_idx);

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
            if (pte->write_protect) {
                cout << " SEGPROT" << endl;
                pte->referenced = 1;
                pstats[proc_curr->pid].segprot++;
                stats->cost += 420;
            }
            else {
                pte->referenced = 1;
                pte->modified = 1;
            }
        }

        if (op == 'r') {
            pte->referenced = 1;
        }
    }
}


void print_stats(Stats *stats, PStat *pstats) {
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
    for (int i = 0; i < MAX_FRAMES; i++) {
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
    string algo;
    string options;
    while ((c = getopt(argc, argv, "f:a:o:")) != -1) {
        switch (c)
        {
        case 'f':
            MAX_FRAMES = atoi(optarg);
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
        pager = new Pager_FIFO();
        pager->algo = 'f';
        break;
    case 'c':
        pager = new Pager_Clock();
        pager->algo = 'c';
        break;
    case 'r':
        pager = new Pager_Random(path_rand);
        pager->algo = 'r';
        break;
    case 'e':
        pager = new Pager_ESC();
        pager->algo = 'e';
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

    frame_table = new frame_t[MAX_FRAMES]();
    Stats *stats = new Stats();
    stats->inst_count = instructionList.size();
    PStat pstats[process_list.size()];

    // simulation
    simulation(instructionList, pager, stats, pstats);

    // print info
    print_stats(stats, pstats);

    return 0;
}

