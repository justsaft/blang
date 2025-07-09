
#include <stdio.h>

#include "state.hpp"
#include "types.hpp"
#include "gen_ir.hpp"
#include "common.hpp"

extern "C"
{
#include "../3rd-party/nob.h"
#include "../3rd-party/stb_c_lexer.h"

#include "output.h"
	bool write_ll_file(const char* original_file, const char* ir, size_t ir_size);
	bool nob_delete_file_silent(const char* path);
}


// states.cpp
bool get_state(States& st);
bool get_file_state(States& st);
void update_state(States& st, const char* sym, const char* file_name, uint8_t fut);


// Driver
bool dispatch_clang(const CStrings& original_file, const std::string& output_file, const IR_Output);
std::string get_target_triple_clang(void);


// Parsing
uint8_t parse_file(size_t file, B_Scope& global, IR& file_ir);
//      Note: gvars and funcs must carry across files
uint8_t parse_scope(size_t file, B_Scope& scope, stb_lexer& l, IR& ir, bool is_function_scope);
uint8_t parse_line(size_t file, B_Scope&, stb_lexer&, IR&, bool is_global_scope);
//      Note: Takes in the scope which is constructed and stored outside


// Parse functions
Variable_Id parse_secondary_expression(stb_lexer& l, IR& ir, B_Scope& sc, const size_t file);
uint8_t parse_assigning_expression(stb_lexer& l, Variable_Id id, B_Scope& sc, IR& ir, const bool is_gvar, const size_t file);
void primary_secondary_intermediate(stb_lexer& l, IR& ir, B_Scope& sc, size_t file, bool is_global_scope, const char* primary);
uint8_t parse_function_definition(size_t file, B_Scope&, stb_lexer&, IR&, const char* name);
uint8_t parse_function_call(size_t file, B_Scope&, stb_lexer&, IR&, const char* name, const bool force_retvalgen = false);


// Parse keywords
uint8_t parse_auto_keyword(stb_lexer& l, IR& ir, B_Scope& scope, const bool is_gvar, const size_t file);
uint8_t parse_extrn_keyword(stb_lexer&, const stb_lex_location&, B_Function_Scope& extrns, size_t file);
uint8_t parse_return_keyword(stb_lexer& l, std::string& ir, B_Scope& sc, const size_t file);


// Declaration
uint8_t variable_declaration(const char* name, B_Variable_Scope&, const size_t file, const stb_lex_location&);
uint8_t variable_declaration(const char* name, B_Variable_Scope&, const size_t file, const stb_lex_location&);
uint8_t parse_assigning_expression(stb_lexer& l, Variable_Id, B_Scope&, IR&, const bool is_gvar, const size_t file);


// Find & Redefinitions
Function_Id find_function(const char* name, const B_Function_Scope& scope);
Variable_Id find_variable(const char* name, const B_Variable_Scope& scope);
bool is_function_redefinition(const B_Function& sym, const B_Function_Scope& scope, const size_t file);
bool is_variable_redefinition(const B_Variable& sym, const B_Variable_Scope& scope, const size_t file);


// Data Storage
States st;

local B_Function_Scope _functions;
B_Function_Scope& B_Scope::functions = _functions;

local B_Function_Scope _extern_functions;
B_Function_Scope& B_Scope::extern_functions = _extern_functions;

local CStrings input_files;
local std::string target;
local bool stop_compilation = false;


// Misc
/* void stop_on_unsupported_target(std::string target); */
Keyword get_keyword(const char* k);
Value_Type get_value_type(Variable_Id, const B_Variable_Scope& scope);
Value_Type get_value_type_from_lval(const char* name, const B_Variable_Scope& scope);
uint16_t count_inline_ops(stb_lexer&);
bool check_for_inline_ops(stb_lexer&);


// Lexer Util (clex_util.cpp)
bool semicolon_next(stb_lexer& l, const char* filename, bool = false, bool = true);
void unexpected_eof(void);
void unexpected_eof(const char* hint);
void get_lexer_location(stb_lexer& l, stb_lex_location& lo);
void step_lexer(stb_lexer& l);
long get_next_token(stb_lexer& l);
bool get_and_expect_token(stb_lexer&, const long token);
bool expect_token(stb_lexer& l, long token);


// cli.cpp
void parse_cli_arguments(int argc, char** argv, std::string& target, std::string& output, CStrings& input_files, IR_Output&);


