
#ifndef _GEN_IR_HPP
#define _GEN_IR_HPP

#include "types.hpp"
extern Compilation compilation;


// gen_ir.cpp


// Setup
// -----
void setup_ir_gen(const Compilation& c);


// file header
// -----------
void gen_file_ir_info(LLVM_IR& ir, const char* target, const char* file_name);


// function bodies
// ---------------
void gen_all_func_decls(LLVM_IR& ir, const B_Function_Scope& s);
void gen_func_begin(LLVM_IR& ir, const B_Function& sym);
void gen_func_end(LLVM_IR& ir);


// function calls
// --------------
void gen_funccall(LLVM_IR& ir, const std::string& callee);
void gen_funccall(LLVM_IR& ir, Variable_Id retval_dest, const std::string& callee);
void gen_funccall_extrn(LLVM_IR& ir, Variable_Id retval_dest, const std::string& callee);


// function parameters
// -------------------
void gen_func_parameter(LLVM_IR& ir, size_t vn);


// auto keyword
// ------------
void gen_alloca(LLVM_IR& ir, Variable_Id vn);
void gen_store_gvar_dest_ptrsrc(LLVM_IR& ir, const std::string& dest, const Variable_Id ptrsrc);
void gen_gvar_decl(LLVM_IR& ir, const B_Variable& v, const std::string& value);


// assignments
// -----------
void gen_store_rval_to_gvar(LLVM_IR& ir, const std::string& dest, const std::string& value);
void gen_store_rval_to_lval(LLVM_IR& ir, const Variable_Id dest, const std::string& value);
void gen_store_lval_to_gvar(LLVM_IR& ir, const std::string& dest, const Variable_Id src);
void gen_store_gval_to_gvar(LLVM_IR& ir, const std::string& dest, const std::string& src);
void gen_store_lval_to_lval(LLVM_IR& ir, const Variable_Id dest, const Variable_Id src);
void gen_assignment_gvar_to_lvar(LLVM_IR& ir, const Variable_Id dest, const std::string& src);
void gen_load_lval_to_lval(LLVM_IR& ir, const Variable_Id dest, const Variable_Id src);
void gen_load_lvalptr_to_lval(LLVM_IR& ir, const Variable_Id dest, const Variable_Id src);


// return keyword
// --------------
void gen_return_keyword(LLVM_IR& ir);
void gen_return_keyword_rvalue(LLVM_IR& ir, const std::string& n);
void gen_return_keyword_lvalue(LLVM_IR& ir, Variable_Id n);


// Operations
// ----------

void gen_binary_op(LLVM_IR& ir, const Variable_Id dest, const Variable_Id left, const Variable_Id right, B_Variable_Scope& sc, const Ops op);
void gen_unary_op(LLVM_IR& ir, const Variable_Id dest, const Variable_Id src, B_Variable_Scope& sc, const Ops op);

// Casting
// -------

void gen_downsize_cast(LLVM_IR& ir, Variable_Id dest, Variable_Id src);


// Attribute Groups
// ----------------

void gen_attr_group(LLVM_IR& ir, int group);


#endif