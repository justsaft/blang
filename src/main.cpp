
#include <stdio.h>

#include "types.hpp"
#include "gen_ir.hpp"
#include "common.hpp"
#include "clex_wrapper.hpp"

extern "C"
{
#include "../3rd-party/nob.h"
#include "output.h"

	void set_minimal_nob_log_level(Nob_Log_Level level);
}


// Driver
bool dispatch_clang(std::string& output_file_name);
std::string get_target_triple_clang(void);


// Parsing
uint8_t parse_file(const int file, B_Scope&, LLVM_IR&, Lexer&);
//      Note: gvars and funcs must carry across files
uint8_t parse_scope(const int file, B_Scope&, Lexer&, LLVM_IR&, bool is_function_scope);
uint8_t parse_line(const int file, B_Scope&, Lexer&, LLVM_IR&, bool is_global_scope);
//      Note: Takes in the scope which is constructed and stored outside


// Parse functions
Variable_Id parse_secondary_expression(Lexer& l, LLVM_IR& ir, B_Scope& sc, const int file);
uint8_t parse_assigning_expression(Lexer&, const char* variable_name, B_Scope&, LLVM_IR&, const bool is_gvar, const int file);
uint8_t parse_function_definition(const int file, B_Scope&, Lexer&, LLVM_IR&, const char* name);
uint8_t parse_function_call(const int file, B_Scope&, Lexer&, LLVM_IR&, const char* name, const bool force_retvalgen = false);
void backpatch_all_function_calls(const int file, const B_Scope& scope);
/* void primary_secondary_intermediate(stb_lexer& l, IR& ir, B_Scope& sc, const int file, bool is_global_scope, const char* primary); */


// Parse keywords
uint8_t parse_auto_keyword(Lexer&, LLVM_IR&, B_Scope&, const bool is_gvar, const int file);
uint8_t parse_extrn_keyword(Lexer&, B_Function_Scope&, const int file);
uint8_t parse_return_keyword(Lexer&, LLVM_IR&, B_Scope&, const int file);


// Declaration
uint8_t variable_declaration(const char* name, B_Variable_Scope&, const int file, const stb_lex_location&, const bool is_gvar);
uint8_t variable_declaration(const char* name, B_Variable_Scope&, const int file, const stb_lex_location&, const bool is_gvar, B_Variable&);


// Find & Redefinitions
Function_Id find_function(const char* name, const B_Function_Scope& scope);
Variable_Id find_variable(const char* name, const B_Variable_Scope& scope);
void its_function_redefinition(const B_Function& current, const B_Function& previous);
bool is_function_redefinition(const B_Function& new_f, const B_Function_Scope& scope, const int file);
bool is_variable_redefinition(const B_Variable& new_v, const B_Variable_Scope& scope, const int file);


// Data Storage
Compilation compilation;

local B_Function_Scope _functions;
B_Function_Scope& B_Scope::functions = _functions;

local B_Function_Scope _extern_functions;
B_Function_Scope& B_Scope::extern_functions = _extern_functions;

local B_Files input_files;
local std::string target;


// Misc
Keyword check_keyword_identifier(const char* k);
Value_Type get_value_type(Variable_Id, const B_Variable_Scope& scope);
Value_Type get_value_type_from_lval(const char* name, const B_Variable_Scope& scope);


// Lexer Util (clex_util.cpp)
bool semicolon_next(stb_lexer& l, const char* filename, bool advance_pre = false, bool advance_post = true);
void get_lexer_location(stb_lexer& l, stb_lex_location& lo);
void step_lexer(stb_lexer& l);
long get_next_token(stb_lexer& l);
long peak_next_token(stb_lexer& l);
bool get_and_expect_token(stb_lexer&, const long token);
bool expect_token(stb_lexer& l, long token);


// cli.cpp
void parse_cli_arguments(int argc, char** argv, std::string& target, std::string& output, B_Files&, Compilation&);
bool is_clang_installed(void);


#pragma region Main

int main(int argc, char** argv)
{
	std::string output;
	parse_cli_arguments(argc, argv, target, output, input_files, compilation);

	if (input_files.size() == 0) {
		nob_log(NOB_ERROR, "No input files were provided. Specify at least one `.b` file.");
		Compilation_error(NoFilesGiven);
	}

	if (!is_clang_installed()) {
		nob_log(NOB_ERROR, "Cannot continue. Please install `clang`. (it is a hard dependency at this time)");
		return 1;
	}

	std::string current_platform = get_target_triple_clang();

	if (target.empty()) {
		target.append(current_platform);
	} else {
		nob_log(NOB_INFO, "Compiling for %s on %s", target.c_str(), current_platform.c_str());
	}

	B_Scope global_scope;
	Lexer lexer;
	LLVM_IR file_ir;

	for (int file = 0; file < (int)input_files.size(); file_ir.clear(), ++file) { // For each file

		input_files[file].state = (Returns)
			// parse_file parses until end of file
			parse_file(file, global_scope, file_ir, lexer);

		// Once there are no more tokens...
		backpatch_all_function_calls(file, global_scope);

		if (compilation.stop)
			break;

		else if (!write_ll_file(CurrentFile, file_ir.data(), file_ir.size())) {
			nob_log(NOB_WARNING, "Couldn't write LLVM_IR for file %s", CurrentFile);
			Compilation_error(ErrorWritingIrOfFile);
			break;
		}
	}

	if ((!compilation.errors) && (!dispatch_clang(output)))
		Compilation_error(ClangNonZeroExitcode);

	return compilation.state;
}

#pragma endregion
#pragma region Parse Files

uint8_t parse_file(const int file, B_Scope& global, LLVM_IR& file_ir, Lexer& lexer)
{
	gen_file_ir_info(file_ir, target.data(), CurrentFile);
	uint8_t retval = Success;

	retval =
		lexer.InitAndLoadFile(CurrentFile);

	if (retval != Success) {
		file_ir.append("\n\n; There was an error reading the source file.");
		return retval;
	}

	// Setup
	setup_ir_gen(compilation);
	global.extern_functions.clear();

	LLVM_IR inner;

	retval =
		parse_scope(file, global, lexer, inner, false);

	gen_all_func_decls(file_ir, global.functions);
	file_ir.append(inner);
	gen_attr_group(file_ir, 0);

	return retval;
}