int main(int argc, char** argv)
{
#if defined(DEBUG)
	input_files.push_back("../../b_src/return_lval.b");
#endif

	std::string output;
	std::string current_platform = get_target_triple_clang();
	IR_Output irout = DeleteIrAfterCompile;

	parse_cli_arguments(argc, argv, target, output, input_files, irout);

	if (input_files.size() == 0) {
		nob_log(NOB_ERROR, "No input files were provided. Specify at least one `.b` file.");
		Compilation_error(NoFilesGiven);
		return NoFilesGiven;
	}

	if (target.empty()) {
		target.append(current_platform);
	} else {
		nob_log(NOB_INFO, "Compiling for %s on %s", target.c_str(), current_platform.c_str());
	}

	/* stop_on_unsupported_target(target); */

	B_Scope global_scope;

	for (size_t i = 0; i < input_files.size(); ++i) { // For each file
		uint8_t retval = 0;
		std::string file_ir;

		retval = parse_file(i, global_scope, file_ir);
		// parse_file hosts the lexer, buffers, checks and parsing until file end

		// Once there are no more tokens...
		if (!get_file_state(st)) {
			Compilation_error(EverythingCouldBeWrong);
		} else if (stop_compilation) {
			return retval;
		} else if (!write_ll_file(input_files[i], file_ir.data(), file_ir.size() - 1)) {
			nob_log(NOB_WARNING, "Couldn't write IR for file %s", input_files[i]);
			Compilation_error(ErrorWritingIrOfFile);
		} else if (input_files.size() == 1) {
			if (!dispatch_clang(input_files, output, irout)) {
				/* nob_log(NOB_ERROR, "Error when compiling ir of `%s`", input_files[0]); */
				Compilation_error(ClangNonZeroExitcode);
			}
		}
	}

	if ((input_files.size() > 1) && (!stop_compilation)) {
		if (!dispatch_clang(input_files, output, irout)) {
			/* nob_log(NOB_ERROR, "Error when compiling"); */
			Compilation_error(ClangNonZeroExitcode);
			return ClangNonZeroExitcode;
		}
	} else if (input_files.size() == 0) {
		NOB_UNREACHABLE("Unreachable reached: No files given.");
	}

	return Success;
}

uint8_t parse_file(size_t file, B_Scope& global, IR& file_ir)
{
	static Nob_String_Builder clex_is = { nullptr, 0, 0 }; // input stream
	clex_is.count = 0;

	if (nob_read_entire_file(input_files[file], &clex_is)) {
		nob_log(NOB_INFO, "Compiling %s", input_files[file]);
	} else {
		nob_log(NOB_ERROR, "Could not read file %s", input_files[file]);
		Compilation_error(ErrorReadInput);
		return ErrorReadInput;
	}

	std::vector<char> clex_buf;
	clex_buf.resize(CLEX_BUFFER_DEFAULT_SIZE);

	stb_lexer lexer;
	stb_c_lexer_init(&lexer, clex_is.items, clex_is.items + clex_is.count, clex_buf.data(), clex_buf.capacity() - 1);

	// First step of the lexer and check if file is empty
	if (!stb_c_lexer_get_token(&lexer)) {
		nob_log(NOB_ERROR, "File %s is empty.", input_files[file]);
		Compilation_error(FileEmpty);
		return FileEmpty;
	}

	// Sanity check
	switch (lexer.token) {
	case CLEX_eof:
		NOB_UNREACHABLE("Reached end-of-file sanity check unreachable.");

	case CLEX_parse_error:
		nob_log(NOB_ERROR, "CLEX parse error: likely an issue with the buffer");
		Compilation_error(EverythingCouldBeWrong);
		return EverythingCouldBeWrong; // Shutup the compiler

	default:
		break;
	}

	global.extern_functions.clear();

	gen_file_ir_info(file_ir, target.data(), input_files[file]);
	return parse_scope(file, global, lexer, file_ir, false);
}

uint8_t parse_scope(size_t file, B_Scope& scope, stb_lexer& l, IR& ir, bool is_function_scope)
{
	uint8_t retval = Success;

	while (l.token != CLEX_eof) switch (l.token) {
	case '{':
		// Lexer pos: {

		// Note: if this function is called from parse_function_definition
		//       this case is skipped because the '{' is eaten by that function
		//       this is also the reason the compiler used to fail on closing a
		//       scope, because we were keeping track of how many scopes deep we were.

		stb_c_lexer_get_token(&l);
		if (l.token != '}') {
			B_Scope next(scope);
			retval = parse_scope(file, next, l, ir, is_function_scope);
			if (retval != Success) NOB_UNREACHABLE("asdasd");
		}

		stb_c_lexer_get_token(&l);
		break; // breaks switch

	case '}': // Scope ended
		stb_c_lexer_get_token(&l);
		return retval;

	case CLEX_parse_error:
		nob_log(NOB_ERROR, "CLEX parse error: likely out of buffer space");
		NOB_TODO("Increase buffer size as more is needed");

	default:
		retval = parse_line(file, scope, l, ir, (!is_function_scope) /* && (scope_depth == 0) */);
		break; // breaks switch
	}

	return retval;
}

