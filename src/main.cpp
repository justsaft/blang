
#include <stdio.h>

#include "types.hpp"
#include "gen_ir.hpp"
#include "common.hpp"
#include "clex_wrapper.hpp"
#include "backend.hpp"
#include "compilation.hpp"

extern "C"
{
#include "../3rd-party/nob.h"
#include "output.h"

	void set_minimal_nob_log_level(Nob_Log_Level level);
}

template <size_t N>
constexpr size_t nob_countof(const char* const (&)[N])
{
	return N;
}

#undef nob_cmd_append
#define nob_cmd_append(cmd, ...) \
    do { \
        const char* nob_tmp[] = { __VA_ARGS__ }; \
        nob_da_append_many((cmd), nob_tmp, \
        nob_countof(nob_tmp)); \
    } while (0)


// Driver
bool dispatch_clang(const std::string& output_file_name);


// Parsing
Returns parse_file(const int file, B_Scope&, Lexer&, LLVM_IR&);
Returns parse_scope(const int file, B_Scope&, Lexer&, LLVM_IR&);
Returns parse_line(const int file, B_Scope&, Lexer&, LLVM_IR&);


// Parse functions
Variable_Id parse_expression(const int file, B_Scope&, Lexer&, LLVM_IR&);
Returns parse_assigning_expression(const int file, B_Scope&, Lexer&, LLVM_IR&, const char* name);
Returns parse_function_definition(const int file, B_Scope&, Lexer&, LLVM_IR&, const char* name);
Returns parse_function_call(const int file, B_Scope&, Lexer&, LLVM_IR&, const char* name, const bool force_retvalgen = false);
void backpatch_all_function_calls(const int file, const B_Scope& scope);


// Parse keywords
Returns parse_auto_keyword(Lexer&, LLVM_IR&, B_Scope&, const int file);
Returns parse_if_keyword(Lexer&, LLVM_IR&, B_Scope&, const int file);
Returns parse_else_keyword(Lexer&, LLVM_IR&, B_Scope&, const int file);
Returns parse_switch_keyword(Lexer&, LLVM_IR&, B_Scope&, const int file);
Returns parse_goto_keyword(Lexer&, LLVM_IR&, B_Scope&, const int file);
Returns parse_case_keyword(Lexer&, LLVM_IR&, B_Scope&, const int file);
Returns parse_while_keyword(Lexer&, LLVM_IR&, B_Scope&, const int file);
Returns parse_extrn_keyword(Lexer&, B_Function_Scope&, const int file);
Returns parse_return_keyword(Lexer&, LLVM_IR&, B_Scope&, const int file);


// Declaration
Returns variable_declaration(const int file, B_Scope&, LLVM_IR&, const stb_lex_location&, const char* name);
Returns variable_declaration(const int file, B_Scope&, LLVM_IR&, const stb_lex_location&, const char* name, B_Variable&);


// Find & Redefinitions
Function_Id find_function(const char* name, const B_Function_Scope& scope);
Variable_Id find_variable(const char* name, const B_Variable_Scope& scope);
Variable_Id find_variable_both_scopes(const char* name, B_Scope&, Lexer& l);
void its_function_redefinition(const B_Function& current, const B_Function& previous);
bool is_function_redefinition(const B_Function& new_f, const B_Function_Scope& scope, const int file);
bool is_variable_redefinition(const B_Variable& new_v, const B_Variable_Scope& scope, const int file);


Backend backend(CLANG);

// Data Storage
Compilation c { };

B_Function_Scope B_Scope::functions { };
B_Function_Scope B_Scope::extern_functions { };
ssize_t B_Scope::global_variables { };

local B_Files input_files { };


// Misc
Keywords check_statement_identifier(const char*);
Keywords check_statement_identifier(const std::string&);
// Value_Type get_value_type(Variable_Id, const B_Variable_Scope&);
// Value_Type get_value_type_from_lval(const char* name, const B_Variable_Scope&);
const char* returnsno2str(Returns);


// Lexer Util (clex_util.cpp)
bool semicolon_next(stb_lexer&, const char* filename, bool advance_pre = false, bool advance_post = true);
void get_lexer_location(stb_lexer&, stb_lex_location&);
void step_lexer(stb_lexer&);
long get_next_token(stb_lexer&);
long peak_next_token(stb_lexer&);
bool get_and_expect_token(stb_lexer&, const long token);
bool expect_token(stb_lexer&, const long token);


// cli.cpp
void parse_cli_arguments(int argc, char** argv, B_Files&, Compilation&);


#pragma region Main

