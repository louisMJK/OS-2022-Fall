#include <iostream>
#include <fstream>
#include <string.h>

using namespace std;

ifstream input;
int line_inedx;
int line_offset;
int module_index;
string line_str;
char * line;
char * token;


void _parseError(int err_code, int line_num, int line_offset) {
    static const char* err_str [] = {
        "NUM_EXPECTED",             // Number expect, anything >= 2^30 is not a number either
        "SYM_EXPECTED",             // Symbol Expected
        "ADDR_EXPECTED",            // Addressing Expected which is A/E/I/R
        "SYM_TOO_LONG",             // Symbol Name is too long
        "TOO_MANY_DEF_IN_MODULE",   // >16
        "TOO_MANY_USE_IN_MODULE",   // >16
        "TOO_MANY_INSTR",           // Total num_instr exceeds memory size (512)
    };
    cout << "Parse Error line " << line_num << " offset " << line_offset << ": " << err_str[err_code] << endl;
}


char * getToken() {
    if (token == NULL) {
        // get new line
        if (input.eof()) {
            return NULL;
        }
        line_inedx ++;
        line_offset = 0;
        getline(input, line_str);
        delete[] line;
        line = new char[line_str.length() + 1];
        strcpy(line, line_str.c_str());
        cout << "line #" << line_inedx << ":" <<line << endl;
        token = strtok(line, " \t\n");

        // empty line
        if (token == NULL) {
            return getToken();
        }
    }
    else {
        token = strtok(NULL, " \t\n");

        // end of line
        if (token == NULL) {
            return getToken();
        }
        line_offset = token - line + 1;
    }
    return token;
}


int readInt() {

}

int pass_1(char* filename){
    // opens file
    input.clear();
    string path = "lab1_assign/";
    input.open(path + filename);
    if (input.is_open() == false){
        cout << "Unable to open file: " << filename << endl;
        return 1;
    }

    // get tokens
    line_inedx = 0;
    line_offset = 0;
    module_index = 0;
    line = NULL;
    token = NULL;

    while (!input.eof()){
        module_index ++;

        // Def list
        int def_count = readInt();

        token = getToken();
        
        cout << line_offset << ": " << token << endl;

    }

    input.close();
    
    return 0;
}



int main(int argc, char ** argv){

    int pass1 = pass_1(argv[1]);


    return 0;
}