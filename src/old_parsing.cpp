#if OLD_PARSING
void parse_function_body(stb_lexer& l, std::string& final_func_ir, B_Function_Scope&, const B_Variable_Scope&, const B_Function_Scope&);
void parse_function_parameters(stb_lexer&, B_Variable_Scope&, uint8_t& out_amount_params);
void munch_parameters(stb_lexer&, uint8_t& out_amount_params);
#endif

#if OLD_PARSING
void parse_function_body(
    stb_lexer& l,
    std::string& final_func_ir,
    B_Function_Scope& extrns,
    const B_Variable_Scope& gvars,
    const B_Function_Scope& functions)
{
    NOB_UNUSED(gvars);
    NOB_UNUSED(functions);

    std::string func_body_ir;
    B_Variable_Scope vars;

    // Note: Assumes the lexer has already been stepped past
    // the opening curly brace '{' of the function body, or the last semicolon
    while (l.token != '}') {
        if (l.token == CLEX_id) {
            const char* lhs = l.string;

            stb_lex_location location;
            get_lexer_location(l, location);

            if (strcmp(lhs, "extrn") == 0) { // extrn keyword
                parse_extrn_keyword(l, location, extrns);
            }
            else if (strcmp(lhs, "auto") == 0) { // auto keyword
                parse_auto_keyword(l, location, func_body_ir, vars);
            }
            else if (strcmp(lhs, "return") == 0) { // return keyword
                if (get_and_expect_token(l, CLEX_intlit)) { // rvalue
                    gen_return_keyword_rvalue(func_body_ir, l.int_number);
                    get_and_expect_semicolon(l);
                }
                else if (expect_token(l, CLEX_id)) { // lvalue
                    uint32_t srcval = find_variable(lhs, vars); // BUG: used lhs instead of rhs
                    assert(srcval > 0);
                    gen_return_keyword_lvalue(func_body_ir, srcval);
                    get_and_expect_semicolon(l);
                }
                else if (expect_token(l, ';')) { // 0
                    gen_return_keyword(func_body_ir);
                }
                else { // unexpected token
                    nob_log(NOB_ERROR, "%s:%d:%d: invalid syntax: unexpected token after return keyword", input_files[filei], location.line_number, location.line_offset);
                    exit(InvalidSyntax);
                }
            }
            // TODO: Add keywords here
            else { // lhs / rhs situation: could be var assignment, function call, <<=, etc.
                if (get_and_expect_token(l, '(')) { // function call
                    uint8_t amount_params = 0;
                    if (!get_and_expect_token(l, ')')) { // Must parse parameters next
                        parse_function_parameters(l, vars, amount_params);
                    }
                    vars.push_back(B_Variable { }); // ugly hack
                    // TODO: figure out if the function even exists
                    gen_funccall(func_body_ir, vars.size(), lhs);
                    get_and_expect_semicolon(l);
                }
                else if (expect_token(l, '=')) { // assignment
                    step_lexer(l);
                    if (l.token == CLEX_intlit) { // rvalue (int)
                        gen_assignment_rvalue(func_body_ir, find_variable(lhs, vars), std::to_string(l.int_number));
                        get_and_expect_semicolon(l);
                    }
                    else if (l.token == CLEX_floatlit) { // rvalue (float), won't work
                        gen_assignment_rvalue(func_body_ir, find_variable(lhs, vars), std::to_string(l.real_number));
                        get_and_expect_semicolon(l);
                    }
                    else if (l.token == CLEX_id) { // rhs lvalue or funccall
                        const char* rhs = l.string;
                        if (get_and_expect_token(l, '(')) { // function call
                            // trying to assign return value of function call to variable
                            uint8_t amount_params = 0;
                            if (!get_and_expect_token(l, ')')) { // Must parse parameters next
                                parse_function_parameters(l, vars, amount_params);
                            }

                            B_Variable r; // TODO: this var NEEDS a decl
                            r.value_type = Int;
                            r.location = location;
                            r.file = filei;
                            vars.push_back(r);

                            gen_funccall(func_body_ir, vars.size(), rhs);
                            get_and_expect_semicolon(l);
                        }
                        else if (expect_token(l, ';')) { // assignment
                            uint32_t srcval = find_variable(rhs, vars);
                            uint32_t destval = find_variable(lhs, vars);
                            assert(destval != 0);
                            assert(srcval != 0);
                            gen_assignment_lval_to_lval(func_body_ir, destval, srcval);
                            get_and_expect_semicolon(l);
                        }
                        else {
                            NOB_UNREACHABLE("unexpected or unimplemented token after assignment lhs rhs");
                        }
                    }
                    else {
                        nob_log(NOB_ERROR, "%s:%d:%d: invalid syntax: unexpected token after assignment to %s", input_files[filei], location.line_number, location.line_offset, lhs);
                        exit(InvalidSyntax);
                    }
                }
                else {
                    NOB_TODO("Unimplemented token on lhs / rhs operation");
                }
            }
        }
        else {
            NOB_TODO("Enountered something other than CLEX_id in function body");
        }
        step_lexer(l);
    }

    gen_all_var_decls(final_func_ir, vars);
    final_func_ir.append(func_body_ir);
}

