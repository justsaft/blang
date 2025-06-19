
#include <stdio.h>

#include "state.hpp"
#include "types.hpp"
#include "gen_ir.hpp"
#include "common.hpp"

constexpr int MAX_ERRORS_BEFORE_STOP = 15;

extern "C"
{
#include "../3rd-party/nob.h"
#include "../3rd-party/stb_c_lexer.h"

#include "output.h"
	bool write_ll_file(const char* original_file, const char* ir, size_t ir_size);
}

// states.cpp
bool get_state(States& st);
bool get_file_state(States& st);
void update_state(States& st, const char* sym, const char* file_name, uint8_t fut);

// Driver
bool dispatch_clang(const CStrings& original_file, const std::string& output_file);
std::string get_target_triple_clang(void);

// Parsing
uint8_t parse_file(size_t file, B_Scope& global, IR& file_ir);
//      Note: gvars and funcs must carry across files
uint8_t parse_scope(size_t file, B_Scope& scope, stb_lexer& l, IR& ir, bool is_function_scope);
uint8_t parse_line(size_t file, B_Scope&, stb_lexer&, IR&, bool is_global_scope);
//      Note: Takes in the scope which is constructed and stored outside

// Inlining
void parse_post_primary_expr(stb_lexer& l, IR& ir, B_Scope& sc, size_t file, bool is_global_scope, const char* primary);
bool check_for_inline_ops(stb_lexer&);
uint16_t count_inline_ops(stb_lexer&);
Variable_Id parse_inline_ops(stb_lexer& l, IR& ir, B_Scope& sc, const size_t file);
Variable_Id make_inline_var(B_Variable_Scope& scope, const stb_lex_location& lo);

// Parse functions
uint8_t parse_function_definition(size_t file, B_Scope&, stb_lexer&, IR&, const char* name);
uint8_t parse_function_call(size_t file, B_Scope&, stb_lexer&, IR&, const char* name, const bool force_retvalgen = false);


// Parse keywords
Keyword parse_out_keyword(const char* k);
uint8_t parse_auto_keyword(stb_lexer& l, IR& ir, B_Scope& scope, const bool is_gvar, const size_t file);
uint8_t parse_extrn_keyword(stb_lexer&, const stb_lex_location&, B_Function_Scope& extrns, size_t file);
uint8_t parse_return_keyword(stb_lexer& l, std::string& ir, B_Scope& sc, const size_t file);

// Declaration
uint8_t variable_declaration(const char* name, B_Variable_Scope&, const size_t file, const stb_lex_location&);
uint8_t variable_declaration(const char* name, B_Variable_Scope&, const size_t file, const stb_lex_location&);
uint8_t variable_assignment(stb_lexer& l, B_Variable& v, B_Scope& scope, IR& ir, const bool is_gvar, const size_t file);

// Tokens
long get_next_token(stb_lexer& l);
bool get_and_expect_token(stb_lexer&, long token);
bool expect_token(stb_lexer& l, long token);

// Find & Redefinitions
Function_Id find_function(const char* name, const B_Function_Scope& scope);
Variable_Id find_variable(const char* name, const B_Variable_Scope& scope);
bool detect_symbol_redef(const B_Function& sym, const B_Function_Scope& scope, size_t file);
bool detect_var_redef(const B_Variable& sym, const B_Variable_Scope& scope, size_t file);

// Data Storage
States st;
local B_Function_Scope externs;
B_Function_Scope& B_Scope::externf = externs;
local CStrings input_files;
local std::string target;
local bool stop_compilation = true;
/* void stop_on_unsupported_target(std::string target); */

// Lexer Util (clex_util.cpp)
bool get_and_expect_semicolon(stb_lexer& l, const char* filename, const bool advance_lexer = true);
void unexpected_eof(const char* hint);
void get_lexer_location(stb_lexer& l, stb_lex_location& lo);
void step_lexer(stb_lexer& l);

// Cli.cpp
void parse_cli_arguments(int argc, char** argv, std::string& target_override, std::string& output_override, CStrings& input_files);