#pragma endregion
#pragma region Parse Scopes

uint8_t parse_scope(const int file, B_Scope& scope, Lexer& l, LLVM_IR& ir, bool is_function_scope)
{
	uint8_t retval = Success;

	while (l.GetToken() != CLEX_eof) switch (l.GetToken()) {
		case '{':
			// Lexer pos: {

			// Note: if this function is called from parse_function_definition
			//       this case is skipped because the '{' is eaten by that function
			//       this is also the reason the compiler used to fail on closing a
			//       scope, because we were keeping track of how many scopes deep we were.
			// TODO: maybe re-add scope depth

			l.Step();

			if (l.GetToken() != '}') {
				B_Scope next(scope);
				retval = parse_scope(file, next, l, ir, is_function_scope);
				if (retval != Success) NOB_UNREACHABLE("asdasd");
			}

			l.Step();
			break; // breaks switch

		case '}': // Scope ended
			l.Step();
			return retval;

		case CLEX_parse_error:
			nob_log(NOB_ERROR, "CLEX parse error: likely out of buffer space");
			NOB_TODO("Increase buffer size as more is needed");

		default:
			retval =
				parse_line(file, scope, l, ir, !is_function_scope);
			break; // breaks switch
	}

	return retval;
}

#pragma endregion
#pragma region Parse Lines

uint8_t parse_line(const int file, B_Scope& sc, Lexer& l, LLVM_IR& ir, bool is_global_scope)
{
	l.Locate();

	if (!l.Expect(CLEX_id)) {
		nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: expected an identifier such as a funciton name, but got `%s` instead.",
				l.filename, l.location.line_number, l.location.line_offset,
				std::to_string(l.token < 256 ? (char)l.token : l.token).c_str());
		Compilation_error(InvalidSyntax);
		return InvalidSyntax;
	}

	const char* primary = strdup(l.string);
	uint8_t retval = Success;

	switch (check_keyword_identifier(primary)) {
		case Extrn:
			retval =
				parse_extrn_keyword(l, sc.extern_functions, file);
			break; // -> semicolon next

		case Auto:
			retval =
				parse_auto_keyword(l, ir, sc, is_global_scope, file);
			break; // -> semicolon next

		case Return:
			if (!is_global_scope) {
				retval = parse_return_keyword(l, ir, sc, file);
			} else {
				nob_log(NOB_ERROR, "%s:%d:%d: Syntax error: keyword `return` not valid in global scope.",
						l.filename, l.location.line_number, l.location.line_offset);
				Compilation_error(InvalidSyntax);
				retval = InvalidSyntax;
			}
			break; // -> semicolon next

		case NotKeyword: // lhs / rhs situation: could be var assignment, function call, >>=, etc.
			l.Step();

			if (l.Expect(CLEX_id)) {
				nob_log(NOB_ERROR, "%s:%d:%d: Didn't expect identifier `%s` after primary identifier `%s`",
						l.filename, l.location.line_number, l.location.line_offset,
						l.string, primary);
				Compilation_error(InvalidSyntax);
				return InvalidSyntax;
			}

			if (l.Expect(';'))
				break; // -> semicolon next

			if (is_global_scope) {
				if (l.Expect('(')) {
					retval =
						parse_function_definition(file, sc, l, ir, primary);
					return retval; // No semicolon, skip call to semicolon_next
				} else if (l.Expect('=') ||
						   l.Expect(CLEX_id) ||
						   l.Expect(CLEX_intlit) ||
						   l.Expect(CLEX_floatlit) ||
						   l.Expect(CLEX_dqstring)) {

					if (l.Expect('='))
						l.Step();

					retval = parse_assigning_expression(l, primary, sc, ir, is_global_scope, file);

				} else {
					retval = parse_secondary_expression(l, ir, sc, file) != INVALID_VARIABLE ? Success : EverythingCouldBeWrong;
				}
			} else {
				if (l.Expect('='))
					retval = parse_assigning_expression(l, primary, sc, ir, is_global_scope, file);
				else
					retval = parse_secondary_expression(l, ir, sc, file) != INVALID_VARIABLE ? Success : EverythingCouldBeWrong;
			}
			break; // -> semicolon next

		default:
			NOB_TODO("Keyword not yet implemented!");
	}

	l.Semicolon();

	delete primary;
	return retval;
}

#pragma endregion
#pragma region Variables

uint8_t variable_declaration(const char* name, B_Variable_Scope& vsc, const int file, const stb_lex_location& lo, const bool is_gvar)
{
	B_Variable var {
		.name = strdup(name),
		.value_type = Uninitialized,
		.in_file = file,
		.location = lo,
		.global = is_gvar };

	Variable_Id id = vsc.size();

	if (name != nullptr) if (is_variable_redefinition(var, vsc, file)) {
		Compilation_error(VariableRedefinition);
		return VariableRedefinition;
	}

	// TODO: Shadowing check

	gen_alloca(var.ir, id);
	vsc.push_back(var);
	return Success;
}

uint8_t variable_declaration(const char* name, B_Variable_Scope& vsc, const int file, const stb_lex_location& lo, const bool is_gvar, B_Variable& var)
{
	var.name = strdup(name);
	var.location = lo;
	var.in_file = file;
	var.value_type = Uninitialized;
	var.global = is_gvar;

	Variable_Id id = vsc.size();

	if (name && is_variable_redefinition(var, vsc, file)) {
		Compilation_error(VariableRedefinition);
		return VariableRedefinition;
	}

	gen_alloca(var.ir, id);
	return Success;
}

