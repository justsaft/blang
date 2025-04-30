
#include <array>

extern "C" {
#include "../3rd-party/nob.h"
}

#include "types.hpp"
#include "gen_ir.hpp"

static std::array<const char*, 4> valid_triples = {
	"x86_64-pc-windows-msvc",
	"x86_64-pc-windows-gnu",
	"x86_64-unknown-linux-gnu",
	"x86_64-pc-linux-gnu"
};

size_t find_function(const char* name, const B_Function_Scope& scope);
size_t find_variable(const char* name, const B_Stack& scope);

/* const char* get_target_triple(int i)
{
	return valid_triples[i];
} */

// Function to verify target triple
bool verify_target_triple(const std::string& target)
{
	if (target.empty()) {
		nob_log(NOB_ERROR, "Null target triple");
		//exit(Exitcodes::INVALID_TARGET_TRIPLE);
		return false;
	}

	for (uint8_t i = 0; i < valid_triples.size(); ++i) {
		if (strcmp(target.data(), valid_triples[i]) == 0) {
			return true;
		}
	}

	nob_log(NOB_ERROR, "Invalid target triple %s", target.c_str());
	return false;
}

void gen_basic_ir_info(std::string& ir, const std::string& target, const char* file_name)
{
	/*if (!verify_target_triple(target)) {
		exit(INVALID_TARGET_TRIPLE);
	}*/

	ir += "source_filename = \"";
	ir += file_name;
	ir += "\"\n";
	ir += "target triple = \"";
	ir += target;
	ir += "\"\n";
}

void gen_var_decl(std::string& ir, const B_Variable& svar)
{
	NOB_UNUSED(ir);
	NOB_UNUSED(svar);
	NOB_TODO("Implement var declarations");
}

void gen_func_decl(std::string& ir, const B_Function_Scope& s)
{
	// Iterate over the vector of FunctionSymbol tuples
	for (const auto& symbol : s) {
		const char* name = std::get<0>(symbol); // Extract the name from the tuple
		ir += "$";
		ir += name;
		ir += " = comdat any\n";
	}
}

// TODO: future: add function parameters
// TODO: future: save function parameters in the IR
// TODO: future: save/return the current end of ir and use it to insert stuff above the function definition

void gen_function_def(std::string& ir, const B_Function& sym)
{
	const char* name = std::get<FunctionSymbolIdx::NAME>(sym);
	ir += "\n; Function Attrs: noinline nounwind optnone\n";
	ir += "define dso_local i64 @";
	ir += name;
	ir += "() {\n";
}

void gen_function_def_end(std::string& ir)
{
	ir += "}\n\n";
}

void gen_keyword(std::string& ir, uint8_t kw_id, const B_Stack& sv)
{
	switch (kw_id) {
	case Return:
		ir += "  ret i64 ";
		ir += std::to_string(std::get<StackVarIdx::VALUE>(sv[find_variable("___return", sv)]));
		ir += "\n";
		break;

	default:
		NOB_TODO("kw_id not found or implemented");
		break;
	}
}

// input given to LLVM:
/*int main()
{
	const int32_t test = 1;
	printf("Test string %d", test);
	return 0;
}*/
// Also headers were included in the .c file and got linked into the llvm-IR

// LLVM-IR output:
/*; Function Attrs: noinline nounwind optnone
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  store i32 0, ptr %1, align 4
  store i32 1, ptr %2, align 4
  %3 = call i32 (ptr, ...) @printf(ptr noundef @"??_C@_0P@EEPKMKGD@Test?5string?5?$CFd?$AA@", i32 noundef 1) <--- printf call
  ret i32 0
}
*/

// Output for printf()
/*; Function Attrs: noinline nounwind optnone
define linkonce_odr dso_local i32 @printf(ptr noundef %0, ...) #0 comdat {
  %2 = alloca ptr, align 4
  %3 = alloca i32, align 4
  %4 = alloca ptr, align 4
  store ptr %0, ptr %2, align 4
  call void @llvm.va_start.p0(ptr %4)
  %5 = load ptr, ptr %4, align 4
  %6 = load ptr, ptr %2, align 4
  %7 = call ptr @__acrt_iob_func(i32 noundef 1)
  %8 = call i32 @_vfprintf_l(ptr noundef %7, ptr noundef %6, ptr noundef null, ptr noundef %5)
  store i32 %8, ptr %3, align 4
  call void @llvm.va_end.p0(ptr %4)
  %9 = load i32, ptr %3, align 4
  ret i32 %9
}*/

// TODO: future: @__local_stdio_printf_options._OptionsStorage = internal global i64 0, align 8 <--- function parameters

// TODO: future:  string literals
/*
$"??_C@_0P@EEPKMKGD@Test?5string?5?$CFd?$AA@" = comdat any <--- cstring declaration
@"??_C@_0P@EEPKMKGD@Test?5string?5?$CFd?$AA@" = linkonce_odr dso_local unnamed_addr constant [15 x i8] c"Test string %d\00", comdat, align 1 <--- cstring definition/assignment
*/

// TODO: future: @__acrt_iob_func = external dso_local i8* (i32) <--- printf() function

// TODO: future: figure out what this means
/*
!llvm.module.flags = !{!0, !1, !2, !3}
!llvm.ident = !{!4}

!0 = !{i32 1, !"NumRegisterParameters", i32 0}
!1 = !{i32 1, !"wchar_size", i32 2}
!2 = !{i32 7, !"frame-pointer", i32 2}
!3 = !{i32 1, !"MaxTLSAlign", i32 65536}
!4 = !{!"clang version 19.1.1"}
*/