int main(int argc, char** argv)
{
	std::string target_override;
	std::string output_override;
	parse_cli_arguments(argc, argv, target_override, output_override, input_files);

	if (input_files.size() == 0) {
		nob_log(NOB_ERROR, "No input files were provided. Specify at least one `.b` file.");
		Compilation_error(NoFilesGiven);
		return NoFilesGiven;
	}

	if (target_override.empty()) {
		target.append(get_target_triple_clang());
	}
	else {
		target.append(target_override);
	}

	nob_log(NOB_INFO, "Compiling for %s on %s", target.c_str(), target.c_str());
	/* stop_on_unsupported_target(target); */

	B_Scope global_scope;

	for (uint8_t filei = 0; filei < input_files.size(); ++filei/* , clex_buf.clear(), clex_is.count = 0 */) { // For each file
		uint8_t retval = 0;
		std::string file_ir;

		retval = parse_file(filei, global_scope, file_ir);
		// parse_file hosts the lexer, buffers, checks and parsing until file end

		// Once there are no more tokens...
		if (!get_file_state(st)) {
			Compilation_error(EverythingCouldBeWrong);
		}
		else if (stop_compilation) {
			return retval;
		}
		else if (!write_ll_file(input_files[filei], file_ir.data(), file_ir.size() - 1)) {
			nob_log(NOB_WARNING, "Couldn't generate IR for file %s", input_files[filei]);
			Compilation_error(ErrorWritingIrOfFile);
		}
		else if (input_files.size() == 1) {
			if (!dispatch_clang(input_files, output_override)) {
				nob_log(NOB_ERROR, "Error when compiling ir of %s", input_files[0]);
				Compilation_error(ClangNonZeroExitcode);
			}
		}
	}

	if (input_files.size() > 1 && !stop_compilation) {
		if (!dispatch_clang(input_files, output_override)) {
			nob_log(NOB_ERROR, "Error when compiling");
			Compilation_error(ClangNonZeroExitcode);
		}
	}
	else if (input_files.size() == 0) {
		NOB_UNREACHABLE("Unreachable reached: No files given.");
	}

	return Success;
}

uint8_t parse_file(size_t file, B_Scope& global, IR& file_ir)
{
	// Lexer input stream
	static Nob_String_Builder clex_is = { nullptr, 0, 0 };
	// Make static to workaround memory leaking what would be the old buffer
	clex_is.count = 0;

	if (nob_read_entire_file(input_files[file], &clex_is)) {
		nob_log(NOB_INFO, "Compiling %s", input_files[file]);
	}
	else {
		nob_log(NOB_ERROR, "Could not read file %s", input_files[file]);
		Compilation_error(ErrorReadInput);
		return ErrorReadInput;
	}

	// Lexer buffer
	std::vector<char> clex_buf;
	clex_buf.resize(CLEX_BUFFER_SIZE);

	// Lexer
	stb_lexer lexer;
	stb_c_lexer_init(&lexer, clex_is.items, clex_is.items + clex_is.count, clex_buf.data(), clex_buf.size());

	// First step of the lexer and check if file is empty
	if (!stb_c_lexer_get_token(&lexer)) {
		nob_log(NOB_ERROR, "File %s is empty.", input_files[file]);
		Compilation_error(FileEmpty);
	}

	// Sanity check
	switch (lexer.token) {
	case CLEX_eof:
		Compilation_error(FileEmpty);
		return FileEmpty;

	case CLEX_parse_error:
		nob_log(NOB_ERROR, "CLEX parse error: there's probably an issue with the buffer");
		Compilation_error(EverythingCouldBeWrong);
		return EverythingCouldBeWrong; // Shutup the compiler

	default:
		break;
	}

	global.externf.clear();

	gen_file_ir_info(file_ir, target.data(), input_files[file]);
	return parse_scope(file, global, lexer, file_ir, false);
}

uint8_t parse_scope(size_t file, B_Scope& scope, stb_lexer& l, IR& ir, bool is_function_scope)
{
	local uint32_t scope_depth = 0;

	while ((l.token != CLEX_eof) && !is_function_scope) switch (l.token) {
	case ';':
		step_lexer(l);
		break; // See what the next iteration beholds

	case CLEX_eof:
		NOB_TODO("Does this need handling?");

	case CLEX_parse_error:
		NOB_TODO("Does this need handling?");

	case '}': // Scope ended
		if (scope_depth == 0) {
			nob_log(NOB_ERROR, "Invalid syntax: there is no scope to close.");
			Compilation_error(NoScopeStarted);
			return NoScopeStarted;
		}
		step_lexer(l);
		//  Lexer pos: <next>

		--scope_depth;
		return Success;

	case '{': // scope begins
		// Lexer pos: {

		step_lexer(l);
		// Lexer pos: <first token of scope>

		if (l.token == '}') {
			step_lexer(l);
			return Success;
		}
		else {
			++scope_depth;
			{
				B_Scope next(scope);

				if (parse_scope(file, next, l, ir, is_function_scope) == Success) {
					break; // See what the next iteration beholds
				}
			}
		}
		NOB_TODO("Handle gracefully");

		/* step_lexer(l);
		// Lexer pos: <next>
		return Success; // break out of the switch */

	default:
		parse_line(file, scope, l, ir, !is_function_scope && (scope_depth == 0));
		break; // break out of the switch
	}

	return Success;
}

