
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
#include <unordered_map>
#include <tuple>

extern "C" {
#include "../3rd-party/stb_c_lexer.h"
}

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
	FileNotCompiledYet,
};

enum Ops : uint8_t {
	NoOp,
	Equals,
	UnaryMinus, // -
	UnaryBoolean, // !
	BitwiseOr, // |
	BitwiseAnd, // &
	EqualsEquals, // ==
	NotEquals, // !=
	LessThan, // <
	LessOrEqualThan, // <=
	GreaterThan, // >
	GreaterOrEqualThan, // >=
	ShiftLeft, // <<
	ShiftRight, // >>
	Minus, // -
	Plus, // +
	Mod, // %
	Mult, // *
	Div, // /
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

enum Keywords : uint8_t {
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

typedef ssize_t Function_Id;
typedef ssize_t Variable_Id;

struct LLVM_IR : std::string {
	inline void append_vt(Value_Type vt);
	inline void append_align(Value_Type vt);
	inline void append_vid(Variable_Id vid);
	inline void append_attr_group(int group);
	inline void nl(void); // New line
};

struct B_File {
	const char* filepath;
	Returns state = FileNotCompiledYet;
};

typedef std::vector<B_File> B_Files;

//typedef struct B_ItemLocation {
//	int in_file = -1,
//		line = -1,
//		line_offset = -1;
//};

struct B_Variable {
	const char* name = nullptr;
	Value_Type value_type = Dead;
	int in_file;
	stb_lex_location location;
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

typedef std::vector<B_Variable> B_Variable_Scope;
typedef std::vector<B_Function> B_Function_Scope;
//typedef std::map<std::string, std::vector<B_FunctionInfo>> B_FunctionScope_New;

struct B_Scope {
public:
	static B_Function_Scope functions; // For all scopes
	static B_Function_Scope extern_functions; // For all scopes

public:
	const B_Variable_Scope& UpstreamVariables(void) const
	{
		return upstreamv;
	}

	B_Variable_Scope& Variables(void)
	{
		return localv;
	}

	B_Variable_Scope GluedVariables(void)
	{
		B_Variable_Scope temp;
		temp.resize(localv.size() + upstreamv.size());
		temp.insert(temp.end(), upstreamv.begin(), upstreamv.end());
		temp.insert(temp.end(), localv.begin(), localv.end());
		return temp;
	}

	Variable_Id GetTopOfStack(void) const
	{
		return localv.size() + upstreamv.size() - global_variables;
	}

	B_Variable& GetVariableFromStack(Variable_Id id)
	{
		ssize_t uv = upstreamv.size() - global_variables;
		return id >= uv ? localv[-uv + id] : upstreamv[id];
	}

	Variable_Id LocalId2StackId(Variable_Id localid) const
	{
		return upstreamv.size() - global_variables + localid;
	}

	Variable_Id UpstreamId2StackId(Variable_Id l) const
	{
		return l - global_variables;
	}

	ssize_t GetAmountGlobalVars(void) const
	{
		return global_scope ? localv.size() : global_variables;
	}

	bool IsGlobalScope(void) const
	{
		return global_scope;
	}

public:
	B_Scope()
	{
		localv.reserve(6);
		functions.reserve(6);
		extern_functions.reserve(6);
	}

	B_Scope(const B_Scope& upstr)
	{
		upstreamv.resize(upstr.localv.size() + upstr.upstreamv.size());
		upstreamv.insert(upstreamv.end(), upstr.upstreamv.begin(), upstr.upstreamv.end());
		upstreamv.insert(upstreamv.end(), upstr.localv.begin(), upstr.localv.end());

		localv.reserve(6);

		global_scope = false;

		if (upstr.global_scope) {
			localv.push_back(B_Variable {
				.value_type = Invalid_Value_Type,
				.in_file = -1,
				.location = { -1, -1 },
				.global = true });

			global_variables = upstr.localv.size();
		}
	}

	~B_Scope() = default;

private:
	static ssize_t global_variables;
	bool global_scope = true;
	B_Variable_Scope upstreamv;
	B_Variable_Scope localv;
};

enum Reserved_Variable_Ids : Variable_Id {
	Boolean_Literal_True = -256,
	Boolean_Literal_False,
	INVALID_VARIABLE = 0,
	FIRST_VARIABLE_ID,
};

constexpr Function_Id INVALID_FUNCTION = -1;
constexpr Function_Id FIRST_FUNCTION_ID = 0;
constexpr int MAX_ERRORS_BEFORE_STOP = 15;
constexpr int WHERES_YOUR_GOD_NOW = INT32_MAX;

#endif