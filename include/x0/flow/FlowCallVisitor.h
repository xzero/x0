/* <flow/FlowCallVisitor.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.xzero.io/projects/flow
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_flow_Flow_h
#define sw_flow_Flow_h

#include <x0/flow/ASTVisitor.h>
#include <x0/Api.h>
#include <vector>

namespace x0 {

//! \addtogroup Flow
//@{

class ASTNode;

class X0_API FlowCallVisitor :
    public ASTVisitor
{
private:
    std::vector<CallExpr*> calls_;

public:
    explicit FlowCallVisitor(ASTNode* root = nullptr);
    ~FlowCallVisitor();

    void visit(ASTNode* root);

    void clear() { calls_.clear(); }

    const std::vector<CallExpr*>& calls() const { return calls_; }

protected:
    // symbols
    virtual void accept(Unit& symbol);
    virtual void accept(Variable& variable);
    virtual void accept(Handler& handler);
    virtual void accept(BuiltinFunction& symbol);
    virtual void accept(BuiltinHandler& symbol);

    // expressions
    virtual void accept(UnaryExpr& expr);
    virtual void accept(BinaryExpr& expr);
    virtual void accept(CallExpr& expr);
    virtual void accept(VariableExpr& expr);
    virtual void accept(HandlerRefExpr& expr);

    virtual void accept(StringExpr& expr);
    virtual void accept(NumberExpr& expr);
    virtual void accept(BoolExpr& expr);
    virtual void accept(RegExpExpr& expr);
    virtual void accept(IPAddressExpr& expr);
    virtual void accept(CidrExpr& cidr);
    virtual void accept(ArrayExpr& array);

    // statements
    virtual void accept(ExprStmt& stmt);
    virtual void accept(CompoundStmt& stmt);
    virtual void accept(CondStmt& stmt);
    virtual void accept(MatchStmt& stmt);
    virtual void accept(AssignStmt& stmt);
};

//!@}

} // namespace x0

#endif
