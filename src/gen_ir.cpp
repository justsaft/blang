
#include "types.hpp"
#include "gen_ir.hpp"
#include "common.hpp"

extern "C" {
#include "../3rd-party/nob.h"
#include "../3rd-party/stb_c_lexer.h"
}

/* static std::array<const char*, 4> valid_triples = {
	"x86_64-pc-windows-msvc",
	"x86_64-pc-windows-gnu",
	"x86_64-unknown-linux-gnu",
	"x86_64-pc-linux-gnu"
}; */

Function_Id find_function(const char* name, const B_Function_Scope& scope);
Variable_Id find_variable(const char* name, const B_Variable_Scope& scope);



// ----------------------------
//
// Codegen
//
// ----------------------------


// file header
// -----------

void gen_file_ir_info(IR& ir, const char* target, const char* file_name)
{
	ir += "source_filename = \"";
	ir += file_name;
	ir += "\"\n";
	ir += "target triple = \"";
	ir += target;
	ir += "\"\n\n";
}


// variables
// ---------

/* void gen_var_decl(IR& ir, const B_Variable& svar)
{
	NOB_UNUSED(ir);
	NOB_UNUSED(svar);
	NOB_TODO("Implement var declarations");
} */

void gen_all_var_decls(IR& ir, const B_Variable_Scope& s, CStrings& input_files)
{

	for (size_t i = 1; i < s.size(); ++i)
	{
		if (s[i].value_type == Uninitialized)
		{
			nob_log(NOB_WARNING, "%s:%d:%d: Unused variable: `%s`.", input_files[s[i].file], s[i].location.line_number, s[i].location.line_offset, s[i].name);
			compilation_error(UnusedVariable);
		}
		ir += s[i].ir;
	}
}


// function bodies
// ---------------

void gen_all_func_decls(IR& ir, const B_Function_Scope& s)
{
	for (auto& symbol : s)
	{
		ir += "$";
		ir += symbol.name;
		ir += " = comdat any\n";
	}
}

void gen_func_begin(IR& ir, const B_Function& sym)
{
	ir += "\n; Function Attrs: noinline nounwind optnone\n";
	ir += "define dso_local i64 @";
	ir += sym.name;
	ir += "() {\n";
	/* ir += "  %0 = alloca i64, align 8\n"; */
	// TODO: future: add function parameters
	// TODO: future: save function parameters in the IR
}

void gen_func_end(IR& ir)
{
	ir += "}\n\n";
}

/* void gen_all_func_bodies(IR& ir, const B_Function_Scope& s)
{
	for (size_t i = 0; i < s.size(); ++i) {
		ir += s[i].ir;
	}
} */


// function calls
// --------------

// TODO: future: %3 = call i32 (ptr, ...) @printf(ptr noundef @.str.1, i64 noundef 1)
// TODO: future: add parameters

void gen_funccall(IR& ir, size_t retval_dest, const std::string& callee)
{
	// %4 = call i64 @print1()
	ir += "  %";
	ir.append(std::to_string(retval_dest));
	ir += " = call i64 @";
	ir.append(callee);
	ir += "()\n";
}

void gen_funccall_extrn(IR& ir, size_t retval_dest, const std::string& callee)
{
	NOB_TODO("Extrn function calls");
	ir += "  %";
	ir.append(std::to_string(retval_dest));
	ir += " = call i32 @";
	ir.append(callee);
	ir += "()\n";
}

void gen_funccall(IR& ir, const std::string& callee)
{
	// call void @print1()
	ir += "  call void @";
	ir += callee;
	ir += "()\n";
}


// function parameters
// -------------------

void gen_func_parameter(IR& ir, size_t vn)
{
	NOB_UNUSED(ir);
	NOB_UNUSED(vn);
	NOB_TODO("Implement generating function parameters");
	/* ir += ""; */
}


// auto keyword
// ------------

void gen_auto_keyword_gvar(IR& ir, const B_Variable& v, const std::string& value)
{
	// @globvar2 = dso_local global i64 0, align 8
	ir += "@";
	ir += v.name;
	ir += " = dso_local global i64 ";
	ir += value;
	ir += ", align 8\n";
}

