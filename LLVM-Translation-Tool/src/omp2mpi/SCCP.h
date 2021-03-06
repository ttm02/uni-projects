//===- SCCP.cpp - Sparse Conditional Constant Propagation -------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// \file
// This file implements sparse conditional constant propagation and merging:
//
// Specifically, this:
//   * Assumes values are constant unless proven otherwise
//   * Assumes BasicBlocks are dead unless proven otherwise
//   * Proves values to be constant, and replaces them with constants
//   * Proves conditional branches to be unconditional
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_SCALAR_SCCP_H
#define LLVM_TRANSFORMS_SCALAR_SCCP_H

#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"

// TODO
// As i cannot figure out how to use this Pass from Our Pass
// I will just export the needed Function
bool runSCCP(llvm::Function &F, const llvm::DataLayout &DL,
             const llvm::TargetLibraryInfo *TLI);

namespace llvm
{

class Function;
/// This pass performs function-level constant propagation and merging.
class SCCPPass : public PassInfoMixin<SCCPPass>
{
  public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

bool runIPSCCP(Module &M, const DataLayout &DL, const TargetLibraryInfo *TLI);
} // end namespace llvm

#endif // LLVM_TRANSFORMS_SCALAR_SCCP_H
