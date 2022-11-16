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
    bool allocated;
    int pid;
    int vpage;
};


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


// deque, frames
class Pager
{
public:
    virtual int select_victim_frame_index() {return -1;};
    virtual void add_frame(int idx) {};
    Pager() {};
    ~Pager() {};
};

class Pager_FIFO: public Pager
{
public:
    deque<int> q;

    int select_victim_frame_index() {
        if (q.empty()) {
            return -1;
        }
        int idx = q.front();
        q.pop_front();
        return idx;
    }

    void add_frame(int idx) {
        q.push_back(idx);
    }

    Pager_FIFO() {};

    ~Pager_FIFO() {};
};


class Stats
{
public:
    int proc_count;
    unsigned long inst_count;
    unsigned long ctx_switches;
    unsigned long process_exits;
    unsigned long long cost;
    Stats() {
        proc_count = 0;
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


void simulation (frame_t *frame_table, vector<Process *> process_list, vector<instruction> instr_list, Pager *pager, Stats *stats, PStat *pstats) {
    deque<int> free_frame_list;
    int pid;
    int vpage;
    Process *proc_curr;
    vector<VMA> VMAList;

    for (int i = 0; i < MAX_FRAMES; i++) {
        free_frame_list.push_back(i);
        frame_table[i].allocated = false;
        frame_table[i].pid = -1;
        frame_table[i].vpage = -1;
    }

    // instructions
    for (int i = 0; i < instr_list.size(); i++) {
        char op = instr_list[i].op;
        int val = instr_list[i].val;

        cout << i << ": ==> " << op << " " << val << endl;

        // read or write instruction
        if (op == 'c') {
            pid = val;
            proc_curr = process_list[pid];
            stats->ctx_switches++;
            stats->cost += 130;
            continue;
        }
        if (op == 'e') {
            stats->process_exits++;
            stats->cost += 1250;
            continue;
        }
        stats->cost += 1;

        vpage = val;
        pte_t *pte = &proc_curr->pageTable[vpage];

        // page fault exception
        if (!pte->present) {
            // verify this is actually a valid page in a vma, if not raise error and next inst
            bool isValid = false;
            int write_protect = 0;
            VMAList = proc_curr->VMAList;
            for (int j = 0; j < VMAList.size(); j++) {
                VMA vma = VMAList[j];
                if (vpage >= vma.start_page && vpage <= vma.end_page) {
                    isValid = true;
                    write_protect = vma.write_protected;
                    break;
                }
            }
            if (!isValid) {
                cout << " SEGV" << endl;
                pstats[proc_curr->pid].segv++;
                stats->cost += 340;
                continue;
            }
            pte->write_protect = write_protect;

            // vpage valid -> get new frame
            int new_frame_idx;
            if (!free_frame_list.empty()) {
                new_frame_idx = free_frame_list[0];
                free_frame_list.pop_front();
            }
            else {
                new_frame_idx = pager->select_victim_frame_index();
                // cout << " victim frame: " << new_frame_idx << endl;
            }
            frame_t *frame_new = &frame_table[new_frame_idx];

            // update PTE -> new frame
            pte->present = 1;
            pte->frame_index = new_frame_idx;

            // Pager, FIFO???
            pager->add_frame(new_frame_idx);

            // frame is mapped -> Unmap
            if (frame_new->allocated) {
                int pid_prev = frame_new->pid;
                int vpage_prev = frame_new->vpage;
                Process *proc_prev = process_list[pid_prev];
                pte_t *pte_prev = &proc_prev->pageTable[vpage_prev];

                printf(" UNMAP %d:%d\n", pid_prev, vpage_prev);
                pstats[proc_curr->pid].unmaps++;
                stats->cost += 400;

                if (pte_prev->modified) {
                    cout << " OUT" << endl;
                    pstats[proc_curr->pid].outs++;
                    stats->cost += 2700;
                }

                // if (pte_prev->paged_out) {
                //     cout << " IN" << endl;
                //     pstats[proc_curr->pid].ins++;
                //     stats->cost += 3100;
                // }

                // update old PTE
                pte_prev->present = 0;
                if (pte_prev->modified) {
                    pte_prev->paged_out = 1;
                    pte_prev->modified = 0;
                }
            }

            // update frame
            frame_new->allocated = true;
            frame_new->pid = proc_curr->pid;
            frame_new->vpage = vpage;

            if (pte->paged_out) {
                cout << " IN" << endl;
                pstats[proc_curr->pid].ins++;
                stats->cost += 3100;
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
        // check write protection
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


void print_stats(Stats *stats, PStat *pstats, frame_t *frame_table, vector<Process *> processList) {
    // page tables
    for (int i = 0; i < stats->proc_count; i++) {
        Process *p = processList[i];
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
    }
    cout << endl;
    // frame table
    cout << "FT:";
    for (int i = 0; i < MAX_FRAMES; i++) {
        if (frame_table[i].pid == -1) {
            cout << " *";
        }
        else {
            printf(" %d:%d", frame_table[i].pid, frame_table[i].vpage);
        }
    }
    cout << endl;
    // processes
    for (int i = 0; i < stats->proc_count; i++) {
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

    // create Pager
    Pager *pager;
    switch (algo[0])
    {
    case 'f':
        pager = new Pager_FIFO();
        break;
    default:
        break;
    }


    // creat Processes(VMA, page_table) and Instructions
    string path_input = argv[optind];
    string path_rand = argv[optind + 1];
    ifstream input;
    string line;
    int N;
    int index = 0;
    int pid = 0;
    vector<Process *> processList;
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
            pte_t page_table[MAX_VPAGES] = {0};     // PTEs initialzed to 0
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
            processList.push_back(proc);
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


    frame_t frame_table[MAX_FRAMES]; 
    Stats *stats = new Stats();
    stats->proc_count = processList.size();
    stats->inst_count = instructionList.size();
    PStat pstats[processList.size()];

    // simulation
    simulation(frame_table, processList, instructionList, pager, stats, pstats);

    // print info
    print_stats(stats, pstats, frame_table, processList);

    return 0;
}

