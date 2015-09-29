//
//  asm6502.cpp
//  
//
//  Created by Carl-Henrik Skårstedt on 9/23/15.
//
//
//	A simple 6502 assembler
//
//
// The MIT License (MIT)
//
// Copyright (c) 2015 Carl-Henrik Skårstedt
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software
// and associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute,
// sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
// FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// https://github.com/Sakrac/struse/wiki/Asm6502-Syntax
//

#define _CRT_SECURE_NO_WARNINGS		// Windows shenanigans
#define STRUSE_IMPLEMENTATION		// include implementation of struse in this file
#include "struse.h"
#include <vector>
#include <stdio.h>
#include <stdlib.h>

// if the number of resolved labels exceed this in one late eval then skip
//	checking for relevance and just eval all unresolved expressions.
#define MAX_LABELS_EVAL_ALL 16

// Max number of nested scopes (within { and })
#define MAX_SCOPE_DEPTH 32

// The maximum complexity of expressions to be evaluated
#define MAX_EVAL_VALUES 32
#define MAX_EVAL_OPER 64

// Internal status and error type
enum StatusCode {
	STATUS_OK,			// everything is fine
	STATUS_NOT_READY,	// label could not be evaluated at this time
	ERROR_UNEXPECTED_CHARACTER_IN_EXPRESSION,
	ERROR_TOO_MANY_VALUES_IN_EXPRESSION,
	ERROR_TOO_MANY_OPERATORS_IN_EXPRESSION,
	ERROR_UNBALANCED_RIGHT_PARENTHESIS,
	ERROR_EXPRESSION_OPERATION,
	ERROR_EXPRESSION_MISSING_VALUES,
	ERROR_INSTRUCTION_NOT_ZP,
	ERROR_INVALID_ADDRESSING_MODE_FOR_BRANCH,
	ERROR_BRANCH_OUT_OF_RANGE,
	ERROR_LABEL_MISPLACED_INTERNAL,
	ERROR_BAD_ADDRESSING_MODE,
	ERROR_UNEXPECTED_CHARACTER_IN_ADDRESSING_MODE,
	
	ERROR_STOP_PROCESSING_ON_HIGHER,	// errors greater than this will stop execution
	
	ERROR_TARGET_ADDRESS_MUST_EVALUATE_IMMEDIATELY,
	ERROR_TOO_DEEP_SCOPE,
	ERROR_UNBALANCED_SCOPE_CLOSURE,
	ERROR_BAD_MACRO_FORMAT,
	ERROR_ALIGN_MUST_EVALUATE_IMMEDIATELY,
	ERROR_OUT_OF_MEMORY_FOR_MACRO_EXPANSION,
};

// The following strings are in the same order as StatusCode
const char *aStatusStrings[] = {
	"ok",
	"not ready",
	"Unexpected character in expression",
	"Too many values in expression",
	"Too many operators in expression",
	"Unbalanced right parenthesis in expression",
	"Expression operation",
	"Expression missing values",
	"Instruction can not be zero page",
	"Invalid addressing mode for branch instruction",
	"Branch out of range",
	"Internal label organization mishap",
	"Bad addressing mode",
	"Unexpected character in addressing mode",
	"Errors after this point will stop execution",
	"Target address must evaluate immediately for this operation",
	"Scoping is too deep",
	"Unbalanced scope closure",
	"Unexpected macro formatting",
	"Align must evaluate immediately",
	"Out of memory for macro expansion",
};

// Operators are either instructions or directives
enum OperationType {
	OT_NONE,
	OT_MNEMONIC,
	OT_DIRECTIVE
};

// Opcode encoding
typedef struct {
	unsigned int op_hash;
	unsigned char group;	// group #
	unsigned char index;	// ground index
	unsigned char type;		// mnemonic or
} OP_ID;

//
// 6502 instruction encoding according to this page
// http://www.llx.com/~nparker/a2/opcodes.html
// decoded instruction:
// XXY10000 for branches
// AAABBBCC for CC=00, 01, 10
// and some custom ops
//

enum AddressingMode {
	AM_REL_ZP_X,		// 0 (zp,x)
	AM_ZP,				// 1 zp
	AM_IMMEDIATE,		// 2 #$hh
	AM_ABSOLUTE,		// 3 $hhhh
	AM_REL_ZP_Y,		// 4 (zp),y
	AM_ZP_X,			// 5 zp,x
	AM_ABSOLUTE_Y,		// 6 $hhhh,y
	AM_ABSOLUTE_X,		// 7 $hhhh,x
	AM_RELATIVE,		// 8 ($xxxx)
	AM_ACCUMULATOR,		// 9 A
	AM_NONE,			// 10 <empty>
	AM_INVALID,			// 11
};

// How instruction argument is encoded
enum CODE_ARG {
	CA_NONE,			// single byte instruction
	CA_ONE_BYTE,		// instruction carries one byte
	CA_TWO_BYTES,		// instruction carries two bytes
	CA_BRANCH			// instruction carries a relative address
};

// opcode groups
enum OP_GROUP {
	OPG_SUBROUT,
	OPG_CC01,
	OPG_CC10,
	OPG_STACK,
	OPG_BRANCH,
	OPG_FLAG,
	OPG_CC00,
	OPG_TRANS
};

// opcode exception indices
enum OP_INDICES {
	OPI_JSR = 1,
	OPI_LDX = 5,
	OPI_STX = 4,
	OPI_STA = 4,
	OPI_JMP = 1,
};

// opcode names in groups (prefix by group size)
const char aInstr[] = {
	"BRK,JSR,RTI,RTS\n"
	"ORA,AND,EOR,ADC,STA,LDA,CMP,SBC\n"
	"ASL,ROL,LSR,ROR,STX,LDX,DEC,INC\n"
	"PHP,PLP,PHA,PLA,DEY,TAY,INY,INX\n"
	"BPL,BMI,BVC,BVS,BCC,BCS,BNE,BEQ\n"
	"CLC,SEC,CLI,SEI,TYA,CLV,CLD,SED\n"
	"BIT,JMP,,STY,LDY,CPY,CPX\n"
	"TXA,TXS,TAX,TSX,DEX,,NOP"
};

// group # + index => base opcode
const unsigned char aMulAddGroup[][2] = {
	{ 0x20,0x00 },
	{ 0x20,0x01 },
	{ 0x20,0x02 },
	{ 0x20,0x08 },
	{ 0x20,0x10 },
	{ 0x20,0x18 },
	{ 0x20,0x20 },
	{ 0x10,0x8a }
};

char aCC00Modes[] = { AM_IMMEDIATE, AM_ZP, AM_INVALID, AM_ABSOLUTE, AM_INVALID, AM_ZP_X, AM_INVALID, AM_ABSOLUTE_X };
char aCC01Modes[] = { AM_REL_ZP_X, AM_ZP, AM_IMMEDIATE, AM_ABSOLUTE, AM_REL_ZP_Y, AM_ZP_X, AM_ABSOLUTE_X, AM_ABSOLUTE_Y };
char aCC10Modes[] = { AM_IMMEDIATE, AM_ZP, AM_NONE, AM_ABSOLUTE, AM_INVALID, AM_ZP_X, AM_INVALID, AM_ABSOLUTE_X };

