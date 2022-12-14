#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <vector>
#include <unordered_map>
#include <set>

using namespace std;


ifstream input;
int line_index;
int line_offset;
int module_index;
int module_base;
int num_instruction;
string line_str;
char * line;
char * token;
unordered_map<string, int> symbolTable;


void _parseError(int err_code, int line_index, int line_offset) {
    static const char * err_str [] = {
        "NUM_EXPECTED",             // Number expect, anything >= 2^30 is not a number either
        "SYM_EXPECTED",             // Symbol Expected
        "ADDR_EXPECTED",            // Addressing Expected which is A/E/I/R
        "SYM_TOO_LONG",             // Symbol Name is too long
        "TOO_MANY_DEF_IN_MODULE",   // >16
        "TOO_MANY_USE_IN_MODULE",   // >16
        "TOO_MANY_INSTR",           // Total num_instr exceeds memory size (512)
    };
    cout << "Parse Error line " << line_index << " offset " << line_offset << ": " << err_str[err_code] << endl;
}


void _errorMessage(int err_code, string symbol) {
    if (err_code == 3) {
        cout << " Error: " << symbol << " is not defined; zero used";
    }
    else if (err_code < 7) {
        static const char * err_str [] = {
            "Error: Absolute address exceeds machine size; zero used", 
            "Error: Relative address exceeds module size; zero used", 
            "Error: External address exceeds length of uselist; treated as immediate", 
            "Error: Symbol is not defined; zero used",
            "Error: This variable is multiple times defined; first value used", 
            "Error: Illegal immediate value; treated as 9999", 
            "Error: Illegal opcode; treated as 9999"
        };
        cout << " " << err_str[err_code];
    }
    else {
        cout << "err_code not defined!";
    }
}


void _warningMessage(int err_code, int module_index, string symbol, int offset, int module_size) {
    if (err_code == 0) {
        printf("Warning: Module %d: %s too big %d (max=%d) assume zero relative\n", module_index, symbol.c_str(), offset, module_size);
    }
    else if (err_code == 1) {
        printf("Warning: Module %d: %s appeared in the uselist but was not actually used\n", module_index, symbol.c_str());
    }
    else if (err_code == 2) {
        printf("Warning: Module %d: %s was defined but never used\n", module_index, symbol.c_str());
    }
    else {
        cout << "Warning: err_code not defined!";
    }
}


