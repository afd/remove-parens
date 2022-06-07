// Copyright 2022 Imperial College London
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstddef>
#include <memory>
#include <random>
#include <string>
#include <type_traits>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/Expr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/raw_ostream.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#elif defined(_MSC_VER)
#pragma warning(push)
#endif

// Set up the command line options
// NOLINTNEXTLINE
static llvm::cl::extrahelp common_help(
    clang::tooling::CommonOptionsParser::HelpMessage);
// NOLINTNEXTLINE
static llvm::cl::OptionCategory remove_parens_category("remove-parens options");

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

class RemoveParensVisitor : public clang::RecursiveASTVisitor<RemoveParensVisitor> {
 public:
  RemoveParensVisitor(const clang::ASTContext& ast_context) : ast_context_(ast_context) {}

  bool TraverseParenExpr(clang::ParenExpr* paren_expr) {
    RecursiveASTVisitor::TraverseParenExpr(paren_expr);
    const auto* sub_expr = paren_expr->getSubExpr();
    // TODO: perhaps it is not necessary to require that the subexpression
    //  starts and ends in the main source file.
    if (StartsAndEndsInMainSourceFile(*paren_expr) && StartsAndEndsInMainSourceFile(*sub_expr)) {
      if (llvm::dyn_cast<clang::ParenExpr>(sub_expr)
          || llvm::dyn_cast<clang::IntegerLiteral>(sub_expr)
          || llvm::dyn_cast<clang::CallExpr>(sub_expr)) {
        // TODO: more kinds of expression could be added as disjuncts of this
        //  conditional, e.g. variable identifier expressions - any expression
        //  for which it's definitely safe to remove parentheses.
        to_remove_.push_back(paren_expr);
      }
    }
    return true;
  }

  const std::vector<const clang::ParenExpr*>& GetParensToRemove() {
    return to_remove_;
  }

 private:
  template <typename HasSourceRange>
  [[nodiscard]] bool StartsAndEndsInMainSourceFile(
      const HasSourceRange& ast_node) const;

  const clang::ASTContext& ast_context_;
  std::vector<const clang::ParenExpr*> to_remove_;
};

template <typename HasSourceRange>
bool RemoveParensVisitor::StartsAndEndsInMainSourceFile(
    const HasSourceRange& ast_node) const {
  const clang::SourceManager& source_manager = ast_context_.getSourceManager();
  auto begin_file_id =
      source_manager.getFileID(ast_node.getSourceRange().getBegin());
  auto end_file_id =
      source_manager.getFileID(ast_node.getSourceRange().getEnd());
  auto main_file_id = source_manager.getMainFileID();
  return begin_file_id == main_file_id && end_file_id == main_file_id;
}

class RemoveParensAstConsumer : public clang::ASTConsumer {
 public:
  RemoveParensAstConsumer(const clang::CompilerInstance& compiler_instance)
      : compiler_instance_(compiler_instance),
        visitor_(std::make_unique<RemoveParensVisitor>(
            compiler_instance.getASTContext())) {}

  void HandleTranslationUnit(clang::ASTContext& context) override;

 private:
  const clang::CompilerInstance& compiler_instance_;
  std::unique_ptr<RemoveParensVisitor> visitor_;
  clang::Rewriter rewriter_;
};

void RemoveParensAstConsumer::HandleTranslationUnit(clang::ASTContext& context) {
  if (context.getDiagnostics().hasErrorOccurred()) {
    // There has been an error, so we don't do any processing.
    return;
  }
  visitor_->TraverseDecl(context.getTranslationUnitDecl());
  rewriter_.setSourceMgr(compiler_instance_.getSourceManager(),
                         compiler_instance_.getLangOpts());

  // Do the rewrites.
  for (const auto* paren_expr : visitor_->GetParensToRemove()) {
    bool result = rewriter_.ReplaceText(paren_expr->getSourceRange(), rewriter_.getRewrittenText(paren_expr->getSubExpr()->getSourceRange()));
    (void) result;
    assert (!result && "Rewrite failed");
  }

  bool result = rewriter_.overwriteChangedFiles();
  (void)result;  // Keep release mode compilers happy
  assert(!result && "Something went wrong emitting rewritten files.");
}

class RemoveParensFrontendAction : public clang::ASTFrontendAction {
 public:
  RemoveParensFrontendAction() {}

  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance& ci, llvm::StringRef file) override;

};

std::unique_ptr<clang::tooling::FrontendActionFactory>
NewRemoveParensFrontendActionFactory() {
  class RemoveParensFrontendActionFactory
      : public clang::tooling::FrontendActionFactory {
   public:
    RemoveParensFrontendActionFactory() {}

    std::unique_ptr<clang::FrontendAction> create() override {
      return std::make_unique<RemoveParensFrontendAction>();
    }
  };

  return std::make_unique<RemoveParensFrontendActionFactory>();
}

std::unique_ptr<clang::ASTConsumer> RemoveParensFrontendAction::CreateASTConsumer(
    clang::CompilerInstance& ci, llvm::StringRef file) {
  (void)file;
  return std::make_unique<RemoveParensAstConsumer>(ci);
}

int main(int argc, const char** argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

  llvm::Expected<clang::tooling::CommonOptionsParser> options =
      clang::tooling::CommonOptionsParser::create(argc, argv, remove_parens_category,
                                                  llvm::cl::OneOrMore);
  if (!options) {
    llvm::errs() << toString(options.takeError());
    return 1;
  }

  clang::tooling::ClangTool Tool(options.get().getCompilations(),
                                 options.get().getSourcePathList());

  std::unique_ptr<clang::tooling::FrontendActionFactory> factory =
      NewRemoveParensFrontendActionFactory();

  return Tool.run(factory.get());
}
