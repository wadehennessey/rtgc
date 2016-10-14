// Run: ./wb filename.c --

#include <string>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Lex/Lexer.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory MatcherSampleCategory("Matcher Sample");

class IncrementForLoopHandler : public MatchFinder::MatchCallback {
public:
  IncrementForLoopHandler(Rewriter &Rewrite) : Rewrite(Rewrite) {}

  virtual void run(const MatchFinder::MatchResult &Result) {
    const VarDecl *IncVar = Result.Nodes.getNodeAs<VarDecl>("incVarName");
    Rewrite.InsertText(IncVar->getLocStart(), "/* increment */", true, true);
  }
  
private:
  Rewriter &Rewrite;
};

class AssignHandler : public MatchFinder::MatchCallback {
public:
  AssignHandler(Rewriter &Rewrite) : Rewrite(Rewrite) {}

  virtual void run(const MatchFinder::MatchResult &Result) {
    const BinaryOperator *Assign = Result.Nodes.getNodeAs<BinaryOperator>("assign");
    Rewrite.InsertText(Assign->getLHS()->getLocStart(), 
		       "RTwrite_barrier(&( ", 
		       true, 
		       true);
    Rewrite.ReplaceText((Assign->getOperatorLoc()), 
			Assign->getOpcodeStr().size(),
			"),");
    Rewrite.InsertTextAfterToken(Assign->getRHS()->getLocEnd(), ")");
  }
  
private:
  Rewriter &Rewrite;
};

class MemcpyHandler : public MatchFinder::MatchCallback {
public:
  MemcpyHandler(Rewriter &Rewrite) : Rewrite(Rewrite) {}

  virtual void run(const MatchFinder::MatchResult &Result) {
    const CallExpr *Memcpy = Result.Nodes.getNodeAs<CallExpr>("memcpy");
    Rewrite.InsertText(Memcpy->getLocStart(), "RT", true, true);
  }
  
private:
  Rewriter &Rewrite;
};

class MemsetHandler : public MatchFinder::MatchCallback {
public:
  MemsetHandler(Rewriter &Rewrite) : Rewrite(Rewrite) {}

  virtual void run(const MatchFinder::MatchResult &Result) {
    const CallExpr *Memset = Result.Nodes.getNodeAs<CallExpr>("memset");
    Rewrite.InsertText(Memset->getLocStart(), "RT", true, true);
  }
  
private:
  Rewriter &Rewrite;
};

// Should we be calling BinaryOperator::isCompundAssignmentOp() on all
// assignment ops to end up doing this rewrite?
class RecordAssignHandler : public MatchFinder::MatchCallback {
public:
  RecordAssignHandler(Rewriter &Rewrite) : Rewrite(Rewrite) {}

  virtual void run(const MatchFinder::MatchResult &Result) {
    const BinaryOperator *Assign = 
      Result.Nodes.getNodeAs<BinaryOperator>("assign");
    SourceManager &sm = Rewrite.getSourceMgr();
    LangOptions lopt;
    SourceLocation b(Assign->getLHS()->getLocStart());
    SourceLocation e(Lexer::getLocForEndOfToken(Assign->getLHS()->getLocEnd(),
						0,
						sm,
						lopt));
    std::string lhs_text  = 
      std::string(sm.getCharacterData(b),
		  sm.getCharacterData(e) - sm.getCharacterData(b));

    Rewrite.InsertText(Assign->getLHS()->getLocStart(), 
		       "RTrecord_write_barrier(&( ", 
		       true, 
		       true);
    Rewrite.ReplaceText((Assign->getOperatorLoc()), 
			Assign->getOpcodeStr().size(),
			"),");
    Rewrite.InsertTextAfterToken(Assign->getRHS()->getLocEnd(), 
				 " , sizeof(" + lhs_text + "))");
  }
  
private:
  Rewriter &Rewrite;
};


// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser. It registers a couple of matchers and runs them on
// the AST.
class MyASTConsumer : public ASTConsumer {
public:
  MyASTConsumer(Rewriter &R) : HandlerForAssign(R),
                               HandlerForMemcpy(R),
			       HandlerForMemset(R),
                               HandlerForRecordAssign(R) {
    Matcher.addMatcher(
	binaryOperator(hasOperatorName("="), 
	       hasRHS(expr(hasType(isAnyPointer()))),
	       // skip local variable assignments
	       unless(hasLHS(declRefExpr(to(varDecl(hasAutomaticStorageDuration())))))).bind("assign"), 
	&HandlerForAssign);
    Matcher.addMatcher(
        callExpr(callee(functionDecl(hasName("memcpy")))).bind("memcpy"),
        &HandlerForMemcpy);
    Matcher.addMatcher(
        callExpr(callee(functionDecl(hasName("memset")))).bind("memset"),
        &HandlerForMemset);
    // Match struct, class, and union assignments
    Matcher.addMatcher(
        binaryOperator(hasOperatorName("="), 
		       hasType(qualType(hasCanonicalType(recordType()))), 	                         unless(hasLHS(declRefExpr(to(varDecl(hasAutomaticStorageDuration())))))).bind("assign"),
	&HandlerForRecordAssign);
  }

  void HandleTranslationUnit(ASTContext &Context) override {
    // Run the matchers when we have the whole TU parsed.
    Matcher.matchAST(Context);
  }

private:
  AssignHandler HandlerForAssign;
  MemcpyHandler HandlerForMemcpy;
  MemsetHandler HandlerForMemset;
  RecordAssignHandler HandlerForRecordAssign;
  MatchFinder Matcher;
};

// For each source file provided to the tool, a new FrontendAction is created.
class MyFrontendAction : public ASTFrontendAction {
public:
  MyFrontendAction() {}
  void EndSourceFileAction() override {
    TheRewriter.getEditBuffer(TheRewriter.getSourceMgr().getMainFileID())
        .write(llvm::outs());
  }

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef file) override {
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return llvm::make_unique<MyASTConsumer>(TheRewriter);
  }

private:
  Rewriter TheRewriter;
};

int main(int argc, const char **argv) {
  CommonOptionsParser op(argc, argv, MatcherSampleCategory);
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());

  return Tool.run(newFrontendActionFactory<MyFrontendAction>().get());
}