Variable_Id find_variable(const char* name, const B_Variable_Scope& scope)
{
	if (!name)
		NOB_UNREACHABLE("Searched for nullptr named variable");

	if (strcmp(name, "false") == 0)
		return Special_Expr_False;

	if (strcmp(name, "true") == 0)
		return Special_Expr_True;

	for (Variable_Id i = static_cast<Variable_Id>(scope.size()) - 1; i >= FIRST_VARIABLE_ID; --i) {
		if (!scope[i].name)
			continue;

		if (strcmp(scope[i].name, name) == 0)
			return i; // Found
	}

	return INVALID_VARIABLE; // Not found
}

bool is_variable_redefinition(const B_Variable& s, const B_Variable_Scope& vsc, const int file)
{
	for (Variable_Id i = FIRST_VARIABLE_ID; i < static_cast<Variable_Id>(vsc.size()); ++i) {
		if (strcmp(s.name, vsc[i].name) == 0) {
			nob_log(NOB_ERROR, "%s:%d:%d: Variable redefinition: attempted to redefine %s",
					CurrentFile, s.location.line_number, s.location.line_offset, s.name);
			nob_log(NOB_ERROR, "%s:%d:%d: <--- %s is first defined here",
					input_files[vsc[i].in_file].filepath,
					vsc[i].location.line_number,
					vsc[i].location.line_offset,
					vsc[i].name);
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

void backpatch_all_var_decls(LLVM_IR& ir, const B_Variable_Scope& vars)
{
	for (auto& i : vars) {
		switch (i.value_type) {
			case Uninitialized:
				nob_log(NOB_WARNING, "%s:%d:%d: Uninitialized variable: `%s`.",
						input_files[i.in_file].filepath, i.location.line_number, i.location.line_offset, i.name);
				Compilation_error(UnusedVariable);
				break;

			case Float:
			case Void:
			case String:
			case Pointer:
				NOB_TODO("Unimplemented variable type");

			case Invalid_Value_Type:
				NOB_UNREACHABLE("No variable should have this");

			default:
				break;
		}

		ir.append(i.ir);
	}
}

#pragma endregion
#pragma region Assigning

uint8_t parse_assigning_expression(Lexer& l, const char* variable_name, B_Scope& sc, LLVM_IR& ir, const bool asgn_to_gvar, const int file)
{
	// This should be the function to call when you've got `auto var = 2;` and `var = 3;` to handle the assigning part.
	// Lexer: auto variable <<=>> asd + 1;
	// Lexer: a <<=>> asd + 1;

	if (l.token == ';')
		NOB_UNREACHABLE("Call to `variable_assignment` but next token is a semicolon.");

	Variable_Id outcome_id = find_variable(variable_name, sc.localv);

	if (outcome_id == INVALID_VARIABLE) {
		nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: variable `%s` wasn't declared prior to attempting to assign to it.",
				l.filename, l.location.line_number, l.location.line_offset, variable_name);
		Compilation_error(InvalidSyntax);
		return InvalidSyntax;
	}

	Ops op = NoOp;

	switch (l.token) {
		case '=': op = Ops::Equals; break;
		default: if (compilation.IsLangMode(Modernized)) switch (l.token) {
			case CLEX_minuseq: op = Ops::MinusEquals; break;
			case CLEX_pluseq: op = Ops::PlusEquals; break;
			case CLEX_muleq: op = Ops::MultEquals; break;
			case CLEX_diveq: op = Ops::DivEquals; break;
			default: NOB_UNREACHABLE("Modernized Mode: Unimplemented op");
		} else {
			Compilation_error(LangFeatureUnavailable);
			return LangFeatureUnavailable;
		}
	}

	if (!op && !asgn_to_gvar) {
		nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: expected `=` after variable named `%s` but got `%s`.",
				l.filename, l.location.line_number, l.location.line_offset,
				variable_name, l.GetTokenForText().c_str());
		Compilation_error(InvalidSyntax);
		return InvalidSyntax;
	}

#if defined(DEBUG)
	constexpr bool debug = false;
#else
	constexpr bool debug = true;
#endif

	// assert(strcmp(variable_name, sc.localv.at(outcome_id).name) == 0); // For fun
	bool step_lexers = true;

	std::vector<Variable_Id> ab;
	std::vector<Ops> ops;

	ab.reserve(4);
	ops.reserve(4);

	Variable_Id working_id = sc.localv.size();
	Value_Type final_type = Invalid_Value_Type;


	l.StepChecked();
	// Lexer: ... = <<asd>> + 1;

	Lexer la = l;
	Lexer lb = l;

	lb.StepChecked();
	// Lexer A: ... = <<asd>> + 1;
	// Lexer B: ... = asd <<+>> 1;
	// Lexer B is always 1 ahead

	if (asgn_to_gvar) {
		if ((lb.token == ',') || (lb.token == ';') || (lb.token == ')') || (lb.token == '{')) {

		} else {
			NOB_TODO("Support basic math with constants in global variables.");
		}
	}

	do {
		if ((lb.token == ',') || (lb.token == ';') || (lb.token == ')') || (lb.token == '{'))
			step_lexers = false;

		switch (la.token) {
			case CLEX_intlit:
			{
				sc.localv.push_back(B_Variable {
					.in_file = file,
					.location = l.location,
					.ir = debug ? nob_temp_sprintf("  ; Stub generated at %s:%d\n", __FILE__, __LINE__) : "" });
				gen_alloca(ir, working_id);

				sc.localv.push_back(B_Variable {
					.value_type = Int,
					.in_file = file,
					.location = l.location,
					.ir = debug ? nob_temp_sprintf("  ; Stub generated at %s:%d\n", __FILE__, __LINE__) : "" });
				gen_store_rval_to_lval(ir, working_id, std::to_string(la.GetLexer().int_number));
				gen_load_lval_to_lval(ir, working_id + 1, working_id);

				ab.push_back(working_id + 1);

				if (final_type == Invalid_Value_Type)
					final_type = Int;

				break;
			}

			case CLEX_id:
			{
				if (lb.token == '(') {
					if (step_lexers) {
						gen_alloca(ir, working_id);
						ab.push_back(working_id + 1);
					}

					parse_function_call(file, sc, lb, ir, la.string, true);

					sc.localv.push_back(B_Variable {
						.in_file = file,
						.location = l.location });

					sc.localv.push_back(B_Variable {
						.value_type = Int,
						.in_file = file,
						.location = l.location });

					if (final_type == Invalid_Value_Type)
						final_type = Int;

					la.GetLexer() = lb.GetLexer();
					lb.Step();
				} else {
					// lookup variable
					const Variable_Id var1 = find_variable(la.string, sc.localv);
					const Variable_Id var2 = find_variable(la.string, sc.upstreamv);

					switch (((var1 == INVALID_VARIABLE) << 1) + ((var2 == INVALID_VARIABLE) << 0)) {
						case 0b11:
							nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: variable `%s` is not declared.",
									l.filename, l.location.line_number, l.location.line_offset, la.string);
							Compilation_error(InvalidSyntax);
							break;

						case 0b10:
							if (final_type == Invalid_Value_Type)
								final_type = Int;

							if (sc.upstreamv[working_id].global) {
								if (sc.upstreamv[var2].global) {
									gen_store_gval_to_gvar(ir, sc.upstreamv[working_id].name, sc.upstreamv[var2].name);
								} else {
									gen_store_lval_to_gvar(ir, sc.upstreamv[working_id].name, var2);
								}
							} else {
								gen_store_lval_to_lval(ir, working_id, var2);
							}
							break;

						case 0b00:
							nob_log(NOB_WARNING, "%s:%d:%d: Variable `%s` is being shadowed.",
									l.filename, l.location.line_number, l.location.line_offset, la.string);
							Compilation_error(VariableIsShadowed);
							[[fallthrough]];
						case 0b01:
							if (final_type == Invalid_Value_Type)
								final_type = Int;

							if (sc.localv.at(var1).value_type == Uninitialized) {
								ab.push_back(working_id);

								gen_load_lval_to_lval(ir, working_id, var1);
								sc.localv.at(working_id).name = sc.localv.at(var1).name;
								// delete sc.localv.at(var1).name;
							}

							sc.localv.push_back(B_Variable { .in_file = file, .location = l.location });
							break;

						default: NOB_UNREACHABLE("You did a fucky wucky");
					}
				}
				break;
			}

			case CLEX_floatlit: NOB_TODO("Implement Floats");
			case CLEX_dqstring: NOB_TODO("Implement Strings");
			default: NOB_UNREACHABLE("Unexpected token");
		}

		working_id = (Variable_Id)sc.localv.size();

		if (ops.size() > 0) {
			sc.localv.push_back(B_Variable {
				.in_file = file,
				.location = l.location,
				.ir = debug ? nob_temp_sprintf("  ; Stub generated at %s:%d\n", __FILE__, __LINE__) : "" });

			const Variable_Id opa = ab.rbegin()[1];
			const Variable_Id opb = ab.rbegin()[0];

			gen_binary_op(ir, working_id, opa, opb, sc.localv, ops.back());

			if (step_lexers) {
				gen_store_lval_to_lval(ir, working_id + 1, working_id);
				working_id = (Variable_Id)sc.localv.size();
			} else {
				gen_store_lval_to_lval(ir, outcome_id, working_id);
			}
		}

		if (step_lexers) {
			switch (lb.token) {
				case '+': ops.push_back(Plus); break;
				case '-': ops.push_back(Minus); break;
				case '*': ops.push_back(Mult); break;
				case '/': ops.push_back(Div); break;
				case '%': ops.push_back(Mod); break;
				default: NOB_UNREACHABLE("Unexpected token in inline op");
			}

			la.StepChecked(2), lb.StepChecked(2);
		}

	} while (step_lexers);

	// TODO: restore only the parse point to make copy more effective
	l.GetLexer() = lb.GetLexer(); // Update the main lexer to the end of the inline ops
	assert((l.token == ',') || (l.token == ';') || (l.token == ')') || (lb.token == '{'));

	gen_load_lval_to_lval(ir, sc.localv.size(), outcome_id);

	{
		const B_Variable& old = sc.localv.at(outcome_id);
		sc.localv.push_back(B_Variable {
			// Do not copy the LLVM-IR AAAAAAAAAAAAAAAAAAAAAAH
			.name = old.name, // strdup maybe. For now keep the name around for debugging
			.in_file = old.in_file,
			.location = old.location,
			.global = old.global, });
	}

	// Take references after there's no possibility for reallocations
	B_Variable& new_element = sc.localv.back();
	B_Variable& old_element = sc.localv.at(outcome_id);

	// Housekeeping
	switch (old_element.value_type) {
		case Uninitialized:
			new_element.value_type = final_type;
			break;

		case Dead:
			NOB_UNREACHABLE("It probably shouldn't be dead");
			break; // Shut up compiler

		default:
			new_element.value_type = old_element.value_type;
			break;
	}

	old_element.value_type = Dead; // Must be dead'ed so the compiler shuts up
	// delete old_element.name;

	return Success;
}