uint8_t parse_line(size_t file, B_Scope& sc, stb_lexer& l, IR& ir, bool is_global_scope)
{
	get_lexer_location(l, sc.lex_location);
	const char* primary = l.string;
	uint8_t retval = Success;

	switch (parse_out_keyword(primary)) {
	case Extrn:
		retval = parse_extrn_keyword(l, sc.lex_location, sc.externf, file);
		break;

	case Auto:
		retval = parse_auto_keyword(l, ir, sc, is_global_scope, file);
		break;

	case Return:
		if (is_global_scope) {
			nob_log(NOB_ERROR, "%s:%d:%d: Syntax error: keyword `return` not valid in global scope.", input_files[file], sc.lex_location.line_number, sc.lex_location.line_offset);
			Compilation_error(InvalidSyntax);
			retval = InvalidSyntax;
		}
		else {
			retval = parse_return_keyword(l, ir, sc, file);
		}
		break;

	case NoKeyword: // lhs / rhs situation: could be var assignment, function call, >>=, etc.
		step_lexer(l);
		if (is_global_scope) {
			if (l.token == '(') {
				parse_function_definition(file, sc, l, ir, primary);
				break; // -> semicolon next
			}
			else {
				if (l.token == '=') {
					step_lexer(l);
				}

				switch (l.token) {
				case ';':
					variable_declaration(primary, sc.localv, file, sc.lex_location);
					break; // -> semicolon next

				case CLEX_dqstring:
					NOB_TODO("Implement Strings");

				case CLEX_floatlit:
					NOB_TODO("Implement Floats");

				case CLEX_intlit:
				case CLEX_id:
					parse_post_primary_expr(l, ir, sc, file, is_global_scope, primary);
					break; // -> semicolon next

				default:
					NOB_UNREACHABLE("Unexpected token when trying to assign variable in global scope.");
				}
			}
		}

		parse_inline_ops(l, ir, sc, file);
		break; // -> semicolon next

	default:
		NOB_TODO("Keyword not yet implemented!");
	}

	get_and_expect_semicolon(l, input_files[file]);
	return retval;
}

void parse_post_primary_expr(stb_lexer& l, IR& ir, B_Scope& sc, size_t file, bool is_global_scope, const char* primary)
{
	Variable_Id src = parse_inline_ops(l, ir, sc, file);
	if (is_global_scope) {
		/* gen_assignment_lval_to_gvar(ir, primary, src); */
		gen_assignment_rval_to_gvar(ir, primary, l.token == CLEX_id ? l.string : l.token == CLEX_intlit ? std::to_string(l.int_number) : l.token == CLEX_floatlit ? std::to_string(l.real_number) : l.string);
	}
	else {
		Variable_Id dest = find_variable(primary, sc.localv);
		gen_assignment_lval_to_lval(ir, dest, src);
	}
}

Keyword parse_out_keyword(const char* k)
{
	if (strcmp(k, "extrn") == 0) {
		return Extrn;
	}
	else if (strcmp(k, "auto") == 0) {
		return Auto;
	}
	else if (strcmp(k, "return") == 0) {
		return Return;
	}
	else if (strcmp(k, "switch") == 0) {
		return Switch;
	}
	else if (strcmp(k, "case") == 0) {
		return Case;
	}
	else if (strcmp(k, "if") == 0) {
		return If;
	}
	else if (strcmp(k, "else") == 0) {
		return Else;
	}
	else if (strcmp(k, "while") == 0) {
		return While;
	}
	else if (strcmp(k, "goto") == 0) {
		return Goto;
	}
	else {
		return NoKeyword;
	}
}

