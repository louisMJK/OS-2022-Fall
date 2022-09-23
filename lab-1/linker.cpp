#include<iostream>
#include<string.h>

using namespace std;


void _parseError(int err_code, int line_num, int line_offset){
    static char * err_str [] = {
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


char * getToken(){

}

void pass_1(FILE *file){

}


int main(int argc, char *argv []){
    int def_count, use_count, code_count;


    cout << "You entered: " << 12345 << endl;
    // cin.get();

    return 0;
}