int main(int argc, char** argv)
{
	parse_cli_arguments(argc, argv, input_files, c);
	setup_ir_gen();

	if (input_files.size() == 0) {
		nob_log(NOB_ERROR, "No input files were provided. Specify at least one `.b` file.");
		Compilation_error(NoFilesGiven);
	}

	std::string current_triple = backend.GetTargetTriple(); // 60ms - 90ms

	if (c.target.empty()) {
		c.target.append(current_triple);
	} else {
		nob_log(NOB_INFO, "Compiling for %s on %s", c.target.c_str(), current_triple.c_str());
	}

	B_Scope global_scope;
	Lexer lexer;
	LLVM_IR file_ir;

	file_ir.resize(256);

#if defined(DEBUG) && (COMPILATION_RECAP == ENABLE)
	std::string resultsbuf;
#endif

	for (int file = 0; file < (int)input_files.size(); ++file) { // For each file
		if (_debug) nob_log(NOB_INFO, "%s: compiling", CurrentFile);

		CurrentFileState =
			// parse_file parses until end of file
			parse_file(file, global_scope, lexer, file_ir);

		backpatch_all_function_calls(file, global_scope);

#if defined(DEBUG) && (COMPILATION_RECAP == ENABLE)
		resultsbuf.append(
			nob_temp_sprintf("%-40s | %3d:  %-16s\n",
							 CurrentFile, CurrentFileState, returnsno2str(CurrentFileState)));
#elif defined(DEBUG)
		nob_log(NOB_INFO, "%s: result: %d (%s))",
				CurrentFile, CurrentFileState, returnsno2str(CurrentFileState));
#endif

		if (c.stop)
			break;

		else if (!write_ll_file(CurrentFile, file_ir.data(), file_ir.size())) {
			nob_log(NOB_WARNING, "Couldn't write IR for file %s", CurrentFile);
			CurrentFileState = ErrorWritingIrOfFile;
			Compilation_error(ErrorWritingIrOfFile);
		}

		file_ir.clear();
		global_scope.functions.clear();
		global_scope.extern_functions.clear();
	}

#if defined(DEBUG) && (COMPILATION_RECAP == ENABLE)
	fprintf(stderr, "\nCompilation recap:\n%s\n", resultsbuf.c_str());
#endif

	if (c.errors) {
		nob_log(NOB_INFO, "Compilation: %*d errors, %*d warnings", 3, c.errors, 3, c.warnings);
		c.state = input_files.back().state;
	} else {
		if (c.warnings)
			nob_log(NOB_INFO, "Compilation: %*d warnings", 3, c.warnings);

		if ((!c.stop) && (!dispatch_clang(c.output)))
			c.state = BackendNonZeroExitcode;
	}

	return c.state;
}

#pragma endregion
#pragma region Parse Files

Returns parse_file(const int file, B_Scope& global, Lexer& lexer, LLVM_IR& file_ir)
{
	gen_file_info(file_ir, CurrentFile);
	Returns retval = Success;

	retval =
		lexer.InitAndLoadFile(CurrentFile);

	if (retval != Success) {
		Compilation_error(retval);
		file_ir.append("\n\n; There was an error reading the source file.");
		return retval;
	}

	LLVM_IR inner;

	retval =
		parse_scope(file, global, lexer, inner);

	gen_all_func_decls(file_ir, global.functions);
	file_ir.append(inner);
	gen_attr_group(file_ir, 0);

	return retval;
}

#pragma endregion
#pragma region Parse Scopes

Returns parse_scope(const int file, B_Scope& scope, Lexer& l, LLVM_IR& ir)
{
	Returns retval = Success;

	while (l.GetToken() != CLEX_eof) switch (l.GetToken()) {
		case '{':
			// Lexer pos: {

			// Note: if this function is called from parse_function_definition
			//       this case is skipped because the '{' is eaten by that function
			//       this is also the reason the compiler used to fail on closing a
			//       scope, because we were keeping track of how many scopes deep we were.

			l.Step();

			if (l.GetToken() != '}') {
				B_Scope next(scope);
				retval = parse_scope(file, next, l, ir);
				if (retval != Success) NOB_UNREACHABLE("asdasd");
			}

			l.Step();
			break; // breaks switch

		case '}': // Scope ended
			l.Step();
			return retval;

		case CLEX_parse_error:
			NOB_UNREACHABLE("CLEX parse error: likely out of buffer space");

		default:
			retval =
				parse_line(file, scope, l, ir);
			break; // breaks switch
	}

	return retval;
}

#pragma endregion
#pragma region Parse Lines

Returns parse_line(const int file, B_Scope& sc, Lexer& l, LLVM_IR& ir)
{
	local std::string primary;
	l.Locate();

	if (!l.Expect(CLEX_id)) {
		nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: expected an identifier such as a function name, but got `%s` instead.",
				l.filename, l.location.line_number, l.location.line_offset,
				std::to_string(l.token < 256 ? (char)l.token : l.token).c_str());
		Compilation_error(InvalidSyntax);
		return InvalidSyntax;
	}

	primary.assign(l.string);

	Returns retval = Success;

	switch (check_statement_identifier(primary)) {
		case Switch:
			if (sc.is_switch) {
				NOB_UNREACHABLE("Nested switch cases not implemented");
			} else {
				retval = parse_switch_keyword(l, ir, sc, file); return retval;
			}
			return retval; // No semicolon

		case Case:
			if (sc.is_switch) {
				retval = parse_case_keyword(l, ir, sc, file); return retval;
				return retval;
			} else {
				nob_log(NOB_ERROR, "%s:%d:%d: Syntax error: keyword `case` not valid outside of a switch body.",
						l.filename, l.location.line_number, l.location.line_offset);
				return InvalidSyntax;
			}

		case If: retval = parse_if_keyword(l, ir, sc, file); break; // -> semicolon next
		case Else: retval = parse_else_keyword(l, ir, sc, file); break; // -> semicolon next
		case While: retval = parse_while_keyword(l, ir, sc, file); break; // -> semicolon next
		case Goto: retval = parse_goto_keyword(l, ir, sc, file); break; // -> semicolon next
		case Extrn: retval = parse_extrn_keyword(l, sc.extern_functions, file); break; // -> semicolon next
		case Auto: retval = parse_auto_keyword(l, ir, sc, file); break; // -> semicolon next

		case Return:
			if (sc.IsGlobalScope()) {
				nob_log(NOB_ERROR, "%s:%d:%d: Syntax error: keyword `return` not valid in global scope.",
						l.filename, l.location.line_number, l.location.line_offset);
				Compilation_error(InvalidSyntax);
				retval = InvalidSyntax;
			} else {
				retval = parse_return_keyword(l, ir, sc, file);
			}
			break; // -> semicolon next

		case NotKeyword: // lhs / rhs situation: could be var assignment, function call, >>=, etc.
			l.Step();

			if (l.Expect(CLEX_id)) {
				nob_log(NOB_ERROR, "%s:%d:%d: Didn't expect identifier `%s` after primary identifier `%s`",
						l.filename, l.location.line_number, l.location.line_offset,
						l.string, primary.c_str());
				Compilation_error(InvalidSyntax);
				return InvalidSyntax; // No semicolon next
			}

			if (l.Expect(';')) break; // -> semicolon next

			if (sc.IsGlobalScope()) {
				if (l.Expect('(')) {
					retval =
						parse_function_definition(file, sc, l, ir, primary.c_str());
					return retval; // No semicolon, skip call to semicolon_next
				} else if (l.Expect('=') ||
						   l.Expect(CLEX_id) ||
						   l.Expect(CLEX_intlit) ||
						   l.Expect(CLEX_floatlit) ||
						   l.Expect(CLEX_dqstring)) {

					if (l.Expect('='))
						l.Step();

					retval = parse_assigning_expression(file, sc, l, ir, primary.c_str());

				} else {
					retval = parse_expression(file, sc, l, ir) >= FIRST_VARIABLE_ID
						? Success : EverythingCouldBeWrong;
				}
			} else {
				if (l.Expect('=')) {
					retval = parse_assigning_expression(file, sc, l, ir, primary.c_str());
				} else {
					retval = parse_expression(file, sc, l, ir) >= FIRST_VARIABLE_ID
						? Success : EverythingCouldBeWrong;
				}
			}
			break; // -> semicolon next

		case Invalid_Keyword:
			NOB_UNREACHABLE("Invalid keyword");
	}

	if (l.Semicolon() == ExpectedSemicolon) {
		Compilation_error(ExpectedSemicolon);
		retval = ExpectedSemicolon;
	}

	return retval;
}

