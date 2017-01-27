/*
 * Copyright 2017 Wade Lawrence Hennessey
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

// Make: clang -o wb -g wb.c -lclang

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <clang-c/CXCompilationDatabase.h>
#include <clang-c/Index.h>
#include <clang-c/BuildSystem.h>
#include <clang-c/CXErrorCode.h>
#include <clang-c/CXString.h>
#include <clang-c/Documentation.h>
#include <clang-c/Platform.h>

// YOW! What a hack, but 
int isAssignment(CXCursor cursor, CXTranslationUnit tu) {
  int result = 0;
  CXToken *tokens;
  unsigned numTokens;
  CXSourceRange range = clang_getCursorExtent(cursor);
  clang_tokenize(tu, range, &tokens, &numTokens);
  for(unsigned i = 0; i <numTokens; i++) {
    CXString s = clang_getTokenSpelling(tu, tokens[i]);
    const char* str = clang_getCString(s);
    printf("%s ", str);
    if(strcmp(str,"=") == 0 ) {
      result = 1;
    }
    clang_disposeString(s);
  }
  clang_disposeTokens(tu, tokens, numTokens);
  printf("\n");
  return(result);
}

enum CXChildVisitResult functionVisitor(CXCursor cursor, 
					CXCursor parent,
					CXClientData tu) {
  enum CXCursorKind kind = clang_getCursorKind(cursor);
  if ((kind == CXCursor_BinaryOperator) &&
      (isAssignment(cursor, tu))) {
    CXSourceRange extent = clang_getCursorExtent(cursor);
    CXSourceLocation start_location = clang_getRangeStart(extent);
    CXSourceLocation end_location = clang_getRangeEnd(extent);

    unsigned int startLine = 0, startColumn = 0;
    unsigned int endLine   = 0, endColumn   = 0;

    clang_getSpellingLocation(start_location, 0, &startLine, &startColumn, 0);
    clang_getSpellingLocation(end_location, 0, &endLine, &endColumn, 0);
    printf("wb needed from %d to %d\n", 
	   start_location.int_data, end_location.int_data);
  }
  return(CXChildVisit_Recurse);
}

void walk_ast(CXTranslationUnit tu) {
  CXCursor root_cursor = clang_getTranslationUnitCursor(tu);
  clang_visitChildren(root_cursor, functionVisitor, tu);
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
