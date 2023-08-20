#include "ards_compiler.hpp"

#include <cassert>

namespace ards
{

void compiler_t::transform_left_assoc_infix(ast_node_t& n)
{
    if(!errs.empty()) return;
    // transform left-associative infix chains into left binary trees
    for(auto& child : n.children)
        transform_left_assoc_infix(child);
    switch(n.type)
    {
    case AST::OP_EQUALITY:
    case AST::OP_RELATIONAL:
    case AST::OP_ADDITIVE:
    case AST::OP_MULTIPLICATIVE:
    {
        assert(n.children.size() >= 3);
        assert(n.children.size() % 2 == 1);
        ast_node_t a = std::move(n.children[0]);
        for(size_t i = 1; i < n.children.size(); i += 2)
        {
            ast_node_t op = std::move(n.children[i]);
            ast_node_t b = std::move(n.children[i + 1]);
            op.type = n.type;
            op.children.emplace_back(std::move(a));
            op.children.emplace_back(std::move(b));
            a = std::move(op);
        }
        n = std::move(a);
        break;
    }
    default:
        break;
    }
}

}