unsigned char CC00ModeAdd[] = { 0xff, 4, 0, 12, 0xff, 20, 0xff, 28 };
unsigned char CC00Mask[] = { 0x0a, 0x08, 0x08, 0x2a, 0xae, 0x0e, 0x0e };
unsigned char CC10ModeAdd[] = { 0xff, 4, 0, 12, 0xff, 20, 0xff, 28 };
unsigned char CC10Mask[] = { 0xaa, 0xaa, 0xaa, 0xaa, 0x2a, 0xae, 0xaa, 0xaa };

static const strref c_comment("//");
static const strref word_char_range("!0-9a-zA-Z_@$!");
static const strref label_char_range("!0-9a-zA-Z_@$!.");

// pairArray is basically two vectors sharing a size without using constructors
template <class H, class V> class pairArray {
protected:
	H *keys;
	V *values;
	unsigned int _count;
	unsigned int _capacity;
public:
	pairArray() : keys(nullptr), values(nullptr), _count(0), _capacity(0) {}
	void reserve(unsigned int size) {
		if (size>_capacity) {
			H *new_keys = (H*)malloc(sizeof(H) * size); if (!new_keys) { return; }
			V *new_values = (V*)malloc(sizeof(V) * size); if (!new_values) { free(new_keys); return; }
			if (keys && values) {
				memcpy(new_keys, keys, sizeof(H) * _count);
				memcpy(new_values, values, sizeof(V) * _count);
				free(keys); free(values);
			}
			keys = new_keys;
			values = new_values;
			_capacity = size;
		}
	}
	bool insert(unsigned int pos) {
		if (pos>_count)
			return false;
		if (_count==_capacity)
			reserve(_capacity+64);
		if (pos<_count) {
			memmove(keys+pos+1, keys+pos, sizeof(H) * (_count-pos));
			memmove(values+pos+1, values+pos, sizeof(V) * (_count-pos));
		}
		memset(keys+pos, 0, sizeof(H));
		memset(values+pos, 0, sizeof(V));
		_count++;
		return true;
	}
	bool insert(unsigned int pos, H key) {
		if (insert(pos)) {
			keys[pos] = key;
			return true;
		}
		return false;
	}
	void remove(unsigned int pos) {
		if (pos<_count) {
			_count--;
			if (pos<_count) {
				memmove(keys+pos, keys+pos+1, sizeof(H) * (_count-pos));
				memmove(values+pos, values+pos+1, sizeof(V) * (_count-pos));
			}
		}
	}
	H* getKeys() { return keys; }
	H& getKey(unsigned int pos) { return keys[pos]; }
	V* getValues() { return values; }
	V& getValue(unsigned int pos) { return values[pos];  }
	unsigned int count() const { return _count; }
	unsigned int capacity() const { return _capacity; }
	void clear() {
		if (keys!=nullptr)
			free(keys);
		keys = nullptr;
		if (values!=nullptr)
			free(values);
		values = nullptr;
		_capacity = 0;
		_count = 0;
	}
	~pairArray() { clear(); }
};

// Data related to a label
typedef struct {
public:
	strref label_name;		// the name of this label
	strref expression;		// the expression of this label (optional, if not possible to evaluate yet)
	int value;
	bool evaluated;			// a value may not yet be evaluated
	bool zero_page;			// addresses known to be zero page
	bool pc_relative;		// this is an inline label describing a point in the code
} Label;

// When an expression is evaluated late, determine how to encode the result
enum LateEvalType {
	LET_LABEL,				// this evaluation applies to a label and not memory
	LET_ABS_REF,			// calculate an absolute address and store at 0, +1
	LET_BRANCH,				// calculate a branch offset and store at this address
	LET_BYTE,				// calculate a byte and store at this address
};

// If an expression can't be evaluated immediately, this is required
// to reconstruct the result when it can be.
typedef struct {
	unsigned char* target;	// offset into output buffer
	int address;			// current pc
	int scope;				// scope pc
	strref label;			// valid if this is not a target but another label
	strref expression;
	strref source_file;
	LateEvalType type;
} LateEval;

// A macro is a text reference to where it was defined
typedef struct {
	strref name;
	strref macro;
	strref source_name;		// source file name (error output)
	strref source_file;		// entire source file (req. for line #)
} Macro;

// Source context is current file (include file, etc.) or current macro.
typedef struct {
	strref source_name;	// source file name (error output)
	strref source_file;	// entire source file (req. for line #)
	strref code_segment; // the segment of the file for this context
	strref read_source; // current position/length in source file
} SourceContext;

class ContextStack : private std::vector<SourceContext> {
public:
	SourceContext& curr() { return (*this)[size()-1]; }
	void push(strref src_name, strref src_file, strref code_seg) {
		SourceContext context;
		context.source_name = src_name;
		context.source_file = src_file;
		context.code_segment = code_seg;
		context.read_source = code_seg;
		push_back(context);
	}
	void pop() { pop_back();  }
	bool has_work() { return !empty(); }
};

// Assembler directives such as org / pc / load / etc.
enum AssemblerDirective {
	AD_ORG,
	AD_LOAD,
	AD_ALIGN,
	AD_MACRO,
	AD_EVAL,
	AD_BYTES,
	AD_WORDS,
	AD_TEXT,
	AD_INCLUDE,
	AD_INCBIN,
};

// The state of the assembly
class Asm {
public:
	pairArray<unsigned int, Label> labels;
	pairArray<unsigned int, Macro> macros;
	std::vector<LateEval> lateEval;
	std::vector<strref> localLabels; // remove these labels when a global pc label is added
	std::vector<char*> loadedData;	// free when 
	
	// context for macros / include files
	ContextStack contextStack;
	
	// target output memory
	unsigned char *output, *curr;
	size_t output_capacity;

	unsigned int address;
	unsigned int load_address;
	int scope_address[MAX_SCOPE_DEPTH];
	int scope_depth;
	bool set_load_address;

	// Convert source to binary
	void Assemble(strref source, strref filename);

	// Clean up memory allocations
	void Cleanup();
	
	// Make sure there is room to write more code
	void CheckOutputCapacity(unsigned int addSize);

	// Add and build a macro
	StatusCode AddMacro(strref macro, strref source_name, strref source_file);
	StatusCode BuildMacro(Macro &m, strref arg_list);

	// Calculate a value based on an expression.
	StatusCode EvalExpression(strref expression, int pc, int scope_pc,
							  int scope_end_pc, int &result);

	// Access labels
	Label* GetLabel(strref label);
	Label* AddLabel(unsigned int hash);

