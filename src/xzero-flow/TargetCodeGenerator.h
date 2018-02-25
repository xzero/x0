// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/defines.h>
#include <xzero-flow/FlowType.h>
#include <xzero-flow/ir/InstructionVisitor.h>
#include <xzero-flow/vm/ConstantPool.h>
#include <xzero/net/IPAddress.h>
#include <xzero/net/Cidr.h>

#include <string>
#include <vector>
#include <list>
#include <utility>
#include <memory>

namespace xzero::flow {
  class Value;
  class Instr;
  class IRProgram;
  class IRHandler;
  class BasicBlock;
  class IRBuiltinHandler;
  class IRBuiltinFunction;
}

namespace xzero::flow::vm {
  class Program;
}

namespace xzero::flow {

//! \addtogroup Flow
//@{

using StackPointer = size_t;

class TargetCodeGenerator : public InstructionVisitor {
 public:
  TargetCodeGenerator();
  ~TargetCodeGenerator();

  std::shared_ptr<vm::Program> generate(IRProgram* program);

 protected:
  using Opcode = vm::Opcode;
  using Operand = vm::Operand;
  using Instruction = vm::Instruction;
  using ConstantPool = vm::ConstantPool;

  void generate(IRHandler* handler);
  size_t handlerRef(IRHandler* handler);

  size_t makeNumber(FlowNumber value);
  size_t makeNativeHandler(IRBuiltinHandler* builtin);
  size_t makeNativeFunction(IRBuiltinFunction* builtin);

  /**
   * Ensures @p value is available on top of the stack.
   *
   * May emit a LOAD instruction if stack[sp] is not on top of the stack.
   */
  StackPointer emitLoad(Value* value);

  size_t emit(Opcode opc) { return emit(vm::makeInstruction(opc)); }
  size_t emit(Opcode opc, Operand op1) {
    return emit(vm::makeInstruction(opc, op1));
  }
  size_t emit(Opcode opc, Operand op1, Operand op2) {
    return emit(vm::makeInstruction(opc, op1, op2));
  }
  size_t emit(Opcode opc, Operand op1, Operand op2, Operand op3) {
    return emit(vm::makeInstruction(opc, op1, op2, op3));
  }
  size_t emit(Instruction instr);

  /**
   * Emits conditional jump instruction.
   *
   * @param opcode Opcode for the conditional jump.
   * @param cond Condition to to evaluated by given \p opcode.
   * @param bb Target basic block to jump to by \p opcode.
   *
   * This function will just emit a placeholder and will remember the
   * instruction pointer and passed operands for later back-patching once all
   * basic block addresses have been computed.
   */
  size_t emit(Opcode opcode, Register cond, BasicBlock* bb);

  /**
   * Emits unconditional jump instruction.
   *
   * @param opcode Opcode for the conditional jump.
   * @param bb Target basic block to jump to by \p opcode.
   *
   * This function will just emit a placeholder and will remember the
   * instruction pointer and passed operands for later back-patching once all
   * basic block addresses have been computed.
   */
  size_t emit(Opcode opcode, BasicBlock* bb);

  size_t emitBinaryAssoc(Instr& instr, Opcode opcode);
  size_t emitBinary(Instr& instr, Opcode opcode);
  size_t emitUnary(Instr& instr, Opcode opcode);

  /**
   * Emits call args.
   *
   * @returns number of args pushed.
   */
  size_t emitCallArgs(Instr& instr);

  /**
   * Emits @p value and returns its stack position.
   *
   * If the value is not available on the stack yet, it'll be allocated a slot.
   */
  StackPointer emit(Value* value);

  Operand getConstantInt(Value* value);

  /**
   * Retrieves the instruction pointer of the next instruction to be emitted.
   */
  size_t getInstructionPointer() const { return code_.size(); }

  StackPointer allocate(const Value* alias);
  StackPointer findOnStack(const Value* value);
  void discard(const Value* alias);

  void visit(NopInstr& instr) override;

  // storage
  void visit(AllocaInstr& instr) override;
  void visit(StoreInstr& instr) override;
  void visit(LoadInstr& instr) override;
  void visit(PhiNode& instr) override;

  // calls
  void visit(CallInstr& instr) override;
  void visit(HandlerCallInstr& instr) override;

  // terminator
  void visit(CondBrInstr& instr) override;
  void visit(BrInstr& instr) override;
  void visit(RetInstr& instr) override;
  void visit(MatchInstr& instr) override;

  // type cast
  void visit(CastInstr& instr) override;

  // numeric
  void visit(INegInstr& instr) override;
  void visit(INotInstr& instr) override;
  void visit(IAddInstr& instr) override;
  void visit(ISubInstr& instr) override;
  void visit(IMulInstr& instr) override;
  void visit(IDivInstr& instr) override;
  void visit(IRemInstr& instr) override;
  void visit(IPowInstr& instr) override;
  void visit(IAndInstr& instr) override;
  void visit(IOrInstr& instr) override;
  void visit(IXorInstr& instr) override;
  void visit(IShlInstr& instr) override;
  void visit(IShrInstr& instr) override;
  void visit(ICmpEQInstr& instr) override;
  void visit(ICmpNEInstr& instr) override;
  void visit(ICmpLEInstr& instr) override;
  void visit(ICmpGEInstr& instr) override;
  void visit(ICmpLTInstr& instr) override;
  void visit(ICmpGTInstr& instr) override;

  // boolean
  void visit(BNotInstr& instr) override;
  void visit(BAndInstr& instr) override;
  void visit(BOrInstr& instr) override;
  void visit(BXorInstr& instr) override;

  // string
  void visit(SLenInstr& instr) override;
  void visit(SIsEmptyInstr& instr) override;
  void visit(SAddInstr& instr) override;
  void visit(SSubStrInstr& instr) override;
  void visit(SCmpEQInstr& instr) override;
  void visit(SCmpNEInstr& instr) override;
  void visit(SCmpLEInstr& instr) override;
  void visit(SCmpGEInstr& instr) override;
  void visit(SCmpLTInstr& instr) override;
  void visit(SCmpGTInstr& instr) override;
  void visit(SCmpREInstr& instr) override;
  void visit(SCmpBegInstr& instr) override;
  void visit(SCmpEndInstr& instr) override;
  void visit(SInInstr& instr) override;

  // ip
  void visit(PCmpEQInstr& instr) override;
  void visit(PCmpNEInstr& instr) override;
  void visit(PInCidrInstr& instr) override;

 private:
  struct ConditionalJump {
    size_t pc;
    Opcode opcode;
    Register condition;
  };

  struct UnconditionalJump {
    size_t pc;
    Opcode opcode;
  };

  //! list of raised errors during code generation.
  std::vector<std::string> errors_;

  std::unordered_map<BasicBlock*, std::list<ConditionalJump>> conditionalJumps_;
  std::unordered_map<BasicBlock*, std::list<UnconditionalJump>> unconditionalJumps_;
  std::list<std::pair<MatchInstr*, size_t /*matchId*/>> matchHints_;

  size_t handlerId_;                    //!< current handler's ID
  std::vector<Instruction> code_;       //!< current handler's code

  /** SP of current top value on the stack at the time of code generation. */
  StackPointer sp_;

  /** value-to-stack-offset assignment-map */
  std::unordered_map<const Value*, StackPointer> variables_;

  std::deque<Value*> stack_;

  // target program output
  ConstantPool cp_;
};

//!@}

}  // namespace xzero::flow
