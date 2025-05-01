
#include <stdint.h>
#include <vector>
#include <string>

enum Exitcodes {
	SUCCESS,
	EVERYTHING_COULD_BE_WRONG,
	NO_FILES,
	NO_ARGUMENTS,
	UNEXPECTED_ARGUMENTS,
	UNEXPECTED_EOF,
	INVALID_SYNTAX,
	UNSUPPORTED_SYNTAX,
	UNSUPPORTED_DATA,
	IS_THIS_YOURS,
	VARIABLE_REDEF,
	FUNCTION_REDEF,
	EXTRN_REDEF,
	ERROR_READ_INPUT,
	ERROR_WRITE_OUTPUT,
	INVALID_TARGET_TRIPLE,
};

enum InlineOperation {
	Unsuccessful = -1,
	NotAnOperation,
	Plus,
	Minus,
	Multiply,
	Divide,
	BinaryAnd,
	BinaryOr,
};

enum /* struct */ Value_Type {
	Int,
	Float,
	String,
	AutoVar,
};

typedef struct {
	const char* name;
	mutable std::string value;
	int line_number;
	int char_offset;
	uint8_t file;
	Value_Type value_type;
} B_Variable;

typedef std::vector<B_Variable> B_Variable_Scope;

typedef struct {
	const char* name;
	uint8_t amount_params;
	int line_number;
	int char_offset;
	uint8_t filei;
} B_Function;

typedef std::vector<B_Function> B_Function_Scope;

typedef std::vector<const char*> CStrings;


typedef struct {
	enum struct Arch {
		x86,
		x86_64,
	};

	enum struct Vendor {
		PC,
		Unknown,
	};

	enum struct OS {
		Windows,
		Linux,
		Unknown,
	};

	enum struct ABI {
		MSVC,
		GLIBC,
	};

	int arch;
	int vendor;
	int os;
	int abi;
} Target;


struct Datatypes{ // TODO: To be implemented
	enum {         // This should determine what the compiler will tell
		INT,       //       LLVM what the type should become
		UINT,      //       Internal data storage data types will be hindered once
		FLOAT32,   //       floats and/or strings become supported and must change
		FLOAT64,
		CSTRING,   // TODO: enum structs ??????
	};
};