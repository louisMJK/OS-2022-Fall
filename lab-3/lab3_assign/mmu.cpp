#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <vector>


using namespace std;


const int MAX_VPAGES = 64;
const int MAX_FRAMES = 128;


struct pte_t 
{
    unsigned int valid:1;
    unsigned int referenced:1;
    unsigned int modified:1;
    unsigned int write_protect:1;
    unsigned int paged_out:1;
    unsigned int frame_address:7;

    unsigned int file_mapped:1;
};


struct frame_t 
{
    bool allocated;
    int pid;
    int vpage;
};


// maintain reverse mappings to the process and the vpage that maps a particular frame
frame_t frame_table[MAX_FRAMES];    // physical frames
pte_t page_table[MAX_VPAGES];       // virtual pages -> physical frames


struct instruction
{
    char op;
    int val;
    instruction(char instruction, int value) {
        op = instruction;
        val = value;
    }
};


class Pager
{
public:
    virtual frame_t *select_victim_frame() = 0;
    Pager(/* args */);
    ~Pager() {};
};



class VMA
{
public:
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

    VMA() {};

    ~VMA() {};
};


// Each process has a page_table (64 PTEs)
class Process
{
public:
    int pid;
    int numVMA;
    vector<VMA *> VMAList;
    pte_t *pageTable;

    Process(int id, vector<VMA *> vmaList, pte_t *page_table) {
        pid = id;
        VMAList = vmaList;
        numVMA = VMAList.size();
        pageTable = page_table;
    };

    Process() {};

    ~Process() {};
};



void simulation () {
    frame_t frame_table[MAX_FRAMES];
    pte_t page_table[MAX_VPAGES];

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


    // read input
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
            vector<VMA *> VMAList;
            pte_t page_table[MAX_VPAGES];
            int numVMA = stoi(line);
            
            int start_page;
            int end_page;
            int write_protected;
            int file_mapped;
            for (int i = 0; i < numVMA; i++) {
                input >> start_page >> end_page >> write_protected >> file_mapped;
                VMA *vma = new VMA(start_page, end_page, write_protected, file_mapped);
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


    // parsed input: processList -> VMAList
    for (int i = 0; i < N; i++) {
        Process *p = processList[i];
        cout << "Process: " << p->pid << endl;
        for (int j = 0; j < p->numVMA; j++) {
            VMA *vma = p->VMAList[j];
            printf("\t%d %d %d %d\n", vma->start_page, vma->end_page, vma->write_protected, vma->file_mapped);
        }
    }
    cout << "Instructions:" << endl;
    for (int i = 0; i < instructionList.size(); i++) {
        cout << instructionList[i].op << " " << instructionList[i].val << endl;
    }

    // simulation
    simulation();


    return 0;
}