	// Late expression evaluation
	void AddLateEval(int pc, int scope_pc, unsigned char *target,
					 strref expression, strref source_file, LateEvalType type);
	void AddLateEval(strref label, int pc, int scope_pc,
					 strref expression, LateEvalType type);
	StatusCode CheckLateEval(strref added_label=strref(), int scope_end = -1);

	// Manage locals
	void MarkLabelLocal(strref label);
	void FlushLocalLabels();

	// Assembler steps
	StatusCode ApplyDirective(AssemblerDirective dir, strref line, strref source_file);
	AddressingMode GetAddressMode(strref line, bool flipXY,
								  StatusCode &error, strref &expression);
	StatusCode AddOpcode(strref line, int group, int index, strref source_file);
	StatusCode BuildSegment(OP_ID *pInstr, int numInstructions);

	// constructor
	Asm() : address(0x1000), load_address(0x1000), scope_depth(0), set_load_address(false),
		output(nullptr), curr(nullptr), output_capacity(0) { localLabels.reserve(256); }
};

// Binary search over an array of unsigned integers, may contain multiple instances of same key
unsigned int FindLabelIndex(unsigned int hash, unsigned int *table, unsigned int count)
{
	unsigned int max = count;
	unsigned int first = 0;
	while (count!=first) {
		int index = (first+count)/2;
		unsigned int read = table[index];
		if (hash==read) {
			while (index && table[index-1]==hash)
				index--;	// guarantee first identical index returned on match
			return index;
		} else if (hash>read)
			first = index+1;
		else
			count = index;
	}
	if (count<max && table[count]<hash)
		count++;
	else if (count && table[count-1]>hash)
		count--;
	return count;
}

// Read in text data (main source, include, etc.)
char* LoadText(strref filename, size_t &size) {
	strown<512> file(filename);
	if (FILE *f = fopen(file.c_str(), "r")) {
		fseek(f, 0, SEEK_END);
		size_t _size = ftell(f);
		fseek(f, 0, SEEK_SET);
		if (char *buf = (char*)malloc(_size)) {
			fread(buf, 1, _size, f);
			fclose(f);
			size = _size;
			return buf;
		}
		fclose(f);
	}
	size = 0;
	return nullptr;
}

// Read in binary data (incbin)
char* LoadBinary(strref filename, size_t &size) {
	strown<512> file(filename);
	if (FILE *f = fopen(file.c_str(), "rb")) {
		fseek(f, 0, SEEK_END);
		size_t _size = ftell(f);
		fseek(f, 0, SEEK_SET);
		if (char *buf = (char*)malloc(_size)) {
			fread(buf, _size, 1, f);
			fclose(f);
			size = _size;
			return buf;
		}
		fclose(f);
	}
	size = 0;
	return nullptr;
}

// Clean up work allocations
void Asm::Cleanup() {
	for (std::vector<char*>::iterator i=loadedData.begin(); i!=loadedData.end(); ++i)
		free(*i);
	loadedData.clear();
	labels.clear();
	macros.clear();
	if (output)
		free(output);
	output = nullptr;
	curr = nullptr;
	output_capacity = 0;
}

// Make sure there is room to assemble in
void Asm::CheckOutputCapacity(unsigned int addSize) {
	size_t currSize = curr - output;
	if ((addSize + currSize) >= output_capacity) {
		size_t newSize = currSize * 2;
		if (newSize < 64*1024)
			newSize = 64*1024;
		if ((addSize+currSize) > newSize)
			newSize += newSize;
		unsigned char *new_output = (unsigned char*)malloc(newSize);
		curr = new_output + (curr-output);
		free(output);
		output = new_output;
		output_capacity = newSize;
	}
}

// add a custom macro
StatusCode Asm::AddMacro(strref macro, strref source_name, strref source_file)
{
	// name(optional params) { actual macro }
	strref name = macro.split_label();
	macro.skip_whitespace();
	if (macro[0]!='(' && macro[0]!='{')
		return ERROR_BAD_MACRO_FORMAT;
	unsigned int hash = name.fnv1a();
	unsigned int ins = FindLabelIndex(hash, macros.getKeys(), macros.count());
	Macro *pMacro = nullptr;
	while (ins < macros.count() && macros.getKey(ins)==hash) {
		if (name.same_str_case(macros.getValue(ins).name)) {
			pMacro = macros.getValues() + ins;
			break;
		}
		++ins;
	}
	if (!pMacro) {
		macros.insert(ins, hash);
		pMacro = macros.getValues() + ins;
	}
	pMacro->name = name;
	int pos_bracket = macro.find('{');
	if (pos_bracket < 0) {
		pMacro->macro = strref();
		return ERROR_BAD_MACRO_FORMAT;
	}
	strref macro_body = (macro + pos_bracket).scoped_block_skip();
	pMacro->macro = strref(macro.get(), pos_bracket + macro_body.get_len() + 2);
	pMacro->source_name = source_name;
	pMacro->source_file = source_file;
	return STATUS_OK;
}


// mark a label as a local label
void Asm::MarkLabelLocal(strref label)
{
	localLabels.push_back(label);
}

// find all local labels and remove them
void Asm::FlushLocalLabels()
{
	std::vector<strref>::iterator i = localLabels.begin();
	while (i!=localLabels.end()) {
		unsigned int index = FindLabelIndex(i->fnv1a(), labels.getKeys(), labels.count());
		while (index<labels.count()) {
			if (i->same_str_case(labels.getValue(index).label_name)) {
				labels.remove(index);
				break;
			}
		}
		i = localLabels.erase(i);
	}
}

// if an expression could not be evaluated, add it along with
// the action to perform if it can be evaluated later.
void Asm::AddLateEval(int pc, int scope_pc, unsigned char *target, strref expression, strref source_file, LateEvalType type)
{
	LateEval le;
	le.address = pc;
	le.scope = scope_pc;
	le.target = target;
	le.label.clear();
	le.expression = expression;
	le.source_file = source_file;
	le.type = type;
	
	lateEval.push_back(le);
}

void Asm::AddLateEval(strref label, int pc, int scope_pc, strref expression, LateEvalType type)
{
	LateEval le;
	le.address = pc;
	le.scope = scope_pc;
	le.target = 0;
	le.label = label;
	le.expression = expression;
	le.source_file.clear();
	le.type = type;
	
	lateEval.push_back(le);
}

