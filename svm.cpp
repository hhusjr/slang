/*
 * SVM, SLang Stack-based Virtual Machine
 * Write once, run anywhere :
 * Quite appreciate "CPython Virtual Machine", from which I learnt a lot.
 *
 * Usage:
 * $ g++ svm.cpp -o svm
 * $ svm -r (-e) ./helloworld.slb (-v) (-p password) -- Run program (-v: in verbose mode, -e: performance evaluator)
 * $ svm -d ./helloworld.slb (-p password) -- Disassembly
 * $ svm -i (-v) (-e) -- Interact Mode (-v: in verbose mode, -e: performance evaluator)
 * $ svm -a ./helloworld.txt -o ./helloworld.slb (-p password) -- Assembly input file
 *
 * @author Junru Shen
 */
#define MAGIC "80JF34R9S "
#define MAX_INSTRUCTION_NUM 1000000
#define MAX_INSTRUCTION_ADDR 2000000
#define MEM_DBG
#undef MEM_DBG
#define OP_POP() (*operands)[(*op_top_ptr)--]
#define OP_TOP() (*operands)[*op_top_ptr]
#define OP_PUSH(slot) (*operands)[++(*op_top_ptr)] = slot
#ifdef MEM_DBG
#define SLOT_INCREF(slot, reason)                                       \
    do {                                                                \
        slot->ref_cnt++;                                                \
        std::cout << "+MD* inc slot ref count because " << reason;      \
        std::cout << " [" << slot->ref_cnt << "]" << std::endl;         \
    } while (0)
#else
#define SLOT_INCREF(slot, reason)                                       \
    do {                                                                \
        slot->ref_cnt++;                                                \
    } while (0)
#endif
#ifdef MEM_DBG
#define SLOT_DECREF(slot, reason)                                       \
  do {                                                                  \
    slot->ref_cnt--;                                                    \
    std::cout << "-MD* Decreased slot ref count because " << reason;    \
    std::cout << " [" << slot->ref_cnt << "]" << std::endl;             \
    if (!slot->ref_cnt) {                                               \
        std::cout << "xMD* Freed slot because " << reason << " v=";     \
        std::cout << slot->as_string() << std::endl;                    \
        RELEASE(slot);                                                  \
        slot = nullptr;                                                 \
    }                                                                   \
  } while (0)
#else
#define SLOT_DECREF(slot, reason)                                       \
  do {                                                                  \
    if (slot == nullptr) break;                                         \
    slot->ref_cnt--;                                                    \
    if (!slot->ref_cnt) {                                               \
        RELEASE(slot);                                                  \
    }                                                                   \
  } while (0)
#endif

#define RELEASE(slot) \
do { \
    if (slot == nullptr) break; \
    if (slot->type == ARRAY) { \
        for (int i = 0; i < slot->array_size; i++) { \
            slot->array_val[i]->ref_cnt--; \
            if (!slot->array_val[i]->ref_cnt) { \
                if (slot->array_val[i] == nullptr) continue; \
                delete slot->array_val[i]; \
                slot->array_val[i] = nullptr; \
            } \
        } \
    } \
    delete slot; \
    slot = nullptr; \
} while (0)

#define DISPATCH goto dispatch
#define FULL_DISPATCH goto full_dispatch
#if __WORDSIZE == 64
typedef long int      int_tp;
#else
__extension__
typedef long long int int_tp;
#endif
typedef double float_tp;
typedef char char_tp;

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <getopt.h>
#include <ctime>
#include <iomanip>

void panic(const std::string& msg) {
    std::cout << "Runtime error: " << msg << std::endl;
    std::cout << "Enter verbose mode to see details." << std::endl;
    std::cout << "ABORTING..." << std::endl;
    abort();
}

// Instruction codes
enum instruct_code {
    CMALLOC,
    VMALLOC,
    CONSTANT,
    NOOP,
    POP_OP,
    TYPE_CVT,
    // Load const and name
    LOAD_NULL,
    LOAD_CONSTANT,
    LOAD_NAME,
    LOAD_NAME_GLOBAL,
    LOAD_INT,
    LOAD_FLOAT,
    LOAD_CHAR,
    BINARY_SUBSCR,
    STORE_SUBSCR,
    STORE_SUBSCR_INPLACE,
    STORE_SUBSCR_NOPOP,
    STORE_NAME,
    STORE_NAME_GLOBAL,
    STORE_NAME_NOPOP,
    STORE_NAME_GLOBAL_NOPOP,
    // Build array
    BUILD_ARR,
    SIZE_OF,
    // Operators
    BINARY_OP,
    UNARY_OP,
    // Jump
    JMP,
    JMP_TRUE,
    JMP_FALSE,
    // Push and pop stack frame to the control stack (for function call)
    PUSH,
    RET,
    CALL,
    LOAD_GLOBAL,
    STORE_GLOBAL,
    // Halt
    HALT,
    // Debugging
    PRINTK,
    // Basic I/O
    PUTCH,
    GETCH
};

// Basic data types
enum basic_data_types {
    INT = 0,
    FLOAT,
    CHAR,
    VOID,
    ARRAY
};

// Slot
struct slot {
    basic_data_types type = VOID;
    int_tp int_val{};
    float_tp float_val{};
    char_tp char_val{};
    slot **array_val{};
    int array_size{};
    basic_data_types arr_element_type = VOID;
    int ref_cnt = 1;

    explicit slot(int_tp _int_val) : int_val(_int_val), type(INT) {}

    explicit slot(bool bool_val) : int_val(bool_val ? 1 : 0), type(INT) {}

    explicit slot(float_tp _float_val) : float_val(_float_val), type(FLOAT) {}

