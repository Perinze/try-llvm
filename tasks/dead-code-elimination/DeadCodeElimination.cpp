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
        static void deleteTriviallyDeadInstruction(Function &F) {
            errs() << "Delete trivially dead instructions in " << F.getName() << "\n";

            // TODO: change to reverse approach
            // TODO: iterate basic blocks
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
            } while (local_changed);

            errs() << "Done\n";
        }

        static void simplifyConditionalBranch(Function &F) {
            errs() << "Simplifying conditional branches that always branch to the same block\n";
            for (auto I = inst_begin(F); I != inst_end(F); ++I) {
                auto inst = &(*I);
                if (auto *BI = dyn_cast<BranchInst>(inst)) {
                    if (BI->getNumSuccessors() == 2) {
                        errs() << *BI << " has 2 successors\n";
                        if (BI->getSuccessor(0) == BI->getSuccessor(1)) {
                            errs() << *BI << "'s successors branch to the same basic blocks\n";
                            auto newBI = BranchInst::Create(BI->getSuccessor(0));
                            ReplaceInstWithInst(BI, newBI);
                            errs() << "Replaced with unconditional branch instruction\n";
                        }
                    }
                }
            }
            errs() << "Done\n";
        }

        static void simplifySingleBranchBlock(Function &F) {
            errs() << "Blocks that just contain a single unconditional branch instruction to the next block\n";
            for (auto BBI = F.begin(), E = F.end(); BBI != E; ++BBI) {
                auto instList = BBI->getInstList();
            }
            errs() << "Done\n";
        }

        static void simplifyBasicBlock(Function &F) {
            errs() << "Simplifying basic blocks in " << F.getName() << "\n";
            simplifySingleBranchBlock(F);
            simplifyConditionalBranch(F);
            errs() << "Done\n";
        }

        PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
            errs() << "Running DeadCodeEliminationPass on function " << F.getName() << "\n";

            deleteTriviallyDeadInstruction(F);
            simplifyBasicBlock(F);

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