void gen_auto_keyword_decl(IR& ir, Variable_Id id)
{
	ir += "  %";
	ir.append(std::to_string(id));
	ir += " = alloca i64";
	ir += ", align 8\n";
}

void gen_auto_keyword_def(IR& ir, Variable_Id id, const std::string& value)
{
	ir += "  store i64 ";
	ir.append(value);
	ir += ", ptr %";
	ir.append(std::to_string(id));
	ir += ", align 8\n";
}


// assignments
// -----------

void gen_assignment_gvar_to_lvar(IR& ir, const Variable_Id dest, const std::string& src)
{
	//   %1 = load i64, ptr @globvar, align 8
	ir += "  %";
	ir.append(std::to_string(dest));
	ir += " = load i64, ptr @";
	ir.append(src);
	ir += ", align 8\n";
	// yields a pointer!
}

void gen_assignment_rval_to_gvar(IR& ir, const std::string& dest, const std::string& value)
{
	// store i64 45, ptr @globvar2, align 8
	ir += "  store i64 ";
	ir.append(value);
	ir += ", ptr @";
	ir.append(dest);
	ir += ", align 8\n";
}

void gen_assignment_lval_to_gvar(IR& ir, const std::string& dest, const Variable_Id src)
{
	// store i64 45, ptr @globvar2, align 8
	ir += "  store i64 %";
	ir.append(std::to_string(src));
	ir += ", ptr @";
	ir.append(dest);
	ir += ", align 8\n";
}

void gen_assignment_rval_to_lval(IR& ir, const Variable_Id dest, const std::string& value)
{
	/*
	%ptr = alloca i64                               ; yields ptr -- var decl
	store i64 69, ptr %ptr                           ; yields void
	%val = load i64, ptr %ptr                       ; yields i32:val = i32 3
	*/

	/* ir += "  store i64 ";
	ir.append(value);
	ir += ", ptr %";
	ir += dest;
	ir += ", align 8\n";

	ir += "  %";
	ir += dest + 1;
	ir += " = load i64, ptr %";
	ir += dest;
	ir += ", align 8\n"; */
	/* Reassigning is possible:
	  store i64 1, ptr % 2, align 8
	  store i64 % 11, ptr % 2, align 8 */

	ir += "  store i64 ";
	ir.append(value);
	ir += ", ptr %";
	ir.append(std::to_string(dest));
	ir += ", align 8\n";
}

void gen_assignment_lval_to_lval(IR& ir, const Variable_Id dest, const Variable_Id src)
{
	// %1 = load i64* %2, align 8
	ir += "  %";
	ir.append(std::to_string(dest));
	ir += " = load i64* %";
	ir.append(std::to_string(src));
	ir += ", align 8\n";
}


// return keyword
// --------------

void gen_return_keyword(IR& ir)
{
	ir += "  ret i64 0\n";
}

void gen_return_keyword_rvalue(IR& ir, const std::string& n)
{
	ir += "  ret i64 ";
	ir.append(n);
	ir += "\n";
}

void gen_return_keyword_lvalue(IR& ir, Variable_Id n)
{
	ir += "  ret i64 %";
	ir.append(std::to_string(n));
	ir += "\n";
}


// Load ptr
// --------

void gen_load(IR& ir, Variable_Id dest, Variable_Id src)
{
	ir += "  %";
	ir.append(std::to_string(dest));
	ir += " = load i64, ptr %";
	ir.append(std::to_string(src));
	ir += ", align 8\n";
}


// Plus op
// -------

/*
  %7 = load i64, ptr %4, align 8
  %8 = load i64, ptr %3, align 8
  %9 = add i64 %7, %8

  %10 = load i64, ptr %5, align 8
  %11 = load i64, ptr %6, align 8
  %12 = mul i64 %10, %11

  %13 = call i64 @print1()
  %14 = udiv i64 %12, %13
  %15 = sub i64 %9, %14 // last op into result

  store i64 %15, ptr %2, align 8 // result
 */

 // lvalue = lvalue + lvalue
void gen_plus_op(IR& ir, Variable_Id dest, Variable_Id left, Variable_Id right)
{
	// add
	ir += "  %";
	ir.append(std::to_string(dest));
	ir += " = add i64 %";
	ir.append(std::to_string(left));
	ir += ", %";
	ir.append(std::to_string(right));
	ir += "\n";
}

