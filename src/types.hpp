
#include <stdint.h>
#include <vector>
#include <tuple>
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

//                 name         value    where     file
typedef std::tuple<const char*, int64_t, uint64_t, uint8_t> B_Variable;
typedef std::vector<B_Variable> B_Stack;

typedef struct {
	enum {
		NAME,
		VALUE,
		WHERE,
		FILE,
	};
} StackVarIdx;

//                 name         params   where     file
typedef std::tuple<const char*, uint8_t, uint64_t, uint8_t> B_Function;
typedef std::vector<B_Function> B_Function_Scope;

typedef struct {
	enum {
		NAME,
		PARAMS_AMOUNT,
		WHERE,
		FILE,
	};
} FunctionSymbolIdx;

typedef std::vector<const char*> CStrings;


typedef struct /*_Target*/ {
	enum struct Bitness {
		x86,
		x86_64,
		None,
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

	int bitness;
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