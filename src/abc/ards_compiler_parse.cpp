#include "ards_compiler.hpp"

#include <iostream>
#include <streambuf>
#include <variant>

#include <peglib.h>

namespace ards
{

template<AST T> ast_node_t infix(peg::SemanticValues const& v)
{
    size_t num_ops = v.size() / 2;
    if(num_ops == 0)
        return std::any_cast<ast_node_t>(v[0]);
    ast_node_t a{ v.line_info(), T, v.token() };
    for(auto& child : v)
        a.children.emplace_back(std::move(std::any_cast<ast_node_t>(child)));
    return a;
};

void compiler_t::parse(std::istream& fi)
{
    error_t e;
    peg::parser p;

    p.set_logger([&](size_t line, size_t column, std::string const& msg) {
        errs.push_back({ msg, { line, column } });
    });
    p.load_grammar(
#if 0
// fuller grammar (TODO)
R"(
        
program             <- global_stmt*

global_stmt         <- decl_stmt / func_stmt
decl_stmt           <- type_name decl_list ';'
func_stmt           <- type_name ident '(' arg_decl_list? ')' compound_stmt
compound_stmt       <- '{' stmt* '}'
stmt                <- compound_stmt /
                       decl_stmt     /
                       if_stmt       /
                       while_stmt    /
                       return_stmt   /
                       break_stmt    /
                       continue_stmt /
                       expr_stmt
if_stmt             <- 'if' '(' expr ')' stmt ('else' stmt)?
while_stmt          <- 'while' '(' expr ')' stmt
return_stmt         <- 'return' expr ';'
break_stmt          <- 'break' ';'
continue_stmt       <- 'continue' ';'
return_stmt         <- 'return' expr? ';'
expr_stmt           <- ';' / expr ';'

# right-associative binary assignment operator
expr                <- conditional_expr / unary_expr assignment_op expr

# ternary conditional operator
conditional_expr    <- logical_or_expr ('?' expr ':' conditional_expr)?

# left-associative binary operators
logical_or_expr     <- logical_and_expr    ('||'              logical_and_expr   )*
logical_and_expr    <- bitwise_or_expr     ('&&'              bitwise_or_expr    )*
bitwise_or_expr     <- bitwise_xor_expr    ('|'               bitwise_xor_expr   )*
bitwise_xor_expr    <- bitwise_and_expr    ('^'               bitwise_and_expr   )*
bitwise_and_expr    <- equality_expr       ('&'               equality_expr      )*
equality_expr       <- relational_expr     (equality_op       relational_expr    )*
relational_expr     <- shift_expr          (relational_op     shift_expr         )*
shift_expr          <- additive_expr       (shift_op          additive_expr      )*
additive_expr       <- multiplicative_expr (additive_op       multiplicative_expr)*
multiplicative_expr <- cast_expr           (multiplicative_op cast_expr          )*

# unary operators
cast_expr           <- unary_expr / '(' type_name ')' cast_expr
unary_expr          <- postfix_expr / prefix_op unary_expr / unary_op cast_expr
postfix_expr        <- primary_expr postfix*
primary_expr        <- ident / decimal_literal / '(' expr ')'

postfix             <- postfix_op / '.' ident / '[' expr ']' / '(' arg_expr_list? ')'

type_name           <- ident
decl_list           <- decl_item (',' decl_item)*
decl_item           <- ident ('=' expr)?
arg_decl_list       <- type_name ident (',' type_name ident)*
arg_expr_list       <- expr (',' expr)*

prefix_op           <- < '++' / '--' >
postfix_op          <- < '++' / '--' >
unary_op            <- < [~!+-] >
multiplicative_op   <- < [*/%] >
additive_op         <- < [+-] >
shift_op            <- < '<<' / '>>' >
relational_op       <- < '<=' / '>=' / '<' / '>' >
equality_op         <- < '==' / '!=' >
assignment_op       <- < '=' / [*/%&|^+-]'=' / '<<=' / '>>=' >
decimal_literal     <- < [0-9]+ >
ident               <- < [a-zA-Z_][a-zA-Z_0-9]* >

%whitespace         <- [ \t\r\n]*

    )"
#else
R"(

program             <- global_stmt*

global_stmt         <- decl_stmt / func_stmt
decl_stmt           <- type_name ident ';'
func_stmt           <- type_name ident '(' arg_decl_list? ')' compound_stmt
compound_stmt       <- '{' stmt* '}'
stmt                <- compound_stmt /
                       return_stmt   /
                       if_stmt       /
                       while_stmt    /
                       for_stmt      /
                       decl_stmt     /
                       expr_stmt
if_stmt             <- 'if' '(' expr ')' stmt ('else' stmt)?
while_stmt          <- 'while' '(' expr ')' stmt
for_stmt            <- 'for' '(' for_init_stmt expr ';' expr ')' stmt
for_init_stmt       <- decl_stmt / expr_stmt
expr_stmt           <- ';' / expr ';'
return_stmt         <- 'return' expr? ';'

# right-associative binary assignment operator
expr                <- postfix_expr assignment_op expr / equality_expr

# left-associative binary operators
equality_expr       <- relational_expr (equality_op   relational_expr)*
relational_expr     <- additive_expr   (relational_op additive_expr  )*
additive_expr       <- cast_expr       (additive_op   cast_expr      )*

cast_expr           <- unary_expr / '(' type_name ')' cast_expr
unary_expr          <- postfix_expr / unary_op cast_expr
postfix_expr        <- primary_expr postfix*
primary_expr        <- ident / decimal_literal / '(' expr ')'

postfix             <- '(' arg_expr_list? ')'

type_name           <- ident
arg_decl_list       <- type_name ident (',' type_name ident)*
arg_expr_list       <- expr (',' expr)*

equality_op         <- < '==' / '!=' >
additive_op         <- < [+-] >
relational_op       <- < '<=' / '>=' / '<' / '>' >
assignment_op       <- < '=' >
unary_op            <- < [!-] >
decimal_literal     <- < [0-9]+ >
ident               <- < [a-zA-Z_][a-zA-Z_0-9]* >

%whitespace         <- [ \t\r\n]*

)"
#endif
    );

    if(!errs.empty()) return;

    /*
    * 
    At the AST level, for statements are transformed:

        for(A; B; C)
            D;

    is transformed into

        {
            A;
            while(B)
            {
                D;
                C;
            }
        }

    */
    p["for_stmt"] = [](peg::SemanticValues const& v) {
        ast_node_t a{ v.line_info(), AST::BLOCK, v.token() };
        auto A = std::any_cast<ast_node_t>(v[0]);
        auto B = std::any_cast<ast_node_t>(v[1]);
        auto C = std::any_cast<ast_node_t>(v[2]);
        auto D = std::any_cast<ast_node_t>(v[3]);
        a.children.push_back(A);
        a.children.push_back({ v.line_info(), AST::WHILE_STMT, v.token() });
        auto& w = a.children.back();
        w.children.push_back(B);
        w.children.push_back({ v.line_info(), AST::BLOCK, v.token() });
        auto& wb = w.children.back();
        wb.children.push_back(D);
        wb.children.push_back({ C.line_info, AST::EXPR_STMT, C.data, { C } });
        return a;
    };
    p["for_init_stmt"] = [](peg::SemanticValues const& v) {
        return std::any_cast<ast_node_t>(v[0]);
    };

    p["decimal_literal"] = [](peg::SemanticValues const& v) {
        return ast_node_t{ v.line_info(), AST::INT_CONST, v.token(), {}, v.token_to_number<int64_t>() };
    };
    p["ident"] = [](peg::SemanticValues const& v) {
        return ast_node_t{ v.line_info(), AST::IDENT, v.token() };
    };
    p["type_name"] = [](peg::SemanticValues const& v) {
        return ast_node_t{ v.line_info(), AST::TYPE, v.token() };
    };
    p["cast_expr"] = [](peg::SemanticValues const& v) -> ast_node_t {
        if(v.choice() == 0)
            return std::any_cast<ast_node_t>(v[0]);
        return {
            v.line_info(), AST::OP_CAST, v.token(),
            { std::any_cast<ast_node_t>(v[0]), std::any_cast<ast_node_t>(v[1]) }
        };
    };
    p["unary_expr"] = [](peg::SemanticValues const& v) -> ast_node_t {
        if(v.choice() == 0)
            return std::any_cast<ast_node_t>(v[0]);
        return {
            v.line_info(), AST::OP_UNARY, v.token(),
            { std::any_cast<ast_node_t>(v[0]), std::any_cast<ast_node_t>(v[1]) }
        };
    };

    // form a left-associative binary tree
    p["postfix_expr"] = [](peg::SemanticValues const& v) -> ast_node_t {
        ast_node_t a = std::any_cast<ast_node_t>(v[0]);
        if(v.size() == 1) return a;
        for(size_t i = 1; i < v.size(); ++i)
        {
            ast_node_t b = std::any_cast<ast_node_t>(v[i]);
            ast_node_t pair{ v.line_info(), AST::NONE, v.token() };
            if(b.type == AST::FUNC_ARGS)
                pair.type = AST::FUNC_CALL;
            pair.children.emplace_back(std::move(a));
            pair.children.emplace_back(std::move(b));
            a = std::move(pair);
        }
        return a;
    };

    p["postfix"] = [](peg::SemanticValues const& v) -> ast_node_t {
        if(v.choice() == 0)
        {
            // function call args
            ast_node_t a = { v.line_info(), AST::FUNC_ARGS, v.token() };
            if(v.size() == 1)
            {
                auto child = std::any_cast<ast_node_t>(v[0]);
                a.children = std::move(child.children);
            }
            return a;
        }
        return {};
    };

    p["arg_decl_list"] = [](peg::SemanticValues const& v) {
        assert(v.size() % 2 == 0);
        ast_node_t a = { v.line_info(), AST::LIST, v.token() };
        for(auto& child : v)
            a.children.emplace_back(std::move(std::any_cast<ast_node_t>(child)));
        return a;
    };
    p["arg_expr_list"] = [](peg::SemanticValues const& v) {
        ast_node_t a = { v.line_info(), AST::LIST, v.token() };
        for(auto& child : v)
            a.children.emplace_back(std::move(std::any_cast<ast_node_t>(child)));
        return a;
    };

    p["primary_expr"] = [](peg::SemanticValues const& v) {
        return std::any_cast<ast_node_t>(v[0]);
    };

    const auto token = [](peg::SemanticValues const& v) {
        return ast_node_t{ v.line_info(), AST::TOKEN, v.token() };
    };
    p["equality_op"  ] = token;
    p["relational_op"] = token;
    p["additive_op"  ] = token;
    p["unary_op"     ] = token;

    p["equality_expr"  ] = infix<AST::OP_EQUALITY>;
    p["relational_expr"] = infix<AST::OP_RELATIONAL>;
    p["additive_expr"  ] = infix<AST::OP_ADDITIVE>;

    p["assignment_op"] = [](peg::SemanticValues const& v) -> ast_node_t {
        return { v.line_info(), AST::TOKEN, v.token() };
    };
    p["expr"] = [](peg::SemanticValues const& v) -> ast_node_t {
        auto& child0 = std::any_cast<ast_node_t>(v[0]);
        if(v.choice() == 1) return child0;
        auto& child1 = std::any_cast<ast_node_t>(v[1]);
        auto& child2 = std::any_cast<ast_node_t>(v[2]);
        assert(child1.type == AST::TOKEN);
        auto type = AST::OP_ASSIGN;
        return { v.line_info(), type, v.token(), { std::move(child0), std::move(child2) } };
    };
    p["if_stmt"] = [](peg::SemanticValues const& v) -> ast_node_t {
        ast_node_t a{
            v.line_info(), AST::IF_STMT, v.token(),
            { std::any_cast<ast_node_t>(v[0]), std::any_cast<ast_node_t>(v[1]) }
        };
        // always include else clause, even if only an empty statement
        ast_node_t else_stmt{};
        if(v.size() >= 3)
            a.children.emplace_back(std::move(std::any_cast<ast_node_t>(v[2])));
        else
            a.children.push_back({ {}, AST::EMPTY_STMT, "" });
        return a;
    };
    p["while_stmt"] = [](peg::SemanticValues const& v) -> ast_node_t {
        return {
            v.line_info(), AST::WHILE_STMT, v.token(),
            { std::any_cast<ast_node_t>(v[0]), std::any_cast<ast_node_t>(v[1]) }
        };
    };
    p["expr_stmt"] = [](peg::SemanticValues const& v) -> ast_node_t {
        if(v.empty()) return { v.line_info(), AST::EMPTY_STMT, v.token() };
        ast_node_t a{ v.line_info(), AST::EXPR_STMT, v.token() };
        a.children.emplace_back(std::move(std::any_cast<ast_node_t>(v[0])));
        return a;
    };
    p["compound_stmt"] = [](peg::SemanticValues const& v) -> ast_node_t {
        ast_node_t a{ v.line_info(), AST::BLOCK, v.token() };
        for(auto& child : v)
            a.children.emplace_back(std::move(std::any_cast<ast_node_t>(child)));
        return a;
    };
    p["func_stmt"] = [](peg::SemanticValues const& v) -> ast_node_t {
        ast_node_t a{ v.line_info(), AST::FUNC_STMT, v.token() };
        a.children.emplace_back(std::move(std::any_cast<ast_node_t>(v[0])));
        a.children.emplace_back(std::move(std::any_cast<ast_node_t>(v[1])));
        a.children.emplace_back(std::move(std::any_cast<ast_node_t>(v.back())));
        auto& block = a.children.back();
        if(block.children.empty() || block.children.back().type != AST::RETURN_STMT)
            block.children.push_back({ {}, AST::RETURN_STMT, "" });
        if(v.size() == 4)
        {
            // arg decls
            a.children.emplace_back(std::move(std::any_cast<ast_node_t>(v[2])));
        }
        else
        {
            a.children.push_back({ {}, AST::LIST, "" });
        }
        return a;
    };
    p["decl_stmt"] = [](peg::SemanticValues const& v) -> ast_node_t {
        ast_node_t a{ v.line_info(), AST::DECL_STMT, v.token() };
        a.children.emplace_back(std::move(std::any_cast<ast_node_t>(v[0])));
        a.children.emplace_back(std::move(std::any_cast<ast_node_t>(v[1])));
        return a;
    };
    p["global_stmt"] = [](peg::SemanticValues const& v) -> ast_node_t {
        return std::any_cast<ast_node_t>(v[0]);
    };
    p["while_stmt"] = [](peg::SemanticValues const& v) -> ast_node_t {
        ast_node_t a{ v.line_info(), AST::WHILE_STMT, v.token() };
        a.children.emplace_back(std::move(std::any_cast<ast_node_t>(v[0])));
        a.children.emplace_back(std::move(std::any_cast<ast_node_t>(v[1])));
        return a;
    };
    p["return_stmt"] = [](peg::SemanticValues const& v) -> ast_node_t {
        ast_node_t a{ v.line_info(), AST::RETURN_STMT, v.token() };
        if(!v.empty())
            a.children.emplace_back(std::move(std::any_cast<ast_node_t>(v[0])));
        return a;
    };
    p["program"] = [](peg::SemanticValues const& v) -> ast_node_t {
        ast_node_t a{ v.line_info(), AST::PROGRAM, v.token() };
        for(auto& child : v)
            a.children.emplace_back(std::move(std::any_cast<ast_node_t>(child)));
        return a;
    };
    input_data = std::string(
        (std::istreambuf_iterator<char>(fi)),
        (std::istreambuf_iterator<char>()));
    
    if(!p.parse(input_data, ast) && errs.empty())
        errs.push_back({ "An unknown parse error occurred." });
}

}