#pragma endregion
#pragma region Variables

Returns variable_declaration(const int file, B_Scope& sc, LLVM_IR& ir, const stb_lex_location& lo, const char* name)
{
	assert(name);

	B_Variable var {
		.name = strdup(name),
		.value_type = Uninitialized,
		.in_file = file,
		.location = lo,
		.global = sc.IsGlobalScope() };

	if (is_variable_redefinition(var, sc.Variables(), file)) {
		Compilation_error(VariableRedefinition);
		return VariableRedefinition;
	}

	Variable_Id uvid = find_variable(name, sc.UpstreamVariables());

	if (uvid >= FIRST_VARIABLE_ID) {
		nob_log(NOB_WARNING, "%s:%d:%d: declared variable `%s` is shadowing a variable outside of its scope.",
				CurrentFile, lo.line_number, lo.line_offset, name);
		const B_Variable& var = sc.UpstreamVariables()[uvid];
		nob_log(NOB_WARNING, "%s:%d:%d: <--- before declared here",
				input_files[var.in_file].filepath, var.location.line_number, var.location.line_offset);

		Compilation_error(VariableIsShadowed);
		return VariableIsShadowed;
	}

	gen_alloca(ir, sc.GetTopOfStack());
	sc.Variables().push_back(var);
	return Success;
}

Returns variable_declaration(const int file, B_Scope& sc, LLVM_IR& ir, const stb_lex_location& lo, const char* name, B_Variable& var)
{
	assert(name);

	var.name = strdup(name);
	var.location = lo;
	var.in_file = file;
	var.value_type = Uninitialized;
	var.global = sc.IsGlobalScope();

	if (is_variable_redefinition(var, sc.Variables(), file)) {
		Compilation_error(VariableRedefinition);
		return VariableRedefinition;
	}

	// TODO: Why does this not have the upstream vars check again?
	// This is not called at this time so I won't care

	gen_alloca(ir, sc.GetTopOfStack());
	return Success;
}

Variable_Id find_variable(const char* name, const B_Variable_Scope& vsc)
{
	if (!name)
		NOB_UNREACHABLE("Searched for nullptr named variable");

	if (strcmp(name, "false") == 0)
		return Boolean_Literal_False;

	if (strcmp(name, "true") == 0)
		return Boolean_Literal_True;

	for (Variable_Id i = static_cast<Variable_Id>(vsc.size()) - 1; i >= FIRST_VARIABLE_ID; --i) {
		if (!vsc[i].name)
			continue;

		if (strcmp(vsc[i].name, name) == 0)
			return i; // Found
	}

	return INVALID_VARIABLE; // Not found
}

