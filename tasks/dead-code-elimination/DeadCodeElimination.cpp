#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#define DEBUG_TYPE "remove-dead-code"

using namespace llvm;

namespace {
    struct DeadCodeEliminationPass : PassInfoMixin<DeadCodeEliminationPass> {
        static bool deleteTriviallyDeadInstruction(Function &F) {
            errs() << "Delete trivially dead instructions in " << F.getName() << "\n";

            // TODO: change to reverse approach
            // TODO: iterate basic blocks
            bool changed = false;
            bool local_changed;
            do {
                local_changed = false;
                auto dead_inst = std::set<Instruction*>();
                for (auto I = inst_begin(F), E = inst_end(F); I != E; ++I) {
                    auto inst = &(*I);
                    if (isInstructionTriviallyDead(inst)) {
                        dead_inst.insert(inst);
                    }
                }
                for (auto inst : dead_inst) {
                    local_changed = true;
                    inst->eraseFromParent();
                }
                if (local_changed) changed = true;
            } while (local_changed);

            errs() << "Done\n";
            return changed;
        }

        static void mergeSameSuccessor(BranchInst *BI) {
            assert(BI->getSuccessor(0) == BI->getSuccessor(1));
            errs() << *BI << "'s successors branch to the same basic blocks\n";
            auto newBI = BranchInst::Create(BI->getSuccessor(0));
            ReplaceInstWithInst(BI, newBI);
            errs() << "Replaced with unconditional branch instruction\n";
        }

        static bool simplifyConditionalBranch(Function &F) {
            errs() << "Simplifying conditional branches that always branch to the same block\n";
            bool changed = true;
            bool local_changed = false;
            do {
                local_changed = false;
                for (auto I = inst_begin(F); I != inst_end(F); ++I) {
                    auto inst = &(*I);
                    if (auto *BI = dyn_cast<BranchInst>(inst)) {
                        if (BI->getNumSuccessors() == 2 and BI->getSuccessor(0) == BI->getSuccessor(1)) {
                            errs() << *BI << " has 2 same successors\n";
                            mergeSameSuccessor(BI);
                            local_changed = true;
                            break;
                        }
                    }
                }
                if (local_changed) changed = true;
            } while (local_changed);
            errs() << "Done\n";
            return changed;
        }

        static bool hasOneOnlyUnconditionalBranch(BasicBlock &BB) {
            errs() << "Checking if " << BB.getName() << " has one only uncond branch\n";
            if (BB.size() != 1) return false;
            errs() << "It has only one instruction\n";
            if (auto *BI = dyn_cast<BranchInst>(&BB.front())) {
                errs() << "It's branch\n";
                if (BI->isUnconditional()) {
                    errs() << "It's uncond\n";
                    return true;
                }
            }
            return false;
        }

        static bool simplifySingleBranchBlock(Function &F) {
            errs() << "Simplifying bb have one uncond branch only\n";
            auto pass = 0U;
            bool changed = false;
            bool local_changed = false;
            do {
                auto toErase = std::set<BasicBlock*>();
                local_changed = false;
                errs() << "Merge pass #" << pass++ << "\n";
                for (auto & BB : F) {
                    if (toErase.count(&BB)) continue;
                    for (auto & I : BB) {
                        auto inst = &I;
                        if (auto *BI = dyn_cast<BranchInst>(inst)) {
                            auto n = BI->getNumSuccessors();
                            errs() << "Current is a branch instruction with " << n << " successor\n";
                            for (auto i = 0U; i < n; ++i) {
                                auto *successorBB = BI->getSuccessor(i);
                                if (!hasOneOnlyUnconditionalBranch(*successorBB)) continue;
                                errs() << "Successor #" << i << " has one only uncond branch\n";
                                BI->setSuccessor(i, successorBB->getUniqueSuccessor());
                                toErase.insert(successorBB);
                                local_changed = true;
                            }
                        }
                    }
                }
                errs() << "Cleaning up useless bb\n";
                for (auto & BB : toErase) {
                    BB->eraseFromParent();
                }
                toErase.clear();
                if (local_changed) changed = true;
            } while (local_changed);
            errs() << "Done\n";
            return changed;
        }

        static bool simplifyBasicBlock(Function &F) {
            errs() << "Simplifying basic blocks in " << F.getName() << "\n";
            bool changed = false;
            bool local_changed = false;
            do {
                local_changed = false;
                local_changed |= simplifySingleBranchBlock(F);
                local_changed |= simplifyConditionalBranch(F);
                if (local_changed) changed = true;
            } while (local_changed);
            errs() << "Done\n";
            return changed;
        }