// When a label is defined or a scope ends check if there are
// any related late label evaluators that can now be evaluated.
StatusCode Asm::CheckLateEval(strref added_label, int scope_end)
{
	std::vector<LateEval>::iterator i = lateEval.begin();
	bool evaluated_label = true;
	strref new_labels[MAX_LABELS_EVAL_ALL];
	int num_new_labels = 0;
	if (added_label)
		new_labels[num_new_labels++] = added_label;
	
	while (evaluated_label) {
		evaluated_label = false;
		while (i != lateEval.end()) {
			int value = 0;
			// check if this expression is related to the late change (new label or end of scope)
			bool check = num_new_labels==MAX_LABELS_EVAL_ALL;
			for (int l=0; l<num_new_labels && !check; l++)
				check = i->expression.find(new_labels[l]) >= 0;
			if (!check && scope_end>0) {
				int gt_pos = 0;
				while (gt_pos>=0 && !check) {
					gt_pos = i->expression.find_at('%', gt_pos);
					if (gt_pos>=0) {
						if (i->expression[gt_pos+1]=='%')
							gt_pos++;
						else
							check = true;
						gt_pos++;
					}
				}
			}
			if (check) {
				int ret = EvalExpression(i->expression, i->address, i->scope, scope_end, value);
				if (ret == STATUS_OK) {
					switch (i->type) {
						case LET_BRANCH:
							value -= i->address;
							if (value<-128 || value>127)
								return ERROR_BRANCH_OUT_OF_RANGE;
							*i->target = (unsigned char)value;
							break;
						case LET_BYTE:
							i->target[0] = value&0xff;
							break;
						case LET_ABS_REF:
							i->target[0] = value&0xff;
							i->target[1] = (value>>8)&0xff;
							break;
						case LET_LABEL: {
							Label *label = GetLabel(i->label);
							if (!label)
								return ERROR_LABEL_MISPLACED_INTERNAL;
							label->value = value;
							label->evaluated = true;
							if (num_new_labels<MAX_LABELS_EVAL_ALL)
								new_labels[num_new_labels++] = label->label_name;
							evaluated_label = true;
							break;
						}
						default:
							break;
					}
					i = lateEval.erase(i);
				} else
					++i;
			} else
				++i;
		}
		added_label.clear();
	}
	return STATUS_OK;
}

// Get a labelc record if it exists
Label *Asm::GetLabel(strref label)
{
	unsigned int label_hash = label.fnv1a();
	unsigned int index = FindLabelIndex(label_hash, labels.getKeys(), labels.count());
	while (index < labels.count() && label_hash == labels.getKey(index)) {
		if (label.same_str(labels.getValue(index).label_name))
			return labels.getValues() + index;
		index++;
	}
	return nullptr;
}

// These are expression tokens in order of precedence (last is highest precedence)
enum EvalOperator {
	EVOP_NONE,
	EVOP_VAL,	// value => read from value queue
	EVOP_LPR,	// left parenthesis
	EVOP_RPR,	// right parenthesis
	EVOP_ADD,	// +
	EVOP_SUB,	// -
	EVOP_MUL,	// * (note: if not preceded by value or right paren this is current PC)
	EVOP_DIV,	// /
	EVOP_AND,	// &
	EVOP_OR,	// |
	EVOP_EOR,	// ^
	EVOP_SHL,	// <<
	EVOP_SHR	// >>
};

//
// EvalExpression
//	Uses the Shunting Yard algorithm to convert to RPN first
//	which makes the actual calculation trivial and avoids recursion.
//	https://en.wikipedia.org/wiki/Shunting-yard_algorithm
//
// Return:
//	STATUS_OK means value is completely evaluated
//	STATUS_NOT_READY means value could not be evaluated right now
//	ERROR_* means there is an error in the expression
//

StatusCode Asm::EvalExpression(strref expression, int pc, int scope_pc, int scope_end_pc, int &result)
{
	int sp = 0;
	int numValues = 0;
	int numOps = 0;
	char op_stack[MAX_EVAL_OPER];

	char ops[MAX_EVAL_OPER];		// RPN expression
	int values[MAX_EVAL_VALUES];	// RPN values (in order of RPN EVOP_VAL operations)

	bool hiByte = false;
	bool loByte = false;
	values[0] = 0;

	if (expression[0]=='>') { hiByte = true; ++expression; }
	else if (expression[0]=='<') { loByte = true; ++expression; }

	EvalOperator prev_op = EVOP_NONE;
	while (expression) {
		int value = 0;
		expression.skip_whitespace();
		// Read a token from the expression (op)
		EvalOperator op = EVOP_NONE;
		char c = expression.get_first();
		switch (c) {
			case '$': ++expression; value = expression.ahextoui_skip(); op = EVOP_VAL; break;
			case '-': ++expression; op = EVOP_SUB; break;
			case '+': ++expression;	op = EVOP_ADD; break;
			case '*': // asterisk means both multiply and current PC, disambiguate!
				if (prev_op==EVOP_VAL || prev_op==EVOP_RPR)	op = EVOP_MUL;
				else { op = EVOP_VAL; value = pc; }
				++expression;
				break;
			case '/': ++expression; op = EVOP_DIV; break;
			case '>': if (expression.get_len()>=2 && expression[1]=='>') {
					expression += 2; op = EVOP_SHR;	} break;
			case '<': if (expression.get_len()>=2 && expression[1]=='<') {
					expression += 2; op = EVOP_SHL; } break;
			case '%': if (scope_end_pc<0) return STATUS_NOT_READY;
					  ++expression; op = EVOP_VAL; value = scope_end_pc; break;
			case '|': ++expression; op = EVOP_OR; break;
			case '&': ++expression; op = EVOP_AND; break;
			case '(': ++expression; op = EVOP_LPR; break;
			case ')': ++expression; op = EVOP_RPR; break;
			default: {
				if (c=='!' && !(expression+1).len_label()) {
					if (scope_pc<0)	// ! by itself is current scope, !+label char is a local label
						return STATUS_NOT_READY;
					++expression;
					op = EVOP_VAL; value = scope_pc;
					break;
				} else if (strref::is_number(c)) {
					value = expression.atoi_skip(); op = EVOP_VAL;
				} else if (c=='!' || strref::is_valid_label(c)) {
					strref label = expression.split_range_trim(label_char_range);//.split_label();
					Label *pValue = GetLabel(label);
					if (!pValue || !pValue->evaluated)	// this label could not be found (yet)
						return STATUS_NOT_READY;
					value = pValue->value; op = EVOP_VAL;
				} else
					return ERROR_UNEXPECTED_CHARACTER_IN_EXPRESSION;
				break;
			}
		}

		// this is the body of the shunting yard algorithm
		if (op == EVOP_VAL) {
			values[numValues++] = value;
			ops[numOps++] = op;
		} else if (op == EVOP_LPR) {
			op_stack[sp++] = op;
		} else if (op == EVOP_RPR) {
			while (sp && op_stack[sp-1]!=EVOP_LPR) {
				sp--;
				ops[numOps++] = op_stack[sp];
			}
			// check that there actually was a left parenthesis
			if (!sp || op_stack[sp-1]!=EVOP_LPR)
				return ERROR_UNBALANCED_RIGHT_PARENTHESIS;
			sp--; // skip open paren
		} else {
			while (sp) {
				EvalOperator p = (EvalOperator)op_stack[sp-1];
				if (p==EVOP_LPR || op>p)
					break;
				ops[numOps++] = p;
				sp--;
			}
			op_stack[sp++] = op;
		}
		// check for out of bounds or unexpected input
		if (numValues==MAX_EVAL_VALUES)
			return ERROR_TOO_MANY_VALUES_IN_EXPRESSION;
		else if (numOps==MAX_EVAL_OPER || sp==MAX_EVAL_OPER)
			return ERROR_TOO_MANY_OPERATORS_IN_EXPRESSION;
		
		prev_op = op;
	}
	while (sp) {
		sp--;
		ops[numOps++] = op_stack[sp];
	}

	// processing the result RPN will put the completed expression into values[0].
	// values is used as both the queue and the stack of values since reads/writes won't
	// exceed itself.
	int valIdx = 0;
	for (int o = 0; o<numOps; o++) {
		EvalOperator op = (EvalOperator)ops[o];
		if (op!=EVOP_VAL && sp<2)
			break; // ignore suffix operations that are lacking values
		switch (op) {
			case EVOP_VAL:	// value
				values[sp++] = values[valIdx++]; break;
			case EVOP_ADD:	// +
				sp--; values[sp-1] += values[sp]; break;
			case EVOP_SUB:	// -
				sp--; values[sp-1] -= values[sp]; break;
			case EVOP_MUL:	// *
				sp--; values[sp-1] *= values[sp];; break;
			case EVOP_DIV:	// /
				sp--; values[sp-1] /= values[sp]; break;
			case EVOP_AND:	// &
				sp--; values[sp-1] &= values[sp]; break;
			case EVOP_OR:	// |
				sp--; values[sp-1] |= values[sp]; break;
			case EVOP_EOR:	// ^
				sp--; values[sp-1] ^= values[sp]; break;
			case EVOP_SHL:	// <<
				sp--; values[sp-1] <<= values[sp]; break;
			case EVOP_SHR:	// >>
				sp--; values[sp-1] >>= values[sp]; break;
			default:
				return ERROR_EXPRESSION_OPERATION;
				break;
		}
	}
	// check hi/lo byte filter
	int val = values[0];
	if (hiByte)
		val = (val>>8)&0xff;
	else if (loByte)
		val &= 0xff;
	result = val;
	
	return STATUS_OK;
}

