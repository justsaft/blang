
#include "types.hpp"
#include "gen_ir.hpp"
#include "common.hpp"
#include "compilation.hpp"

extern "C" {
#include "../3rd-party/nob.h"
#include "../3rd-party/stb_c_lexer.h"
}

extern Compilation c;


static std::array<std::tuple<const char*, int>, 2> int_variants = {
	std::make_tuple("i16", 2),
	std::make_tuple("i64", 8),
};

static std::map<Value_Type, std::tuple<const char*, int>> VT_to_LLVM = {
	{ Invalid_Value_Type, std::make_tuple("invalid", 0) },
	{ Dead, std::make_tuple("errortype", 0) },
	{ Uninitialized, std::make_tuple("uninitialized", 0) },
	{ Void, std::make_tuple("void", 0) },
	{ Int, std::make_tuple(nullptr, 0) },
	{ Float, std::make_tuple("f64", 8) },
	{ String, std::make_tuple("todostr", 0) },
	{ Pointer, std::make_tuple("todoptr", 0) },
};

inline void LLVM_IR::append_vt(Value_Type vt)
{
	append(std::get<const char*>(VT_to_LLVM.at(vt)));
	// If this fails with vt == Int then you forgot to run setup_ir_gen()
}

inline void LLVM_IR::append_align(Value_Type vt)
{
	append("align ");
	append(std::to_string(std::get<int>(VT_to_LLVM.at(vt))));
}

inline void LLVM_IR::append_vid(Variable_Id vid)
{
	if (vid == Invalid_Value_Type) {
		append_vt(Invalid_Value_Type);
	} else {
		append(std::to_string(vid));
	}
}

inline void LLVM_IR::append_attr_group(int group)
{
	if (group < 0) return;

	append(" #");
	append(std::to_string(group));
}

inline void LLVM_IR::nl(void)
{
	push_back('\n');
}



// ----------------------------
//
// Codegen
//
// ----------------------------


// Setup
// -----
void setup_ir_gen(void)
{
	VT_to_LLVM.at(Int) = int_variants.at(c.GetWordSize());
}


// file header
// -----------

void gen_file_info(LLVM_IR& ir, const char* file_name)
{
	ir += "source_filename = \"";
	ir.append(file_name);
	ir += "\"\n";
	ir += "target triple = \"";
	ir.append(c.target);
	ir += "\"\n\n";
}


// function bodies
// ---------------

void gen_all_func_decls(LLVM_IR& ir, const B_Function_Scope& scope)
{
	for (auto& function : scope) {
		ir += "$";
		ir.append(function.name);
		ir += " = comdat any\n";
	}
}

void gen_func_begin(LLVM_IR& ir, const B_Function& sym)
{
	ir += "\n";
	ir += "define dso_local ";
	ir.append_vt(Int);
	ir += " @";
	ir.append(sym.name);
	ir += "()";
	ir.append_attr_group(sym.attr_group);
	ir += " {\n";
}

void gen_func_end(LLVM_IR& ir)
{
	ir += "}\n\n";
}

/* void gen_all_func_bodies(LLVM_IR& ir, const B_Function_Scope& s)
{
	for (size_t i = 0; i < s.size(); ++i) {
		ir += s[i].ir;
	}
} */


// function calls
// --------------

// TODO: future: %3 = call i32 (ptr, ...) @printf(ptr noundef @.str.1, i64 noundef 1)
// TODO: future: add parameters

void gen_funccall(LLVM_IR& ir, Variable_Id retval_dest, const std::string& callee)
{
	// %4 = call i64 @print1()
	ir += "  %";
	ir.append_vid(retval_dest);
	ir += " = call ";
	ir.append_vt(Int);
	ir += " @";
	ir.append(callee);
	ir += "()\n";
}

void gen_funccall_extrn(LLVM_IR& ir, Variable_Id retval_dest, const std::string& callee)
{
	NOB_TODO("Extrn function calls");
	ir += "  %";
	ir.append_vid(retval_dest);
	ir += " = call ";
	ir.append_vt(Int);
	ir += " @";
	ir.append(callee);
	ir += "()\n";
}

void gen_funccall(LLVM_IR& ir, const std::string& callee)
{
	// call void @print1()
	ir += "  call ";
	ir.append_vt(Void);
	ir += " @";
	ir += callee;
	ir += "()\n";
}


// function parameters
// -------------------

void gen_func_parameter(LLVM_IR& ir, size_t vn)
{
	NOB_UNUSED(ir);
	NOB_UNUSED(vn);
	NOB_TODO("Implement generating function parameters");
	/* ir += ""; */
}