bool detect_symbol_redef(const B_Function& s, const B_Function_Scope& scope, size_t file)
{
	for (auto& i : scope) {
		if (strcmp(i.name, s.name) == 0) {
			nob_log(NOB_ERROR, "%s:%d:%d: Variable redefinition: attempted to redefine %s", input_files[file], s.location.line_number, s.location.line_offset, s.name);
			nob_log(NOB_ERROR, "%s:%d:%d: <--- %s is first defined here", input_files[i.filei], i.location.line_number, i.location.line_offset, i.name);
			return 0; // Returns 0 if exists
		}
	}
	return 1; // Returns 1 if doesn't exist
}

bool detect_var_redef(const B_Variable& s, const B_Variable_Scope& scope, size_t file)
{
	for (auto& i : scope) {
		if (strcmp(i.name, s.name) == 0) {
			nob_log(NOB_ERROR, "%s:%d:%d: Variable redefinition: attempted to redefine %s", input_files[file], s.location.line_number, s.location.line_offset, s.name);
			nob_log(NOB_ERROR, "%s:%d:%d: <--- %s is first defined here", input_files[i.file], i.location.line_number, i.location.line_offset, i.name);
			return 0; // Returns 0 if it's a redefinition
		}
	}
	return 1; // Returns 1 if it's NOT a redefinition
}

uint8_t variable_declaration(const char* primary, B_Variable_Scope& vsc, const size_t file, const stb_lex_location& lo)
{
	B_Variable var;
	var.name = primary;
	var.location = lo;
	var.file = file;
	var.value_type = Uninitialized;
	Variable_Id id = vsc.size();

	if (primary != nullptr)	if (!detect_var_redef(var, vsc, file)) {
		Compilation_error(VariableRedefinition);
		return VariableRedefinition;
	}

	gen_auto_keyword_decl(var.ir, id);
	gen_load(var.ir, id + 1, id);
	vsc.push_back(var);
	return Success;
}

uint8_t variable_declaration(const char* primary, B_Variable_Scope& vsc, const size_t file, const stb_lex_location& lo, B_Variable& var)
{
	var.name = primary;
	var.location = lo;
	var.file = file;
	var.value_type = Uninitialized;
	Variable_Id id = vsc.size();

	if (primary != nullptr)	if (!detect_var_redef(var, vsc, file)) {
		Compilation_error(VariableRedefinition);
		return VariableRedefinition;
	}

	gen_auto_keyword_decl(var.ir, id);
	gen_load(var.ir, id + 1, id);
	return Success;
}

uint8_t variable_assignment(stb_lexer& l, B_Variable& v, B_Scope& scope, IR& ir, const bool is_gvar, const size_t file)
{
	// auto variable = asd + 1;
	// Lexer pos: variable

	const char* varname = l.string;

	switch (get_next_token(l)) {
		// Lexer pos: =
	case ';':
		nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: expected `=` after variable name `%s`", input_files[file], scope.lex_location.line_number, scope.lex_location.line_offset, v.name);
		Compilation_error(InvalidSyntax);
		return InvalidSyntax;

	case '=':
		step_lexer(l);
		// Lexer pos: asd
		switch (l.token) {
		case CLEX_dqstring:
			NOB_TODO("Implement Strings");

		case CLEX_floatlit:
			NOB_TODO("Implement Floats");

		case CLEX_id:
		case CLEX_intlit:
			{
				parse_post_primary_expr(l, ir, scope, file, is_gvar, varname);
				get_and_expect_semicolon(l, input_files[file]);
				return Success;
			}

		default:
			break;
		}
		// Fall through

	default:
		// Syntax error:
		nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: expected `=` after variable name `%s` but got `%s`", input_files[file], scope.lex_location.line_number, scope.lex_location.line_offset, v.name, l.token > 255 ? std::to_string((long)l.token).c_str() : std::to_string((char)l.token).c_str());
		Compilation_error(InvalidSyntax);
		return InvalidSyntax;
	}
	return Success;
}

uint8_t parse_auto_keyword(stb_lexer& l, IR& ir, B_Scope& scope, const bool is_gvar, const size_t file)
{
	// auto variable = 69;
	// Lexer pos: auto

	step_lexer(l);
	// Lexer pos: variable

	if (l.token != CLEX_id) {
		nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: expected name of variable.", input_files[file], scope.lex_location.line_number, scope.lex_location.line_offset);
		Compilation_error(InvalidSyntax);
		return InvalidSyntax;
	}

	B_Variable v;
	uint8_t retval = Success;

	retval = variable_declaration(l.string, scope.localv, file, scope.lex_location);
	retval = variable_assignment(l, v, scope, ir, is_gvar, file);

	return retval;
}