// Add a label entry
Label* Asm::AddLabel(unsigned int hash) {
	unsigned int index = FindLabelIndex(hash, labels.getKeys(), labels.count());
	labels.insert(index, hash);
	return labels.getValues() + index;
}

// unique key binary search
int LookupOpCodeIndex(unsigned int hash, OP_ID *lookup, int count)
{
	int first = 0;
	while (count!=first) {
		int index = (first+count)/2;
		unsigned int read = lookup[index].op_hash;
		if (hash==read) {
			return index;
		} else if (hash>read)
			first = index+1;
		else
			count = index;
	}
	return -1;	// index not found
}

typedef struct {
	const char *name;
	AssemblerDirective directive;
} DirectiveName;

DirectiveName aDirectiveNames[] {
	{ "PC", AD_ORG },
	{ "ORG", AD_ORG },
	{ "LOAD", AD_LOAD },
	{ "ALIGN", AD_ALIGN },
	{ "MACRO", AD_MACRO },
	{ "EVAL", AD_EVAL },
	{ "BYTE", AD_BYTES },
	{ "BYTES", AD_BYTES },
	{ "WORD", AD_WORDS },
	{ "WORDS", AD_WORDS },
	{ "TEXT", AD_TEXT },
	{ "INCLUDE", AD_INCLUDE },
	{ "INCBIN", AD_INCBIN },
};

static const int nDirectiveNames = sizeof(aDirectiveNames) / sizeof(aDirectiveNames[0]);

int sortHashLookup(const void *A, const void *B) {
	const OP_ID *_A = (const OP_ID*)A;
	const OP_ID *_B = (const OP_ID*)B;
	return _A->op_hash > _B->op_hash ? 1 : -1;
}

int BuildInstructionTable(OP_ID *pInstr, strref instr_text, int maxInstructions)
{
	// create an instruction table (mnemonic hash lookup)
	int numInstructions = 0;
	char group_num = 0;
	while (strref line = instr_text.next_line()) {
		int index_num = 0;
		while (line) {
			strref mnemonic = line.split_token_trim(',');
			if (mnemonic) {
				OP_ID &op_hash = pInstr[numInstructions++];
				op_hash.op_hash = mnemonic.fnv1a_lower();
				op_hash.group = group_num;
				op_hash.index = index_num;
				op_hash.type = OT_MNEMONIC;
			}
			index_num++;
		}
		group_num++;
	}
	
	// add assembler directives
	for (int d=0; d<nDirectiveNames; d++) {
		OP_ID &op_hash = pInstr[numInstructions++];
		op_hash.op_hash = strref(aDirectiveNames[d].name).fnv1a_lower();
		op_hash.group = 0xff;
		op_hash.index = (unsigned char)aDirectiveNames[d].directive;
		op_hash.type = OT_DIRECTIVE;
	}
	
	// sort table by hash for binary search lookup
	qsort(pInstr, numInstructions, sizeof(OP_ID), sortHashLookup);
	return numInstructions;
}

AddressingMode Asm::GetAddressMode(strref line, bool flipXY, StatusCode &error, strref &expression)
{
	bool force_zp = false;
	bool need_more = true;
	strref arg, deco;
	AddressingMode addrMode = AM_INVALID;
	while (need_more) {
		need_more = false;
		switch (line.get_first()) {
			case 0:		// empty line, empty addressing mode
				addrMode = AM_NONE;
				break;
			case '(':	// relative (jmp (addr), (zp,x), (zp),y)
				deco = line.scoped_block_skip();
				line.skip_whitespace();
				expression = deco.split_token_trim(',');
				addrMode = AM_RELATIVE;
				if (deco[0]=='x' || deco[0]=='X')
					addrMode = AM_REL_ZP_X;
				else if (line[0]==',') {
					++line;
					line.skip_whitespace();
					if (line[0]=='y' || line[0]=='Y') {
						addrMode = AM_REL_ZP_Y;
						++line;
					}
				}
				break;
			case '#':	// immediate, determine if value is ok
				++line;
				addrMode = AM_IMMEDIATE;
				expression = line;
				break;
			case '.': {	// .z => force zp (needs more info)
				++line;
				char c = line.get_first();
				if (c=='z' || c=='Z') {
					force_zp = true;
					++line;
					need_more = true;
				} else
					error = ERROR_UNEXPECTED_CHARACTER_IN_ADDRESSING_MODE;
				break;
			}
			default: {	// accumulator or absolute
				if (line) {
					if (line.get_label().same_str("A")) {
						addrMode = AM_ACCUMULATOR;
					} else {	// absolute (zp, offs x, offs y)
						addrMode = force_zp ? AM_ZP : AM_ABSOLUTE;
						expression = line.split_token_trim(',');
						bool relX = line && (line[0]=='x' || line[0]=='X');
						bool relY = line && (line[0]=='y' || line[0]=='Y');
						if ((flipXY && relY) || (!flipXY && relX))
							addrMode = addrMode==AM_ZP ? AM_ZP_X : AM_ABSOLUTE_X;
						else if ((flipXY && relX) || (!flipXY && relY)) {
							if (force_zp) {
								error = ERROR_INSTRUCTION_NOT_ZP;
								break;
							}
							addrMode = AM_ABSOLUTE_Y;
						}
					}
				}
				break;
			}
		}
	}
	return addrMode;
}