// lvalue = lvalue + rvalue
void gen_plus_op(IR& ir, Variable_Id dest, Variable_Id left, std::string& right)
{

	// add
	ir += "  %";
	ir.append(std::to_string(dest));
	ir += " = add i64 %";
	ir.append(std::to_string(left));
	ir += ", ";
	ir.append(right);
	ir += "\n";
}

// lvalue = rvalue + rvalue
void gen_plus_op(IR& ir, Variable_Id dest, std::string& left, std::string& right)
{
	// add
	ir += "  %";
	ir.append(std::to_string(dest));
	ir += " = add i64 ";
	ir.append(left);
	ir += ", ";
	ir.append(right);
	ir += "\n";
}


// Minus op
// --------

void gen_minus_op(IR& ir, Variable_Id dest, Variable_Id left, Variable_Id right)
{
	// add
	ir += "  %";
	ir.append(std::to_string(dest));
	ir += " = sub i64 %";
	ir.append(std::to_string(left));
	ir += ", %";
	ir.append(std::to_string(right));
	ir += "\n";
}

void gen_minus_op(IR& ir, Variable_Id dest, Variable_Id left, std::string& right)
{
	// add
	ir += "  %";
	ir.append(std::to_string(dest));
	ir += " = sub i64 %";
	ir.append(std::to_string(left));
	ir += ", ";
	ir.append(right);
	ir += "\n";
}

void gen_minus_op(IR& ir, Variable_Id dest, std::string& left, std::string& right)
{
	// add
	ir += "  %";
	ir.append(std::to_string(dest));
	ir += " = sub i64 ";
	ir.append(left);
	ir += ", ";
	ir.append(right);
	ir += "\n";
}


// Multiplication op
// -----------------

void gen_mul_op(IR& ir, Variable_Id dest, Variable_Id left, Variable_Id right)
{
	// %12 = mul i64 %10, %11
	ir += "  %";
	ir.append(std::to_string(dest));
	ir += " = mul i64 ";
	ir.append(std::to_string(left));
	ir += ", %";
	ir.append(std::to_string(right));
	ir += "\n";
}

void gen_mul_op(IR& ir, Variable_Id dest, Variable_Id left, std::string& right)
{
	// add
	ir += "  %";
	ir.append(std::to_string(dest));
	ir += " = mul i64 %";
	ir.append(std::to_string(left));
	ir += ", ";
	ir.append(right);
	ir += "\n";
}

void gen_mul_op(IR& ir, Variable_Id dest, std::string& left, std::string& right)
{
	// add
	ir += "  %";
	ir.append(std::to_string(dest));
	ir += " = mul i64 ";
	ir.append(left);
	ir += ", ";
	ir.append(right);
	ir += "\n";
}


// Division op
// -----------

void gen_udiv_op(IR& ir, Variable_Id dest, Variable_Id left, Variable_Id right)
{
	// %12 = mul i64 %10, %11
	ir += "  %";
	ir.append(std::to_string(dest));
	ir += " = udiv i64 ";
	ir.append(std::to_string(left));
	ir += ", %";
	ir.append(std::to_string(right));
	ir += "\n";
}

void gen_udiv_op(IR& ir, Variable_Id dest, Variable_Id left, std::string& right)
{
	// add
	ir += "  %";
	ir.append(std::to_string(dest));
	ir += " = udiv i64 %";
	ir.append(std::to_string(left));
	ir += ", ";
	ir.append(right);
	ir += "\n";
}

void gen_udiv_op(IR& ir, Variable_Id dest, std::string& left, std::string& right)
{
	// add
	ir += "  %";
	ir.append(std::to_string(dest));
	ir += " = udiv i64 ";
	ir.append(left);
	ir += ", ";
	ir.append(right);
	ir += "\n";
}

/* %17 = trunc i64 %16 to i32
  ret i32 %17 */

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
  /*
  attributes #0 = { alwaysinline alignstack=4 } <--- may exist above the function ir and is applied to functions with #0
  declare i32 @printf()



  ; Function Attrs: noinline nounwind optnone
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