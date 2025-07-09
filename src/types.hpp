
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
	IsThisYours,
	VariableRedefinition,
	FunctionRedefinition,
	ExternRedefinition,
	ErrorReadInput,
	ErrorWriteOutput,
	InvalidTargetTriple,
	FunctionNotFound,
	UnusedVariable,
	ExpectedSemicolon,
	ErrorWritingIrOfFile,
	VariableIsShadowed,
	TotalAmountOfReturns,
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
	Invalid_Keyword,
};

enum Value_Type {
	Invalid_Value_Type = -1,
	Dead,
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
typedef ssize_t Variable_Id;

constexpr int CLEX_BUFFER_DEFAULT_SIZE = 0x1000;
constexpr int MAX_ERRORS_BEFORE_STOP = 15;

typedef struct {
	const char* name = nullptr;
	Value_Type value_type = Dead;
	size_t file;
	stb_lex_location location = { 0, 0 };
	IR ir = "";
} B_Variable;

typedef struct {
	const char* name;
	size_t filei;
	stb_lex_location location;
} B_Function;

typedef std::vector<B_Variable> B_Variable_Scope;
typedef std::vector<B_Function> B_Function_Scope;


// POS Types
typedef struct B_Scope {
	static B_Function_Scope& functions; // For all files
	static B_Function_Scope& extern_functions; // For the whole file
	stb_lex_location lex_location;
	B_Variable_Scope upstreamv;
	B_Variable_Scope localv;

	B_Scope()
	{
		localv.push_back(B_Variable { });
	}

	B_Scope(const B_Scope& upstr)
	{ // uuuuhhh
		upstreamv.reserve(upstr.localv.size() + upstr.upstreamv.size());
		upstreamv.insert(upstreamv.end(), upstr.localv.begin(), upstr.localv.end());
		upstreamv.insert(upstreamv.end(), upstr.upstreamv.begin(), upstr.upstreamv.end());

		localv.push_back(B_Variable { });
	}

	~B_Scope() = default;
} B_Scope;

/* typedef struct Compiler {
	size_t file;
	std::vector<char>& clex_buffer;
}; */

enum Reserved_Variables : Variable_Id {
	Bool_True = -2,
	Bool_False,
	INVALID_VARIABLE,
	FIRST_VARIABLE_ID = 1,
};

constexpr Function_Id INVALID_FUNCTION = 0;
constexpr Function_Id FIRST_FUNCTION_ID = 1;

#endif