// Action based on assembler directive
StatusCode Asm::ApplyDirective(AssemblerDirective dir, strref line, strref source_file)
{
	StatusCode error = STATUS_OK;
	switch (dir) {
		case AD_ORG: {		// org / pc: current address of code
			int addr;
			if (line[0]=='=' || line.get_word().same_str("equ"))
				line.next_word_ws();
			if ((error = EvalExpression(line, address, scope_address[scope_depth], -1, addr))) {
				error = error == STATUS_NOT_READY ? ERROR_TARGET_ADDRESS_MUST_EVALUATE_IMMEDIATELY : error;
				break;
			}
			address = addr;
			scope_address[scope_depth] = address;
			if (!set_load_address) {
				load_address = address;
				set_load_address = true;
			}
			break;
		}
		case AD_LOAD: {		// load: address for target to load code at
			int addr;
			if (line[0]=='=' || line.get_word().same_str("equ"))
				line.next_word_ws();
			if ((error = EvalExpression(line, address, scope_address[scope_depth], -1, addr))) {
				error = error == STATUS_NOT_READY ? ERROR_TARGET_ADDRESS_MUST_EVALUATE_IMMEDIATELY : error;
				break;
			}
			address = addr;
			scope_address[scope_depth] = address;
			if (!set_load_address) {
				load_address = address;
				set_load_address = true;
			}
			break;
		}
		case AD_ALIGN:		// align: align address to multiple of value, fill space with 0
			if (line) {
				int value;
				int status = EvalExpression(line, address, scope_address[scope_depth], -1, value);
				if (status == STATUS_NOT_READY)
					error = ERROR_ALIGN_MUST_EVALUATE_IMMEDIATELY;
				else if (status == STATUS_OK && value>0) {
					int add = (address + value-1) % value;
					address += add;
					CheckOutputCapacity(add);
					for (int a = 0; a<add; a++)
						*curr++ = 0;
				}
			}
			break;
		case AD_EVAL: {		// eval: display the result of an expression in stdout
			int value = 0;
			strref description = line.split_token_trim(':');
			line.trim_whitespace();
			if (line && EvalExpression(line, address, scope_address[scope_depth], -1, value) == STATUS_OK)
				printf("EVAL(%d): " STRREF_FMT ": \"" STRREF_FMT "\" = $%x\n",
					contextStack.curr().source_file.count_lines(description), STRREF_ARG(description), STRREF_ARG(line), value);
			else
				printf("EVAL(%d): \"" STRREF_FMT ": " STRREF_FMT"\"\n",
				contextStack.curr().source_file.count_lines(description), STRREF_ARG(description), STRREF_ARG(line));
			break;
		}
		case AD_BYTES:		// bytes: add bytes by comma separated values/expressions
			while (strref exp = line.split_token_trim(',')) {
				int value;
				error = EvalExpression(exp, address, scope_address[scope_depth], -1, value);
				if (error>STATUS_NOT_READY)
					break;
				else if (error==STATUS_NOT_READY)
					AddLateEval(address, scope_address[scope_depth], curr, exp, source_file, LET_BYTE);
				CheckOutputCapacity(1);
				*curr++ = value;
				address++;
			}
			break;
		case AD_WORDS:		// words: add words (16 bit values) by comma separated values
			while (strref exp = line.split_token_trim(',')) {
				int value;
				error = EvalExpression(exp, address, scope_address[scope_depth], -1, value);
				if (error>STATUS_NOT_READY)
					break;
				else if (error==STATUS_NOT_READY)
					AddLateEval(address, scope_address[scope_depth], curr, exp, source_file, LET_ABS_REF);
				CheckOutputCapacity(2);
				*curr++ = (char)value;
				*curr++ = (char)(value>>8);
				address+=2;
			}
			break;
		case AD_TEXT:		// text: add text within quotes
			// for now just copy the windows ascii. TODO: Convert to petscii.
			line.trim_whitespace();
			if (line[0]=='"') {
				++line;
				if (line.get_last()=='"')
					line.clip(1);
			}
			CheckOutputCapacity(line.get_len());
			memcpy(curr, line.get(), line.get_len());
			curr += line.get_len();
			address += line.get_len();
			break;
		case AD_MACRO: {	// macro: create an assembler macro
			strref from_here = contextStack.curr().code_segment + 
				strl_t(line.get()-contextStack.curr().code_segment.get());
			int block_start = from_here.find('{');
			if (block_start > 0) {
				strref block = (from_here + block_start).scoped_block_skip();
				error = AddMacro(strref(line.get(), strl_t(block.get()+block.get_len()+1-line.get())),
								 contextStack.curr().source_name, contextStack.curr().source_file);
				contextStack.curr().read_source +=
					strl_t(block.get()+block.get_len()+1-contextStack.curr().read_source.get());
			}
			break;
		}
		case AD_INCLUDE: {	// include: assemble another file in place
			line = line.between('"', '"');
			size_t size = 0;
			if (char *buffer = LoadText(line, size)) {
				loadedData.push_back(buffer);
				strref src(buffer, size);
				contextStack.push(line, src, src);
			}
			break;
		}
		case AD_INCBIN: {	// incbin: import binary data in place
			line = line.between('"', '"');
			strown<512> filename(line);
			size_t size = 0;
			if (char *buffer = LoadBinary(line, size)) {
				CheckOutputCapacity((unsigned int)size);
				memcpy(curr, buffer, size);
				free(buffer);
				curr += size;
				address += (unsigned int)size;
			}
			break;
		}
	}
	return error;
}