uint8_t parse_line(size_t file, B_Scope& sc, stb_lexer& l, IR& ir, bool is_global_scope)
{
	get_lexer_location(l, sc.lex_location);

	if (l.token != CLEX_id) {
		nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: expected an identifier such as a keyword or but got %s instead.",
				input_files[file], sc.lex_location.line_number, sc.lex_location.line_offset,
				std::to_string(l.token < 256 ? (char)l.token : l.token).c_str());
		Compilation_error(InvalidSyntax);
		return InvalidSyntax;
	}

	std::basic_string primary = l.string;
	uint8_t retval = Success;

	stb_c_lexer_get_token(&l);

	switch (get_keyword(primary.c_str())) {
	case Extrn: retval = parse_extrn_keyword(l, sc.lex_location, sc.extern_functions, file); break;
	case Auto: retval = parse_auto_keyword(l, ir, sc, is_global_scope, file); break;

	case Return:
		if (is_global_scope) {
			nob_log(NOB_ERROR, "%s:%d:%d: Syntax error: keyword `return` not valid in global scope.", input_files[file], sc.lex_location.line_number, sc.lex_location.line_offset);
			Compilation_error(InvalidSyntax);
			retval = InvalidSyntax;
		} else {
			retval = parse_return_keyword(l, ir, sc, file);
		}
		break; // -> semicolon next

	case NoKeyword: // lhs / rhs situation: could be var assignment, function call, >>=, etc.
		if (l.token == ';')
			break; // -> semicolon next

		if (is_global_scope) {
			if (l.token == '(') {
				retval = parse_function_definition(file, sc, l, ir, primary.data());
				return retval; // No semicolon, skip call to semicolon_next
			} else if ((l.token == '=') || (l.token == CLEX_id) || (l.token == CLEX_intlit) || (l.token == CLEX_floatlit) || (l.token == CLEX_dqstring)) {
				retval = parse_assigning_expression(l, find_variable(primary.data(), sc.localv), sc, ir, is_global_scope, file);
			} else {
				retval = parse_secondary_expression(l, ir, sc, file);
			}
		} else {
			if (l.token == '=')
				retval = parse_assigning_expression(l, find_variable(primary.data(), sc.localv), sc, ir, is_global_scope, file);
			else retval = parse_secondary_expression(l, ir, sc, file);
		}
		break; // -> semicolon next

	default:
		NOB_TODO("Keyword not yet implemented!");
	}

	semicolon_next(l, input_files[file]);
	return retval;
}

void primary_secondary_intermediate(stb_lexer& l, IR& ir, B_Scope& sc, size_t file, bool is_global_scope, const char* primary)
{
	Variable_Id src = parse_secondary_expression(l, ir, sc, file);
	if (is_global_scope) {
		/* gen_store_lval_to_gvar(ir, primary, src); */
		gen_store_rval_to_gvar(ir, primary,
							   l.token == CLEX_id ? l.string
							   : l.token == CLEX_intlit ? std::to_string(l.int_number)
							   : l.token == CLEX_floatlit ? std::to_string(l.real_number)
							   : l.string);
	} else {
		Variable_Id dest = find_variable(primary, sc.localv);
		gen_store_lval_to_lval(ir, dest, src);
	}
}

Keyword get_keyword(const char* k)
{
	if (strcmp(k, "extrn") == 0) {
		return Extrn;
	} else if (strcmp(k, "auto") == 0) {
		return Auto;
	} else if (strcmp(k, "return") == 0) {
		return Return;
	} else if (strcmp(k, "switch") == 0) {
		return Switch;
	} else if (strcmp(k, "case") == 0) {
		return Case;
	} else if (strcmp(k, "if") == 0) {
		return If;
	} else if (strcmp(k, "else") == 0) {
		return Else;
	} else if (strcmp(k, "while") == 0) {
		return While;
	} else if (strcmp(k, "goto") == 0) {
		return Goto;
	} else {
		return NoKeyword;
	}
}

uint8_t variable_declaration(const char* name, B_Variable_Scope& vsc, const size_t file, const stb_lex_location& lo)
{
	B_Variable var { name, Uninitialized, file, lo, "" };
	Variable_Id id = vsc.size();

	if (name != nullptr) if (is_variable_redefinition(var, vsc, file)) {
		Compilation_error(VariableRedefinition);
		return VariableRedefinition;
	}

	gen_alloca(var.ir, id);
	/* gen_load(var.ir, id + 1, id); */
	vsc.push_back(var);
	return Success;
}

