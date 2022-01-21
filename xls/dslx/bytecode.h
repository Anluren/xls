// Copyright 2022 The XLS Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef XLS_DSLX_BYTECODE_H_
#define XLS_DSLX_BYTECODE_H_

#include <memory>

#include "absl/types/variant.h"
#include "xls/common/strong_int.h"
#include "xls/dslx/ast.h"
#include "xls/dslx/concrete_type.h"
#include "xls/dslx/interp_value.h"
#include "xls/dslx/pos.h"

namespace xls::dslx {

// Defines a single "instruction" for the DSLX bytecode interpreter: an opcode
// and optional accessory data (load/store value name, function call target).
class Bytecode {
 public:
  // In these descriptions, "TOS1" refers to the second-to-top-stack element and
  // "TOS0" refers to the top stack element.
  enum class Op {
    // Adds the top two values on the stack.
    kAdd,
    // Performs a bitwise AND of the top two values on the stack.
    kAnd,
    // Invokes the function given in the Bytecode's data argument. Arguments are
    // given on the stack with deeper elements being earlier in the arg list
    // (rightmost arg is TOS0 because we evaluate args left-to-right).
    kCall,
    // Casts the element on top of the stack to the type given in the optional
    // arg.
    kCast,
    // Concatenates TOS1 and TOS0, with TOS1 comprising the most significant
    // bits of the result.
    kConcat,
    // Combines the top N values on the stack (N given in the data argument)
    // into an array.
    kCreateArray,
    // Creates an N-tuple (N given in the data argument) from the values on the
    // stack.
    kCreateTuple,
    // Divides the N-1th value on the stack by the Nth value.
    kDiv,
    // Expands the N-tuple on the top of the stack by one level, placing leading
    // elements at the top of the stack. In other words, expanding the tuple
    // `(a, (b, c))` will result in a stack of `(b, c), a`, where `a` is on top
    // of the stack.
    kExpandTuple,
    // Compares TOS1 to TOS0, storing true if TOS1 == TOS0.
    kEq,
    // Compares TOS1 to TOS0, storing true if TOS1 >= TOS0.
    kGe,
    // Compares TOS1 to TOS0, storing true if TOS1 > TOS0.
    kGt,
    // Selects the TOS0'th element of the array- or tuple-typed value at TOS1.
    kIndex,
    // Inverts the bits of TOS0.
    kInvert,
    // Unconditional jump (relative).
    kJumpRel,
    // Pops the entry at the top of the stack and jumps (relative) if it is
    // true, otherwise PC proceeds as normal (i.e. PC = PC + 1).
    kJumpRelIf,
    // Indicates a jump destination PC for control flow integrity checking.
    // (Note that there's no actual execution logic for this opcode.)
    kJumpDest,
    // Compares TOS1 to TOS0, storing true if TOS1 <= TOS0.
    kLe,
    // Pushes the literal in the data argument onto the stack.
    kLiteral,
    // Loads the value from the data-arg-specified slot and pushes it onto the
    // stack.
    kLoad,
    // Performs a logical AND of TOS1 and TOS0.
    kLogicalAnd,
    // Performs a logical OR of TOS1 and TOS0.
    kLogicalOr,
    // Compares TOS1 to B, storing true if TOS1 < TOS0.
    kLt,
    // Multiplies the top two values on the stack.
    kMul,
    // Compares TOS1 to B, storing true if TOS1 != TOS0.
    kNe,
    // Performs two's complement negation of TOS0.
    kNegate,
    // Performs a bitwise OR of the top two values on the stack.
    kOr,
    // Performs a logical left shift of the second-to-top stack element by the
    // top element's number.
    kShll,
    // Performs a logical right shift of the second-to-top stack element by the
    // top element's number.
    kShrl,
    // Slices out a subset of the bits-typed value on TOS2,
    // starting at index TOS1 and ending at index TOS0.
    kSlice,
    // Stores the value at stack top into the arg-data-specified slot.
    kStore,
    // Subtracts the Nth value from the N-1th value on the stack.
    kSub,
    // Slices out TOS0 bits of the array- or bits-typed value on TOS2,
    // starting at index TOS1.
    kWidthSlice,
    // Performs a bitwise XOR of the top two values on the stack.
    kXor,
  };

