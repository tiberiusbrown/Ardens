#pragma once

#include <iostream>
#include <variant>
#include <vector>

#include "ards_assembler.hpp"
#include "ards_error.hpp"

namespace ards
{

enum class AST
{
    NONE,

    //
    // pseudo-nodes
    //

    TOKEN,      // used to pass through token    
    LIST,       // used to pass through list of nodes
    FUNC_ARGS,  // for function call arg list
    FUNC_DECLS, // for function decl arg list

    //
    // program/statement nodes
    //

    PROGRAM,     // children are global declarations and functions
    BLOCK,       // children are child statements
    EMPTY_STMT,
    EXPR_STMT,   // child is expr
    DECL_STMT,   // children are type, ident
    FUNC_STMT,   // children are type, ident, block, args
    IF_STMT,     // children are expr, stmt, stmt (for else)
    WHILE_STMT,  // children are expr and stmt
    RETURN_STMT, // child is expr if it exists

    //
    // expression nodes
    //

    // left-associative chained infix operators
    OP_EQUALITY,   // chain of ops and infix == / != tokens
    OP_RELATIONAL, // chain of ops and infix <= / >= / < / > tokens
    OP_ADDITIVE,   // chain of ops and infix + / - tokens
    OP_MULTIPLICATIVE,

    // right-associative assignment operators
    OP_ASSIGN,

    OP_UNARY, // children are op and expr

    OP_CAST, // children are type and expr

    FUNC_CALL, // children are func-expr and FUNC_ARGS

    //
    // atoms
    //

    INT_CONST,
    IDENT,
    TYPE,
};

struct compiler_type_t
{
    size_t prim_size; // 0 means void
    bool prim_signed;
    bool is_bool;
    auto tie() const { return std::tie(prim_size, prim_signed, is_bool); }
    bool operator==(compiler_type_t const& t) const { return tie() == t.tie(); }
    bool operator!=(compiler_type_t const& t) const { return tie() != t.tie(); }
};

constexpr compiler_type_t TYPE_NONE = { 0, true };

constexpr compiler_type_t TYPE_VOID = { 0, false };
constexpr compiler_type_t TYPE_BOOL = { 1, false, true };
constexpr compiler_type_t TYPE_U8 = { 1, false };
constexpr compiler_type_t TYPE_U16 = { 2, false };
constexpr compiler_type_t TYPE_U24 = { 3, false };
constexpr compiler_type_t TYPE_U32 = { 4, false };
constexpr compiler_type_t TYPE_I8 = { 1, true };
constexpr compiler_type_t TYPE_I16 = { 2, true };
constexpr compiler_type_t TYPE_I24 = { 3, true };
constexpr compiler_type_t TYPE_I32 = { 4, true };

struct compiler_instr_t
{
    instr_t instr;
    uint32_t imm;
    std::string label; // can also be label arg of instr
    bool is_label;
};

struct compiler_func_decl_t
{
    compiler_type_t return_type;
    std::vector<compiler_type_t> arg_types;
};

struct compiler_global_t
{
    compiler_type_t type;
    std::string name;
};

struct compiler_local_t
{
    size_t frame_offset;
    compiler_type_t type;
};

struct compiler_scope_t
{
    size_t size;
    std::unordered_map<std::string, compiler_local_t> locals;
};

struct compiler_frame_t
{
    size_t size;
    std::vector<compiler_scope_t> scopes; // in-order
    void push()
    {
        scopes.resize(scopes.size() + 1);
    }
    void pop()
    {
        size -= scopes.back().size;
        scopes.pop_back();
    }
};

struct compiler_lvalue_t
{
    size_t size;
    bool is_global;
    uint8_t stack_index;
    std::string global_name;
};

struct ast_node_t
{
    std::pair<size_t, size_t> line_info;
    AST type;
    std::string_view data;
    std::vector<ast_node_t> children;
    int64_t value;
    compiler_type_t comp_type;

    ast_node_t* parent;

    template<class F>
    void recurse(F&& f)
    {
        for(auto& child : children)
            child.recurse(std::forward<F>(f));
        f(*this);
    }
};

struct compiler_func_t
{
    ast_node_t block;
    compiler_func_decl_t decl;
    std::string name;
    std::vector<std::string> arg_names;
    std::vector<compiler_instr_t> instrs;
    size_t label_count;
    sysfunc_t sys;
    bool is_sys;
};

extern std::unordered_map<sysfunc_t, compiler_func_decl_t> const sysfunc_decls;
extern std::unordered_map<std::string, compiler_type_t> const primitive_types;

struct compiler_t
{
    compiler_t() {};

    void compile(std::istream& fi, std::ostream& fo);
    std::vector<error_t> const& errors() const { return errs; }
    std::vector<error_t> const& warnings() const { return warns; }

private:

    void parse(std::istream& fi);

    compiler_type_t resolve_type(ast_node_t const& n);
    compiler_func_t resolve_func(ast_node_t const& n);
    compiler_lvalue_t resolve_lvalue(ast_node_t const& n, compiler_frame_t const& frame);
    compiler_lvalue_t return_lvalue(compiler_func_t const& f, compiler_frame_t const& frame);
    void type_annotate(ast_node_t& n, compiler_frame_t const& frame);

    void transform_left_assoc_infix(ast_node_t& n);
    void transform_casts(ast_node_t& n);

    void codegen_function(compiler_func_t& f);
    void codegen(compiler_func_t& f, compiler_frame_t& frame, ast_node_t& a);
    void codegen_expr(compiler_func_t& f, compiler_frame_t& frame, ast_node_t const& a);
    void codegen_store_lvalue(compiler_func_t& f, compiler_lvalue_t const& lvalue);
    void codegen_convert(
        compiler_func_t& f, compiler_frame_t& frame,
        compiler_type_t const& to, compiler_type_t const& from);
    void codegen_return(compiler_func_t& f, compiler_frame_t& frame, ast_node_t const& n);
    std::string codegen_label(compiler_func_t& f);

    // returns true if any optimization happened
    bool peephole(compiler_func_t& f);

    void write(std::ostream& f);

    // parse data
    std::string input_data;
    ast_node_t ast;

    // codegen data
    std::unordered_map<std::string, compiler_func_t> funcs;
    std::unordered_map<std::string, compiler_global_t> globals;

    std::vector<error_t> errs;
    std::vector<error_t> warns;
};

}