uint8_t variable_declaration(const char* name, B_Variable_Scope& vsc, const size_t file, const stb_lex_location& lo, B_Variable& var)
{
	var.name = name;
	var.location = lo;
	var.file = file;
	var.value_type = Uninitialized;
	Variable_Id id = vsc.size();

	if (name != nullptr) if (is_variable_redefinition(var, vsc, file)) {
		Compilation_error(VariableRedefinition);
		return VariableRedefinition;
	}

	gen_alloca(var.ir, id);
	/* gen_load(var.ir, id + 1, id); */
	return Success;
}

uint8_t parse_assigning_expression(stb_lexer& l, Variable_Id id, B_Scope& sc, IR& ir, const bool is_gvar, const size_t file)
{
	// This should be the function to call when you've got `auto var = 2;` and `var = 3;` to handle the assigning part.
	// Lexer: auto variable <<=>> asd + 1;
	// Lexer: a <<=>> asd + 1;

	if (l.token == ';')
		NOB_UNREACHABLE("Call to `variable_assignment` was unnessesary.");

	if (id < FIRST_VARIABLE_ID) {
		nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: variable `%s` wasn't declared prior to attempting to assign to it.",
				Filename, sc.lex_location.line_number, sc.lex_location.line_offset, "TODO: var name here");
	}

	if ((l.token != '=') && (!is_gvar)) {
		nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: expected `=` after variable name `%s` but got `%s`.",
				Filename, sc.lex_location.line_number, sc.lex_location.line_offset,
				sc.localv[id].name, std::to_string(l.token < 256 ? (char)l.token : l.token).c_str());
		Compilation_error(InvalidSyntax);
		return InvalidSyntax;
	}

	Variable_Id working_with;
	bool step_lexers;

	step_lexer(l);
	// Lexer: auto variable = <<asd>> + 1;

	stb_lexer la = l;
	stb_lexer lb = l;
	step_lexer(lb);
	// Lexer B: auto variable = asd <<+>> 1;
	// Lexer b is always 1 ahead

	// Storage
	std::vector<Variable_Id> ab;
	std::vector<Ops> ops;

	do {
		if ((lb.token == ',') || (lb.token == ';') || (lb.token == ')')) {
			working_with = id;
			step_lexers = false;
		} else {
			working_with = sc.localv.size();
			step_lexers = true;
		}

		if (ops.size() > 0) {
			// Create a variable to be used as the destination of the result
			if (working_with != id) {
				gen_alloca(ir, working_with);
				ab.push_back(working_with);
				++working_with;
			}

			sc.localv.push_back(B_Variable { nullptr, Dead, file, sc.lex_location, "" });
			sc.localv.push_back(B_Variable { nullptr, Uninitialized, file, sc.lex_location, "" });

			switch (ops[ops.size() - 1]) {
			case Plus:
				gen_plus_op(ir, working_with, ab[ab.size() - 1], ab[ab.size() - 2]); break;
			case Minus:
				gen_minus_op(ir, working_with, ab[ab.size() - 1], ab[ab.size() - 2]); break;
			case Mult:
				gen_mul_op(ir, working_with, ab[ab.size() - 1], ab[ab.size() - 2]); break;
			case Div:
				gen_udiv_op(ir, working_with, ab[ab.size() - 1], ab[ab.size() - 2]); break;
			default:
				NOB_UNREACHABLE("Unknown operation");
			}
		}

		if (lb.token == '+') {
			ops.push_back(Plus);
		} else if (lb.token == '-') {
			ops.push_back(Minus);
		} else if (lb.token == '*') {
			ops.push_back(Mult);
		} else if (lb.token == '/') {
			ops.push_back(Div);
		} else {
			NOB_UNREACHABLE("Unexpected token in inline op");
		}

		switch (la.token) {
		case CLEX_intlit:
			if (working_with == id) {
				sc.localv[id].value_type = Int;
				gen_store_rval_to_lval(ir, working_with, std::to_string(la.int_number));
			} else {
				gen_alloca(ir, working_with);
				gen_store_rval_to_lval(ir, working_with, std::to_string(la.int_number));
				gen_load_lval_to_lval(ir, working_with + 1, working_with);
				sc.localv.push_back(B_Variable { nullptr, Dead, file, sc.lex_location, "" });
				sc.localv.push_back(B_Variable { nullptr, Int, file, sc.lex_location, "" });
				ab.push_back(working_with + 1);
			}
			break;

		case CLEX_id:
			{
				if (lb.token == '(') {
					if (working_with == id) {
						parse_function_call(file, sc, lb, ir, la.string, true);
					} else {
						gen_alloca(ir, working_with);
						ab.push_back(working_with + 1);
						parse_function_call(file, sc, lb, ir, la.string, true);
						sc.localv.push_back(B_Variable { nullptr, Dead, file, sc.lex_location, "" });
						sc.localv.push_back(B_Variable { nullptr, Int, file, sc.lex_location, "" });
					}
					la = lb;
					step_lexer(lb);
				} else {
					// lookup variable
					const Variable_Id var1 = find_variable(la.string, sc.localv);
					const Variable_Id var2 = find_variable(la.string, sc.upstreamv);

					const bool var1_is_valid = var1 != INVALID_VARIABLE;
					const bool var2_is_valid = var2 != INVALID_VARIABLE;

					if (var1_is_valid) {
						gen_load_lval_to_lval(ir, working_with, var1);
						ab.push_back(working_with);
						sc.localv.push_back(B_Variable { nullptr, Dead, file, sc.lex_location, "" });
					} else if (var2_is_valid) {
						NOB_TODO("Make global variables actually usable");
					} else if (var1_is_valid && var2_is_valid) {
						gen_load_lval_to_lval(ir, working_with, var1);
						ab.push_back(var1);
						sc.localv.push_back(B_Variable { nullptr, Dead, file, sc.lex_location, "" });
						nob_log(NOB_WARNING, "%s:%d:%d: Variable `%s` is being shadowed.", input_files[file], sc.lex_location.line_number, sc.lex_location.line_offset, la.string);
						Compilation_error(VariableIsShadowed);
					} else {
						nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: `%s` is not declared.", input_files[file], sc.lex_location.line_number, sc.lex_location.line_offset, la.string);
						Compilation_error(InvalidSyntax);
					}

					// TODO: Make global variables usable
				}
			}
			break;

		case CLEX_floatlit:
			NOB_TODO("Implement Floats");
		case CLEX_dqstring:
			NOB_TODO("Implement Strings");
		default:
			NOB_UNREACHABLE("Unexpected token");
		}

		if (step_lexers) {
			step_lexer(la), step_lexer(lb), step_lexer(la), step_lexer(lb);
		}

	} while (true);

	// End deliminated by , or ;

	// TODO: restore only the parse point to make copy more effective
	l = lb; // Update the main lexer to the end of the inline ops
	assert((l.token == ',') || (l.token == ';') || (l.token == ')'));

	return Success;
}

