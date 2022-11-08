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


struct pte_t {
    unsigned int valid:1;
    unsigned int referenced:1;
    unsigned int modified:1;
    unsigned int write_protect:1;
    unsigned int paged_out:1;
    unsigned int frameAddress:7;

    unsigned int file_mapped:1;
};


struct frame_t {

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

    Process(int id, vector<VMA *> vmaList) {
        pid = id;
        VMAList = vmaList;
        numVMA = VMAList.size();
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
            // read VMAs
            int numVMA = stoi(line);
            vector<VMA *> VMAList;
            int start_page;
            int end_page;
            int write_protected;
            int file_mapped;
            for (int i = 0; i < numVMA; i++) {
                input >> start_page >> end_page >> write_protected >> file_mapped;
                VMA *vma = new VMA(start_page, end_page, write_protected, file_mapped);
                VMAList.push_back(vma);
            }
            Process *proc = new Process(pid, VMAList);
            processList.push_back(proc);
            pid++;
        }
        else {
            // read instructions
            // cout << line << endl;
        }
        index++;
    }
    input.close();


    // parsed input
    for (int i = 0; i < N; i++) {
        Process *p = processList[i];
        cout << "Process: " << p->pid << endl;
        for (int j = 0; j < p->numVMA; j++) {
            VMA *vma = p->VMAList[j];
            printf("\t%d %d %d %d\n", vma->start_page, vma->end_page, vma->write_protected, vma->file_mapped);
        }
    }



    return 0;
}