bool is_variable_redefinition(const B_Variable& s, const B_Variable_Scope& vsc, const int file)
{
	for (Variable_Id i = FIRST_VARIABLE_ID; i < static_cast<Variable_Id>(vsc.size()); ++i) {
		if (!vsc[i].name)
			continue;

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

// Value_Type get_value_type(Variable_Id id, const B_Variable_Scope& scope)
// {
// 	return scope[id].value_type;
// }

// Value_Type get_value_type_from_lval(const char* name, const B_Variable_Scope& scope)
// {
// 	Variable_Id v = find_variable(name, scope);
// 	if (v != INVALID_VARIABLE) {
// 		return scope[v].value_type;
// 	}
// 	return Invalid_Value_Type;
// }
// TODO: these are kinda "dangerous" in this iteration of things

void backpatch_all_var_decls(LLVM_IR& ir, const B_Variable_Scope& vsc)
{
	(void)ir;

	for (auto& i : vsc) {
		// ir.append(i.ir);

		if (i.value_type == Uninitialized) {
			nob_log(NOB_WARNING, "%s:%d:%d: Uninitialized variable: `%s`.",
					input_files[i.in_file].filepath, i.location.line_number, i.location.line_offset, i.name);
			Compilation_error(UnusedVariable);
			break;
		}
	}
}

Variable_Id find_variable_both_scopes(const char* name, B_Scope& sc, Lexer& l)
{
	const Variable_Id var1 = find_variable(name, sc.Variables());
	const Variable_Id var2 = find_variable(name, sc.UpstreamVariables());

	switch (((var1 != INVALID_VARIABLE) << 1) +
			((var2 != INVALID_VARIABLE) << 0)) {

		case 0b00:
			nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: variable `%s` is not declared.",
					l.filename, l.location.line_number, l.location.line_offset, name);
			Compilation_error(InvalidSyntax);
			return INVALID_VARIABLE;

		case 0b01:
			assert(!sc.IsGlobalScope());
			// TODO: Make global scope actually useful

			if (sc.UpstreamVariables()[var2].global) {
				NOB_TODO("assign to variable in non-global context with a value from global context");
			} else {
				return sc.UpstreamId2StackId(var2);
			}

		case 0b11:
			nob_log(NOB_WARNING, "%s:%d:%d: Variable `%s` is being shadowed.",
					l.filename, l.location.line_number, l.location.line_offset, name);
			Compilation_error(VariableIsShadowed);
			[[fallthrough]];

		case 0b10:
			return sc.LocalId2StackId(var1);

		default: NOB_UNREACHABLE("This condition should literally be impossible");
	}
}

#pragma endregion
#pragma region Assigning

Returns parse_assigning_expression(const int file, B_Scope& sc, Lexer& l, LLVM_IR& ir, const char* name)
{
	// This should be the function to call when you've got `auto var = 2;` and `var = 3;` to handle the assigning part.
	// Lexer: auto variable <<=>> asd + 1;
	// Lexer: a <<=>> asd + 1;

	Returns retval = Success;
	Ops op = NoOp;

	switch (l.token) {
		case ';': NOB_UNREACHABLE("Call to `variable_assignment` but next token is a semicolon.");
		case '=': op = Ops::Equals; break;

		case CLEX_minuseq:
			if (!c.IsLangMode(Modernized)) {
				nob_log(NOB_ERROR, "Assignment with `-=` is only available in modernised language mode.");
				Compilation_error(LangFeatureUnavailable);
				retval = LangFeatureUnavailable;
			} else op = Ops::MinusEquals;
			break;

		case CLEX_pluseq:
			if (!c.IsLangMode(Modernized)) {
				nob_log(NOB_ERROR, "Assignment with `+=` is only available in modernised language mode.");
				Compilation_error(LangFeatureUnavailable);
				retval = LangFeatureUnavailable;
			} else op = Ops::PlusEquals;
			break;

		case CLEX_muleq:
			if (!c.IsLangMode(Modernized)) {
				nob_log(NOB_ERROR, "Assignment with `*=` is only available in modernised language mode.");
				Compilation_error(LangFeatureUnavailable);
				retval = LangFeatureUnavailable;
			} else op = Ops::MultEquals;
			break;

		case CLEX_diveq:
			if (!c.IsLangMode(Modernized)) {
				nob_log(NOB_ERROR, "Assignment with `/=` is only available in modernised language mode.");
				Compilation_error(LangFeatureUnavailable);
				retval = LangFeatureUnavailable;
			} else op = Ops::DivEquals;
			break;

		default:
			if (c.IsLangMode(Modernized))
				NOB_UNREACHABLE("Modernized Mode: Unimplemented op");
			else
				NOB_UNREACHABLE("Unimplemented op");
	}

	Variable_Id original_id = find_variable_both_scopes(name, sc, l);

	switch ((Reserved_Variable_Ids)original_id) {
		case INVALID_VARIABLE:
			nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: variable `%s` wasn't declared prior to attempting to assign to it.",
					l.filename, l.location.line_number, l.location.line_offset, name);
			Compilation_error(InvalidSyntax);
			retval = InvalidSyntax;
			break;

		case Boolean_Literal_False:
			NOB_TODO("Boolean_Literal_False is not handled yet.");

		case Boolean_Literal_True:
			NOB_TODO("Boolean_Literal_True is not handled yet.");

		default:
			if (original_id < FIRST_VARIABLE_ID) // confusing check
				NOB_UNREACHABLE("Unhandled special variable id");

			assert(strcmp(name, sc.GetVariableFromStack(original_id).name) == 0);
	}

	std::vector<Variable_Id> ab;
	std::vector<Ops> ops;

	ab.reserve(6);
	ops.reserve(6);

	l.StepChecked();
	// Breaks the assert(name, ...) later if the function is called with .string
	// of this lexer, which is gonna be different after the .Step()
	// TODO: Maybe remove the assert and strdup(_name) later
	// Lexer: ... = <<asd>> + 1;

	Lexer la = l;
	Lexer lb = l;

	lb.StepChecked();
	// Lexer A: ... = <<asd>> + 1;
	// Lexer B: ... = asd <<+>> 1;
	// Lexer B is always 1 ahead

	if (sc.IsGlobalScope()) {
		NOB_TODO("Assign to global variables");
	}

	bool step_lexers = true;
	Variable_Id working_id = sc.GetTopOfStack();

	do {
		if ((lb.token == ',') || (lb.token == ';') || (lb.token == ')') || (lb.token == '{'))
			step_lexers = false;

		switch (la.token) {
			case CLEX_intlit:
				sc.Variables().push_back(B_Variable {
					.in_file = file,
					.location = l.location, });
				gen_alloca(ir, working_id);

				sc.Variables().push_back(B_Variable {
					.value_type = Int,
					.in_file = file,
					.location = l.location, });
				gen_store_rval_to_lval(ir, working_id, std::to_string(la.GetLexer().int_number));
				gen_load_lval_to_lval(ir, working_id + 1, working_id);

				ab.push_back(working_id + 1);
				break;

			case CLEX_id:
				if (lb.token == '(') {
					if (step_lexers) {
						gen_alloca(ir, working_id);
						ab.push_back(working_id + 1);
					}

					retval = parse_function_call(file, sc, lb, ir, la.string, true);
					Compilation_error(retval);

					sc.Variables().push_back(B_Variable {
						.in_file = file,
						.location = l.location });

					sc.Variables().push_back(B_Variable {
						.value_type = Int,
						.in_file = file,
						.location = l.location });

					la.GetLexer() = lb.GetLexer();
					lb.Step();
				} else {
					// lookup variable
					ab.push_back(find_variable_both_scopes(la.string, sc, l));
				}
				break;

			case CLEX_floatlit: NOB_TODO("Implement Floats");
			case CLEX_dqstring: NOB_TODO("Implement Strings");
			default: NOB_UNREACHABLE("Unexpected token");
		}

		working_id = sc.GetTopOfStack();

		if (ops.size() > 0) {
			sc.Variables().push_back(B_Variable {
				.in_file = file,
				.location = l.location, });

			assert(ab.size() >= 2);
			gen_binary_op(ir, working_id, ab.rbegin()[1], ab.rbegin()[0], sc, ops.back());

			if (step_lexers) {
				ab.push_back(working_id);
				working_id = sc.GetTopOfStack();
			} else {
				gen_store_lval_to_lval(ir, original_id, working_id);
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
	assert((l.token == ',') || (l.token == ';') || (l.token == ')') || (l.token == '{'));

	// Generate the load instruction for the variable after generating ops for it
	if (ops.size()) switch (op) {
		case Ops::Equals: gen_load_lval_to_lval(ir, working_id + 1, original_id); break;
		default: NOB_UNREACHABLE("Op not implemented");
	}

	B_Variable& var = sc.Variables().back();

	switch (((bool)ops.size() << 1) +
			((original_id >= FIRST_VARIABLE_ID) << 0)) {

		case 0b00:
			var.value_type = Invalid_Value_Type;
			var.in_file = file;
			var.location = l.location;
			break;

		case 0b11:
		{
			B_Variable& work = sc.GetVariableFromStack(working_id);
			B_Variable& orig = sc.GetVariableFromStack(original_id);

			sc.Variables().push_back(B_Variable {
				.name = orig.name,
				.value_type = work.value_type,
				.in_file = work.in_file,
				.location = work.location, });

			orig.value_type = Dead;

			assert(strcmp(name, sc.Variables().back().name) == 0);
			break;
		}

		case 0b10:
			sc.Variables().push_back(B_Variable {
				// .name = "deadvar",
				.value_type = Invalid_Value_Type,
				.in_file = file,
				.location = l.location, });
			break;

		case 0b01:
		{
			B_Variable& old = sc.GetVariableFromStack(original_id);

			var.name = old.name;
			var.in_file = old.in_file;
			var.location = old.location;
			old.value_type = Dead;

			assert(strcmp(name, sc.Variables().back().name) == 0);
			break;
		}

		default: NOB_UNREACHABLE("This condition should literally be impossible");
	}

	// free((void*)name);
	return retval;
}

#pragma endregion
#pragma region Keywords

Keywords check_statement_identifier(const std::string& k)
{
	const static std::unordered_map<std::string, Keywords> String2Keyword = {
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

	auto it = String2Keyword.find(k);

	if (it == String2Keyword.end())
		return NotKeyword;

	return it->second;
}

Keywords check_statement_identifier(const char* k)
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

Returns parse_label(Lexer& l, LLVM_IR& ir, B_Scope& scope, const int file)
{
	// Lexer:

	scope.labels.insert({ l.string, scope.labels.size() });
	ir.append(nob_temp_sprintf("!%zu", scope.labels.size() - 1));

	// auto it = labels.find(name);

	// if (it == labels.end())
	// 	NOB_UNREACHABLE("")
		// return InvalidSyntax;


	// ir += nob_temp_sprintf("!%d", it->second);
	return Success;
}

Returns parse_goto_keyword(Lexer& l, LLVM_IR& ir, B_Scope& scope, const int file)
{
	NOB_UNREACHABLE("goto not yet implemented");
}

Returns parse_while_keyword(Lexer& l, LLVM_IR& ir, B_Scope& scope, const int file)
{
	NOB_UNREACHABLE("while not yet implemented");

}

Returns parse_else_keyword(Lexer& l, LLVM_IR& ir, B_Scope& scope, const int file)
{
	NOB_UNREACHABLE("else not yet implemented");

}

Returns parse_if_keyword(Lexer& l, LLVM_IR& ir, B_Scope& scope, const int file)
{
	NOB_UNREACHABLE("if not yet implemented");

}

Returns parse_case_keyword(Lexer& l, LLVM_IR& ir, B_Scope& scope, const int file)
{
	NOB_UNREACHABLE("case not yet implemented");

}

Returns parse_switch_keyword(Lexer& l, LLVM_IR& ir, B_Scope& scope, const int file)
{
	NOB_UNREACHABLE("switch not yet implemented");

}

Returns parse_auto_keyword(Lexer& l, LLVM_IR& ir, B_Scope& scope, const int file)
{
	l.Step();
	// auto <<variable>> = 69;

	local std::string keyword;
	keyword.assign(l.string);

	if (l.token != CLEX_id) {
		nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: expected name of variable after this auto keyword.",
				CurrentFile, l.location.line_number, l.location.line_offset);
		Compilation_error(InvalidSyntax);
		return InvalidSyntax;
	}

	Returns retval = variable_declaration(file, scope, ir, l.GetLocation(), keyword.c_str());
	// variable_declaration handles push_back

	switch (l.GetNextToken()) {
		/* case CLEX_eof:
			unexpected_eof("auto variable declaration");
			exit(UnexpectedEndOfFile); */

			// TODO: I should probably care about unexpected tokens

		case '=':
			if (retval == Success) {
				retval = parse_assigning_expression(file, scope, l, ir, keyword.c_str());
			} else {
				NOB_TODO("Stuff that's gonna happen after non succesful variable declaration");
			}
			break;

		case ';':
			break;

		default:
			nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: unexpected token in auto variable declaration for `%s`.",
					CurrentFile, l.location.line_number, l.location.line_offset, keyword.c_str());
			Compilation_error(InvalidSyntax);
			return InvalidSyntax;
	}

	// delete name;

	return retval;
}

Returns parse_extrn_keyword(Lexer& l, B_Function_Scope& extrns, const int file)
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

Returns parse_return_keyword(Lexer& l, LLVM_IR& ir, B_Scope& sc, const int file)
{
	l.Step();
	// Lexer pos: return <<asd>>; next

	Returns retval = Success;

	switch (l.token) {
		case ';':
			gen_return_keyword(ir);
			break;

		case CLEX_floatlit:
		case CLEX_id:
		case CLEX_intlit:
		{
			Variable_Id vid = parse_expression(file, sc, l, ir);
			// Lexer pos: ';' ideally

			if (Int != sc.GetVariableFromStack(vid).value_type) {
				nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: cannot return non-integer variable `%s`",
						l.filename, l.location.line_number, l.location.line_offset, l.string);
				Compilation_error(InvalidSyntax);
				retval = InvalidSyntax;
			}

			gen_return_keyword_lvalue(ir, vid);
		}
		break;

		default:
			nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: unexpected token after return keyword",
					l.filename, l.location.line_number, l.location.line_offset);
			Compilation_error(InvalidSyntax);
			retval = InvalidSyntax;
	}

	return retval;
}