    explicit slot(char_tp _char_val) : char_val(_char_val), type(CHAR) {}

    slot(int _array_size, basic_data_types _type) {
        if (_type == ARRAY || _type == VOID) {
            // do not support nested array
            return;
        }
        type = ARRAY;
        array_size = _array_size;
        array_val = new slot *[array_size];
        for (int i = 0; i < array_size; i++) {
            slot *fill_slot = nullptr;
            switch (_type) {
                case INT:
                    fill_slot = new slot((int_tp) 0);
                    arr_element_type = INT;
                    break;
                case FLOAT:
                    fill_slot = new slot((float_tp) 0.0);
                    arr_element_type = FLOAT;
                    break;
                case CHAR:
                    fill_slot = new slot((char) '\0');
                    arr_element_type = CHAR;
                    break;
                default:
                    panic("Unsupported type here");
                    break;
            }
            *(array_val + i) = fill_slot;
            SLOT_INCREF(fill_slot, "array initialize");
        }
    }

    slot() = default;

    std::string as_string() {
        std::stringstream res;
        std::string ret;
        switch (type) {
            case INT:
                res << int_val << "(int)";
                break;
            case FLOAT:
                res << float_val << "(float)";
                break;
            case CHAR:
                res << char_val << "(char)";
                break;
            case ARRAY:
                res << "array[" << array_size << "]";
                break;
            case VOID:
                res << "(null)";
                break;
        }
        res >> ret;
        return ret;
    }
};

typedef slot *T_OPSTACK[2000];
typedef slot **T_VARIABLES;

// Instruction = Code + no more than 1 operand
struct instruct {
    instruct_code code = NOOP;
    int operand{};
    int address = -1;

    instruct(int _addr, instruct_code _code, int _operand) : address(_addr), code(_code), operand(_operand) {}

    instruct(int _addr, instruct_code _code) : address(_addr), code(_code) {}

    instruct() = default;
};

// Stack frame
struct frame {
    T_VARIABLES locals{};
    int var_cnt = 0;
    int return_ip{};
    T_OPSTACK local_operands{};
    int op_top = -1;
    frame *caller;

    explicit frame(frame *_caller) : caller(_caller) {}
};

instruct instructs[MAX_INSTRUCTION_NUM]; // Instructions
int addrs[MAX_INSTRUCTION_ADDR + 1];
slot **constants;
T_VARIABLES globals;
int var_cnt = 0;
int constant_cnt = 0;
T_OPSTACK global_operands;

// Virtual Machine
class Machine {
private:
    int ins_cnt{};
    frame *esp{};
    int op_top;
    int ip{};
    bool verbose = false;
    bool evaluator = false;
    long long int n_ins = 0;
    T_OPSTACK *operands{};
    int *op_top_ptr{};

public:
    static std::unordered_map<std::string, instruct_code> string_inscode_mapping;
    static int inscode_param_cnt_mapping[200];

    static void load_name_code_mapping() {
        string_inscode_mapping["CMALLOC"] = CMALLOC;
        string_inscode_mapping["VMALLOC"] = VMALLOC;
        string_inscode_mapping["CONSTANT"] = CONSTANT;
        string_inscode_mapping["LOAD_CONSTANT"] = LOAD_CONSTANT;
        string_inscode_mapping["POP_OP"] = POP_OP;
        string_inscode_mapping["NOOP"] = NOOP;
        string_inscode_mapping["LOAD_NULL"] = LOAD_NULL;
        string_inscode_mapping["LOAD_NAME"] = LOAD_NAME;
        string_inscode_mapping["LOAD_NAME_GLOBAL"] = LOAD_NAME_GLOBAL;
        string_inscode_mapping["LOAD_INT"] = LOAD_INT;
        string_inscode_mapping["LOAD_FLOAT"] = LOAD_FLOAT;
        string_inscode_mapping["LOAD_CHAR"] = LOAD_CHAR;
        string_inscode_mapping["STORE_NAME"] = STORE_NAME;
        string_inscode_mapping["STORE_NAME_GLOBAL"] = STORE_NAME_GLOBAL;
        string_inscode_mapping["STORE_NAME_NOPOP"] = STORE_NAME_NOPOP;
        string_inscode_mapping["STORE_NAME_GLOBAL_NOPOP"] = STORE_NAME_GLOBAL_NOPOP;
        string_inscode_mapping["JMP"] = JMP;
        string_inscode_mapping["JMP_TRUE"] = JMP_TRUE;
        string_inscode_mapping["JMP_FALSE"] = JMP_FALSE;
        string_inscode_mapping["BINARY_OP"] = BINARY_OP;
        string_inscode_mapping["UNARY_OP"] = UNARY_OP;
        string_inscode_mapping["HALT"] = HALT;
        string_inscode_mapping["RET"] = RET;
        string_inscode_mapping["PUSH"] = PUSH;
        string_inscode_mapping["CALL"] = CALL;
        string_inscode_mapping["LOAD_GLOBAL"] = LOAD_GLOBAL;
        string_inscode_mapping["STORE_GLOBAL"] = STORE_GLOBAL;
        string_inscode_mapping["BUILD_ARR"] = BUILD_ARR;
        string_inscode_mapping["TYPE_CVT"] = TYPE_CVT;
        string_inscode_mapping["BINARY_SUBSCR"] = BINARY_SUBSCR;
        string_inscode_mapping["STORE_SUBSCR"] = STORE_SUBSCR;
        string_inscode_mapping["STORE_SUBSCR_INPLACE"] = STORE_SUBSCR_INPLACE;
        string_inscode_mapping["STORE_SUBSCR_NOPOP"] = STORE_SUBSCR_NOPOP;
        string_inscode_mapping["PRINTK"] = PRINTK;
        string_inscode_mapping["PUTCH"] = PUTCH;
        string_inscode_mapping["GETCH"] = GETCH;
        string_inscode_mapping["SIZE_OF"] = SIZE_OF;
    }