uint8_t parse_auto_keyword(stb_lexer& l, IR& ir, B_Scope& scope, const bool is_gvar, const size_t file)
{
	// auto <<variable>> = 69;

	if (l.token != CLEX_id) {
		nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: expected name of variable after this auto keyword.", input_files[file], scope.lex_location.line_number, scope.lex_location.line_offset);
		Compilation_error(InvalidSyntax);
		return InvalidSyntax;
	}

	const char* name = l.string;
	uint8_t retval = variable_declaration(name, scope.localv, file, scope.lex_location);
	// variable_declaration handles push_back

	switch (get_next_token(l)) {
		/* case CLEX_eof:
			unexpected_eof("auto variable declaration");
			exit(UnexpectedEndOfFile); */

	case '=':
		if (retval == Success) {
			retval = parse_assigning_expression(l, scope.localv.size() - 1, scope, ir, is_gvar, file);
		} else {
			NOB_TODO("Stuff that's gonna happen after non succesful variable declaration");
		}
		break;

	case ';':
		break;

	default:
		nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: unexpected token in auto variable declaration for `%s`.", input_files[file], scope.lex_location.line_number, scope.lex_location.line_offset, name);
		Compilation_error(InvalidSyntax);
		return InvalidSyntax;
	}

	return retval;
}

uint8_t parse_extrn_keyword(stb_lexer& l, const stb_lex_location& lo, B_Function_Scope& extrns, size_t file)
{
	if (!expect_token(l, CLEX_id)) {
		nob_log(NOB_ERROR, "Syntax error: expected a name after `extrn`");
		Compilation_error(InvalidSyntax);
		return InvalidSyntax;
	}

	do {
		B_Function e { l.string, file, lo };
		if (is_function_redefinition(e, extrns, file)) {
			NOB_TODO("Handle this");
		}
		extrns.push_back(e);
	} while (false);

	return Success;
}

uint8_t parse_return_keyword(stb_lexer& l, std::string& ir, B_Scope& sc, const size_t file)
{
	// Lexer pos: return <<asd>>; next

	switch (l.token) {
	case ';':
		gen_return_keyword(ir);
		break;

	case CLEX_floatlit:
	case CLEX_id:
	case CLEX_intlit:
		{
			Variable_Id vid = parse_secondary_expression(l, ir, sc, file);
			// Lexer pos: ';' ideally

			/* if (Int != get_value_type(vid, sc.localv)) {
				nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: cannot return non-integer variable `%s`", input_files[file], sc.lex_location.line_number, sc.lex_location.line_offset, l.string);
				Compilation_error(InvalidSyntax);
				return InvalidSyntax;
			} */

			gen_return_keyword_lvalue(ir, vid);
		}
		break;

	default:
		nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: unexpected token after return keyword", input_files[file], sc.lex_location.line_number, sc.lex_location.line_offset);
		Compilation_error(InvalidSyntax);
		return InvalidSyntax;
	}

	return Success;
}

