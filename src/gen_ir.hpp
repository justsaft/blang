
// gen_ir.cpp
void gen_basic_ir_info(std::string& ir, const std::string& target, const char* file_name);
void gen_var_decl(std::string& ir, const B_Variable& svar);
void gen_func_decl(std::string& ir, const B_Function_Scope& s);
void gen_function_def(std::string& ir, const B_Function& sym);
void gen_function_def_end(std::string& ir);
void gen_keyword(std::string& ir, uint8_t statement, const B_Stack& sv);
bool verify_target_triple(const std::string& target);

enum Keywords {
	Return,
	Extrn,
	Auto,
};