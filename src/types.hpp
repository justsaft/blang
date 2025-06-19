
#ifndef _TYPES_HPP
#define _TYPES_HPP


#include <stdint.h>
#include <vector>
#include <string>

#define local thread_local static

extern "C" {
#include "../3rd-party/stb_c_lexer.h"
}

enum IR_Output {
	DeleteIrAfterCompile,
	KeepIrAfterCompile,
	DontCompile,
};

enum Returns {
	Success,
	CompilationHadWarnings,
	ClangNonZeroExitcode,
	EverythingCouldBeWrong,
	NoFilesGiven,
	NoArgumentsGiven,
	FileEmpty,
	FunctionEmpty,
	UnexpectedArguments,
	UnexpectedEndOfFile,
	InvalidSyntax,
	UnimplementedSyntax,
	IsThisYours,
	VariableRedefinition,
	FunctionRedefinition,
	ExternRedefinition,
	ErrorReadInput,
	ErrorWriteOutput,
	InvalidTargetTriple,
	ReachedUnreachable,
	NoScopeStarted,
	FunctionNotFound,
	UnusedVariable,
	UnsupportedTarget,
	TotalAmountOfReturns,
	ExpectedSemicolon,
	ErrorWritingIrOfFile,
};

enum Ops {
	/*
	unary ::=
		-
		!

	binary ::=
		|
		&
		==
		!=
		<
		<=
		>
		>=
		<<
		>>
		-
		+
		%
		*
		/
*/
	NoOp,
	InvertMinus,
	InvertBoolean,
	Pipe,
	BitwiseOr,
	BitwiseAnd,
	Equals,
	NotEquals,
	LessThan,
	LessOrEqualThan,
	GreaterThan,
	GreaterOrEqualThan,
	ShiftLeft,
	ShiftRight,
	Minus,
	Plus,
	Mod,
	Mult,
	Div,
	// Non-B ops:
	Increment,
	Decrement,
	ShiftLeftEquals,
	ShiftRightEquals,
	// Total amount:
	OpsTotalAmount,
};

enum Keyword {
	NoKeyword,
	Return,
	Auto,
	Extrn,
	While,
	If,
	Else,
	Switch,
	Case,
	Goto,
	Invalid,
};

enum Value_Type {
	UnknownIntAssumed,
	Uninitialized,
	Int,
	Float,
	String,
	Pointer,
};

// this is cringe but works well enough
typedef std::vector<const char*> CStrings;
typedef std::string IR;
typedef uint32_t Function_Id;
typedef size_t Variable_Id;

constexpr Variable_Id LLVM_LOAD_OFFSET = 1;
constexpr int CLEX_BUFFER_SIZE = 0x1000;

typedef struct {
	const char* name;
	Value_Type value_type;
	size_t file;
	stb_lex_location location;
	IR ir;
	bool inline_var;
} B_Variable;

typedef struct {
	const char* name;
	size_t filei;
	stb_lex_location location;
} B_Function;

/* typedef struct {
	B_Variable& b_var;
	size_t llvm_ptr;
	size_t llvm_load;
	mutable IR decl;
} LLVM_Variable; */

/* typedef struct {
	B_Variable b_var;
	mutable IR decl;
} LLVM_gVariable; */

typedef std::vector<B_Variable> B_Variable_Scope;
typedef std::vector<B_Function> B_Function_Scope;


// POS Types
typedef struct B_Scope {
	static B_Function_Scope& externf;
	stb_lex_location lex_location;
	B_Variable_Scope upstreamv;
	B_Function_Scope upstreamf;
	B_Variable_Scope localv;
	B_Function_Scope localf;


	B_Scope() = default;

	B_Scope(const B_Scope& upstr)
	{ // uuuuhhh
		upstreamf.reserve(upstr.localf.size() + upstr.upstreamf.size());
		upstreamv.reserve(upstr.localv.size() + upstr.upstreamv.size());
		upstreamf.insert(upstreamf.end(), upstr.localf.begin(), upstr.localf.end());
		upstreamf.insert(upstreamf.end(), upstr.upstreamf.begin(), upstr.upstreamf.end());
		upstreamv.insert(upstreamv.end(), upstr.localv.begin(), upstr.localv.end());
		upstreamv.insert(upstreamv.end(), upstr.upstreamv.begin(), upstr.upstreamv.end());
	}

	~B_Scope() = default;
} B_Scope;



#endif