#pragma endregion
#pragma region Secondary

Variable_Id parse_expression(const int file, B_Scope& sc, Lexer& l, LLVM_IR& ir)
{
	std::vector<Variable_Id> ab;
	std::vector<Ops> ops;

	ab.reserve(6);
	ops.reserve(6);

	// Lexers
	bool step_lexers = true;
	Lexer la = l;
	Lexer lb = l;

	lb.StepChecked();
	// Lexer b is always 1 ahead

	// return c + b - d * e / func()
	// Lexer la pos: c
	// Lexer lb pos: +

	Variable_Id output = sc.GetTopOfStack();

	do {
		if ((lb.token == ',') || (lb.token == ';') || (lb.token == ')') || (lb.token == '{'))
			step_lexers = false;

		switch (la.token) {
			case CLEX_intlit:
				gen_alloca(ir, output);
				gen_store_rval_to_lval(ir, output, std::to_string(la.GetLexer().int_number));
				gen_load_lval_to_lval(ir, output + 1, output);
				sc.Variables().push_back(B_Variable { .in_file = file, .location = l.location });
				sc.Variables().push_back(B_Variable { .value_type = Int, .in_file = file, .location = l.location });
				ab.push_back(output + 1);
				break;

			case CLEX_id:
				if (lb.token == '(') {
					gen_alloca(ir, output);
					sc.Variables().push_back(B_Variable { .in_file = file, .location = l.location });
					Returns retval = parse_function_call(file, sc, lb, ir, la.string, true);
					if (retval != Success)
						Compilation_error(retval);
					sc.Variables().push_back(B_Variable { .value_type = Int, .in_file = file, .location = l.location });
					la.GetLexer() = lb.GetLexer();
					lb.Step();
					ab.push_back(output + 1);
				} else {
					// lookup variable
					ab.push_back(find_variable_both_scopes(la.string, sc, l));
#if 0
					const Variable_Id var1 = find_variable(la.string, sc.Variables());
					const Variable_Id var2 = find_variable(la.string, sc.UpstreamVariables());

					switch (((var1 != INVALID_VARIABLE) << 1) +
							((var2 != INVALID_VARIABLE) << 0)) {

						case 0b00:
							nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: variable `%s` is not declared.",
									CurrentFile, l.location.line_number, l.location.line_offset, la.string);
							Compilation_error(InvalidSyntax);
							ab.push_back(INVALID_VARIABLE);
							break;

						case 0b01:
						{
							const B_Variable& var = sc.UpstreamVariables()[var2];
							if (var.global) {
								gen_alloca(ir, output);
								gen_load_gvar_to_lvar(ir, output, var.name);
								ab.push_back(sc.GetTopOfStack());
								sc.Variables().push_back(var);
								sc.Variables().back().global = false;
							} else {
								ab.push_back(sc.UpstreamId2StackId(var2));
							}
							break;
						}

						case 0b11:
							nob_log(NOB_WARNING, "%s:%d:%d: Variable `%s` is being shadowed.",
									CurrentFile, l.location.line_number, l.location.line_offset, la.string);
							Compilation_error(VariableIsShadowed);
							[[fallthrough]];

						case 0b10:
							ab.push_back(sc.LocalId2StackId(var1));
							break;

						default: NOB_UNREACHABLE("You did a fucky whucky");
					}
#endif
				}
				break;

			case CLEX_floatlit: NOB_TODO("Implement Floats");
			case CLEX_dqstring: NOB_TODO("Implement Strings");
			default: NOB_UNREACHABLE("Unexpected token");
		}

		output = sc.GetTopOfStack();

		if (ops.size() > 0) {
			// Create a variable to be used as the destination of the result
			gen_alloca(ir, output);
			++output;

			sc.Variables().push_back(B_Variable { .in_file = file, .location = l.location });
			sc.Variables().push_back(B_Variable { .value_type = Int, .in_file = file, .location = l.location });

			assert(ab.size() >= 2);
			gen_binary_op(ir, output, ab.rbegin()[1], ab.rbegin()[0], sc, ops.back());

			ab.push_back(output);
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
	assert((l.token == ',') || (l.token == ';') || (l.token == ')') || (l.token == '{'));

	assert(ab.size() >= 1);

	return ab.back();
}

#pragma endregion
#pragma region Functions

Returns parse_function_call(const int file, B_Scope& sc, Lexer& l, LLVM_IR& ir, const char* callee, const bool force_retvalgen)
{
	// Lexer pos: (
	assert(l.token == '(');

	// LLVM_IR& parameters = sc.Variables().at(0).ir;
	// NOB_UNUSED(parameters);
	// TODO: Function parameters

	if (!l.StepAndExpect(')')) {
		for (; (l.token != ',') && (l.token != ')'); l.Step()) { // Parameters
			NOB_UNREACHABLE("Implement parameters");
		}
	}

	assert(l.token == ')');
	// Lexer pos: )

	assert(l.PeakNextTokenAndExpect('{')); // check to see if this is a function definition

	B_Variable b_retval;

	const B_Function f {
		.name = strdup(callee),
		.in_file = file,
		.calls = 1,
		.location = l.location };

	const Function_Id bfid = find_function(callee, sc.functions);
	const Function_Id efid = find_function(callee, sc.extern_functions);

	switch (((bfid == INVALID_FUNCTION) << 1) +
			((efid == INVALID_FUNCTION) << 0)) {

		case 0b00:
			nob_log(NOB_ERROR, "%s:%d:%d: Ambiguous function call: `%s` is an `extrn` function and B function.",
					CurrentFile, l.location.line_number, l.location.line_offset, f.name);
			Compilation_error(FunctionRedefinition);
			[[fallthrough]];

		case 0b10:
			if (force_retvalgen) {
				gen_funccall(ir, sc.GetTopOfStack(), callee);
			} else {
				gen_funccall(ir, callee);
			}

			sc.functions[bfid].calls += 1;
			break;

		case 0b01:
			gen_funccall_extrn(ir, sc.GetTopOfStack(), f.name);
			if (!force_retvalgen)
				sc.Variables().push_back(b_retval);

			sc.extern_functions[efid].calls += 1;
			break;

		case 0b11:
			// Generate the funccall now, assuming it exists as a B function
			// Backpatching will error out as functions don't exist.
			// This removes forward declaring
			if (force_retvalgen) {
				gen_funccall(ir, sc.Variables().size(), callee);
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

Returns parse_function_definition(const int file, B_Scope& sc, Lexer& l, LLVM_IR& ir, const char* name)
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

		if (c.has_entry)
			Compilation_error(MultipleEntryPoints);

		c.has_entry = true;
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

	Returns retval = Success;

	gen_func_begin(ir, f);

	switch (l.GetNextToken()) {
		// TODO: Reconsider stepping ahead here
		// TODO: I should probably care about unexpected tokens here

		case '}':
			l.Step();
			nob_log(NOB_ERROR, "%s:%d:%d: empty function `%s`",
					CurrentFile, l.location.line_number, l.location.line_offset, name);
			retval = FunctionEmpty;
			Compilation_error(FunctionEmpty);
			ir.append("  ; Empty function\n");
			break;

		default:
		{
			nested = true;
			LLVM_IR inner;
			B_Scope next(sc);
			retval = parse_scope(file, next, l, inner);
			backpatch_all_var_decls(ir, next.Variables());
			ir.append(inner);
			nested = false;
			break;
		}
	}

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
			nob_log(NOB_INFO, "%s:%d:%d: The defined function `%s` is not called.",
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

	if ((!c.has_entry) && c.wants_executable)
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
		case UnusedVariable:
		case VariableIsShadowed:
		case FunctionNeverCalled:
			++c.warnings;
			break;

		case NoEntryPoint:
			// Assumes that compilation WANTS an executable,
			// (which logically implies that there must be an entry point)
			nob_log(NOB_ERROR, "No entry point was found. Define main as `main() { ... }`");
			++c.errors;
			c.stop = true;
			break;

		case EntryPointEmpty:
			// A function in LLVM IR "must have at least one basic block" aka some code, so we stop compiling.
			nob_log(NOB_ERROR, "Found entry function `main`, but it is empty.");
			++c.errors;
			c.stop = true;
			break;

		case MultipleEntryPoints:
			nob_log(NOB_ERROR, "Encountered multiple definitions of `main`");
			++c.errors;
			c.stop = true;
			break;


			// Stop later
		case FunctionEmpty:
		case FileEmpty:
		case InvalidSyntax:
		case ExpectedSemicolon:
		case FunctionRedefinition:
			++c.errors;
			if (c.errors >= MAX_ERRORS_BEFORE_STOP)
				c.stop = true;
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
			++c.errors;
			c.stop = true;
			break;

		case BackendNonZeroExitcode:
			c.stop = true;
			break;


		case Success: NOB_UNREACHABLE(nob_temp_sprintf("`%s` (%d) passed to `compilation_error`", returnsno2str(e), e));
		default: NOB_UNREACHABLE(nob_temp_sprintf("compilation_error: `%s` (%d) not implemented.", returnsno2str(e), e));
	}

	if (c.stop) {
		nob_log(NOB_INFO, "Stopping compilation: %*d errors, %*d warnings", 3, c.errors, 3, c.warnings);
		if (_debug) {
			if ((compiler_file != nullptr) && (file_line > -1))
				nob_log(NOB_INFO, "%s:%d:%d: <--- compilation stopped here in compiler.", compiler_file, file_line, 1);
		}
		exit(e);
	} else return;
}

const char* returnsno2str(Returns no)
{
	switch (no) {
		case Success: return "Success";
		case CompilationHadWarnings: return "Compilation had warnings";
		case BackendNonZeroExitcode: return "Backend non zero exitcode";
		case EverythingCouldBeWrong: return "Everything could be wrong";
		case NoFilesGiven: return "No files given";
		case NoFunctions: return "No functions";
		case NoArgumentsGiven: return "No arguments given";
		case FileEmpty: return "File empty";
		case FunctionEmpty: return "Function empty";
		case EntryPointEmpty: return "Entry point empty";
		case UnexpectedArguments: return "Unexpected arguments";
		case UnexpectedEndOfFile: return "Unexpected end of file";
		case InvalidSyntax: return "Invalid syntax";
		case IsThisYours: return "Is this yours?";
		case VariableRedefinition: return "Variable redefinition";
		case FunctionRedefinition: return "Function redefinition";
		case ErrorReadInput: return "Read error";
		case ErrorWriteOutput: return "Write error";
		case InvalidTargetTriple: return "Invalid target triple";
		case FunctionNeverCalled: return "Function never called";
		case FunctionNeverDefined: return "Function never defined";
		case UnusedVariable: return "Unused variable";
		case ExpectedSemicolon: return "Expected semicolon";
		case ErrorWritingIrOfFile: return "Error writing IR of file";
		case VariableIsShadowed: return "Variable is shadowed";
		case NestedFunction: return "Nested function";
		case UnsupportedTarget: return "Unsupported target";
		case NoEntryPoint: return "No entry point";
		case MultipleEntryPoints: return "Multiple entry points";
		case LangFeatureUnavailable: return "Language feature unavailable";
		case TotalAmountOfReturns: NOB_UNREACHABLE(nob_temp_sprintf("returns2str: `TotalAmountOfReturns` (%d) should not be getting passed here", no));
		case FileNotCompiledYet: return "File not compiled yet";
		default: NOB_UNREACHABLE("returnsno2str: unhandled case.");
	}
}

bool dispatch_clang(const std::string& output_file)
{
	bool delete_ir = false;
	switch (c.GetIROutput()) {
		case DontCompile: return true;
		case DeleteIrAfterCompile:
#if (FILE_DELETIONS == ENABLE) && !defined(DEBUG)
			delete_ir = true; [[fallthrough]];
#endif
		case KeepIrAfterCompile: break;
		default: NOB_UNREACHABLE("Unhandled case for keeping or deleting IR");
	}

	bool use_custom_output = false;
	if (!output_file.empty()) {
		use_custom_output = true;

		const char* extension = strrchr(output_file.data(), '.');

		if (strcmp(extension, "o") == 0)
			c.wants_executable = false;
	}

	static Nob_Cmd clang_cmd { };
	static Nob_Procs procs { };

	clang_cmd.count = 0;
	procs.count = 0;

	if (c.wants_executable && (input_files.size() == 1)) {
		std::string ll_file = swap_extension(input_files[0].filepath, "ll");

		for (uint8_t a = 0; backend.GetCommand()[a] != nullptr; ++a) {
			nob_cmd_append(&clang_cmd, backend.GetCommand()[a]); // hack I don't really like
		}

		nob_cmd_append(&clang_cmd, "-o");

		if (use_custom_output) nob_cmd_append(&clang_cmd, output_file.data());
		else nob_cmd_append(&clang_cmd, chop_extension(input_files[0].filepath));

		nob_cmd_append(&clang_cmd, ll_file.data());

		if (!nob_cmd_run(&clang_cmd))
			return false;

		if (delete_ir)
			nob_delete_file(ll_file.data());

		return true;
	}

	// Compile our generated LLVM IR to an object
	for (int i = 0; i < (int)input_files.size(); ++i) {
		std::string ll_file = swap_extension(input_files[i].filepath, "ll");
		std::string obj_file = swap_extension(input_files[i].filepath, "o");

		for (uint8_t a = 0; backend.GetCommand()[a] != nullptr; ++a) {
			nob_cmd_append(&clang_cmd, backend.GetCommand()[a]);
		}

		nob_cmd_append(&clang_cmd, "-c", "-o", obj_file.data(), ll_file.data());

		if (!nob_cmd_run(&clang_cmd, .async = &procs))
			return false;

		clang_cmd.count = 0;
	}

	if (!nob_procs_wait(procs))
		return false;

	procs.count = 0;

	if (c.wants_executable) {
		for (uint8_t a = 0; backend.GetCommand()[a] != nullptr; ++a) {
			nob_cmd_append(&clang_cmd, backend.GetCommand()[a]);
		}

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

// void update_state(const bool is_main, const int file, Returns fut)
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