uint8_t parse_extrn_keyword(stb_lexer& l, const stb_lex_location& lo, B_Function_Scope& extrns, size_t file)
{
	if (!get_and_expect_token(l, CLEX_id)) {
		nob_log(NOB_ERROR, "Syntax error: expected a name after `extrn`");
		Compilation_error(InvalidSyntax);
	}

	B_Function e;
	e.name = l.string;
	e.location = lo;

	detect_symbol_redef(e, extrns, file);
	extrns.push_back(e);
	return Success;
}

uint8_t parse_return_keyword(stb_lexer& l, std::string& ir, B_Scope& sc, const size_t file)
{
	// return abc;
	// Lexer pos: return
	/* Inline_Ops inlo; */

	if (get_and_expect_token(l, ';')) // Lexer pos: abc;
	{ // always returns 0
		gen_return_keyword(ir);
	}
	else if (expect_token(l, CLEX_intlit)) { // rvalue
		if (check_for_inline_ops(l)) {
			Variable_Id temp = make_inline_var(sc.localv, sc.lex_location);
			parse_inline_ops(l, ir, sc, file);
			get_and_expect_semicolon(l, input_files[file]);
			gen_return_keyword_lvalue(ir, temp);
		}
		else {
			get_and_expect_semicolon(l, input_files[file]);
			gen_return_keyword_rvalue(ir, std::to_string(l.int_number));
		}
	}
	else if (expect_token(l, CLEX_id)) { // lvalue
		Variable_Id invalid = sc.localv.size();
		Variable_Id vid = parse_inline_ops(l, ir, sc, file);
		assert(vid != invalid);

		if (sc.localv[vid].value_type != Int) {
			NOB_TODO("Implement other types");
		}
		gen_return_keyword_lvalue(ir, vid);
		get_and_expect_semicolon(l, input_files[file]);
	}
	else if (expect_token(l, CLEX_floatlit)) { // float literal
		nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: cannot return float literals.", input_files[file], sc.lex_location.line_number, sc.lex_location.line_offset);
		Compilation_error(InvalidSyntax);
		return InvalidSyntax;
	}
	else { // unexpected token
		nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: unexpected token after return keyword", input_files[file], sc.lex_location.line_number, sc.lex_location.line_offset);
		Compilation_error(InvalidSyntax);
		return InvalidSyntax;
	}

	return Success;
}

bool check_for_inline_ops(stb_lexer& l)
{
	return (bool)count_inline_ops(l);
}

uint16_t count_inline_ops(stb_lexer& la)
{
	// Lexer pos: intlit
	stb_lexer lb = la;
	uint16_t r = 0;

	// Lexer2 pos: , or ; -> ends loop
	for (step_lexer(lb); (lb.token != ',') && (lb.token != ';') && (lb.token != ')'); ++r, step_lexer(la), step_lexer(lb), step_lexer(la), step_lexer(lb)) {
		if ((la.token != CLEX_id) || (la.token != CLEX_intlit) || (la.token != CLEX_floatlit))
			NOB_UNREACHABLE("Recieved something other");
	}

	return r;
}