    static void load_param_mapping() {
        inscode_param_cnt_mapping[VMALLOC] = 1;
        inscode_param_cnt_mapping[CMALLOC] = 1;
        inscode_param_cnt_mapping[POP_OP] = 0;
        inscode_param_cnt_mapping[NOOP] = 0;
        inscode_param_cnt_mapping[LOAD_NULL] = 0;
        inscode_param_cnt_mapping[LOAD_CONSTANT] = 1;
        inscode_param_cnt_mapping[LOAD_NAME] = 1;
        inscode_param_cnt_mapping[LOAD_NAME_GLOBAL] = 1;
        inscode_param_cnt_mapping[LOAD_INT] = 1;
        inscode_param_cnt_mapping[LOAD_FLOAT] = 1;
        inscode_param_cnt_mapping[LOAD_CHAR] = 1;
        inscode_param_cnt_mapping[STORE_NAME] = 1;
        inscode_param_cnt_mapping[STORE_NAME_NOPOP] = 1;
        inscode_param_cnt_mapping[STORE_NAME_GLOBAL] = 1;
        inscode_param_cnt_mapping[STORE_NAME_GLOBAL_NOPOP] = 1;
        inscode_param_cnt_mapping[JMP] = 1;
        inscode_param_cnt_mapping[JMP_TRUE] = 1;
        inscode_param_cnt_mapping[JMP_FALSE] = 1;
        inscode_param_cnt_mapping[BINARY_OP] = 1;
        inscode_param_cnt_mapping[UNARY_OP] = 1;
        inscode_param_cnt_mapping[HALT] = 0;
        inscode_param_cnt_mapping[RET] = 0;
        inscode_param_cnt_mapping[PUSH] = 0;
        inscode_param_cnt_mapping[CALL] = 1;
        inscode_param_cnt_mapping[LOAD_GLOBAL] = 0;
        inscode_param_cnt_mapping[STORE_GLOBAL] = 0;
        inscode_param_cnt_mapping[BUILD_ARR] = 1;
        inscode_param_cnt_mapping[TYPE_CVT] = 1;
        inscode_param_cnt_mapping[BINARY_SUBSCR] = 0;
        inscode_param_cnt_mapping[STORE_SUBSCR] = 0;
        inscode_param_cnt_mapping[STORE_SUBSCR_INPLACE] = 0;
        inscode_param_cnt_mapping[STORE_SUBSCR_NOPOP] = 0;
        inscode_param_cnt_mapping[PRINTK] = 0;
        inscode_param_cnt_mapping[PUTCH] = 0;
        inscode_param_cnt_mapping[GETCH] = 0;
        inscode_param_cnt_mapping[SIZE_OF] = 0;
        // only used for assemble/disassemble
        inscode_param_cnt_mapping[CONSTANT] = 3;
    }

    Machine() {
        op_top = -1;
        reset();
    }

    ~Machine() {
        reset();
    }

    void enable_verbose() {
        verbose = true;
    }

    void enable_evaluator() {
        evaluator = true;
    }

    void reset() {
        ip = -1;
        ins_cnt = 0;
        while (op_top > -1) {
            SLOT_DECREF(global_operands[op_top--], "Reset");
        }
        esp = nullptr;
        while (var_cnt--) {
            SLOT_DECREF(globals[var_cnt], "Reset");
        }
        while (constant_cnt--) {
            SLOT_DECREF(constants[constant_cnt], "Reset");
        }
        var_cnt = 0;
        constant_cnt = 0;
    }

    void add_instruct(instruct ins) {
        instructs[ins_cnt++] = ins;
        addrs[ins.address] = ins_cnt - 1;
    }