#pragma endregion
#pragma region Keywords

Keyword check_keyword_identifier(std::string& k)
{
	static std::map<std::string, Keyword> String2Keyword = {
		{ "extrn", Extrn },
		{ "auto", Auto },
		{ "return", Return },
		{ "switch", Switch },
		{ "case", Case },
		{ "if", If },
		{ "else", Else },
		{ "while", While },
		{ "goto", Goto },
	};

	return String2Keyword.at(k);
}

Keyword check_keyword_identifier(const char* k)
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
	}

	return NotKeyword;
}

uint8_t parse_auto_keyword(Lexer& l, LLVM_IR& ir, B_Scope& scope, const bool is_gvar, const int file)
{
	l.Step();
	// auto <<variable>> = 69;

	if (l.token != CLEX_id) {
		nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: expected name of variable after this auto keyword.",
				CurrentFile, l.location.line_number, l.location.line_offset);
		Compilation_error(InvalidSyntax);
		return InvalidSyntax;
	}

	uint8_t retval = variable_declaration(l.string, scope.localv, file, l.location, is_gvar);
	// variable_declaration handles push_back

	switch (l.GetNextToken()) {
		/* case CLEX_eof:
			unexpected_eof("auto variable declaration");
			exit(UnexpectedEndOfFile); */

		case '=':
			if (retval == Success) {
				retval = parse_assigning_expression(l, l.string, scope, ir, is_gvar, file);
			} else {
				NOB_TODO("Stuff that's gonna happen after non succesful variable declaration");
			}
			break;

		case ';':
			break;

		default:
			nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: unexpected token in auto variable declaration for `%s`.",
					CurrentFile, l.location.line_number, l.location.line_offset, l.string);
			Compilation_error(InvalidSyntax);
			return InvalidSyntax;
	}

	return retval;
}