Variable_Id parse_inline_ops(stb_lexer& l, IR& ir, B_Scope& sc, const size_t file)
{
	stb_lexer la = l;
	stb_lexer lb = l;
	IR temp_ir;
	bool step_lexers = false;

	// a = c + b - d * e / func()
	// Lexer la pos: c

	variable_declaration(nullptr, sc.localv, file, sc.lex_location);
	step_lexer(lb);
	// Lexer b is 1 ahead

	std::vector<Variable_Id> ab;
	std::vector<Ops> ops;

	do {
		if (la.token == CLEX_id) {
			stb_lexer lc = lb;
			if (lc.token == '(') { // is function call
				parse_function_call(file, sc, lc, ir, la.string, true);
				Variable_Id temp2 = sc.localv.size();
				Variable_Id temp = make_inline_var(sc.localv, sc.lex_location);
				assert(temp == temp2);
				ab.push_back(temp);
			}
			else { // lookup variable
				ab.push_back(find_variable(la.string, sc.localv));
			}
		}
		else if (la.token == CLEX_intlit) {
			variable_declaration(nullptr, sc.localv, file, sc.lex_location);
			/* Variable_Id a = make_inline_var(sc.localv, sc.lex_location); */
			ab.push_back(ab.size() - 1);

		}
		else if (la.token == CLEX_floatlit) {
			NOB_TODO("Implement Floats");
		}
		else if (la.token == CLEX_dqstring) {
			NOB_TODO("Implement Strings");
		}
		else {
			NOB_UNREACHABLE("Unexpected token");
		}

		if (ops.size() > 0) {
			switch (ops[ops.size() - 1]) {
			case Plus:
				gen_plus_op(ir, 0, ab.size() - 2, ab.size() - 1); break;
			case Minus:
				gen_minus_op(ir, 0, ab.size() - 2, ab.size() - 1); break;
			case Mult:
				gen_mul_op(ir, 0, ab.size() - 2, ab.size() - 1); break;
			case Div:
				gen_udiv_op(ir, 0, ab.size() - 2, ab.size() - 1); break;
			default:
				NOB_UNREACHABLE("Unknown operation");
			}
		}

		if (lb.token != ',' && lb.token != ';' && lb.token != ')')
			step_lexers = true;
		else
			break;

		if (lb.token == '+') {
			ops.push_back(Plus);
		}
		else if (lb.token == '-') {
			ops.push_back(Minus);
		}
		else if (lb.token == '*') {
			ops.push_back(Mult);
		}
		else if (lb.token == '/') {
			ops.push_back(Div);
		}
		else {
			NOB_UNREACHABLE("Unexpected token in inline op");
		}

		if (step_lexers) {
			step_lexer(la), step_lexer(lb), step_lexer(la), step_lexer(lb);
		}
	}
	while (true);

	// End deliminated by , or ;

	l = lb; // Update the main lexer to the end of the inline ops
	return ab[0];
}

uint8_t parse_function_call(size_t file, B_Scope& sc, stb_lexer& l, IR& ir, const char* name, const bool force_retvalgen)
{
	// Lexer pos: (

	if (!get_and_expect_token(l, ')')) { // Parameters
		NOB_TODO("Implement function parameters for function calls");
	}
	// Lexer pos: )

	/* get_and_expect_semicolon(l); */
	// /\-- should be handled outside since we might not always have a semicolon at the end.
	// see inline ops
	// ~~Lexer pos: ;~~

	assert(l.token == ')');

	stb_lexer lb = l;
	step_lexer(lb);
	assert(lb.token != '{');

	if ((int)find_function(name, sc.localf)) {
		if (force_retvalgen) gen_funccall(ir, sc.localv.size(), name);
		else gen_funccall(ir, name);
	}
	else if ((int)find_function(name, sc.upstreamf)) {
		if (force_retvalgen) gen_funccall(ir, sc.localv.size(), name);
		else gen_funccall(ir, name);
	}
	else if ((int)find_function(name, sc.externf)) {
		B_Variable retval;
		gen_funccall_extrn(ir, sc.localv.size(), name);
		if (!force_retvalgen) sc.localv.push_back(retval);
	}
	else {
		nob_log(NOB_ERROR, "%s:%d:%d: Cannot call function: could not find function `%s`", input_files[file], sc.lex_location.line_number, sc.lex_location.line_offset, name);
		Compilation_error(FunctionNotFound);
		return FunctionNotFound;
	}
	return Success;
}

uint8_t parse_function_definition(size_t file, B_Scope& sc, stb_lexer& l, IR& ir, const char* name)
{
	// Lexer pos: (

	if (!get_and_expect_token(l, ')')) { // Must parse parameters next
		NOB_TODO("Implement function parameters");
	}
	// Lexer pos: )

	assert(get_and_expect_token(l, '{'));
	// Lexer pos: {

	B_Function f;
	f.name = name;
	f.filei = file;
	f.location = sc.lex_location;

	// Check if the body is empty
	if (get_and_expect_token(l, '}')) {
		nob_log(NOB_WARNING, "%s:%d:%d: Warning: encountered empty function: %s", input_files[file], sc.lex_location.line_number, sc.lex_location.line_offset, name);
		update_state(st, name, input_files[file], States::FOUND_EMPTY_FUNC);
		Compilation_error(FunctionEmpty);
		return FunctionEmpty;
	}
	else {
		// Construct the next scope
		IR func_ir;
		B_Scope next(sc);

		gen_func_begin(ir, f);
		parse_scope(file, next, l, func_ir, true);
		gen_all_var_decls(ir, next.localv, input_files);
		if (detect_symbol_redef(f, sc.localf, file)) {
			sc.localf.push_back(f);
			ir.append(func_ir);
			gen_func_end(ir);
			update_state(st, name, input_files[file], States::FOUND_A_FUNC);
		}
		else {
			Compilation_error(FunctionRedefinition);
			return FunctionRedefinition;
		}
	}
	return Success;
}