/* bool check_for_inline_ops(stb_lexer& l)
{
	return (bool)count_inline_ops(l);
} */

/* uint16_t count_inline_ops(stb_lexer& la)
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
} */

Variable_Id parse_secondary_expression(stb_lexer& l, IR& ir, B_Scope& sc, const size_t file)
{
	// Lexers
	stb_lexer la = l;
	stb_lexer lb = l;
	bool step_lexers;

	// Storage
	std::vector<Variable_Id> ab;
	std::vector<Ops> ops;

	/* ab.push_back(Invalid_Variable); */

	step_lexer(lb); // Lexer b is always 1 ahead

	// a = c + b - d * e / func()
	// Lexer la pos: c
	// Lexer lb pos: +

	do {
		step_lexers = false;

		switch (la.token) {
		case CLEX_intlit:
			{// convert it to a variable
				Variable_Id id = sc.localv.size();
				gen_alloca(ir, id);
				gen_store_rval_to_lval(ir, id, std::to_string(la.int_number));
				gen_load_lval_to_lval(ir, id + 1, id);

				sc.localv.push_back(B_Variable { nullptr, Dead, file, sc.lex_location, "" });
				sc.localv.push_back(B_Variable { nullptr, Int, file, sc.lex_location, "" });
				ab.push_back(id + 1);
			}
			break;

		case CLEX_id:
			{
				if (lb.token == '(') {
					Variable_Id id = sc.localv.size();
					gen_alloca(ir, id);
					ab.push_back(id + 1);
					parse_function_call(file, sc, lb, ir, la.string, true);
					sc.localv.push_back(B_Variable { nullptr, Dead, file, sc.lex_location, "" });
					sc.localv.push_back(B_Variable { nullptr, Int, file, sc.lex_location, "" });
					la = lb;
					step_lexer(lb);
				} else {
					// lookup variable
					const Variable_Id var1 = find_variable(la.string, sc.localv);
					const Variable_Id var2 = find_variable(la.string, sc.upstreamv);

					const bool var1_is_valid = var1 > INVALID_VARIABLE;
					const bool var2_is_valid = var2 > INVALID_VARIABLE;

					if (var1_is_valid && var2_is_valid) {
						gen_load_lval_to_lval(ir, sc.localv.size(), var1);
						ab.push_back(var1);
						sc.localv.push_back(B_Variable { nullptr, Dead, file, sc.lex_location, "" });
						nob_log(NOB_WARNING, "%s:%d:%d: Variable `%s` is being shadowed.", input_files[file], sc.lex_location.line_number, sc.lex_location.line_offset, la.string);
						Compilation_error(VariableIsShadowed);
					} else if (var1_is_valid) {
						gen_load_lval_to_lval(ir, sc.localv.size(), var1);
						ab.push_back(sc.localv.size());
						sc.localv.push_back(B_Variable { nullptr, Dead, file, sc.lex_location, "" });
					} else if (var2_is_valid) {
						NOB_TODO("Make global variables actually usable");
					} else {
						nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: `%s` is not declared.", input_files[file], sc.lex_location.line_number, sc.lex_location.line_offset, la.string);
						Compilation_error(InvalidSyntax);
					}

					// TODO: Make global variables usable
				}
			}
			break;

		case CLEX_floatlit:
			NOB_TODO("Implement Floats");
		case CLEX_dqstring:
			NOB_TODO("Implement Strings");
		default:
			NOB_UNREACHABLE("Unexpected token");
		}

		if (ops.size() > 0) {
			// Create a variable to be used as the destination of the result
			Variable_Id dest = sc.localv.size();
			gen_alloca(ir, dest);
			++dest;

			sc.localv.push_back(B_Variable { nullptr, Dead, file, sc.lex_location, "" });
			sc.localv.push_back(B_Variable { nullptr, Uninitialized, file, sc.lex_location, "" });

			switch (ops[ops.size() - 1]) {
			case Plus:
				gen_plus_op(ir, dest, ab[ab.size() - 1], ab[ab.size() - 2]); break;
			case Minus:
				gen_minus_op(ir, dest, ab[ab.size() - 1], ab[ab.size() - 2]); break;
			case Mult:
				gen_mul_op(ir, dest, ab[ab.size() - 1], ab[ab.size() - 2]); break;
			case Div:
				gen_udiv_op(ir, dest, ab[ab.size() - 1], ab[ab.size() - 2]); break;
			default:
				NOB_UNREACHABLE("Unknown operation");
			}

			ab.push_back(dest);
		}

		if ((lb.token == ',') || (lb.token == ';') || (lb.token == ')')) {
			break;
		} else {
			step_lexers = true;
		}

		if (lb.token == '+') {
			ops.push_back(Plus);
		} else if (lb.token == '-') {
			ops.push_back(Minus);
		} else if (lb.token == '*') {
			ops.push_back(Mult);
		} else if (lb.token == '/') {
			ops.push_back(Div);
		} else {
			NOB_UNREACHABLE("Unexpected token in inline op");
		}

		if (step_lexers) {
			step_lexer(la), step_lexer(lb), step_lexer(la), step_lexer(lb);
		}
	} while (true);

	// End deliminated by , or ;

	// TODO: restore only the parse point to make copy more effective
	l = lb; // Update the main lexer to the end of the inline ops
	assert((l.token == ',') || (l.token == ';') || (l.token == ')'));

	return ab[ab.size() - 1];
}