  // Indicates the amount by which the PC should be adjusted.
  // Used by kJumpRel and kJumpRelIf opcodes.
  DEFINE_STRONG_INT_TYPE(JumpTarget, int64_t);

  // Indicates the size of a data structure; used by kCreateArray and
  // kCreateTuple opcodes.
  DEFINE_STRONG_INT_TYPE(NumElements, int64_t);

  // Indicates the index into which to store or from which to load a value. Used
  // by kLoad and kStore opcodes.
  DEFINE_STRONG_INT_TYPE(SlotIndex, int64_t);
  using Data = absl::variant<InterpValue, JumpTarget, NumElements, SlotIndex,
                             std::unique_ptr<ConcreteType>>;

  // Creates an operation w/o any accessory data. The span is present for
  // reporting error source location.
  Bytecode(Span source_span, Op op)
      : source_span_(source_span), op_(op), data_(absl::nullopt) {}

  // Creates an operation with associated string or InterpValue data.
  Bytecode(Span source_span, Op op, absl::optional<Data> data)
      : source_span_(source_span), op_(op), data_(std::move(data)) {}

  Span source_span() const { return source_span_; }
  Op op() const { return op_; }
  const absl::optional<Data>& data() const { return data_; }

  bool has_data() const { return data_.has_value(); }

  absl::StatusOr<JumpTarget> jump_target() const;
  absl::StatusOr<NumElements> num_elements() const;
  absl::StatusOr<InterpValue> value_data() const;
  absl::StatusOr<SlotIndex> slot_index() const;

  std::string ToString(bool source_locs = true) const;

  // Value used as an integer data placeholder in jumps before their
  // target/amount has become known during bytecode emission.
  static constexpr JumpTarget kPlaceholderJumpAmount = JumpTarget(-1);

  // Used for patching up e.g. jump targets.
  //
  // That is, if you're going to jump forward over some code, but you don't know
  // how big that code is yet (in terms of bytecodes), you emit the jump with
  // the kPlaceholderJumpAmount and later, once the code to jump over has been
  // emitted, you go back and make it jump over the right (measured) amount.
  //
  // Note: kPlaceholderJumpAmount is used as a canonical placeholder for things
  // that should be patched.
  void Patch(int64_t value) {
    XLS_CHECK(data_.has_value());
    JumpTarget jump_target = absl::get<JumpTarget>(data_.value());
    XLS_CHECK_EQ(jump_target, kPlaceholderJumpAmount);
    data_ = JumpTarget(value);
  }

 private:
  Span source_span_;
  Op op_;
  absl::optional<Data> data_;
};

// Holds all the bytecode implementing a function along with useful metadata.
class BytecodeFunction {
 public:
  // Note: this is an O(N) operation where N is the number of ops in the
  // bytecode. Also, "source" may be null.
  static absl::StatusOr<BytecodeFunction> Create(
      Function* source, std::vector<Bytecode> bytecode);

  Function* source() const { return source_; }
  const std::vector<Bytecode>& bytecodes() const { return bytecodes_; }
  // Returns the total number of memory "slots" used by the bytecodes.
  int64_t num_slots() const { return num_slots_; }

 private:
  BytecodeFunction(Function* source, std::vector<Bytecode> bytecode);
  absl::Status Init();

  Function* source_;
  std::vector<Bytecode> bytecodes_;
  int64_t num_slots_;
};

// Converts the given sequence of bytecodes to a more human-readable string,
// source_locs indicating whether source locations are annotated on the bytecode
// lines.
std::string BytecodesToString(absl::Span<const Bytecode> bytecodes,
                              bool source_locs);

// Converts a string as given by BytecodesToString(..., /*source_locs=*/false)
// into a bytecode sequence; e.g. for testing.
absl::StatusOr<std::vector<Bytecode>> BytecodesFromString(
    absl::string_view text);

}  // namespace xls::dslx

#endif  // XLS_DSLX_BYTECODE_H_