uint8_t parse_extrn_keyword(Lexer& l, B_Function_Scope& extrns, const int file)
{
	if (!l.StepAndExpect(CLEX_id)) {
		nob_log(NOB_ERROR, "Syntax error: expected a name after `extrn`");
		Compilation_error(InvalidSyntax);
		return InvalidSyntax;
	}

	do {
		B_Function e { .name = strdup(l.string), .in_file = file, .location = l.GetLocation() };
		if (is_function_redefinition(e, extrns, file)) {
			Compilation_error(FunctionRedefinition);
		} else {
			extrns.push_back(e);
		}
	} while (false);

	return Success;
}

uint8_t parse_return_keyword(Lexer& l, LLVM_IR& ir, B_Scope& sc, const int file)
{
	l.Step();
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

			if (Int != get_value_type(vid, sc.localv)) {
				nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: cannot return non-integer variable `%s`",
						l.filename, l.location.line_number, l.location.line_offset, l.string);
				Compilation_error(InvalidSyntax);
				return InvalidSyntax;
			}

			gen_return_keyword_lvalue(ir, vid);
		}
		break;

		default:
			nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: unexpected token after return keyword",
					l.filename, l.location.line_number, l.location.line_offset);
			Compilation_error(InvalidSyntax);
			return InvalidSyntax;
	}

	return Success;
}

#pragma endregion
#pragma region Secondary