char * getToken() {
    if (token == NULL) {
        // get new line
        if (input.eof()) {
            line_offset = line_str.length() + 1;
            return NULL;
        }
        string line_old = line_str;
        getline(input, line_str);
        // last line in file
        if (input.eof()) {
            line_offset = line_old.length() + 1;
            return NULL;
        }
        delete[] line;
        line = new char[line_str.length() + 1];
        line_index++;
        line_offset = 1;
        strcpy(line, line_str.c_str());
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
    char * token = getToken();
    bool isInt = true;
    if (token) {
        // check if token is int
        for (int i = 0; i < strlen(token); i ++) {
            if (!isdigit(token[i])) {
                isInt = false;
                break;
            }
        }
        if (isInt) {
            return atoi(token);
        }
        else {
            return -1;
        }
    }
    return -1;
}


string readSymbol() {
    char * token = getToken();
    if (token == NULL) {
        _parseError(1, line_index, line_offset);
        exit(0);
    }
    string symbol = token;
    if (!regex_match(symbol, regex("[a-zA-z][a-zA-Z0-9]*"))) {
        _parseError(1, line_index, line_offset);
        exit(0);
    }
    if (symbol.length() > 16) {
        _parseError(3, line_index, line_offset);
        exit(0);
    }
    return symbol;
}


string readIEAR() {
    char * token = getToken();
    if (token == NULL) {
        _parseError(2, line_index, line_offset);
        exit(0);
    }
    string instr = token;
    if (instr != "I" && instr != "E" && instr != "A" && instr != "R") {
        _parseError(2, line_index, line_offset);
        exit(0);
    }
    return instr;
}


void pass_1(char * path) {
    // opens file
    input.clear();
    string path_str = path;
    input.open(path_str);
    if (input.is_open() == false){
        cout << "Unable to open: " << path << endl;
        exit(0);
    }

    // parse tokens
    line_index = 0;
    line_offset = 0;
    module_index = 0;
    module_base = 0;
    num_instruction = 0;
    line = NULL;
    token = NULL;
    vector<string> symbolVec;
    set<string> duplicateSymbol;
    symbolTable.clear();

    while (!input.eof()){
        module_index++;

        // Def list
        vector<string> def_list;
        int def_count = readInt();
        if (def_count == -1) {
            if (input.eof()) {
                break;
            }
            _parseError(0, line_index, line_offset);
            exit(0);
        }
        if (def_count > 16) {
            _parseError(4, line_index, line_offset);
            exit(0);
        }
        for (int i = 0; i < def_count; i++) {
            string symbol = readSymbol();
            int symbol_offset = readInt();
            if (symbol_offset == -1) {
                _parseError(0, line_index, line_offset);
                exit(0);
            }
            // rule 2
            bool symbol_exist = (symbolTable.find(symbol) != symbolTable.end());
            if (!symbol_exist) {
                symbolVec.push_back(symbol);
                def_list.push_back(symbol);
                symbolTable[symbol] = symbol_offset + module_base;
            }
            else {
                duplicateSymbol.insert(symbol);
            }
        }

        // Use list
        int use_count = readInt();
        if (use_count == -1) {
            _parseError(0, line_index, line_offset);
            exit(0);
        }
        if (use_count > 16) {
            _parseError(5, line_index, line_offset);
            exit(0);
        }
        for (int i = 0; i < use_count; i++) {
            string symbol = readSymbol();
        }

        // Program text
        int code_count = readInt();
        if (code_count == -1) {
            _parseError(0, line_index, line_offset);
            exit(0);
        }
        num_instruction += code_count;
        if (num_instruction >= 512) {
            _parseError(6, line_index, line_offset);
            exit(0);
        }
        for (int i = 0; i < code_count; i++) {
            string instr = readIEAR();
            int op = readInt();
            if (op == -1) {
                _parseError(0, line_index, line_offset);
                exit(0);
            }
        }
        
        // rule 5
        for (int i = 0; i < def_list.size(); i++) {
            string symbol = def_list[i];
            int offset = symbolTable[symbol] - module_base;
            if (offset >= code_count) {
                _warningMessage(0, module_index, symbol, offset, code_count - 1);
                symbolTable[symbol] = module_base;
            }
        }

        // update module_base
        module_base += code_count;
    }

    // print symbolTable
    cout << "Symbol Table" << endl;
    for (int i = 0; i < symbolVec.size(); i++) {
        string symbol = symbolVec[i];
        cout << symbol << "=" << symbolTable[symbol];
        if (duplicateSymbol.find(symbol) != duplicateSymbol.end()) {
            _errorMessage(4, "");
        }
        cout << endl;
    }
    cout << endl;
    input.close();
}


void pass_2(char * path) {
    // opens file
    input.clear();
    string path_str = path;
    input.open(path_str);
    if (input.is_open() == false){
        cout << "Unable to open file: " << path << endl;
        exit(0);
    }

    cout << "Memory Map" << endl;

    // parse tokens
    line_index = 0;
    line_offset = 0;
    module_index = 0;
    module_base = 0;
    num_instruction = 0;
    line = NULL;
    token = NULL;
    vector<string> symbolVec;
    set<string> used;
    set<string> undefinedSymbol;
    unordered_map<string, int> symbolModule;

    while(!input.eof()) {
        module_index++;

        // Def list
        int def_count = readInt();
        if (def_count == -1 && input.eof()) {
            break;
        }
        for (int i = 0; i < def_count; i++) {
            string symbol = readSymbol();
            int symbol_offset = readInt();
            symbolVec.push_back(symbol);
            symbolModule[symbol] = module_index;
        }

        // Use list
        vector<string> use_list;
        int use_count = readInt();
        for (int i = 0; i < use_count; i++) {
            string symbol = readSymbol();
            bool defined = (symbolTable.find(symbol) != symbolTable.end());
            if (!defined) {
                // rule 3
                undefinedSymbol.insert(symbol);
            }
            use_list.push_back(symbol);
        }

        // Program text
        int address;
        set<string> usedSymbol;
        int code_count = readInt();
        for (int i = 0; i < code_count; i++) {
            string instr = readIEAR();
            int op = readInt();

            int err_code = -1;
            string err_str = "";
            address = module_base + i;

            // instructions
            if (instr == "I") {
                if (op >= 10000) {
                    // rule 10
                    err_code = 5;
                    op = 9999;
                }
            }
            else if (instr == "R") {
                if (op >= 10000) {
                    // rule 11
                    err_code = 6;
                    op = 9999;
                }
                else if (op % 1000 >= code_count) {
                    // rule 9
                    err_code = 1;
                    op = (op / 1000) * 1000 + module_base;
                }
                else {
                    op += module_base;
                }
            }
            else if (instr == "A") {
                if (op >= 10000) {
                    // rule 11
                    err_code = 6;
                    op = 9999;
                }
                else if (op % 1000 >= 512) {
                    // rule 8
                    err_code = 0;
                    op = (op / 1000) * 1000;
                }
            }
            else if (instr == "E") {
                if (op >= 10000) {
                    // rule 11
                    err_code = 6;
                    op = 9999;
                }
                else if (op % 1000 >= use_count) {
                    // rule 6
                    err_code = 2;
                }
                else {
                    int operand = op % 1000;
                    string symbol = use_list[operand];
                    op = (op / 1000) * 1000 + symbolTable[symbol];
                    usedSymbol.insert(symbol);
                    used.insert(symbol);
                    // rule 3
                    if (undefinedSymbol.find(symbol) != undefinedSymbol.end()) {
                        err_code = 3;
                        err_str = symbol;
                    }
                }

            }

            // print Memory Map
            printf("%03d: %04d", address, op);
            if (err_code != -1) {
                _errorMessage(err_code, err_str);
            }
            cout << endl;
        }

        // rule 7
        for (int i = 0; i < use_list.size(); i++) {
            string symbol = use_list[i];
            if (usedSymbol.find(symbol) == usedSymbol.end()) {
                _warningMessage(1, module_index, symbol, -1, -1);
            }
        }

        // update module_base
        module_base += code_count;
    }

    input.close();
    cout << endl;

    // rule 4
    for (int i = 0; i < symbolVec.size(); i++) {
        string symbol = symbolVec[i];
        if (used.find(symbol) == used.end()) {
            _warningMessage(2, symbolModule[symbol], symbol, 0, 0);
        }
    }

    cout << endl;
}


int main(int argc, char ** argv){
    pass_1(argv[1]);
    pass_2(argv[1]);
    return 0;
}