    void dispatch() {
        if (verbose) {
            std::cout << "SLang Virtual Machine Debugger (SVMDB)" << std::endl;
            std::cout << "I am an opcode-level debugging assistant." << std::endl;
            std::cout << "======================================" << std::endl;
            std::cin.get();
            Machine::load_name_code_mapping();
            Machine::load_param_mapping();
        }
        clock_t start = 0, finish;
        if (evaluator) {
            start = clock();
        }
        full_dispatch:
        {
            operands = (esp == nullptr) ? &global_operands : &(esp->local_operands);
            op_top_ptr = (esp == nullptr) ? &op_top : &(esp->op_top);

            dispatch:
            {
                n_ins++;
                instruct ins = instructs[++ip];
                if (verbose) {
                    std::cout << "======================================" << std::endl;
                    std::string code_name_mapping[200];
                    for (const auto& x : Machine::string_inscode_mapping) {
                        code_name_mapping[x.second] = x.first;
                    }
                    std::cout << "#" << ins.address << " $ " << code_name_mapping[ins.code];
                    if (Machine::inscode_param_cnt_mapping[ins.code]) {
                        std::cout << " " << ins.operand;
                    }
                    std::cout << " > ";
                    std::cin.get();
                }
                switch (ins.code) {
                    case VMALLOC: {
                        if (ins.operand) {
                            if (esp == nullptr) {
                                globals = new slot *[ins.operand];
                                for (int i = 0; i < ins.operand; i++) globals[i] = nullptr;
                                var_cnt = ins.operand;
                            } else {
                                esp->locals = new slot *[ins.operand];
                                for (int i = 0; i < ins.operand; i++) esp->locals[i] = nullptr;
                                esp->var_cnt = ins.operand;
                            }
                        }
                        DISPATCH;
                    }

                    case NOOP:
                        DISPATCH;

                    case POP_OP: {
                        slot *op = OP_POP();
                        SLOT_DECREF(op, "Operand is poped from the stack");
                        DISPATCH;
                    }

                    case TYPE_CVT: {
                        slot *op = OP_POP();
                        slot *res = nullptr;
                        switch (ins.operand) {
                            // INT
                            case 0:
                                if (op->type == INT) {
                                    res = new slot((int_tp) op->int_val);
                                } else if (op->type == FLOAT) {
                                    res = new slot((int_tp) op->float_val);
                                }
                                break;
                            // FLOAT
                            case 1:
                                if (op->type == INT) {
                                    res = new slot((float_tp) op->int_val);
                                } else if (op->type == FLOAT) {
                                    res = new slot((float_tp) op->float_val);
                                }
                                break;
                            // CHAR
                            case 2:
                                res = new slot((char_tp) op->char_val);
                                break;
                        }
                        OP_PUSH(res);
                        SLOT_DECREF(op, "Convert type");
                        DISPATCH;
                    }

                    case PUSH: {
                        auto *tmp = new frame(esp != nullptr ? esp : nullptr);
                        esp = tmp;
                        if (verbose) {
                            std::cout << "Frame is pushed into the control stack." << std::endl;
                        }
                        FULL_DISPATCH;
                    }

                    case CALL: {
                        esp->return_ip = ip + 1;
                        if (verbose) {
                            std::cout << "Call subroutine defined at address " << ins.operand
                                      << ", with return address "
                                      << (ip < ins_cnt - 1 ? instructs[ip + 1].address : -1) << "." << std::endl;
                        }
                        ip = addrs[ins.operand] - 1;
                        DISPATCH;
                    }

                    case RET: {
                        int to_ip = esp->return_ip - 1;
                        ip = to_ip;
                        slot *ret;
                        if (esp->caller == nullptr) {
                            ret = global_operands[++op_top] = OP_POP();
                        } else {
                            ret = esp->caller->local_operands[++esp->caller->op_top] = OP_POP();
                        }
                        // 此处不需要对ret进行减引用，因为ret此会在进入了函数之后被减一次
                        if (verbose) {
                            std::cout << "Frame is poped from the control stack. Return to instruct address "
                                      << (to_ip < ins_cnt - 1 ? instructs[to_ip + 1].address : -1)
                                      << " with return value " << ret->as_string() << "." << std::endl;
                        }
                        while (esp->op_top > -1) {
                            SLOT_DECREF(esp->local_operands[esp->op_top], "Return statement op decref");
                        }
                        while (esp->var_cnt--) {
                            SLOT_DECREF(esp->locals[esp->var_cnt], "Return statement var decref");
                        }
                        delete esp->locals;
                        frame *tmp = esp->caller;
                        delete esp;
                        esp = tmp;
                        FULL_DISPATCH;
                    }

                    case LOAD_NULL: {
                        slot* slt = new slot();
                        OP_PUSH(slt);
                        if (verbose) {
                            std::cout << "NULL value (type: void) was loaded to operand stack." << std::endl;
                        }
                        DISPATCH;
                    }

                    case LOAD_INT: {
                        slot *created = new slot((int_tp) ins.operand);
                        OP_PUSH(created);
                        if (verbose) {
                            std::cout << "Int value " << ins.operand << " was loaded to operand stack." << std::endl;
                        }
                        DISPATCH;
                    }

                    case LOAD_FLOAT: {
                        slot *created = new slot((float_tp) ins.operand);
                        OP_PUSH(created);
                        if (verbose) {
                            std::cout << "Float value " << ins.operand << " was loaded to operand stack." << std::endl;
                        }
                        DISPATCH;
                    }

                    case SIZE_OF: {
                        slot *element = OP_POP();
                        SLOT_DECREF(element, "Size of calculate");
                        int size;
                        if (element->type != ARRAY) size = 1;
                        else size = element->array_size;
                        OP_PUSH(new slot((int_tp) size));
                        DISPATCH;
                    }

                    case LOAD_CHAR: {
                        slot *created = new slot((char_tp) ins.operand);
                        OP_PUSH(created);
                        if (verbose) {
                            std::cout << "Char value " << ins.operand << " was loaded to operand stack." << std::endl;
                        }
                        DISPATCH;
                    }

                    case LOAD_CONSTANT: {
                        slot *constant = constants[ins.operand];
                        OP_PUSH(constant);
                        SLOT_INCREF(constant, "LOAD_CONSTANT");
                        if (verbose) {
                            std::cout << "Constant value " << constants[ins.operand]->as_string()
                                      << " was loaded to operand stack." << std::endl;
                        }
                        DISPATCH;
                    }

                    case LOAD_NAME: {
                        slot *var = esp->locals[ins.operand];
                        OP_PUSH(var);
                        SLOT_INCREF(var, "LOAD_NAME");
                        if (verbose) {
                            std::cout << "Loaded name " << ins.operand << "." << std::endl;
                        }
                        DISPATCH;
                    }
                    case LOAD_NAME_GLOBAL: {
                        slot *var = globals[ins.operand];
                        OP_PUSH(var);
                        SLOT_INCREF(var, "LOAD_NAME_GLOBAL");
                        if (verbose) {
                            std::cout << "Loaded global name " << ins.operand << "." << std::endl;
                        }
                        DISPATCH;
                    }
                    case STORE_NAME:
                    case STORE_NAME_NOPOP: {
                        // 千万注意！原来的需要DECREF
                        if (esp->locals[ins.operand] != nullptr) {
                            SLOT_DECREF(esp->locals[ins.operand], "Store override");
                        }
                        if (ins.code == STORE_NAME) {
                            esp->locals[ins.operand] = OP_POP();
                        } else {
                            esp->locals[ins.operand] = OP_TOP();
                        }
                        SLOT_INCREF(esp->locals[ins.operand], "STORE_NAME[_NOPOP]");
                        if (verbose) {
                            std::cout << "Stored " << esp->locals[ins.operand]->as_string() << " to name " << ins.operand << " in locals."
                                      << std::endl;
                        }
                        DISPATCH;
                    }
                    case STORE_NAME_GLOBAL:
                    case STORE_NAME_GLOBAL_NOPOP: {
                        // 千万注意！原来的需要DECREF
                        if (globals[ins.operand] != nullptr) {
                            SLOT_DECREF(globals[ins.operand], "Store global override");
                        }
                        if (ins.code == STORE_NAME_GLOBAL) {
                            globals[ins.operand] = OP_POP();
                        } else {
                            globals[ins.operand] = OP_TOP();
                        }
                        SLOT_INCREF(globals[ins.operand], "STORE_NAME_GLOBAL[_NOPOP]");
                        if (verbose) {
                            std::cout << "Stored " << globals[ins.operand]->as_string() << " to name " << ins.operand << " in globals."
                                      << std::endl;
                        }
                        DISPATCH;
                    }
                    case JMP: {
                        ip = addrs[ins.operand] - 1;
                        if (verbose) {
                            std::cout << "Jumped to instruction address " << ins.operand << "." << std::endl;
                        }
                        DISPATCH;
                    }
                    case JMP_TRUE: {
                        slot *o = OP_POP();
                        if (o->int_val) {
                            ip = addrs[ins.operand] - 1;
                            if (verbose) {
                                std::cout << "The condition is true, jumped to instruction address " << ins.operand
                                          << "."
                                          << std::endl;
                            }
                        }
                        SLOT_DECREF(o, "Jmp true instruct poped op from the stack");
                        DISPATCH;
                    }
                    case JMP_FALSE: {
                        slot *o = OP_POP();
                        if (!o->int_val) {
                            ip = addrs[ins.operand] - 1;
                            if (verbose) {
                                std::cout << "The condition is false, jumped to instruction address " << ins.operand
                                          << "." << std::endl;
                            }
                        }
                        SLOT_DECREF(o, "Jmp false instruct poped op from the stack");
                        DISPATCH;
                    }
                    case UNARY_OP: {
                        slot *operand = OP_POP();

                        if (ins.operand == 0 || ins.operand == 1) {
                            slot *res = nullptr;
                            // NOT
                            if (ins.operand == 0) {
                                if (operand->type == INT) {
                                    res = new slot((int_tp) (operand->int_val ? 0 : 1));
                                }
                            }
                            // NEGATIVE
                            if (ins.operand == 1) {
                                if (operand->type == INT) {
                                    res = new slot(-operand->int_val);
                                } else if (operand->type == FLOAT) {
                                    res = new slot(-operand->float_val);
                                }
                            }

                            if (res == nullptr) {
                                panic("Unsupported unary operator");
                            }

                            OP_PUSH(res);
                            if (verbose) {
                                std::cout << "Pop " << operand->as_string() << ", calculate with unary operator "
                                          << ins.operand << ". Result " << res->as_string()
                                          << " is pushed into the stack." << std::endl;
                            }
                            SLOT_DECREF(operand, "Unary-op for the operand, decref it");
                            DISPATCH;
                        }

                        // SELF INCREMENT BY ONE
                        if (ins.operand == 2) {
                            operand->int_val++;
                            if (verbose) {
                                std::cout << "Increased the loaded variable by one." << std::endl;
                            }
                            SLOT_DECREF(operand, "Increased by one");
                            DISPATCH;
                        }
                        // SELF DECREASEMENT BY ONE
                        if (ins.operand == 3) {
                            operand->int_val--;
                            if (verbose) {
                                std::cout << "Decreased the loaded variable by one." << std::endl;
                            }
                            SLOT_DECREF(operand, "Decreased by one");
                            DISPATCH;
                        }
                    }
                    case BINARY_OP: {
                        slot *right = OP_POP();
                        slot *left = OP_POP();

                        slot *res = nullptr;
                        // +
                        if (ins.operand == 0) {
                            if (left->type == INT && right->type == INT) {
                                res = new slot(left->int_val + right->int_val);
                            } else if (left->type == INT && right->type == FLOAT) {
                                res = new slot(left->int_val + right->float_val);
                            } else if (left->type == FLOAT && right->type == INT) {
                                res = new slot(left->float_val + right->int_val);
                            } else if (left->type == FLOAT && right->type == FLOAT) {
                                res = new slot(left->float_val + right->float_val);
                            }
                        }

                            // -
                        else if (ins.operand == 1) {
                            if (left->type == INT && right->type == INT) {
                                res = new slot(left->int_val - right->int_val);
                            } else if (left->type == INT && right->type == FLOAT) {
                                res = new slot(left->int_val - right->float_val);
                            } else if (left->type == FLOAT && right->type == INT) {
                                res = new slot(left->float_val - right->int_val);
                            } else if (left->type == FLOAT && right->type == FLOAT) {
                                res = new slot(left->float_val - right->float_val);
                            }
                        }

                            // *
                        else if (ins.operand == 2) {
                            if (left->type == INT && right->type == INT) {
                                res = new slot(left->int_val * right->int_val);
                            } else if (left->type == INT && right->type == FLOAT) {
                                res = new slot(left->int_val * right->float_val);
                            } else if (left->type == FLOAT && right->type == INT) {
                                res = new slot(left->float_val * right->int_val);
                            } else if (left->type == FLOAT && right->type == FLOAT) {
                                res = new slot(left->float_val * right->float_val);
                            }
                        }

                            // %
                        else if (ins.operand == 3) {
                            if (left->type == INT && right->type == INT) {
                                res = new slot(left->int_val % right->int_val);
                            }
                        }

                            // /
                        else if (ins.operand == 4) {
                            if (left->type == INT && right->type == INT) {
                                res = new slot(left->int_val / right->int_val);
                            } else if (left->type == INT && right->type == FLOAT) {
                                res = new slot(left->int_val / right->float_val);
                            } else if (left->type == FLOAT && right->type == INT) {
                                res = new slot(left->float_val / right->int_val);
                            } else if (left->type == FLOAT && right->type == FLOAT) {
                                res = new slot(left->float_val / right->float_val);
                            }
                        }

                            // &
                        else if (ins.operand == 5) {
                            if (left->type == INT && right->type == INT) {
                                res = new slot((int_tp) ((unsigned int) left->int_val & (unsigned int) right->int_val));
                            }
                        }

                            // |
                        else if (ins.operand == 6) {
                            if (left->type == INT && right->type == INT) {
                                res = new slot((int_tp) ((unsigned int) left->int_val | (unsigned int) right->int_val));
                            }
                        }

                            // <<
                        else if (ins.operand == 7) {
                            if (left->type == INT && right->type == INT) {
                                res = new slot((int_tp) ((unsigned int) left->int_val << (unsigned int) right->int_val));
                            }
                        }

                            // >>
                        else if (ins.operand == 8) {
                            if (left->type == INT && right->type == INT) {
                                res = new slot((int_tp) ((unsigned int) left->int_val >> (unsigned int) right->int_val));
                            }
                        }


                            // ^
                        else if (ins.operand == 9) {
                            if (left->type == INT && right->type == INT) {
                                res = new slot((int_tp) ((unsigned int) left->int_val ^ (unsigned int) right->int_val));
                            }
                        }

                            // <
                        else if (ins.operand == 10) {
                            if (left->type == INT && right->type == INT) {
                                res = new slot(left->int_val < right->int_val);
                            } else if (left->type == INT && right->type == FLOAT) {
                                res = new slot(left->int_val < right->float_val);
                            } else if (left->type == FLOAT && right->type == INT) {
                                res = new slot(left->float_val < right->int_val);
                            } else if (left->type == FLOAT && right->type == FLOAT) {
                                res = new slot(left->float_val < right->float_val);
                            }
                        }

                            // <=
                        else if (ins.operand == 11) {
                            if (left->type == INT && right->type == INT) {
                                res = new slot(left->int_val <= right->int_val);
                            } else if (left->type == INT && right->type == FLOAT) {
                                res = new slot(left->int_val <= right->float_val);
                            } else if (left->type == FLOAT && right->type == INT) {
                                res = new slot(left->float_val <= right->int_val);
                            } else if (left->type == FLOAT && right->type == FLOAT) {
                                res = new slot(left->float_val <= right->float_val);
                            }
                        }

                            // >
                        else if (ins.operand == 12) {
                            if (left->type == INT && right->type == INT) {
                                res = new slot(left->int_val > right->int_val);
                            } else if (left->type == INT && right->type == FLOAT) {
                                res = new slot(left->int_val > right->float_val);
                            } else if (left->type == FLOAT && right->type == INT) {
                                res = new slot(left->float_val > right->int_val);
                            } else if (left->type == FLOAT && right->type == FLOAT) {
                                res = new slot(left->float_val > right->float_val);
                            }
                        }

                            // >=
                        else if (ins.operand == 13) {
                            if (left->type == INT && right->type == INT) {
                                res = new slot(left->int_val >= right->int_val);
                            } else if (left->type == INT && right->type == FLOAT) {
                                res = new slot(left->int_val >= right->float_val);
                            } else if (left->type == FLOAT && right->type == INT) {
                                res = new slot(left->float_val >= right->int_val);
                            } else if (left->type == FLOAT && right->type == FLOAT) {
                                res = new slot(left->float_val >= right->float_val);
                            }
                        }

                        // ==
                        else if (ins.operand == 14) {
                            if (left->type == INT && right->type == INT) {
                                res = new slot(left->int_val == right->int_val);
                            } else if (left->type == FLOAT && right->type == FLOAT) {
                                res = new slot(left->float_val == right->float_val);
                            } else if (left->type == CHAR && right->type == CHAR) {
                                res = new slot(left->char_val == right->char_val);
                            } else {
                                res = new slot(false);
                            }
                        }

                            // !=
                        else if (ins.operand == 15) {
                            if (left->type == INT && right->type == INT) {
                                res = new slot(left->int_val != right->int_val);
                            } else if (left->type == FLOAT && right->type == FLOAT) {
                                res = new slot(left->float_val != right->float_val);
                            } else if (left->type == CHAR && right->type == CHAR) {
                                res = new slot(left->char_val != right->char_val);
                            } else {
                                res = new slot(true);
                            }
                        }

                        if (res == nullptr) {
                            panic("Unsupported binary operator");
                        }

                        OP_PUSH(res);
                        if (verbose) {
                            std::cout << "Pop " << left->as_string() << " and " << right->as_string()
                                      << ", calculate with binary operator " << ins.operand << ". Result "
                                      << res->as_string() << " is pushed into the stack." << std::endl;
                        }
                        SLOT_DECREF(left, "Bin-Op Left operand decref");
                        SLOT_DECREF(right, "Bin-Op Right operand decref");
                        DISPATCH;
                    }
                    case HALT: {
                        if (verbose) {
                            std::cout << "Program received HALT signal, terminating..." << std::endl;
                        }
                        goto finish;
                    }
                    case PRINTK: {
                        slot *slot = OP_POP();
                        std::cout << slot->as_string() << std::endl;
                        SLOT_DECREF(slot, "Printk");
                        DISPATCH;
                    }
                    case PUTCH: {
                        slot *slot = OP_POP();
                        SLOT_DECREF(slot, "Putch");
                        std::cout << slot->char_val;
                        DISPATCH;
                    }
                    case GETCH: {
                        OP_PUSH(new slot((char_tp) getchar()));
                        DISPATCH;
                    }
                    case STORE_GLOBAL: {
                        slot *val = OP_POP();
                        global_operands[++op_top] = val;
                        if (verbose) {
                            std::cout << "Pushed local value " << val->as_string() << " into global operands."
                                      << std::endl;
                        }
                        DISPATCH;
                    }
                    case LOAD_GLOBAL: {
                        slot *val = global_operands[op_top];
                        op_top--;
                        OP_PUSH(val);
                        if (verbose) {
                            std::cout << "Pushed global value " << val->as_string() << " into local operands."
                                      << std::endl;
                        }
                        DISPATCH;
                    }
                    case BUILD_ARR: {
                        basic_data_types type = VOID;
                        if (ins.operand == 0) {
                            type = INT;
                        } else if (ins.operand == 1) {
                            type = FLOAT;
                        } else if (ins.operand == 2) {
                            type = CHAR;
                        } else {
                            panic("Unexpected type");
                        }
                        int val = OP_POP()->int_val;
                        slot *tmp = new slot(val, type);
                        OP_PUSH(tmp);
                        if (verbose) {
                            std::cout << "Built array " << ins.operand << "[" << val << "]." << std::endl;
                        }
                        DISPATCH;
                    }
                    case BINARY_SUBSCR: {
                        /*
                         * e.g.
                         * LOAD_NAME a
                         * LOAD_INT 4
                         * BINARY_SUBSCR
                         */
                        slot *source = OP_POP();
                        slot *target = OP_POP();
                        int subscr = source->int_val;
                        if (subscr < 0 || subscr >= target->array_size) {
                            panic("Array index out of bound");
                        }
                        slot *fresh = OP_PUSH(*(target->array_val + subscr));
                        if (verbose) {
                            std::cout << "Loaded element with index " << subscr << " of the array." << std::endl;
                        }
                        SLOT_DECREF(source, "Binary-subscr array index decref");
                        SLOT_INCREF(fresh, "Array value is referenced");
                        DISPATCH;
                    }
                    case STORE_SUBSCR:
                    case STORE_SUBSCR_INPLACE:
                    case STORE_SUBSCR_NOPOP: {
                        /*
                         * e.g.
                         * LOAD_NAME a
                         * LOAD_INT 4
                         * LOAD_INT 5
                         * a[4] = 5;
                         */
                        slot *val = OP_POP();
                        slot *p_subscr = OP_POP();
                        int subscr = p_subscr->int_val;
                        slot *target = OP_TOP();
                        if (subscr < 0 || subscr >= target->array_size) {
                            panic("Array index out of bound");
                        }
                        SLOT_DECREF(target->array_val[subscr], "Array element store subscr");
                        target->array_val[subscr] = val;
                        if (verbose) {
                            std::cout << "Changed element with index " << subscr << " of the array to "
                                      << val->as_string() << "." << std::endl;
                        }

                        if (ins.code != STORE_SUBSCR_INPLACE) {
                            OP_POP();
                        }
                        if (ins.code == STORE_SUBSCR_NOPOP) {
                            OP_PUSH(val);
                        }
                        SLOT_DECREF(p_subscr, "Poped target subscr");
                        DISPATCH;
                    }
                    default: {
                        panic("Unexpected instruction");
                        break;
                    }
                }
            }
        }
        finish:
        {
            if (evaluator) {
                finish = clock();
                double time_delta = (double) (finish - start) / CLOCKS_PER_SEC;
                std::cout << "<<<<<* Performance evaluator *>>>>>" << std::endl;
                std::cout << n_ins << " instructions executed in total" << std::endl;
                std::cout << "Time consumotion(s): " << std::fixed << std::setprecision(8) << time_delta << std::endl;
                std::cout << "MIPS: " << std::fixed << std::setprecision(8) << (double) n_ins / time_delta * 1e-6 << std::endl;
            }
        }
    }
};

