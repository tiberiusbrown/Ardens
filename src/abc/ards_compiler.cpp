#include "ards_compiler.hpp"

#include <assert.h>

namespace ards
{

std::unordered_map<std::string, compiler_type_t> const primitive_types
{
    { "void",   TYPE_VOID },
    { "bool",   TYPE_BOOL },
    { "u8",     TYPE_U8   },
    { "u16",    TYPE_U16  },
    { "u24",    TYPE_U24  },
    { "u32",    TYPE_U32  },
    { "i8",     TYPE_I8   },
    { "i16",    TYPE_I16  },
    { "i24",    TYPE_I24  },
    { "i32",    TYPE_I32  },
    { "uchar",  TYPE_U8   },
    { "ushort", TYPE_U16  },
    { "uint",   TYPE_U16  },
    { "ulong",  TYPE_U32  },
    { "char",   TYPE_I8   },
    { "short",  TYPE_I16  },
    { "int",    TYPE_I16  },
    { "long",   TYPE_I32  },
};

std::unordered_map<sysfunc_t, compiler_func_decl_t> const sysfunc_decls
{
    { SYS_DISPLAY,          { TYPE_VOID, {} } },
    { SYS_DRAW_PIXEL,       { TYPE_VOID, { TYPE_I16, TYPE_I16, TYPE_U8 } } },
    { SYS_DRAW_FILLED_RECT, { TYPE_VOID, { TYPE_I16, TYPE_I16, TYPE_U8, TYPE_U8, TYPE_U8 } } },
    { SYS_SET_FRAME_RATE,   { TYPE_VOID, { TYPE_U8 } } },
    { SYS_NEXT_FRAME,       { TYPE_BOOL, {} } },
    { SYS_IDLE,             { TYPE_VOID, {} } },
    { SYS_DEBUG_BREAK,      { TYPE_VOID, {} } },
};

static bool isspace(char c)
{
    switch(c)
    {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
        return true;
    default:
        return false;
    }
}

compiler_type_t compiler_t::resolve_type(ast_node_t const& n)
{
    assert(n.type == AST::TYPE);
    if(n.type == AST::TYPE)
    {
        std::string name(n.data);
        auto it = primitive_types.find(name);
        if(it != primitive_types.end())
        {
            return it->second;
        }
        else
        {
            errs.push_back({
                "Unknown type \"" + name + "\"",
                n.line_info });
        }
    }
    return TYPE_NONE;
}

compiler_func_t compiler_t::resolve_func(ast_node_t const& n)
{
    assert(n.type == AST::IDENT);
    std::string name(n.data);

    {
        auto it = sys_names.find(name);
        if(it != sys_names.end())
        {
            compiler_func_t f{};
            f.sys = it->second;
            f.is_sys = true;
            f.name = name;
            auto jt = sysfunc_decls.find(it->second);
            assert(jt != sysfunc_decls.end());
            f.decl = jt->second;
            return f;
        }
    }

    {
        auto it = funcs.find(name);
        if(it != funcs.end())
        {
            return it->second;
        }
    }

    errs.push_back({ "Undefined function: \"" + name + "\"", n.line_info });
    return {};
}

void compiler_t::compile(std::istream& fi, std::ostream& fo)
{
    assert(sysfunc_decls.size() == SYS_NUM);

    parse(fi);
    if(!errs.empty()) return;

    // trim all token whitespace
    ast.recurse([](ast_node_t& n) {
        size_t size = n.data.size();
        size_t i;
        if(size == 0) return;
        for(i = 0; isspace(n.data[i]); ++i);
        n.data.remove_prefix(i);
        for(i = 0; isspace(n.data[size - i - 1]); ++i);
        n.data.remove_suffix(i);
    });

    // gather all functions and globals and check for duplicates
    assert(ast.type == AST::PROGRAM);
    for(auto& n : ast.children)
    {
        if(!errs.empty()) return;
        assert(n.type == AST::DECL_STMT || n.type == AST::FUNC_STMT);
        if(n.type == AST::DECL_STMT)
        {
            assert(n.children.size() == 2);
            assert(n.children[1].type == AST::IDENT);
            std::string name(n.children[1].data);
            auto it = globals.find(name);
            if(it != globals.end())
            {
                errs.push_back({
                    "Duplicate global \"" + name + "\"",
                    n.children[1].line_info });
                return;
            }
            auto& g = globals[name];
            g.name = name;
            g.type = resolve_type(n.children[0]);
            if(g.type.prim_size == 0)
            {
                errs.push_back({
                    "Global variable \"" + name + "\" has zero size",
                    n.line_info });
                return;
            }
        }
        else if(n.type == AST::FUNC_STMT)
        {
            assert(n.children.size() == 4);
            assert(n.children[1].type == AST::IDENT);
            assert(n.children[2].type == AST::BLOCK);
            assert(n.children[3].type == AST::LIST);
            std::string name(n.children[1].data);
            {
                auto it = sys_names.find(name);
                if(it != sys_names.end())
                {
                    errs.push_back({
                        "Function \"" + name + "\" is a system method",
                        n.children[1].line_info });
                    return;
                }
            }
            {
                auto it = funcs.find(name);
                if(it != funcs.end())
                {
                    errs.push_back({
                        "Duplicate function \"" + name + "\"",
                        n.children[1].line_info });
                    return;
                }
            }
            auto& f = funcs[name];
            f.decl.return_type = resolve_type(n.children[0]);
            f.name = name;
            f.block = std::move(n.children[2]);

            // decl args
            auto const& decls = n.children[3].children;
            assert(decls.size() % 2 == 0);
            for(size_t i = 0; i < decls.size(); i += 2)
            {
                auto const& type = decls[i + 0];
                auto const& name = decls[i + 1];
                assert(type.type == AST::TYPE);
                assert(name.type == AST::IDENT);
                f.arg_names.push_back(std::string(name.data));
                f.decl.arg_types.push_back(resolve_type(type));
            }
        }
    }

    //
    // transforms
    //

    // transform left-associative infix exprs into binary trees
    for(auto& [k, v] : funcs)
        transform_left_assoc_infix(v.block);

    // transforms TODO
    // - remove root ops in expr statements that have no side effects

    // transforms are done: set parent pointers
    for(auto& [k, v] : funcs)
    {
        v.block.recurse([](ast_node_t& a) {
            for(auto& child : a.children)
                child.parent = &a;
        });
    }

    // generate code for all functions
    for(auto& [n, f] : funcs)
    {
        if(!errs.empty()) return;
        codegen_function(f);
    }

    // peephole optimizations
    for(auto& [n, f] : funcs)
    {
        if(!errs.empty()) return;
        while(peephole(f))
            ;
    }

    write(fo);
}

}
