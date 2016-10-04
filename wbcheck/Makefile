# (C) Copyright 2015 - 2016 by Wade L. Hennessey. All rights reserved.

CC = clang

wb:
	g++ -fno-rtti -O0 -g `/home/wade/build//bin/llvm-config --cxxflags` -I/home/wade/llvm//tools/clang/include -I/home/wade/build//tools/clang/include wb.cpp \
	-Wl,--start-group -lclangAST -lclangASTMatchers -lclangAnalysis -lclangBasic -lclangDriver -lclangEdit -lclangFrontend -lclangFrontendTool -lclangLex -lclangParse -lclangSema -lclangEdit -lclangRewrite -lclangRewriteFrontend -lclangStaticAnalyzerFrontend -lclangStaticAnalyzerCheckers -lclangStaticAnalyzerCore -lclangSerialization -lclangToolingCore -lclangTooling -lclangFormat -Wl,--end-group `/home/wade/build//bin/llvm-config --ldflags --libs --system-libs` -o wb

wb-libclang:
	$(CC) -o wb-libclang -g wb-libclang.c -lclang

clean:  
	rm -f wb