std::unordered_map<std::string, instruct_code> Machine::string_inscode_mapping;
int Machine::inscode_param_cnt_mapping[200];

void interpret(std::istream &is, bool verbose, bool evaluate, bool in_interact) {
    Machine machine = Machine();
    if (verbose) {
        machine.enable_verbose();
    }
    if (evaluate) {
        machine.enable_evaluator();
    }
    int addr;
    while (is >> addr) {
        if (in_interact && addr == -1) {
            machine.dispatch();
            break;
        }
        instruct_code ins;
        if (in_interact) {
            std::string ins_str;
            is >> ins_str;
            ins = Machine::string_inscode_mapping[ins_str];
        } else {
            int ins_tmp;
            is >> ins_tmp;
            ins = instruct_code(ins_tmp);
        }
        if (ins == CONSTANT) {
            int type;
            is >> type;
            switch (type) {
                // int
                case 0: {
                    int_tp tmp;
                    is >> tmp;
                    constants[addr] = new slot(tmp);
                    break;
                }
                    // float
                case 1: {
                    float_tp tmp;
                    is >> tmp;
                    constants[addr] = new slot(tmp);
                    break;
                }
                    // char
                case 2: {
                    int tmp;
                    is >> tmp;
                    constants[addr] = new slot((char_tp) tmp);
                    break;
                }

                default: {
                    panic("Unexpected type");
                }
            }
            is >> constants[addr]->ref_cnt;
            continue;
        } else if (ins == CMALLOC) {
            is >> constant_cnt;
            if (constant_cnt) constants = new slot*[constant_cnt];
            continue;
        }
        int param_number = Machine::inscode_param_cnt_mapping[ins];
        if (param_number) {
            int param;
            is >> param;
            machine.add_instruct(instruct(addr, ins, param));
        } else {
            machine.add_instruct(instruct(addr, ins));
        }
    }
    if (!in_interact) machine.dispatch();
}

