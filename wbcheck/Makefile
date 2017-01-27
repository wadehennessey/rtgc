# Copyright 2017 Wade Lawrence Hennessey
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

CC = clang

all:	wb

wb:	wb.cpp
	g++ -fno-rtti -O0 -g `/home/wade/build//bin/llvm-config --cxxflags` -I/home/wade/llvm//tools/clang/include -I/home/wade/build//tools/clang/include wb.cpp \
	-Wl,--start-group -lclangAST -lclangASTMatchers -lclangAnalysis -lclangBasic -lclangDriver -lclangEdit -lclangFrontend -lclangFrontendTool -lclangLex -lclangParse -lclangSema -lclangEdit -lclangRewrite -lclangRewriteFrontend -lclangStaticAnalyzerFrontend -lclangStaticAnalyzerCheckers -lclangStaticAnalyzerCore -lclangSerialization -lclangToolingCore -lclangTooling -lclangFormat -Wl,--end-group `/home/wade/build//bin/llvm-config --ldflags --libs --system-libs` -o wb

wb-libclang:
	$(CC) -o wb-libclang -g wb-libclang.c -lclang

clean:  
	rm -f wb wb-libclang


