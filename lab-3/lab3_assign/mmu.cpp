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
    Pager() {};
    ~Pager() {};
};

class Pager_FIFO: public Pager
{
public:
    deque<int> que;

    int select_victim_frame_index() {
        return -1;
    }

    Pager_FIFO(/* args */) {};

    ~Pager_FIFO() {};
};


void simulation (int num_frames, vector<Process *> process_list, vector<instruction> instr_list, Pager *pager) {
    frame_t frame_table[num_frames]; 
    deque<int> free_frame_list;
    int pid;
    int vpage;
    Process *proc_curr;
    vector<VMA> VMAList;

    for (int i = 0; i < num_frames; i++) {
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

        if (op == 'c') {
            pid = val;
            proc_curr = process_list[pid];
            continue;
        }
        if (op == 'e') {
            continue;
        }

        // read or write instruction
        vpage = val;
        pte_t *pte = &proc_curr->pageTable[vpage];

        // page fault exception
        if (!pte->present) {
            // verify this is actually a valid page in a vma, if not raise error and next inst
            bool isValid = false;
            VMAList = proc_curr->VMAList;
            for (int j = 0; j < VMAList.size(); j++) {
                VMA vma = VMAList[j];
                if (vpage >= vma.start_page && vpage <= vma.end_page) {
                    isValid = true;
                    break;
                }
            }
            if (!isValid) {
                cout << " SEGV" << endl;
                continue;
            }

            // vpage valid -> get new frame
            int new_frame_idx;
            if (!free_frame_list.empty()) {
                new_frame_idx = free_frame_list[0];
                free_frame_list.pop_front();
            }
            else {
                new_frame_idx = pager->select_victim_frame_index();
            }
            frame_t frame_new = frame_table[new_frame_idx];

            // PTE
            int pid_prev = frame_new.pid;
            int vpage_prev = frame_new.vpage;
            Process *proc_prev = process_list[pid_prev];
            pte_t *pte_prev = &proc_prev->pageTable[vpage_prev];

            if (frame_new.allocated) {
                // Unmap frame from user
                printf(" UNMAP %d:%d\n", pid_prev, vpage_prev);
                if (pte_prev->modified == 1) {
                    cout << " OUT" << endl;
                }
                if (pte_prev->file_mapped == 1) {
                    cout << " FOUT" << endl;
                    cout << " FIN" << endl;
                }
                if (pte_prev->paged_out == 1) {
                    cout << " IN" << endl;
                }
                if (pte_prev->paged_out == 0 && pte_prev->file_mapped == 0) {
                    cout << " ZERO" << endl;
                }
                pte_prev->paged_out = 1;
            }
            else {
                if (pte_prev->paged_out == 1) {
                    cout << " IN" << endl;
                }
                if (pte_prev->file_mapped == 1) {
                    cout << " FIN" << endl;
                }
            }
        }

        // now the page is definitely present
        // check write protection
        // simulate instruction execution by hardware by updating the R/M PTE bits
    }

}


int main (int argc, char **argv) {
    // get arguments
    int c;
    int num_frames = 128;
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

    // simulation
    simulation(num_frames, processList, instructionList, pager);

    return 0;
}