void interact(bool verbose, bool evaluate) {
    interpret(std::cin, verbose, evaluate, true);
}

void assemble(const std::string& raw_file_path, const std::string& out_file_path, std::string password) {
    std::ifstream raw_file(raw_file_path, std::ios::in);
    std::ofstream out_file(out_file_path, std::ios::out | std::ios::trunc);

    std::cout << "<<<<* SLang Virtual Machine Assembler *>>>>" << std::endl;
    std::stringstream buf;
    int addr;
    buf << MAGIC;
    while (raw_file >> addr) {
        std::string ins_str;
        raw_file >> ins_str;
        std::cout << ":Generating " << ins_str << " at " << addr << "..." << std::endl;
        instruct_code ins = Machine::string_inscode_mapping[ins_str];
        int param_number = Machine::inscode_param_cnt_mapping[ins];
        buf << addr << " " << ins << " ";
        while (param_number--) {
            std::string param;
            raw_file >> param;
            buf << param << " ";
        }
    }

    std::string s = buf.str();
    password = MAGIC + password;
    int len = password.length();
    std::cout << ":Encrypting bytecode..." << std::endl;
    for (int i = 0; i < s.length(); i++) s[i] = (unsigned int) s[i] ^ (unsigned int) password[i % len];
    out_file << s;

    raw_file.close();
    out_file.close();
}

