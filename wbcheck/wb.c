// Make: clang -o wb -g wb.c -lclang

#include <stdlib.h>
#include <stdio.h>
#include <clang-c/CXCompilationDatabase.h>
#include <clang-c/Index.h>
#include <clang-c/BuildSystem.h>
#include <clang-c/CXErrorCode.h>
#include <clang-c/CXString.h>
#include <clang-c/Documentation.h>
#include <clang-c/Platform.h>


enum CXChildVisitResult functionVisitor(CXCursor cursor, 
				   CXCursor parent,
				   CXClientData client_data) {
  enum CXCursorKind kind = clang_getCursorKind(cursor);
  if (kind == CXCursor_BinaryOperator) {
    CXSourceRange extent = clang_getCursorExtent(cursor);
    CXSourceLocation start_location = clang_getRangeStart(extent);
    CXSourceLocation end_location = clang_getRangeEnd(extent);
    printf("wb needed from %d to %d\n", 
	   start_location.int_data, end_location.int_data);
  }
  return(CXChildVisit_Recurse);
}

void walk_ast(CXTranslationUnit tu) {
  CXCursor root_cursor = clang_getTranslationUnitCursor(tu);
  clang_visitChildren(root_cursor, functionVisitor, NULL);
}

// translation unit consists of a single source file plut the contents of any
// header files included (directly or indirectly).
CXTranslationUnit  parse_file(char *filename) {
  // a set of translation units that would typically be linked together into
  // an executable or library.
  CXIndex index = clang_createIndex(0, 1);
  const char *const *cmd_line_args = 0;
  int numargs = 0;
  CXTranslationUnit tu = clang_parseTranslationUnit(index, 
						    filename,
						    cmd_line_args,
						    numargs,
						    0,
						    0,
						    CXTranslationUnit_None);
  return(tu);
}

int main(int argc, char *argv[]) {
  CXTranslationUnit tu = parse_file("wbtest.c");
  walk_ast(tu);
}