Variable_Id parse_secondary_expression(Lexer& l, LLVM_IR& ir, B_Scope& sc, const int file)
{
	std::vector<Variable_Id> ab;
	std::vector<Ops> ops;

	// Lexers
	bool step_lexers = true;
	Lexer la = l;
	Lexer lb = l;

	lb.StepChecked();
	// Lexer b is always 1 ahead

	// return c + b - d * e / func()
	// Lexer la pos: c
	// Lexer lb pos: +

	do {
		if ((lb.token == ',') || (lb.token == ';') || (lb.token == ')') || (lb.token == '{'))
			step_lexers = false;

		Variable_Id id = sc.localv.size(); // New element (probably)

		switch (la.token) {
			case CLEX_intlit:
				gen_alloca(ir, id);
				gen_store_rval_to_lval(ir, id, std::to_string(la.GetLexer().int_number));
				gen_load_lval_to_lval(ir, id + 1, id);
				sc.localv.push_back(B_Variable { .in_file = file, .location = l.location });
				sc.localv.push_back(B_Variable { .value_type = Int, .in_file = file, .location = l.location });
				ab.push_back(id + 1);
				break;

			case CLEX_id:
				if (lb.token == '(') {
					gen_alloca(ir, id);
					ab.push_back(id + 1);
					parse_function_call(file, sc, lb, ir, la.string, true);
					sc.localv.push_back(B_Variable { .in_file = file, .location = l.location });
					sc.localv.push_back(B_Variable { .value_type = Int, .in_file = file, .location = l.location });
					la.GetLexer() = lb.GetLexer();
					lb.Step();
				} else {
					// lookup variable
					const Variable_Id var1 = find_variable(la.string, sc.localv);
					const Variable_Id var2 = find_variable(la.string, sc.upstreamv);

					switch (((var1 == INVALID_VARIABLE) << 1) + ((var2 == INVALID_VARIABLE) << 0)) {
						case 0b11:
							nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: variable `%s` is not declared.",
									CurrentFile, l.location.line_number, l.location.line_offset, la.string);
							Compilation_error(InvalidSyntax);
							break;

						case 0b10:
							if (sc.upstreamv[id].global) {
								if (sc.upstreamv[var2].global) {
									gen_store_gval_to_gvar(ir, sc.upstreamv[id].name, sc.upstreamv[var2].name);
								} else {
									gen_store_lval_to_gvar(ir, sc.upstreamv[id].name, var2);
								}
							} else {
								gen_store_lval_to_lval(ir, id, var2);
							}
							break;

						case 0b00:
							nob_log(NOB_WARNING, "%s:%d:%d: Variable `%s` is being shadowed.",
									CurrentFile, l.location.line_number, l.location.line_offset, la.string);
							Compilation_error(VariableIsShadowed);
							[[fallthrough]];
						case 0b01:
							ab.push_back(sc.localv.size() - 1);
							break;

						default: NOB_UNREACHABLE("You did a fucky whucky");
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

			sc.localv.push_back(B_Variable { .in_file = file, .location = l.location });
			sc.localv.push_back(B_Variable { .value_type = Int, .in_file = file, .location = l.location });

			Variable_Id opa = ab.rbegin()[1];
			Variable_Id opb = ab.rbegin()[0];

			gen_binary_op(ir, dest, opa, opb, sc.localv, ops.back());

			ab.push_back(dest);
		}

		if (step_lexers) {
			switch (lb.token) {
				case '+': ops.push_back(Plus); break;
				case '-': ops.push_back(Minus); break;
				case '*': ops.push_back(Mult); break;
				case '/': ops.push_back(Div); break;
				case '%': ops.push_back(Mod); break;
				default: NOB_UNREACHABLE("Unexpected token in inline op");
			}

			la.StepChecked(2), lb.StepChecked(2);
		}

	} while (step_lexers);

	// End deliminated by , or ;

	// TODO: restore only the parse point to make copy more effective
	l.GetLexer() = lb.GetLexer(); // Update the main lexer to the end of the inline ops
	assert((l.token == ',') || (l.token == ';') || (l.token == ')') || (lb.token == '{'));

	return ab.back();
}

#pragma endregion
#pragma region Functions

uint8_t parse_function_call(const int file, B_Scope& sc, Lexer& l, LLVM_IR& ir, const char* callee, const bool force_retvalgen)
{
	// Lexer pos: (
	assert(l.token == '(');

	LLVM_IR& parameters = sc.localv.at(0).ir;
	NOB_UNUSED(parameters);

	if (!l.StepAndExpect(')')) {
		for (; (l.token != ',') && (l.token != ')'); l.Step()) { // Parameters
			NOB_UNREACHABLE("Implement parameters");
		}
	}

	assert(l.token == ')');
	// Lexer pos: )

	stb_lexer lc = l.GetLexer();
	step_lexer(lc);
	assert(lc.token != '{'); // check to see if this is a function definition

	B_Variable b_retval;

	const B_Function f {
		.name = strdup(callee),
		.in_file = file,
		.calls = 1,
		.location = l.location };

	const Function_Id fid = find_function(callee, sc.functions);
	const Function_Id efid = find_function(callee, sc.extern_functions);

	switch (((fid == INVALID_FUNCTION) << 1) + ((efid == INVALID_FUNCTION) << 0)) {
		case 0b00:
			nob_log(NOB_ERROR, "%s:%d:%d: Ambiguous function call: `%s` is an `extrn` function and B function.",
					CurrentFile, l.location.line_number, l.location.line_offset, callee);
			Compilation_error(FunctionRedefinition);
			[[fallthrough]];
		case 0b10:
			if (force_retvalgen) {
				gen_funccall(ir, sc.localv.size(), callee);
			} else {
				gen_funccall(ir, callee);
			}

			sc.functions[fid].calls += 1;
			break;

		case 0b01:
			gen_funccall_extrn(ir, sc.localv.size(), callee);
			if (!force_retvalgen)
				sc.localv.push_back(b_retval);

			sc.extern_functions[efid].calls += 1;
			break;

		case 0b11:
			// Generate the funccall now, assuming it exists as a B function
			// Backpatching will error out as functions don't exist.
			// This removes forward declaring
			if (force_retvalgen) {
				gen_funccall(ir, sc.localv.size(), callee);
			} else {
				gen_funccall(ir, callee);
			}

			sc.functions.push_back(f);
			break;

		default: NOB_UNREACHABLE("funccall");
	}

	return Success;
	// handling semicolon outside
}

uint8_t parse_function_definition(const int file, B_Scope& sc, Lexer& l, LLVM_IR& ir, const char* name)
{
	// Lexer: main<<(>>) { ...
	assert(l.Expect('('));

	if (!l.StepAndExpect(')')) { // Must parse parameters next
		NOB_TODO("Implement function arguments");
	}
	// Lexer pos: )

	assert(l.StepAndExpect('{'));
	// Lexer pos: {

	static bool nested = false;
	// Nested function detection. Since everything in this compiler is
	// seemingly done using recursion, this is trivial.

	if (nested) {
		nob_log(NOB_ERROR, "%s:%d:%d: Nesting functions is not supported.",
				l.filename, l.location.line_number, l.location.line_offset);
		Compilation_error(NestedFunction);
		exit(NestedFunction);
	}

	B_Function f {
		.name = strdup(name), // memory leak
		.in_file = file,
		.definitions = 1, // This is the definition basically, and is disregarded if it already is defined
		.location = l.location };

	bool is_main = false;

	if (strcmp(f.name, "main") == 0) { // Make `main` special
		f.calls = WHERES_YOUR_GOD_NOW;
		is_main = true;

		if (compilation.has_entry)
			Compilation_error(MultipleEntryPoints);

		compilation.has_entry = true;
	}

	Function_Id fid = find_function(name, sc.functions);
	Function_Id efid = find_function(name, sc.extern_functions);

	switch (((fid == INVALID_FUNCTION) << 1) + ((efid == INVALID_FUNCTION) << 0)) {
		case 0b00: // Both valid, meaning they were defined before
			nob_log(NOB_ERROR, "%s:%d:%d: Ambiguous function: `%s` is defined in multiple ways:",
					l.filename, l.location.line_number, l.location.line_offset, name);
			nob_log(NOB_ERROR, "%s:%d:%d: <--- `%s` is defined as a function here.",
					input_files[sc.functions[fid].in_file].filepath,
					sc.functions[fid].location.line_number,
					sc.functions[fid].location.line_offset,
					sc.functions[fid].name);
			nob_log(NOB_ERROR, "%s:%d:%d: <--- `%s` is defined as an `extrn` function here.",
					input_files[sc.extern_functions[efid].in_file].filepath,
					sc.extern_functions[efid].location.line_number,
					sc.extern_functions[efid].location.line_offset,
					sc.extern_functions[efid].name);
			Compilation_error(FunctionRedefinition);
			break;

		case 0b10: // means this name was defined as an extrn function
			nob_log(NOB_ERROR, "%s:%d:%d: Ambiguous function: `%s` is already defined as an extern function.",
					CurrentFile, l.location.line_number, l.location.line_offset, name);
			nob_log(NOB_ERROR, "%s:%d:%d: <--- `%s` is first defined here.",
					input_files[sc.extern_functions[efid].in_file].filepath,
					sc.extern_functions[efid].location.line_number,
					sc.extern_functions[efid].location.line_offset,
					sc.extern_functions[efid].name);
			Compilation_error(FunctionRedefinition);
			break;

		case 0b01: // means a function with this name already exists (not extrn)
			if (sc.functions[fid].definitions > 0) {
				if (is_main)
					Compilation_error(MultipleEntryPoints);

				its_function_redefinition(f, sc.functions[fid]);
				Compilation_error(FunctionRedefinition);
				// Fall through intentional
			}

			sc.functions[fid].definitions += 1;
			sc.functions[fid].in_file = file;
			sc.functions[fid].location = l.location;
			break;

		case 0b11: // means I can define whatever I needs to be defined
			sc.functions.push_back(f);
			break;

		default: NOB_UNREACHABLE("Is this yours?");
	}

	uint8_t retval = Success;
	LLVM_IR inner;

	gen_func_begin(ir, f);

	switch (l.GetNextToken()) { // TODO: Reconsider stepping ahead here
		case '}':
			l.Step();
			[[fallthrough]];
		case ';':
			// nob_log(NOB_WARNING, "%s:%d:%d: Warning: empty function: `%s`",
			// 		File, sc.lex_location.line_number, sc.lex_location.line_offset, name);
			// Compilation_error(FunctionEmpty);
			// return FunctionEmpty;
			inner.append("  ; Empty function\n");
			break;

			// TODO: I should probably care about unexpected tokens here
		default:
		{
			nested = true;
			B_Scope next(sc);
			retval = parse_scope(file, next, l, inner, true);
			backpatch_all_var_decls(ir, next.localv);
			nested = false;
		}
		break;
	}

	ir.append(inner);
	gen_func_end(ir);
	return retval;
}

void backpatch_all_function_calls(const int file, const B_Scope& scope)
{
	for (auto& i : scope.functions) {
		if (i.definitions > 1) {
			nob_log(NOB_INFO, "%s:%d:%d: The function `%s`, is defined multiple times.",
					input_files[i.in_file].filepath, i.location.line_number, i.location.line_offset, i.name);
		}

		if ((i.definitions > 0) && (i.calls == 0)) {
			nob_log(NOB_INFO, "%s:%d:%d: The function `%s`, defined here, is not called.",
					input_files[i.in_file].filepath, i.location.line_number, i.location.line_offset, i.name);
			Compilation_error(FunctionNeverCalled);
		}

		if ((i.definitions == 0) && (i.calls > 0)) {
			nob_log(NOB_INFO, "%s:%d:%d: The function `%s` is called %d %s but never defined.",
					// Yes, the function location contains the function call location, until it gets defined.
					CurrentFile, i.location.line_number, i.location.line_offset,
					i.name, i.calls, i.calls == 1 ? "time" : "times");
			Compilation_error(FunctionNeverDefined);
		}
	}

	if ((!compilation.has_entry) && compilation.wants_executable)
		Compilation_error(NoEntryPoint);
}

Function_Id find_function(const char* name, const B_Function_Scope& scope)
{
	for (Function_Id i = FIRST_FUNCTION_ID; i < (signed)scope.size(); ++i) {
		if (strcmp(scope[i].name, name) == 0) {
			return i;
		}
	}
	return INVALID_FUNCTION;
}

void its_function_redefinition(const B_Function& current, const B_Function& previous)
{
	nob_log(NOB_ERROR, "%s:%d:%d: Function redefinition: attempted to redefine `%s`.",
			input_files[current.in_file].filepath,
			current.location.line_number,
			current.location.line_offset,
			current.name);
	nob_log(NOB_ERROR, "%s:%d:%d: <--- `%s` is first defined here.",
			input_files[previous.in_file].filepath,
			previous.location.line_number,
			previous.location.line_offset,
			previous.name);
}

bool is_function_redefinition(const B_Function& new_f, const B_Function_Scope& fsc, const int file)
{
	for (Function_Id i = FIRST_FUNCTION_ID; i < (signed)fsc.size(); ++i) {
		if (strcmp(new_f.name, fsc[i].name) == 0) {
			nob_log(NOB_ERROR, "%s:%d:%d: Function redefinition: attempted to redefine `%s`.",
					input_files[file].filepath,
					new_f.location.line_number,
					new_f.location.line_offset, new_f.name);
			nob_log(NOB_ERROR, "%s:%d:%d: <--- `%s` is first defined here.",
					input_files[fsc[i].in_file].filepath,
					fsc[i].location.line_number,
					fsc[i].location.line_offset,
					fsc[i].name);
			return true;
		}
	}
	return false;
}

#pragma endregion
#pragma region Compilation

void compilation_error(Returns e, const char* compiler_file, const int file_line)
{
	// static uint16_t error_count = 0;
	// static uint16_t warning_count = 0;

	switch (e) {
		// Warnings
		case FunctionEmpty:
		case UnusedVariable:
		case VariableIsShadowed:
		case FunctionNeverCalled:
			++compilation.warnings;
			break;


			// nob_log(NOB_ERROR, "No function definitions found in any passed file. Define main as `main() { ... }`");
			// ++compilation.errors;
			// break;

		case NoEntryPoint:
			nob_log(NOB_ERROR, "A function was found, but there was no main function. Define main as `main() { ... }`");
			++compilation.errors;
			compilation.stop = true;
			break;

		case EntryPointEmpty:
			nob_log(NOB_ERROR, "Encountered `main` function definition, but it was empty.");
			++compilation.errors;
			compilation.stop = true;
			break;

		case MultipleEntryPoints:
			nob_log(NOB_ERROR, "Encountered multiple definitions of `main`");
			++compilation.errors;
			compilation.stop = true;
			break;


			// Stop later
		case FileEmpty:
		case InvalidSyntax:
		case ExpectedSemicolon:
		case FunctionRedefinition:
			++compilation.errors;
			if (compilation.errors >= MAX_ERRORS_BEFORE_STOP)
				compilation.stop = true;
			break;


			// Immediately stop
		case FunctionNeverDefined:
		case NoFilesGiven:
		case EverythingCouldBeWrong:
		case ErrorReadInput:
		case ErrorWriteOutput:
		case InvalidTargetTriple:
		case UnexpectedArguments:
		case UnexpectedEndOfFile:
		case LangFeatureUnavailable:
			++compilation.errors;
			compilation.stop = true;
			break;

			// if (input_files.size() > 1)
			// 	return;

			// Ignored
		case CompilationHadWarnings:
		case ClangNonZeroExitcode:
		case Success:
			return;

		default:
			NOB_UNREACHABLE("Unhandled compilation error");
	}

	if (compilation.stop) {
		nob_log(NOB_INFO, "Stopping compilation: %d errors, %d warnings", compilation.errors, compilation.warnings);
#if defined(DEBUG)
		if ((compiler_file != nullptr) && (file_line > -1))
			nob_log(NOB_INFO, "%s:%d:%d: <--- compilation stopped here in compiler.", compiler_file, file_line, 1);
#endif
		exit(e);
	} else return;
}

bool dispatch_clang(std::string& output_file)
{
	bool delete_ir = false;
	switch (compilation.GetIROutput()) {
		case DontCompile: return true;
		case KeepIrAfterCompile: break;
		case DeleteIrAfterCompile:
#if defined(ENABLE_FILE_DELETIONS) && !defined(DEBUG)
			delete_ir = true;
#endif
			break;
		default: NOB_UNREACHABLE("Unhandled case for keeping or deleting IR");
	}

	bool use_custom_output = false;
	if (!output_file.empty()) {
		use_custom_output = true;

		const char* extension = strrchr(output_file.data(), '.');

		if (strcmp(extension, "o") == 0)
			compilation.wants_executable = false;
	}

	static Nob_Cmd clang_cmd;
	static Nob_Procs procs;

	static bool first = true;
	if (first) {
		memset(&clang_cmd, 0, sizeof(clang_cmd));
		memset(&procs, 0, sizeof(procs));
		first = false;
	} else {
		clang_cmd.count = 0;
		procs.count = 0;
	}

	if (compilation.wants_executable && (input_files.size() == 1)) {
		std::basic_string ll_file = swap_extension(input_files[0].filepath, "ll");
		nob_cmd_append(&clang_cmd, "clang");
		nob_cmd_append(&clang_cmd, "-o", use_custom_output ? output_file.data() : chop_extension(input_files[0].filepath));
		nob_cmd_append(&clang_cmd, ll_file.data());

		if (!nob_cmd_run(&clang_cmd))
			return false;

		if (delete_ir)
			nob_delete_file(ll_file.data());

		return true;
	}

	// Compile our generated LLVM IR to an object
	for (int i = 0; i < (int)input_files.size(); ++i) {
		std::basic_string ll_file = swap_extension(input_files[i].filepath, "ll");
		std::basic_string obj_file = swap_extension(input_files[i].filepath, "o");

		nob_cmd_append(&clang_cmd, "clang", "-c", "-o", obj_file.data(), ll_file.data());

		if (!nob_cmd_run(&clang_cmd, .async = &procs))
			return false;

		clang_cmd.count = 0;
	}

	if (!nob_procs_wait(procs))
		return false;

	procs.count = 0;

	// Create executable
	if (compilation.wants_executable) {
		nob_cmd_append(&clang_cmd, "clang");
		nob_cmd_append(&clang_cmd, "-o", use_custom_output ? output_file.data() : chop_extension(input_files[0].filepath));

		for (int i = 0; i < (int)input_files.size(); ++i) {
			if (delete_ir)
				nob_delete_file(swap_extension(input_files[i].filepath, "ll"));

			nob_cmd_append(&clang_cmd, swap_extension(input_files[i].filepath, "o"));
		}

		if (!nob_cmd_run(&clang_cmd, .async = &procs))
			return false;

		clang_cmd.count = 0;

		if (!nob_procs_wait(procs))
			return false;

		procs.count = 0;
	}

	return true;
}

// void update_state(const bool is_main, const int file, uint8_t fut)
// {
	// if (/* strcmp(sym, "main") == 0 */ is_main) {
	// 	if (compilation.file_states[file] > FOUND_A_FUNC) {
	// 		switch (compilation.file_states[file] - FOUND_A_FUNC) {
	// 		case 0: // empty main
	// 			nob_log(NOB_WARNING, "Multiple `main`s: found an empty main in %s", File);
	// 			break;

	// 		case 1: // main
	// 			nob_log(NOB_WARNING, "Multiple `main`s: found a main in %s", File);
	// 			break;

	// 		default: // don't do anything on MULTIPLE_MAIN
	// 			break;
	// 		}
	// 		compilation.file_states[file] = MULTIPLE_MAIN;
	// 	} else if (fut > compilation.file_states[file]) {
	// 		compilation.file_states[file] = fut + 2;
	// 	}

	// 	if (compilation.state > FOUND_A_FUNC) {
	// 		switch (compilation.global - FOUND_A_FUNC) {
	// 		case 0: // empty main
	// 			nob_log(NOB_WARNING, "Multiple `main`s: multiple empty mains found"); break;
	// 		case 1: // main
	// 			nob_log(NOB_WARNING, "Multiple `main`s: "); break;
	// 		default: // don't do anything on MULTIPLE_MAIN
	// 			break;
	// 		}
	// 		compilation.global = MULTIPLE_MAIN;
	// 	} else if (fut > compilation.global) {
	// 		compilation.global = fut + 2;
	// 	}
	// } else {
	// 	if (fut > compilation.file) {
	// 		compilation.file = fut;
	// 	}

	// 	if (fut > compilation.global) {
	// 		compilation.global = fut;
	// 	}
	// }
// }

// bool get_file_state(void)
// {
	// switch (compilation.file) {
	// case HAS_NOTHING:
	// 	nob_log(NOB_ERROR, "No function definitions found in any passed file. Define main as `main() { ... }`");
	// 	break;

	// case FOUND_EMPTY_MAIN:
	// 	nob_log(NOB_ERROR, "Encountered main function definition, but it was empty.");
	// 	break;

	// case MULTIPLE_MAIN:
	// 	nob_log(NOB_ERROR, "Encountered multiple definitions of `main`");
	// 	break;

	// default: return true; // Good state
	// }
	// return false;
// }

#pragma endregion