void parse_function_parameters(stb_lexer& l, B_Variable_Scope& scope, uint8_t& out_amount_params)
{
    do {
        if (l.token == ')') { // We're already past '('
            break;
        }
        else if (l.token == ',') {
            step_lexer(l);
        }
        else if (l.token == CLEX_id) {
            stb_lex_location location;
            stb_c_lexer_get_location(&l, l.where_firstchar, &location);

            if (strcmp(l.string, "auto") != 0) {
                nob_log(NOB_ERROR, "Syntax error: expected variable type declaration");
                exit(InvalidSyntax);
            }

            if (!get_and_expect_token(l, CLEX_id)) {
                ;
            }

            const char* name = l.string;

            if (get_and_expect_token(l, '=')) {
                nob_log(NOB_ERROR, "%s:%d:%d: Syntax error: no default assignment in function parameter list is supported, but used for %s", input_files[filei], location.line_number, location.line_offset, name);
                exit(InvalidSyntax);
            }

            B_Variable p;
            p.name = name;
            p.value_type = Int;
            p.file = filei;
            p.location = location;

            gen_func_parameter(p.ir, scope.size());
            scope.push_back(p);

            ++out_amount_params;
        }
        else {
            nob_log(NOB_ERROR, "Syntax error: unexpected token while parsing function parameter list");
            exit(InvalidSyntax);
        }
    }
    while (l.token != ',');
}

void munch_parameters(stb_lexer& l, uint8_t& out_amount_params)
{
    do {
        if (l.token == ')') { // We're already past '('
            break;
        }
        else if (l.token == ',') {
            step_lexer(l);
        }
        else if (l.token == CLEX_id) {
            stb_lex_location location;
            stb_c_lexer_get_location(&l, l.where_firstchar, &location);

            if (!get_and_expect_token(l, CLEX_id)) {
                ;
            }

            const char* name = l.string;

            if (get_and_expect_token(l, '=')) {
                nob_log(NOB_ERROR, "%s:%d:%d: Syntax error: no default assignment in function parameter list is supported, but used for %s", input_files[filei], location.line_number, location.line_offset, name);
                exit(InvalidSyntax);
            }

            ++out_amount_params;
        }
        else {
            nob_log(NOB_ERROR, "Syntax error: unexpected token while parsing function parameter list");
            exit(InvalidSyntax);
        }
    }
    while (l.token != ',');
}
#endif