// Push an opcode to the output buffer
StatusCode Asm::AddOpcode(strref line, int group, int index, strref source_file)
{
	StatusCode error = STATUS_OK;
	int base_opcode = aMulAddGroup[group][1] + index * aMulAddGroup[group][0];
	strref expression;
	
	// Get the addressing mode and the expression it refers to
	AddressingMode addrMode = GetAddressMode(line,
		group==OPG_CC10&&index>=OPI_STX&&index<=OPI_LDX, error, expression);
	
	int value = 0;
	bool evalLater = false;
	if (expression) {
		error = EvalExpression(expression, address, scope_address[scope_depth], -1, value);
		if (error == STATUS_NOT_READY) {
			evalLater = true;
			error = STATUS_OK;
		}
		if (error != STATUS_OK)
			return error;
	}
	
	// check if address is in zero page range and should use a ZP mode instead of absolute
	if (!evalLater && value>=0 && value<0x100) {
		switch (addrMode) {
			case AM_ABSOLUTE:
				addrMode = AM_ZP;
				break;
			case AM_ABSOLUTE_X:
				addrMode = AM_ZP_X;
				break;
			default:
				break;
		}
	}
	
	CODE_ARG codeArg = CA_NONE;
	unsigned char opcode = base_opcode;
	
	// analyze addressing mode per mnemonic group
	switch (group) {
		case OPG_BRANCH:
			if (addrMode != AM_ABSOLUTE) {
				error = ERROR_INVALID_ADDRESSING_MODE_FOR_BRANCH;
				break;
			}
			codeArg = CA_BRANCH;
			break;
			
		case OPG_SUBROUT:
			if (index==1) {	// jsr
				if (addrMode != AM_ABSOLUTE)
					error = ERROR_INVALID_ADDRESSING_MODE_FOR_BRANCH;
				else
					codeArg = CA_TWO_BYTES;
			}
			break;
		case OPG_STACK:
		case OPG_FLAG:
		case OPG_TRANS:
			codeArg = CA_NONE;
			break;
		case OPG_CC00:
			// jump relative exception
			if (addrMode==AM_RELATIVE && index==OPI_JMP) {
				base_opcode += 0x20;
				addrMode = AM_ABSOLUTE;
			}
			if (addrMode>7 || (CC00Mask[index]&(1<<addrMode))==0)
				error = ERROR_BAD_ADDRESSING_MODE;
			else {
				opcode = base_opcode + CC00ModeAdd[addrMode];
				switch (addrMode) {
					case AM_ABSOLUTE:
					case AM_ABSOLUTE_Y:
					case AM_ABSOLUTE_X:
						codeArg = CA_TWO_BYTES;
						break;
					default:
						codeArg = CA_ONE_BYTE;
						break;
				}
			}
			break;

		case OPG_CC01:
			if (addrMode>7 || (addrMode==AM_IMMEDIATE && index==OPI_STA))
				error = ERROR_BAD_ADDRESSING_MODE;
			else {
				opcode = base_opcode + addrMode*4;
				switch (addrMode) {
					case AM_ABSOLUTE:
					case AM_ABSOLUTE_Y:
					case AM_ABSOLUTE_X:
						codeArg = CA_TWO_BYTES;
						break;
					default:
						codeArg = CA_ONE_BYTE;
						break;
				}
			}
			break;
		case OPG_CC10: {
			if (addrMode == AM_NONE || addrMode == AM_ACCUMULATOR) {
				if (index>=4)
					error = ERROR_BAD_ADDRESSING_MODE;
				else {
					opcode = base_opcode + 8;
					codeArg = CA_NONE;
				}
			} else {
				if (addrMode>7 || (CC10Mask[index]&(1<<addrMode))==0)
					error = ERROR_BAD_ADDRESSING_MODE;
				else {
					opcode = base_opcode + CC10ModeAdd[addrMode];
					switch (addrMode) {
						case AM_IMMEDIATE:
						case AM_ZP:
						case AM_ZP_X:
							codeArg = CA_ONE_BYTE;
							break;
						default:
							codeArg = CA_TWO_BYTES;
							break;
					}
				}
			}
			break;
		}
	}
	
	// Add the instruction and argument to the code
	if (error == STATUS_OK) {
		CheckOutputCapacity(4);
		switch (codeArg) {
			case CA_BRANCH:
				address += 2;
				if (evalLater)
					AddLateEval(address, scope_address[scope_depth], curr+1, expression, source_file, LET_BRANCH);
				else if (((int)value-(int)address)<-128 || ((int)value-(int)address)>127) {
					error = ERROR_BRANCH_OUT_OF_RANGE;
					break;
				}
				*curr++ = opcode;
				*curr++ = evalLater ? 0 : (unsigned char)((int)value-(int)address);
				break;
			case CA_ONE_BYTE:
				*curr++ = opcode;
				if (evalLater)
					AddLateEval(address, scope_address[scope_depth], curr, expression, source_file, LET_BYTE);
				*curr++ = (char)value;
				address += 2;
				break;
			case CA_TWO_BYTES:
				*curr++ = opcode;
				if (evalLater)
					AddLateEval(address, scope_address[scope_depth], curr, expression, source_file, LET_ABS_REF);
				*curr++ = (char)value;
				*curr++ = (char)(value>>8);
				address += 3;
				break;
			case CA_NONE:
				*curr++ = opcode;
				address++;
				break;
		}
	}
	return error;
}

// Compile in a macro
StatusCode Asm::BuildMacro(Macro &m, strref arg_list)
{
	strref macro_src = m.macro;
	strref params = macro_src[0]=='(' ? macro_src.scoped_block_skip() : strref();
	params.trim_whitespace();
	arg_list.trim_whitespace();
	macro_src.skip_whitespace();
	if (params) {
		arg_list = arg_list.scoped_block_skip();
		strref pchk = params;
		strref arg = arg_list;
		int dSize = 0;
		while (strref param = pchk.split_token_trim(',')) {
			strref a = arg.split_token_trim(',');
			if (param.get_len() < a.get_len()) {
				int count = macro_src.substr_case_count(param);
				dSize += count * ((int)a.get_len() - (int)param.get_len());
			}
		}
		int mac_size = macro_src.get_len() + dSize + 32;
		if (char *buffer = (char*)malloc(mac_size)) {
			loadedData.push_back(buffer);
			strovl macexp(buffer, mac_size);
			macexp.copy(macro_src);
			while (strref param = params.split_token_trim(',')) {
				strref a = arg_list.split_token_trim(',');
				macexp.replace(param, a);
			}
			contextStack.push(m.source_name, macexp.get_strref(), macexp.get_strref());
			FlushLocalLabels();
			return STATUS_OK;
		} else
			return ERROR_OUT_OF_MEMORY_FOR_MACRO_EXPANSION;
	}
	contextStack.push(m.source_name, m.source_file, macro_src);
	FlushLocalLabels();
	return STATUS_OK;
}

