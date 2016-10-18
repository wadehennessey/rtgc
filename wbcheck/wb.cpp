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
#include <iostream>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory MatcherSampleCategory("Matcher Sample");

int binop_strings(Rewriter &Rewrite,
		  const BinaryOperator *Assign,
		  std::string *lhs_text, 
		  std::string *rhs_text) {
  LangOptions lopt;
  SourceManager &sm = Rewrite.getSourceMgr();
  SourceLocation lhs_b(Assign->getLHS()->getLocStart());
  SourceLocation lhs_e(Lexer::getLocForEndOfToken(Assign->getLHS()->getLocEnd(),
					      0,
					      sm,
					      lopt));
  SourceLocation rhs_b(Assign->getRHS()->getLocStart());
  SourceLocation rhs_e(Lexer::getLocForEndOfToken(Assign->getRHS()->getLocEnd(),
						  0,
						  sm,
						  lopt));
  
  *lhs_text =  std::string(sm.getCharacterData(lhs_b),
		   sm.getCharacterData(lhs_e) - 
		   sm.getCharacterData(lhs_b));
  *rhs_text =  std::string(sm.getCharacterData(rhs_b),
			   sm.getCharacterData(rhs_e) - 
			   sm.getCharacterData(rhs_b));
  return(sm.getCharacterData(rhs_e) - sm.getCharacterData(lhs_b));
}

class AssignHandler : public MatchFinder::MatchCallback {
public:
  AssignHandler(Rewriter &Rewrite) : Rewrite(Rewrite) {}

  virtual void run(const MatchFinder::MatchResult &Result) {
    const BinaryOperator *Assign = Result.Nodes.getNodeAs<BinaryOperator>("assign");
    std::string lhs_text, rhs_text;
    int assign_length = binop_strings(Rewrite, Assign, &lhs_text, &rhs_text);
    Rewrite.ReplaceText(Assign->getLHS()->getLocStart(),
			assign_length,
			"write_barrier(&(" + lhs_text + "), " + rhs_text + ")");
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

void warn(Rewriter &Rewrite, 
		 const BinaryOperator *Assign,
		 std::string warning) {
  SourceManager &sm = Rewrite.getSourceMgr();
  LangOptions lopt;
  SourceLocation b(Assign->getLHS()->getLocStart());

  unsigned int line = sm.getPresumedLineNumber(b, 0);
  unsigned int col  = sm.getPresumedColumnNumber(b, 0);
  StringRef filename = sm.getBufferName(b, 0);

  std::cout << filename.data()  << " ";
  std::cout << "line:" << line << ",col:" << col  << warning  << std::endl; 
}

// Should we be calling BinaryOperator::isCompundAssignmentOp() on all
// assignment ops to end up doing this rewrite?
class RecordAssignHandler : public MatchFinder::MatchCallback {
public:
  RecordAssignHandler(Rewriter &Rewrite) : Rewrite(Rewrite) {}

  virtual void run(const MatchFinder::MatchResult &Result) {
    const BinaryOperator *Assign = 
      Result.Nodes.getNodeAs<BinaryOperator>("assign");
    warn(Rewrite, Assign, " - RecordAssign without write barrier");

    std::string lhs_text, rhs_text;
    int assign_length = binop_strings(Rewrite, Assign, &lhs_text, &rhs_text);
    Rewrite.ReplaceText(Assign->getLHS()->getLocStart(),
			assign_length,
			"RTrecord_write_barrier(&(" + 
			lhs_text + "), &("
			+ rhs_text + "), " + 
			"sizeof(" + lhs_text + "))");
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