// auto keyword
// ------------

void gen_gvar_decl(LLVM_IR& ir, const B_Variable& v, const std::string& value)
{
	// @globvar2 = dso_local global i64 0, align 8
	ir += "@";
	ir.append(v.name);
	ir += " = dso_local global ";
	ir.append_vt(Int);
	ir += " ";
	ir.append(value);
	ir += ", ";
	ir.append_align(Int);
	ir.nl();
}

void gen_alloca(LLVM_IR& ir, Variable_Id id)
{
	ir += "  %";
	ir.append(std::to_string(id));
	ir += " = alloca ";
	ir.append_vt(Int);
	ir += ", ";
	ir.append_align(Int);
	ir.nl();
}

void gen_store_gvar_dest_ptrsrc(LLVM_IR& ir, const std::string& dest, const Variable_Id ptrsrc)
{
	ir += "  store ";
	ir.append_vt(Int);
	ir += " ";
	ir.append(dest);
	ir += ", ptr %";
	ir.append_vid(ptrsrc);
	ir += ", ";
	ir.append_align(Int);
	ir.nl();
}


// assignments
// -----------

void gen_load_gvar_to_lvar(LLVM_IR& ir, const Variable_Id dest, const std::string& src)
{
	//   %1 = load i64, ptr @globvar, align 8
	ir += "  %";
	ir.append_vid(dest);
	ir += " = load ";
	ir.append_vt(Int);
	ir += ", ptr @";
	ir.append(src);
	ir += ", ";
	ir.append_align(Int);
	ir.nl();
	// yields a pointer!
}

void gen_store_rval_to_gvar(LLVM_IR& ir, const std::string& dest, const std::string& value)
{
	// store i64 45, ptr @globvar2, align 8
	ir += "  store ";
	ir.append_vt(Int);
	ir += " ";
	ir.append(dest);
	ir += ", ptr @";
	ir.append(value);
	ir += ", ";
	ir.append_align(Int);
	ir.nl();
}

void gen_store_lval_to_gvar(LLVM_IR& ir, const std::string& dest, const Variable_Id src)
{
	// store i64 45, ptr @globvar2, align 8
	ir += "  store ";
	ir.append_vt(Int);
	ir += " %";
	ir.append_vid(src);
	ir += ", ptr @";
	ir.append(dest);
	ir += ", ";
	ir.append_align(Int);
	ir.nl();
}

void gen_store_gval_to_gvar(LLVM_IR& ir, const std::string& dest, const std::string& src)
{
	ir += "  store ";
	ir.append_vt(Int);
	ir += " @";
	ir.append(src);
	ir += ", ptr @";
	ir.append(dest);
	ir += ", ";
	ir.append_align(Int);
	ir.nl();
}

void gen_store_rval_to_lval(LLVM_IR& ir, const Variable_Id dest, const std::string& value)
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
	/* Reusing data storage?
	  store i64 1, ptr % 2, align 8
	  store i64 % 11, ptr % 2, align 8 */

	ir += "  store ";
	ir.append_vt(Int);
	ir += " ";
	ir.append(value);
	ir += ", ptr %";
	ir.append_vid(dest);
	ir += ", ";
	ir.append_align(Int);
	ir.nl();
}

void gen_store_lval_to_lval(LLVM_IR& ir, const Variable_Id dest, const Variable_Id src)
{
	// ir += "  %";
	// ir += " = store i64 %";
	ir += "  store ";
	ir.append_vt(Int);
	ir += " %";
	ir.append_vid(src);
	ir += ", ptr %";
	ir.append_vid(dest);
	ir += ", ";
	ir.append_align(Int);
	ir.nl();
}

void gen_load_lval_to_lval(LLVM_IR& ir, const Variable_Id dest, const Variable_Id src)
{
	ir += "  %";
	ir.append_vid(dest);
	ir += " = load ";
	ir.append_vt(Int);
	ir += ", ptr %";
	ir.append_vid(src);
	ir += ", ";
	ir.append_align(Int);
	ir.nl();
}

void gen_load_lvalptr_to_lval(LLVM_IR& ir, const Variable_Id dest, const Variable_Id src)
{
	// %1 = load i64* %2, align 8
	ir += "  %";
	ir.append_vid(dest);
	ir += " = load ";
	ir.append_vt(Int);
	ir += "* %";
	ir.append_vid(src);
	ir += ", ";
	ir.append_align(Int);
	ir.nl();
}


// return keyword
// --------------