void run(const std::string& input_file_path, bool verbose, bool evaluate, std::string password) {
    std::ifstream input_file(input_file_path, std::ios::in);
    std::string content((std::istreambuf_iterator<char>(input_file)),
                        (std::istreambuf_iterator<char>()));
    password = MAGIC + password;
    int len = password.length();
    for (int i = 0; i < content.length(); i++) content[i] = (unsigned int) content[i] ^ (unsigned int) password[i % len];
    std::stringstream ss(content);
    std::string hd;
    ss >> hd;
    interpret(ss, verbose, evaluate, false);
}

void disassemble(const std::string& input_file_path, std::string password) {
    std::ifstream input_file(input_file_path, std::ios::in);
    std::string content((std::istreambuf_iterator<char>(input_file)),
                        (std::istreambuf_iterator<char>()));
    std::string code_name_mapping[200];
    for (const auto& x : Machine::string_inscode_mapping) {
        code_name_mapping[x.second] = x.first;
    }
    password = MAGIC + password;
    int len = password.length();
    for (int i = 0; i < content.length(); i++) content[i] = ((unsigned int) content[i]) ^ ((unsigned int) password[i % len]);
    std::stringstream ss(content);
    std::string hd;
    ss >> hd;
    int addr;
    while (ss >> addr) {
        std::cout << addr << " ";
        int ins_tmp;
        instruct_code ins;
        ss >> ins_tmp;
        ins = instruct_code(ins_tmp);
        std::cout << code_name_mapping[ins] << " ";
        int param_number = Machine::inscode_param_cnt_mapping[ins];
        while (param_number--) {
            std::string param;
            ss >> param;
            std::cout << param << " ";
        }
        std::cout << std::endl;
    }
}