// Build a segment of code (file or macro)
StatusCode Asm::BuildSegment(OP_ID *pInstr, int numInstructions)
{
	StatusCode error = STATUS_OK;
	while (strref line = contextStack.curr().read_source.line()) {
		while (line) {
			strref line_start = line;
			line.skip_whitespace();
			line = line.before_or_full(';');
			line = line.before_or_full(c_comment);
			line.clip_trailing_whitespace();
			if (line[0]==':')	// some assemblers use a colon prefix to indicate macro usage
				++line;
			strref operation = line.split_range_trim(word_char_range, line[0]=='.' ? 1 : 0);
			// instructions and directives ignores leading periods, labels include them.
			strref label = operation;
			if (operation[0]=='.') {
				++operation;
				if (operation.same_str("label") || operation.same_str("const")) {	// skip '.label' directive
					operation = line.split_range_trim(word_char_range, line[0]=='.' ? 1 : 0);
					label = operation;
				}
			}
			if (!operation) {
				// scope open / close
				switch (line[0]) {
					case '{':
						if (scope_depth>=(MAX_SCOPE_DEPTH-1))
							error = ERROR_TOO_DEEP_SCOPE;
						else {
							scope_address[++scope_depth] = address;
							++line;
							line.skip_whitespace();
						}
						break;
					case '}':
						// check for late eval of anything with an end scope
						CheckLateEval(strref(), address);
						--scope_depth;
						if (scope_depth<0)
							error = ERROR_UNBALANCED_SCOPE_CLOSURE;
						++line;
						line.skip_whitespace();
						break;
				}
			} else  {
				// ignore leading period for instructions and directives - not for labels
				unsigned int op_hash = operation.fnv1a_lower();
				int op_idx = LookupOpCodeIndex(op_hash, pInstr, numInstructions);
				if (op_idx >= 0 && line[0]!=':') {
					if (pInstr[op_idx].type==OT_DIRECTIVE) {
						error = ApplyDirective((AssemblerDirective)pInstr[op_idx].index, line, contextStack.curr().source_file);
						line.clear();
					} else if (pInstr[op_idx].type==OT_MNEMONIC) {
						OP_ID &id = pInstr[op_idx];
						int group = id.group;
						int index = id.index;
						error = AddOpcode(line, group, index, contextStack.curr().source_file);
						line.clear();
					}
				} else if (line.get_first()=='=') {
					// assignment of label
					++line;
					line.trim_whitespace();
					int val = 0;
					StatusCode status = EvalExpression(line, address, scope_address[scope_depth], -1, val);
					if (status != STATUS_NOT_READY && status != STATUS_OK) {
						error = status;
						break;
					}
					
					Label *pLabel = AddLabel(label.fnv1a());
					pLabel->label_name = label;
					pLabel->expression = line;
					pLabel->evaluated = status==STATUS_OK;
					pLabel->value = val;
					pLabel->zero_page = pLabel->evaluated && val<0x100;
					pLabel->pc_relative = false;
					
					if (!pLabel->evaluated)
						AddLateEval(label, address, scope_address[scope_depth], line, LET_LABEL);
					else
						error = CheckLateEval(label);
					line.clear();
				} else {
					unsigned int nameHash = label.fnv1a();
					unsigned int macro = FindLabelIndex(nameHash, macros.getKeys(), macros.count());
					bool gotMacro = false;
					while (macro < macros.count() && nameHash==macros.getKey(macro)) {
						if (macros.getValue(macro).name.same_str_case(label)) {
							BuildMacro(macros.getValue(macro), line);
							gotMacro = true;
							line.clear();
							break;
						}
						macro++;
					}
					if (!gotMacro) {
						Label *pLabel = AddLabel(label.fnv1a());
						pLabel->label_name = label;
						pLabel->expression.clear();
						pLabel->value = address;
						pLabel->evaluated = true;
						pLabel->pc_relative = true;
						pLabel->zero_page = false;
						if (line[0]==':')
							++line;
						if (label[0]=='.' || label[0]=='@' || label[0]=='!' || label.get_last()=='$')
							MarkLabelLocal(label);
						else
							FlushLocalLabels();
						error = CheckLateEval(label);
					}
				}
			}
			if (error > STATUS_NOT_READY) {
				strown<512> errorText;
				errorText.sprintf("Error (%d): ", contextStack.curr().source_file.count_lines(line_start));
				errorText.append(aStatusStrings[error]);
				errorText.append(": \"");
				errorText.append(line_start.line());
				errorText.append("\"\n");
				errorText.c_str();
				fwrite(errorText.get(), errorText.get_len(), 1, stderr);
			}
			if (error > ERROR_STOP_PROCESSING_ON_HIGHER)
				break;
			// dealt with error, continue with next instruction
			error = STATUS_OK;
		}
	}
	if (error == STATUS_OK)
		error = CheckLateEval(strref(), address);
	return error;
}

// create an instruction table (mnemonic hash lookup + directives)
void Asm::Assemble(strref source, strref filename)
{
	OP_ID *pInstr = new OP_ID[100];
	int numInstructions = BuildInstructionTable(pInstr, strref(aInstr, sizeof(aInstr)-1), 100);

	StatusCode error = STATUS_OK;
	contextStack.push(filename, source, source);

	scope_address[scope_depth] = address;
	while (contextStack.has_work()) {
		BuildSegment(pInstr, numInstructions);
		contextStack.pop();
	}
	if (error == STATUS_OK) {
		error = CheckLateEval();
		if (error > STATUS_NOT_READY) {
			strown<512> errorText;
			errorText.copy("Error: ");
			errorText.append(aStatusStrings[error]);
			fwrite(errorText.get(), errorText.get_len(), 1, stderr);
		}
		for (std::vector<LateEval>::iterator i = lateEval.begin(); i!=lateEval.end(); ++i) {
			strown<512> errorText;
			int line = i->source_file.count_lines(i->expression);
			errorText.sprintf("Error (%d): ", line+1);
			errorText.append("Failed to evaluate \"");
			errorText.append(i->expression);
			if (line>=0) {
				errorText.append("\" : \"");
				errorText.append(i->source_file.get_line(line));
			}
			errorText.append("\"\n");
			fwrite(errorText.get(), errorText.get_len(), 1, stderr);
		}
	}

//	for (unsigned int i = 0; i<labels.count(); i++) {
//		printf("Label 0x%08x: " STRREF_FMT " = " STRREF_FMT " = 0x%04x\n", labels.getKey(i),
//			   STRREF_ARG(labels.getValue(i).label_name), STRREF_ARG(labels.getValue(i).expression), labels.getValue(i).value);
//	}

}

int main(int argc, char **argv)
{
	bool c64 = true;
	const char* source_filename=nullptr, *binary_out_name=nullptr;
	for (int a=1; a<argc; a++) {
		strref arg(argv[a]);
		if (arg.same_str("-c64"))
			c64 = true;
		else if (arg.same_str("-bin"))
			c64 = false;
		else if (!source_filename)
			source_filename = arg.get();
		else if (!binary_out_name)
			binary_out_name = arg.get();
	}

	if (!source_filename) {
		puts("Usage:\nAsm6502 <-c64 / -bin> filename.s code.prg\n * -c64: Include load address\n * -bin: Raw binary\n");
		return 0;
	}
	
	// Load source
	if (source_filename) {
		size_t size = 0;
		if (char *buffer = LoadText(source_filename, size)) {
			Asm assembler;
			assembler.Assemble(strref(buffer, size), strref(argv[1]));
			
			if (binary_out_name && assembler.curr > assembler.output) {
				if (FILE *f = fopen(binary_out_name, "wb")) {	// /Users/Carl-Henrik/Google Drive/code/c64/
					if (c64) {
						char addr[2] = { (char)assembler.load_address, (char)(assembler.load_address>>8) };
						fwrite(addr, 2, 1, f);
					}
					fwrite(assembler.output, assembler.curr-assembler.output, 1, f);
					fclose(f);
				}
			}
		
			// free some memory
			assembler.Cleanup();
		}
	}
    return 0;
}