void gen_return_keyword(LLVM_IR& ir)
{
	ir += "  ret ";
	ir.append_vt(Int);
	ir += " 0\n";
}

void gen_return_keyword_rvalue(LLVM_IR& ir, const std::string& n)
{
	ir += "  ret ";
	ir.append_vt(Int);
	ir += " ";
	ir.append(n);
	ir += "\n";
}

void gen_return_keyword_lvalue(LLVM_IR& ir, Variable_Id n)
{
	ir += "  ret ";
	ir.append_vt(Int);
	ir += " %";
	ir.append_vid(n);
	ir += "\n";
}


// Binary Ops
// ----------

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


void gen_binary_op(LLVM_IR& ir, const Variable_Id dest, const Variable_Id left, const Variable_Id right, B_Scope& sc, const Ops op)
{
	ir += "  %";
	ir.append_vid(dest);
	ir += " = ";

	Value_Type& dest_type = sc.GetVariableFromStack(dest).value_type;
	Value_Type vta = sc.GetVariableFromStack(left).value_type;
	Value_Type vtb = sc.GetVariableFromStack(right).value_type;

	switch (((bool)(vta == Int) << 3) +
			((bool)(vta == Float) << 2) +
			((bool)(vtb == Int) << 1) +
			((bool)(vtb == Float) << 0)) {

		case 0b1110:
		case 0b1111:
		case 0b0111:
		case 0b1100:
		case 0b0011: NOB_UNREACHABLE("Bogus amogus");

		case 0b0000:
		case 0b1000:
		case 0b0001: NOB_TODO("At least one is not an integer or float");

		case 0b0101:
		case 0b1010: dest_type = vta; break;

		case 0b0110:
		case 0b1001: switch (dest_type) {
			case Uninitialized: dest_type = vta; break;

			case Int:
			case Float: break;

			case Dead: NOB_UNREACHABLE("The dest value type should not be a dead value");
			default: NOB_TODO("Destination wasn't float, int or uninitialized");
		}
	}

	static std::array<const char*, 10> instr = {
		"add", "fadd",
		"sub", "fsub",
		"mul", "fmul",
		"sdiv", "fdiv",
		"srem", "frem", };

	bool is_float = dest_type == Float;

	switch (op) {
		case Plus: ir.append(instr[0 + (int)is_float]); break;
		case Minus: ir.append(instr[2 + (int)is_float]); break;
		case Mult: ir.append(instr[4 + (int)is_float]); break;
		case Div: ir.append(instr[6 + (int)is_float]); break;
		case Mod: ir.append(instr[8 + (int)is_float]); break;
		default: NOB_UNREACHABLE("Unknown operation");
	}

	ir += " ";
	ir.append_vt(dest_type);
	ir += " %";
	ir.append_vid(left);
	ir += ", %";
	ir.append_vid(right);
	ir += "\n";
}


void gen_unary_op(LLVM_IR& ir, const Variable_Id dest, const Variable_Id src, B_Scope& sc, const Ops op)
{
	Value_Type& dest_vt = sc.Variables().at(dest).value_type;
	dest_vt = src < 0 ? sc.UpstreamVariables()[-src].value_type : sc.Variables()[src].value_type;

	ir += "  %";
	ir.append_vid(dest);
	ir += " = ";

	switch (op) {
		case UnaryBoolean:
		case UnaryMinus: ir += "fneg "; break;
		default: NOB_UNREACHABLE("gen_unary_op: passed operation not handled by this function.");
	}

	switch (dest_vt) {
		case Float:
		case Int: ir.append_vt(dest_vt); break;
		default: NOB_UNREACHABLE("Unknown operation");
	}

	ir += " %";
	ir.append_vid(src);
	ir += "\n";
}


// Casting
// -------

void gen_downsize_cast(LLVM_IR& ir, Variable_Id dest, Variable_Id src)
{
	//%12 = trunc i64 %11 to i32
	ir += "  %";
	ir.append_vid(dest);
	ir += " = trunc i64 %";
	ir.append_vid(src);
	ir += " to i32\n";
}


// Attribute Groups
// ----------------

void gen_attr_group(LLVM_IR& ir, int group)
{
	ir.append("\nattributes #");
	ir.append(std::to_string(group));
	ir.append(" = { noinline nounwind optnone uwtable \"frame-pointer\"=\"all\" \"min-legal-vector-width\"=\"0\" \"no-trapping-math\"=\"true\" \"stack-protector-buffer-size\"=\"8\" \"tune-cpu\"=\"generic\" }\n");
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