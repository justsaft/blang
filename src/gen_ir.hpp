
#ifndef _GEN_IR_HPP
#define _GEN_IR_HPP


// gen_ir.cpp


// file header
// -----------
void gen_file_ir_info(IR& ir, const char* target, const char* file_name);


// variables
// ---------
void gen_all_var_decls(IR& ir, const B_Variable_Scope& s, CStrings& input_files);


// function bodies
// ---------------
void gen_all_func_decls(IR& ir, const B_Function_Scope& s);
void gen_func_begin(IR& ir, const B_Function& sym);
void gen_func_end(IR& ir);


// function calls
// --------------
void gen_funccall(IR& ir, const std::string& callee);
void gen_funccall(IR& ir, size_t retval_dest, const std::string& callee);
void gen_funccall_extrn(IR& ir, size_t retval_dest, const std::string& callee);


// function parameters
// -------------------
void gen_func_parameter(IR& ir, size_t vn);


// auto keyword
// ------------
void gen_auto_keyword_decl(IR& ir, Variable_Id vn);
void gen_auto_keyword_def(IR& ir, Variable_Id vn, const std::string& value);
void gen_auto_keyword_gvar(IR& ir, const B_Variable& v, const std::string& value);


// assignments
// -----------
void gen_assignment_rval_to_gvar(IR& ir, const std::string& dest, const std::string& value);
void gen_assignment_rval_to_lval(IR& ir, const Variable_Id dest, const std::string& value);
void gen_assignment_gvar_to_lvar(IR& ir, const Variable_Id dest, const std::string& src);
void gen_assignment_lval_to_gvar(IR& ir, const std::string& dest, const Variable_Id src);
void gen_assignment_lval_to_lval(IR& ir, const Variable_Id dest, const Variable_Id src);


// return keyword
// --------------
void gen_return_keyword(IR& ir);
void gen_return_keyword_rvalue(IR& ir, const std::string& n);
void gen_return_keyword_lvalue(IR& ir, Variable_Id n);


// Load ptr
// --------

void gen_load(IR& ir, Variable_Id dest, Variable_Id src);


// Plus op
// -------

void gen_plus_op(IR& ir, Variable_Id dest, Variable_Id left, Variable_Id right);
void gen_plus_op(IR& ir, Variable_Id dest, Variable_Id left, std::string& right);
void gen_plus_op(IR& ir, Variable_Id dest, std::string& left, std::string& right);


// Minus op
// --------

void gen_minus_op(IR& ir, Variable_Id dest, Variable_Id left, Variable_Id right);
void gen_minus_op(IR& ir, Variable_Id dest, Variable_Id left, std::string& right);
void gen_minus_op(IR& ir, Variable_Id dest, std::string& left, std::string& right);


// Multiplication op
// -----------------
void gen_mul_op(IR& ir, Variable_Id dest, Variable_Id left, Variable_Id right);
void gen_mul_op(IR& ir, Variable_Id dest, Variable_Id left, std::string& right);
void gen_mul_op(IR& ir, Variable_Id dest, std::string& left, std::string& right);


// Division op
// -----------

void gen_udiv_op(IR& ir, Variable_Id dest, Variable_Id left, Variable_Id right);
void gen_udiv_op(IR& ir, Variable_Id dest, Variable_Id left, std::string& right);
void gen_udiv_op(IR& ir, Variable_Id dest, std::string& left, std::string& right);




#endif