        //struct StackSlotStore {
        //    // bool isAlloca; // unnecessary
        //    bool stored;
        //    bool loaded;
        //    bool passed;
        //    StackSlotStore(): stored(false), loaded(false), passed(false) {}
        //};

        static bool removeUselessStoreToStackSlot(Function &F) {
            auto uselessAlloca = std::set<AllocaInst*>();
            for (auto I = inst_begin(F), E = inst_end(F); I != E; ++I) {
                auto inst = &(*I);
                if (auto *AI = dyn_cast<AllocaInst>(inst)) {
                    errs() << "Alloca " << AI->getValueName() << " " << AI->getValueID() << "\n";

                    errs() << "Users:\n";
                    bool onlyStored = true;
                    auto i = 0U;
                    for (auto user : AI->users()) {
                        errs() << "#" << i++ << " : " << user << "\n";
                        //errs() << "droppable: " << user->isDroppable() << "\n";
                        if (auto *SI = dyn_cast<StoreInst>(user)) {
                            errs() << "is a store inst\n";
                        } else {
                            errs() << "not a store inst, onlyStore = false\n";
                            onlyStored = false;
                            break;
                        }
                    }
                    if (onlyStored) {
                        errs() << "This alloca is only stored\n";
                        uselessAlloca.insert(AI);
                        errs() << "Added for erase list\n";
                    }
                    errs() << "\n";
                }
            }
            for (auto *AI : uselessAlloca) {
                for (auto *user : AI->users()) {
                    auto *SI = cast<StoreInst>(user);
                    SI->eraseFromParent();
                    errs() << "Erased " << SI << "\n";
                }
                AI->eraseFromParent();
                errs() << "Erased " << AI << "\n";
            }
            if (uselessAlloca.empty()) return false;
            else return true;
        }

        static void test(Function &F) {
            for (auto I = inst_begin(F), E = inst_end(F); I != E; ++I) {
                auto inst = &(*I);
                if (auto *AI = dyn_cast<AllocaInst>(inst)) {
                    errs() << "Is AllocaInst: " << *AI << "\n";
                    errs() << "Value: " << AI->getValueName() << " : " << AI->getValueID() << "\n";
                } else if (auto *SI = dyn_cast<StoreInst>(inst)) {
                    errs() << "Is StoreInst: " << *SI << "\n";
                    auto n = SI->getNumOperands();
                    errs() << "Operand number: " << n << "\n";
                    for (auto i = 0U; i < n; ++i) {
                        errs() << "Operand " << i << ": " << SI->getOperand(i) << "\n";
                    }
                    if (auto *OpAI = dyn_cast<AllocaInst>(SI->getOperand(1))) {
                        errs() << "Operand 1 is a alloca inst\n";
                        errs() << "It's value name and id : " << OpAI->getValueName() << " " << OpAI->getValueID() << "\n";
                    }
                } else if (auto *LI = dyn_cast<LoadInst>(inst)) {
                    errs() << "Is LoadInst: " << *LI << "\n";
                    auto n = LI->getNumOperands();
                    errs() << "Operand number: " << n << "\n";
                    for (auto i = 0U; i < n; ++i) {
                        errs() << "Operand " << i << ": " << LI->getOperand(i) << "\n";
                    }
                    if (auto *OpAI = dyn_cast<AllocaInst>(LI->getOperand(0))) {
                        errs() << "Operand 0 is a alloca inst\n";
                        errs() << "It's value name and id : " << OpAI->getValueName() << " " << OpAI->getValueID() << "\n";
                    }
                }
                errs() << "\n";
            }
        }

        PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
            errs() << "Running DeadCodeEliminationPass on function " << F.getName() << "\n";

            //test(F);
            //return PreservedAnalyses::none();

            bool changed = false;
            do {
                changed = false;
                changed |= deleteTriviallyDeadInstruction(F);
                changed |= simplifyBasicBlock(F);
                changed |= removeUselessStoreToStackSlot(F);
            } while (changed);

            return PreservedAnalyses::none();
        }
    };

} // namespace

/// Registration
PassPluginLibraryInfo getPassPluginInfo() {
    const auto callback = [](PassBuilder &PB) {
        PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM, auto) {
                    if (Name == "dead-code-elimination") {
                        FPM.addPass(DeadCodeEliminationPass());
                        return true;
                    }
                    return false;
                });
    };
    return {LLVM_PLUGIN_API_VERSION, "DeadCodeEliminationPass",
            LLVM_VERSION_STRING, callback};
};

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return getPassPluginInfo();
}

#undef DEBUG_TYPE