void compilation_error(Returns e, const char* compiler_file, const int file_line)
{
	static uint16_t error_count = 0;
	static uint16_t warning_count = 0;

	switch (e) {
	case FileEmpty:
	case InvalidSyntax:
	case ExpectedSemicolon:
		++error_count;
		if (error_count >= MAX_ERRORS_BEFORE_STOP)
			stop_compilation = true;
		break;

	case NoFilesGiven:
	case EverythingCouldBeWrong:
	case ErrorReadInput:
	case ErrorWriteOutput:
	case InvalidTargetTriple:
		++error_count;
		stop_compilation = true;
		break;

	case Success:
		return;

	default:
		NOB_UNREACHABLE("Unhandled compilation error");
	}

	if (stop_compilation) {
		nob_log(NOB_INFO, "Stopping compilation: %d errors, %d warnings", error_count, warning_count);
		if ((compiler_file != nullptr) && (file_line > -1))
			nob_log(NOB_INFO, "%s:%d:%d: <--- compilation stopped here in compiler.", compiler_file, file_line, 1);
		exit(e);
	}
	else return;
}

bool dispatch_clang(const CStrings& input_files, const std::string& output_file)
{
	bool override = !output_file.empty();

	for (uint8_t i = 0; i < input_files.size(); ++i) {
		const char* ll_file = swap_extension(input_files[i], "ll");
		const char* output_file_type = "";

		if (override) {
			output_file_type = strrchr(output_file.c_str(), '.');
			nob_log(NOB_INFO, "Overridden file name: %s", output_file.c_str());
		}

		if ((strcmp(output_file_type, ".ll") == 0)) {
			nob_log(NOB_INFO, "Outputting LLVM-IR only. Note: LLVM-IR is outputted regardless of what you're compiling to.");
			return true;
		}
		else if ((strcmp(output_file_type, ".obj") == 0) || (strcmp(output_file_type, ".o") == 0)) { // .obj file
			return run_clang(swap_extension(input_files[i], "o"), ll_file, COMPILE);
		}
		else if (strcmp(output_file_type, ".exe") == 0) { // .exe file
			return run_clang(override ? output_file.data() : swap_extension(input_files[i], "exe"), ll_file, COMPILE);
		}

		// TODO: implement other outputs above
		else { // Assume it is NULL and assume executable without file extension (e.g. Linux)
			return run_clang(override ? chop_extension(output_file.c_str()) : chop_extension(input_files[i]), ll_file, COMPILE);
		}
	}

	if (input_files.size() > 1) {
		std::string files_to_link;

		for (uint8_t i = 0; i < input_files.size(); ++i) {
			if (i != 0) {
				files_to_link += swap_extension(input_files[i], "exe");
			}
		}

		nob_log(NOB_INFO, "Linking multiple files into %s", output_file.c_str());
		return run_clang(output_file.data(), files_to_link.data(), LINK);
	}

	return false;
}

Variable_Id find_variable(const char* name, const B_Variable_Scope& scope)
{
	if (name == nullptr) {
		return scope.size();
	}

	if ((strcmp(name, "true") == 0) || (strcmp(name, "false") == 0))
		return scope.size();

	Variable_Id i = 0;
	for (; i < scope.size();) {
		if (strcmp(scope[i].name, name) == 0) {
			break;
		}
		++i;
	}
	return i; // 0 if not found
}

Function_Id find_function(const char* name, const B_Function_Scope& scope)
{
	Function_Id i = 0;

	for (; i < scope.size();) {
		if (strcmp(scope[i].name, name) == 0) {
			break;
		}
		++i;
	}
	return i; // should be size+1 if invalid
}

Variable_Id make_inline_var(B_Variable_Scope& scope, const stb_lex_location& lo)
{
	Variable_Id id = scope.size();
	B_Variable temp;
	temp.value_type = Uninitialized;
	temp.location = lo;
	gen_auto_keyword_decl(temp.ir, id);
	return id;
}