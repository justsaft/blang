
#ifndef _TYPES_HPP
#define _TYPES_HPP


#include <stdint.h>
#include <array>
#include <vector>
#include <string>
#include <map>

#define local thread_local static

extern "C" {
#include "../3rd-party/stb_c_lexer.h"
}

enum IR_Output : uint8_t {
	DeleteIrAfterCompile,
	KeepIrAfterCompile,
	DontCompile,
};

enum Returns : uint8_t {
	Success,
	CompilationHadWarnings,
	ClangNonZeroExitcode,
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

typedef struct {
	const char* filepath;
	Returns state = Success;
	// LLVM_IR ir;
} B_File;

typedef std::vector<B_File> B_Files;

typedef struct {
	int in_file = -1,
		line = -1,
		line_offset = -1;
} B_ItemLocation;

typedef struct B_Variable {
	const char* name = nullptr;
	Value_Type value_type = Dead;
	int file;
	stb_lex_location location;
	LLVM_IR ir = ""; // TODO: remove so that it won't cause SSA to break
	bool global = false;
} B_Variable;

typedef struct B_Function {
	const char* name;
	int in_file = -1,
		definitions = 0,
		calls = 0,
		attr_group = 0;
	stb_lex_location location = { -1, -1 };
} B_Function;

typedef struct {
	int definitions = 0,
		calls = 0,
		attr_group = 0;
	std::vector<B_ItemLocation> locations;
} B_FunctionInfo;

typedef struct {
	bool stop = false;
	bool has_entry = false;
	bool wants_executable = true;

	int current_file = 0, errors = 0, warnings = 0;

	enum /* struct */ Modes : bool {
		Historical,
		Modernized, // == B-Ext
	};

	enum /* struct */ WordSize : bool {
		SixteenBit,
		SixtyfourBit,
	};

	Modes langfeatures = Historical;
	WordSize word_size = SixteenBit;
	IR_Output irout = DeleteIrAfterCompile;
	Returns state;
} Compilation;

typedef std::vector<B_Variable> B_Variable_Scope;
typedef std::vector<B_Function> B_Function_Scope;

typedef std::map<std::string, std::vector<B_FunctionInfo>> B_FunctionScope_New;


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

inline std::array<const char*, 4> LLVM_Known_Target_Triples = {
	"x86_64-pc-windows-msvc",
	"x86_64-pc-windows-gnu",
	"x86_64-unknown-linux-gnu",
	"x86_64-pc-linux-gnu"
};

#endif