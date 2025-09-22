
#ifndef _TYPES_HPP
#define _TYPES_HPP

#ifdef _WIN32
#include <basetsd.h>
typedef SSIZE_T ssize_t;
#endif

#include <stddef.h>
#include <stdint.h>
#include <array>
#include <vector>
#include <string>
#include <map>

#define local thread_local static

extern "C" {
#include "../3rd-party/stb_c_lexer.h"
}

enum Backend : uint8_t {
	Backend_LLC,
	Backend_CLANG,
	TotalAmountOfBackends,
};

enum IR_Output : uint8_t {
	DeleteIrAfterCompile,
	KeepIrAfterCompile,
	DontCompile,
};

enum LangMode : bool {
	Historical,
	Modernized, // == B-Ext
};

enum WordSize : bool {
	SixteenBit,
	SixtyfourBit,
};

enum Returns : uint8_t {
	Success,
	CompilationHadWarnings,
	BackendNonZeroExitcode,
	EverythingCouldBeWrong,
	NoFilesGiven,
	NoFunctions,
	NoArgumentsGiven,
	FileEmpty,
	FunctionEmpty,
	EntryPointEmpty,
	UnexpectedArguments,
	UnexpectedEndOfFile,
	InvalidSyntax,
	IsThisYours,
	VariableRedefinition,
	FunctionRedefinition,
	ErrorReadInput,
	ErrorWriteOutput,
	InvalidTargetTriple,
	FunctionNeverCalled,
	FunctionNeverDefined,
	UnusedVariable,
	ExpectedSemicolon,
	ErrorWritingIrOfFile,
	VariableIsShadowed,
	NestedFunction,
	UnsupportedTarget,
	NoEntryPoint,
	MultipleEntryPoints,
	LangFeatureUnavailable,
	TotalAmountOfReturns,
};

enum Ops : uint8_t {
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
	UnaryMinus,
	UnaryBoolean,
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
	MinusEquals,
	PlusEquals,
	MultEquals,
	DivEquals,
	Increment,
	Decrement,
	ShiftLeftEquals,
	ShiftRightEquals,
	// Total amount:
	OpsTotalAmount,
};

enum Keyword : uint8_t {
	NotKeyword,
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

enum Value_Type : uint8_t {
	Invalid_Value_Type,
	Dead,
	Uninitialized,
	Void,
	String,
	Pointer,
	Int,
	Float,
};

// this is cringe but works well enough
typedef std::vector<const char*> CStrings;

typedef std::string LLVM_IR;
typedef ssize_t Function_Id;
typedef ssize_t Variable_Id;

constexpr int CLEX_BUFFER_DEFAULT_SIZE = 0x1000;
constexpr int MAX_ERRORS_BEFORE_STOP = 15;
constexpr int WHERES_YOUR_GOD_NOW = INT32_MAX;

struct B_File {
	const char* filepath;
	Returns state = Success;
};

typedef std::vector<B_File> B_Files;

//typedef struct B_ItemLocation {
//	int in_file = -1,
//		line = -1,
//		line_offset = -1;
//};

//typedef struct Scope {
//	int depth = 0;
//};

struct B_Variable {
	const char* name = nullptr;
	Value_Type value_type = Dead;
	int in_file = -1;
	stb_lex_location location = { -1, -1 };
	LLVM_IR ir = ""; // TODO: remove so I won't break SSA again
	bool global = false;
};

struct B_Function {
	const char* name;
	int in_file = -1,
		definitions = 0,
		calls = 0,
		attr_group = 0;
	stb_lex_location location = { -1, -1 };
};

//struct B_FunctionInfo {
//	int definitions = 0,
//		calls = 0,
//		attr_group = 0;
//	std::vector<B_ItemLocation> locations;
//};

struct Compilation {
public:
	bool stop = false,
		has_entry = false,
		wants_executable = true;

	int current_file = 0,
		errors = 0,
		warnings = 0;

	Returns state = Success;

public:
	LangMode GetLangMode(void) const
	{
		return lang_mode;
	}

	WordSize GetWordSize(void) const
	{
		return word_size;
	}

	IR_Output GetIROutput(void) const
	{
		return irout;
	}

	bool IsLangMode(LangMode lm) const
	{
		return lang_mode == lm;
	}

	bool IsWordSize(WordSize ws) const
	{
		return word_size == ws;
	}

	bool IsIROutput(IR_Output iro) const
	{
		return irout == iro;
	}

protected:
	friend void parse_cli_arguments(int, char**, std::string&, std::string&, B_Files&, Compilation&);

	void SetLangMode(LangMode lm)
	{
		lang_mode = lm;
	}

	void SetWordSize(WordSize ws)
	{
		word_size = ws;
	}

	void SetIROutput(IR_Output iro)
	{
		irout = iro;
	}

private:
	LangMode lang_mode = Historical;
	WordSize word_size = SixteenBit;
	IR_Output irout = DeleteIrAfterCompile;
};

typedef std::vector<B_Variable> B_Variable_Scope;
typedef std::vector<B_Function> B_Function_Scope;
//typedef std::map<std::string, std::vector<B_FunctionInfo>> B_FunctionScope_New;


// POS Types
typedef struct B_Scope {
	static B_Function_Scope& functions; // For all files
	static B_Function_Scope& extern_functions; // For a file
	B_Variable_Scope upstreamv;
	B_Variable_Scope localv;

	B_Scope()
	{
		localv.reserve(2);
		localv.push_back(B_Variable { });
	}

	B_Scope(const B_Scope& upstr)
	{ // uuuuhhh
		localv.reserve(2);

		upstreamv.resize(upstr.localv.size() + upstr.upstreamv.size());
		upstreamv.insert(upstreamv.end(), upstr.upstreamv.begin(), upstr.upstreamv.end());
		upstreamv.insert(upstreamv.end(), upstr.localv.begin(), upstr.localv.end());

		localv.push_back(B_Variable { });
	}

	~B_Scope() = default;
} B_Scope;

enum Reserved_Variable_Ids : Variable_Id {
	Special_Expr_True = -256,
	Special_Expr_False,
	INVALID_VARIABLE = 0,
	FIRST_VARIABLE_ID,
};

constexpr Function_Id INVALID_FUNCTION = -1;
constexpr Function_Id FIRST_FUNCTION_ID = 0;


#endif