int main(int argc, char *argv[]) {
    Machine::load_param_mapping();
    enum run_mode {
        RUN,
        INTERACT,
        DISASSEMBLE,
        ASSEMBLE
    };
    run_mode rm = RUN;
    char const *optstring = "r:d:a:ivo:p:eh";
    std::string input_path;
    std::string output_path;
    std::string password;
    bool verbose = false;
    bool evaluate = false;
    int o;
    while ((o = getopt(argc, argv, optstring)) != -1) {
        switch (o) {
            case 'e':
                evaluate = true;
                break;
            case 'r':
                rm = RUN;
                input_path.assign(optarg);
                break;
            case 'i':
                rm = INTERACT;
                break;
            case 'd':
                rm = DISASSEMBLE;
                input_path.assign(optarg);
                break;
            case 'a':
                rm = ASSEMBLE;
                input_path.assign(optarg);
                break;
            case 'v':
                verbose = true;
                break;
            case 'o':
                output_path.assign(optarg);
                break;
            case 'p':
                password.assign(optarg);
                break;
            case 'h':
            default:
                std::cout <<
                 "\n"
                 "Usage:\n"
                 "$ svm -r (-e) ./helloworld.slb (-v) (-p password) -- Run program (-v: in verbose mode, -e: performance evaluator)\n"
                 "$ svm -d ./helloworld.slb (-p password) -- Disassembly\n"
                 "$ svm -i (-v) (-e) -- Interact Mode (-v: in verbose mode, -e: performance evaluator)\n"
                 "$ svm -a ./helloworld.txt -o ./helloworld.slb (-p password) -- Assembly input file\n" << std::endl;
                break;
        }
    }
    switch (rm) {
        case RUN:
            run(input_path, verbose, evaluate, password);
            break;
        case INTERACT:
            Machine::load_name_code_mapping();
            interact(verbose, evaluate);
            break;
        case ASSEMBLE:
            Machine::load_name_code_mapping();
            assemble(input_path, output_path, password);
            break;
        case DISASSEMBLE:
            Machine::load_name_code_mapping();
            disassemble(input_path, password);
            break;
    }
}