// main loop
#if OLD_PARSING
while (stb_c_lexer_get_token(&l)) { // For each line in this file
    if (l.token < 256) {
        nob_log(NOB_ERROR, "Unexpected %c", (char)l.token);
        exit(InvalidSyntax);
    }
    else if (l.token == CLEX_eof /*&& !retval*/) {
        // TODO: retval approach, "if (l.token == CLEX_eof" approach or while loop approach
        //if (file_state == States::HAS_NOTHING) {
        //	nob_log(NOB_INFO, "Unexpected EOF in %s. Expected to get any token or function definition.", input_files[filei]);
        //	// TODO: Check if the last token is missing
        //	// TODO: Check if we should use retval instead to end the loop
        //	//       No adjustment needed as diagnosis is already handled elsewhere.
        //	//       Use of do {} while () is also possible.
        //	//       Final implementation depends on which part of the code should print the error.
        //	//       This bit irrelevant if the while loops ends when the EOF is reached.
        //	//       The state HAS_NOTHING is irrelevant for file_state if we end the loop and print here.
        //	//       Since here I'm not expecting to get another specific token I have the choice
        //}
        break; // Advances to the next file
    }
    else if (l.token == CLEX_parse_error) {
        nob_log(NOB_ERROR, "CLEX parse error: there's probably an issue with the buffer");
        exit(EverythingCouldBeWrong);
    }
    else if (l.token == CLEX_id) { // string literals without quotes
        stb_lex_location location;
        const char* lhs = l.string;
        get_lexer_location(l, location);
        nob_log(NOB_INFO, "%s:%d:%d: Found: clex id: %s", input_files[filei], location.line_number, location.line_offset, lhs);

        if (strcmp(lhs, "auto") == 0) {
            parse_auto_keyword(l, location, file_ir, gvariables, true);
        }
        else if (strcmp(lhs, "extrn") == 0) {
            parse_extrn_keyword(l, location, externals);
        }
        else if (expect_token(l, ';')) {
            continue;
        }
        else if (get_and_expect_token(l, '(')) {
            B_Function f;

            if (!get_and_expect_token(l, ')')) { // Must parse parameters next
                parse_function_parameters(l, gvariables, f.amount_params);
            }

            /* std::string func_ir; */

            f.name = lhs;
            f.location = location;
            f.filei = filei;

            detect_symbol_redef(f, functions);
            functions.push_back(f);

            if (!get_and_expect_token(l, '{')) {
                nob_log(NOB_ERROR, "Expected function body to start with '{'");
                exit(InvalidSyntax);
            }

            gen_func_begin(file_ir, f);

            if (get_and_expect_token(l, '}')) { // Check if the body is empty
                nob_log(NOB_WARNING, "%s:%d:%d: Warning: encountered empty function: %s", input_files[filei], location.line_number, location.line_offset, lhs);
                update_state(st, lhs, input_files[filei], States::FOUND_EMPTY_FUNC);
            }
            else {
                parse_function_body(l, file_ir, externals, gvariables, functions);
                update_state(st, lhs, input_files[filei], States::FOUND_A_FUNC);
            }

            gen_func_end(file_ir);
            /* file_ir.append(func_ir); */
        }

    }
    else {
        nob_log(NOB_WARNING, "Token not grabbed: %ld", l.token);
    }
}
#endif

/* if (l.token == CLEX_intlit)
                { // rvalue (int)
                    gen_assignment_rvalue(ir, find_variable(primary, sc.localv), std::to_string(l.int_number));
                    get_and_expect_semicolon(l);
                }
                else if (l.token == CLEX_id)
                { // rhs lvalue or funccall
                    const char* rhs = l.string;
                    if (get_and_expect_token(l, '('))
                    { // trying to assign return value of function call to variable
                        if (!get_and_expect_token(l, ')'))
                        { // Must parse parameters next
                            NOB_TODO("Implement function parameters for function calls 2");
                        }

                        B_Variable v; // TODO: this var NEEDS a decl
                        v.value_type = Int;
                        v.location = location;
                        v.filei = file;
                        sc.localv.push_back(v);

                        gen_funccall(ir, sc.localv.size(), rhs);
                        get_and_expect_semicolon(l);
                    }
                    else if (expect_token(l, ';'))
                    { // assignment
                        uint32_t srcval = find_variable(rhs, sc.localv);
                        uint32_t destval = find_variable(primary, sc.localv);
                        assert(destval != 0);
                        assert(srcval != 0);
                        gen_assignment_lval_to_lval(ir, destval, srcval);
                        get_and_expect_semicolon(l);
                    }
                    else
                    {
                        NOB_UNREACHABLE("unexpected or unimplemented token after assignment lhs rhs");
                    }
                }
                else
                {
                    nob_log(NOB_ERROR, "%s:%d:%d: invalid syntax: unexpected token after assignment to %s", input_files[file], location.line_number, location.line_offset, primary);
                    exit(InvalidSyntax);
                } */