uint8_t parse_function_call(size_t file, B_Scope& sc, stb_lexer& l, IR& ir, const char* name, const bool force_retvalgen)
{
	// Lexer pos: (

	if (!get_and_expect_token(l, ')')) { // Parameters
		NOB_TODO("Implement function parameters for function calls");
	}
	// Lexer pos: )

	/* semicolon_next(l); */
	// /\-- should be handled outside since we might not always have a semicolon at the end.
	// see inline ops
	// ~~Lexer pos: ;~~

	assert(l.token == ')');

	stb_lexer lb = l;
	step_lexer(lb);
	assert(lb.token != '{');

	if ((int)find_function(name, sc.functions)) {
		if (force_retvalgen) gen_funccall(ir, sc.localv.size(), name);
		else gen_funccall(ir, name);
	} else if ((int)find_function(name, sc.extern_functions)) {
		B_Variable retval;
		gen_funccall_extrn(ir, sc.localv.size(), name);
		if (!force_retvalgen) sc.localv.push_back(retval);
	} else {
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

	B_Function f { name, file, sc.lex_location };

	if (is_function_redefinition(f, sc.functions, file)) {
		Compilation_error(FunctionRedefinition);
		return FunctionRedefinition;
	}

	sc.functions.push_back(f);
	gen_func_begin(ir, f);

	if (get_and_expect_token(l, '}')) { // TODO: Stop stepping ahead here
		nob_log(NOB_WARNING, "%s:%d:%d: Warning: empty function: %s", input_files[file], sc.lex_location.line_number, sc.lex_location.line_offset, name);
		update_state(st, name, input_files[file], States::FOUND_EMPTY_FUNC);
		Compilation_error(FunctionEmpty);
		return FunctionEmpty;
	} else {
		IR func_ir;
		B_Scope next(sc);
		parse_scope(file, next, l, func_ir, true);
		gen_all_var_decls(ir, next.localv, input_files);
		ir.append(func_ir);
		update_state(st, name, input_files[file], States::FOUND_A_FUNC);
	}

	gen_func_end(ir);
	return Success;
}

void compilation_error(Returns e, const char* compiler_file, const int file_line)
{
#if !defined(DEBUG)
	(void)compiler_file;
	(void)file_line;
#endif

	static uint16_t error_count = 0;
	static uint16_t warning_count = 0;

	switch (e) {
		// Warnings
	case FunctionEmpty:
	case UnusedVariable:
	case VariableIsShadowed:
		++warning_count;
		break;


		// Stop later
	case FileEmpty:
	case InvalidSyntax:
	case ExpectedSemicolon:
	case FunctionRedefinition:
		++error_count;
		if (error_count >= MAX_ERRORS_BEFORE_STOP)
			stop_compilation = true;
		break;


		// Immediately stop
	case NoFilesGiven:
	case EverythingCouldBeWrong:
	case ErrorReadInput:
	case ErrorWriteOutput:
	case InvalidTargetTriple:
		++error_count;
		stop_compilation = true;
		break;


		// Ignored
	case CompilationHadWarnings:
	case ClangNonZeroExitcode:
	case Success:
		return;

	default:
		NOB_UNREACHABLE("Unhandled compilation error");
	}

	if (stop_compilation) {
		nob_log(NOB_INFO, "Stopping compilation: %d errors, %d warnings", error_count, warning_count);
#if defined(DEBUG)
		if ((compiler_file != nullptr) && (file_line > -1))
			nob_log(NOB_INFO, "%s:%d:%d: <--- compilation stopped here in compiler.", compiler_file, file_line, 1);
#endif
		exit(e);
	} else return;
}

bool dispatch_clang(const CStrings& input_files, const std::string& output_file, const IR_Output irout)
{
	bool override = !output_file.empty();

	for (uint8_t i = 0; i < input_files.size(); ++i) {
		const char* ll_file = swap_extension(input_files[i], "ll");
		const char* output_file_type = "";

		if (override) {
			output_file_type = strrchr(output_file.c_str(), '.');
			nob_log(NOB_INFO, "Output file name: %s", output_file.c_str());
		}

		if ((strcmp(output_file_type, ".ll") == 0) || (irout == DontCompile)) {
			nob_log(NOB_INFO, "Outputting LLVM-IR only");
			return true;
		} else if ((strcmp(output_file_type, ".obj") == 0) || (strcmp(output_file_type, ".o") == 0)) { // .obj file
			return run_clang(swap_extension(input_files[i], "o"), ll_file, COMPILE);
		} else if (strcmp(output_file_type, ".exe") == 0) { // .exe file
			return run_clang(override ? output_file.data() : swap_extension(input_files[i], "exe"), ll_file, COMPILE);
		}

		// TODO: implement other outputs above
		else { // Assume it is NULL and assume executable without file extension (e.g. Linux)
			return run_clang(override ? chop_extension(output_file.c_str()) : chop_extension(input_files[i]), ll_file, COMPILE);
		}

#if true || defined(ENABLE_FILE_DELETIONS)
		if (irout == DeleteIrAfterCompile) {
			nob_delete_file_silent(ll_file);
		}
#endif
	}

	if (input_files.size() > 1) {
		std::string files_to_link;

		for (uint8_t i = 0; i < input_files.size(); ++i) {
			if (i != 0) {
				files_to_link += swap_extension(input_files[i], "exe");
			}
		}

		nob_log(NOB_INFO, "Linking into %s", output_file.c_str());
		return run_clang(output_file.data(), files_to_link.data(), LINK);
	}

	return false;
}

Variable_Id find_variable(const char* name, const B_Variable_Scope& scope)
{
	if (name == nullptr)
		NOB_UNREACHABLE("Searched for nullptr named variable");

	if (strcmp(name, "false") == 0)
		return Bool_False;

	if (strcmp(name, "true") == 0)
		return Bool_True;

	for (Variable_Id i = FIRST_VARIABLE_ID; i < (signed)scope.size(); ++i) {
		if (strcmp(scope[i].name, name) == 0) {
			return i; // Found
		}
	}

	return INVALID_VARIABLE; // Not found
}

Function_Id find_function(const char* name, const B_Function_Scope& scope)
{
	for (Function_Id i = FIRST_FUNCTION_ID; i < scope.size(); ++i) {
		if (strcmp(scope[i].name, name) == 0) {
			return i;
		}
	}
	return INVALID_FUNCTION;
}

bool is_function_redefinition(const B_Function& s, const B_Function_Scope& fsc, const size_t file)
{
	for (Function_Id i = FIRST_FUNCTION_ID; i < fsc.size() + 1; ++i) {
		if (strcmp(s.name, fsc[i].name) == 0) {
			nob_log(NOB_ERROR, "%s:%d:%d: Variable redefinition: attempted to redefine %s", input_files[file], s.location.line_number, s.location.line_offset, s.name);
			nob_log(NOB_ERROR, "%s:%d:%d: <--- %s is first defined here", input_files[fsc[i].filei], fsc[i].location.line_number, fsc[i].location.line_offset, fsc[i].name);
			return true;
		}
	}
	return false;
}

bool is_variable_redefinition(const B_Variable& s, const B_Variable_Scope& vsc, const size_t file)
{
	for (Variable_Id i = FIRST_VARIABLE_ID; i < (signed)vsc.size(); ++i) {
		if (strcmp(s.name, vsc[i].name) == 0) {
			nob_log(NOB_ERROR, "%s:%d:%d: Variable redefinition: attempted to redefine %s", input_files[file], s.location.line_number, s.location.line_offset, s.name);
			nob_log(NOB_ERROR, "%s:%d:%d: <--- %s is first defined here", input_files[vsc[i].file], vsc[i].location.line_number, vsc[i].location.line_offset, vsc[i].name);
			return true;
		}
	}
	return false;
}

Value_Type get_value_type(Variable_Id id, const B_Variable_Scope& scope)
{
	return scope[id].value_type;
}

Value_Type get_value_type_from_lval(const char* name, const B_Variable_Scope& scope)
{
	Variable_Id v = find_variable(name, scope);
	if (v != INVALID_VARIABLE) {
		return scope[v].value_type;
	}
	return Invalid_Value_Type;
}