
.SUFFIXES:	# disable default suffix rules

# compiler specific
CXXFLAGS = -g -O0 -fprofile-arcs -ftest-coverage
CPPFLAGS = -I. 
CXX      = g++
COMPILE	 = $(CXX) $(CPPFLAGS) $(CXXFLAGS) -c
MAKEDEP  = $(CXX) $(CPPFLAGS) -M
LINK     = $(CXX)
AR       = ar cru
PYTHON   = python

all : bin/libocc_parser.a bin/libocc_analyzer.a

test: \
	parser-test.passed \
	elaboration-test.passed \
	lexer-test.passed \
	program-string-test.passed

libocc_parser_a_SRCS = \
    occ-core/parser/BigHashTable.cc \
    occ-core/parser/CerrErrorLog.cc \
    occ-core/parser/DupLeaf.cc \
    occ-core/parser/Encoding.cc \
    occ-core/parser/HashTable.cc \
    occ-core/parser/Leaf.cc \
    occ-core/parser/LeafName.cc \
    occ-core/parser/LeafThis.cc \
    occ-core/parser/Lex.cc \
    occ-core/parser/NonLeaf.cc \
    occ-core/parser/Parser.cc \
    occ-core/parser/Program.cc \
    occ-core/parser/ProgramFile.cc \
    occ-core/parser/ProgramFromStdin.cc \
    occ-core/parser/ProgramString.cc \
    occ-core/parser/Ptree.cc \
    occ-core/parser/PtreeAccessDecl.cc \
    occ-core/parser/PtreeAccessSpec.cc \
    occ-core/parser/PtreeArray.cc \
    occ-core/parser/PtreeBlock.cc \
    occ-core/parser/PtreeBrace.cc \
    occ-core/parser/PtreeClassBody.cc \
    occ-core/parser/PtreeClassSpec.cc \
    occ-core/parser/PtreeConstants.cc \
    occ-core/parser/PtreeDeclaration.cc \
    occ-core/parser/PtreeDeclarator.cc \
    occ-core/parser/PtreeDumper.cc \
    occ-core/parser/PtreeEnumSpec.cc \
    occ-core/parser/PtreeExprStatement.cc \
    occ-core/parser/PtreeExternTemplate.cc \
    occ-core/parser/PtreeFstyleCastExpr.cc \
    occ-core/parser/PtreeLinkageSpec.cc \
    occ-core/parser/PtreeMetaclassDecl.cc \
    occ-core/parser/PtreeName.cc \
    occ-core/parser/PtreeNamespaceAlias.cc \
    occ-core/parser/PtreeNamespaceSpec.cc \
    occ-core/parser/PtreeTemplateDecl.cc \
    occ-core/parser/PtreeTemplateInstantiation.cc \
    occ-core/parser/PtreeTypedef.cc \
    occ-core/parser/PtreeUserAccessSpec.cc \
    occ-core/parser/PtreeUserdefKeyword.cc \
    occ-core/parser/PtreeUsing.cc \
    occ-core/parser/PtreeUtil.cc \
    occ-core/parser/TheErrorLog.cc \
    occ-core/parser/auxil.cc \
    occ-core/parser/deprecated.cc \
    occ-core/parser/ptree-generated.cc

libocc_parser_a_OBJS = $(libocc_parser_a_SRCS:.cc=.o)
libocc_parser_a_DEPS = $(libocc_parser_a_SRCS:.cc=.deps)

libocc_analyzer_a_SRCS = \
    occ-core/analyzer/Bind.cc \
    occ-core/analyzer/BindClassName.cc \
    occ-core/analyzer/BindEnumName.cc \
    occ-core/analyzer/BindTemplateClass.cc \
    occ-core/analyzer/BindTemplateFunction.cc \
    occ-core/analyzer/BindTypedefName.cc \
    occ-core/analyzer/BindVarName.cc \
    occ-core/analyzer/ChangedMemberList.cc \
    occ-core/analyzer/Class.cc \
    occ-core/analyzer/ClassArray.cc \
    occ-core/analyzer/ClassBodyWalker.cc \
    occ-core/analyzer/ClassWalker.cc \
    occ-core/analyzer/EncodingUtil.cc \
    occ-core/analyzer/Environment.cc \
    occ-core/analyzer/Member.cc \
    occ-core/analyzer/MemberFunction.cc \
    occ-core/analyzer/MemberList.cc \
    occ-core/analyzer/OpencxxConfiguration.cc \
    occ-core/analyzer/PtreeIter.cc \
    occ-core/analyzer/PtreeMaker.cc \
    occ-core/analyzer/PtreeTypeUtil.cc \
    occ-core/analyzer/TemplateClass.cc \
    occ-core/analyzer/TypeInfo.cc \
    occ-core/analyzer/Walker.cc

libocc_analyzer_a_OBJS = $(libocc_analyzer_a_SRCS:.cc=.o)
libocc_analyzer_a_DEPS = $(libocc_analyzer_a_SRCS:.cc=.d)

parser_test_glue_SRCS = \
    occ-core/parser/ParserTestGlue.cc

parser_test_glue_OBJS = $(parser_test_glue_SRCS:.cc=.o)
parser_test_glue_DEPS = $(parser_test_glue_SRCS:.cc=.d)

elaboration_test_glue_SRCS = \
    occ-core/analyzer/ElaborationTestGlue.cc

elaboration_test_glue_OBJS = $(elaboration_test_glue_SRCS:.cc=.o)
elaboration_test_glue_DEPS = $(elaboration_test_glue_SRCS:.cc=.d)

lexer_test_glue_SRCS = \
    occ-core/parser/LexerTestGlue.cc

lexer_test_glue_OBJS = $(lexer_test_glue_SRCS:.cc=.o)
lexer_test_glue_DEPS = $(lexer_test_glue_SRCS:.cc=.d)

program_string_test_SRCS = \
    occ-core/parser/ProgramStringTest.cc

program_string_test_OBJS = $(program_string_test_SRCS:.cc=.o)
program_string_test_DEPS = $(program_string_test_SRCS:.cc=.d)


%.d : %.cc
	-$(MAKEDEP) $< -o $@.tmp1
	sed 's&^.*[.]o:&$(<:.cc=.o) $@ :&g' <$@.tmp1 >$@.tmp2
	rm $@.tmp1
	mv $@.tmp2 $@

%.o : %.cc
	$(COMPILE) $< -o $@

elaboration-test.passed : bin/elaboration-test-glue elaboration-test.py
	$(PYTHON) elaboration-test.py 
	touch $@

parser-test.passed : bin/parser-test-glue parser-test.py
	$(PYTHON) parser-test.py 
	touch $@

lexer-test.passed : bin/lexer-test-glue lexer-test.py
	$(PYTHON) lexer-test.py
	touch $@

program-string-test.passed : bin/program-string-test
	./bin/program-string-test
	touch $@

bin/libocc_parser.a : $(libocc_parser_a_OBJS)
	$(AR) $@ $(libocc_parser_a_OBJS)

bin/libocc_analyzer.a : $(libocc_analyzer_a_OBJS)
	$(AR) $@ $(libocc_analyzer_a_OBJS)

bin/program-string-test : $(program_string_test_OBJS) bin/libocc_parser.a
	$(LINK) $^ -o $@

bin/parser-test-glue : $(parser_test_glue_OBJS) bin/libocc_parser.a
	$(LINK) $^ -o $@ 

bin/elaboration-test-glue : $(elaboration_test_glue_OBJS) \
		bin/libocc_analyzer.a bin/libocc_parser.a 
	$(LINK) $^ -o $@

bin/lexer-test-glue : $(lexer_test_glue_OBJS) \
		bin/libocc_parser.a
	$(LINK) $^ -o $@

occ-core/parser/ptree-generated.cc occ-core/parser/ptree-generated.h \
	: occ-core/parser/ptree-gen.sh
	cd occ-core/parser && $(SHELL) ./ptree-gen.sh

gcov-clean:
	rm -f `find . -name '*.da'` 
	rm -f `find . -name '*.gcov'`

dist-clean: clean
	rm -f *.passed *.pyc
	rm -f occ-core/parser/ptree-generated.h
	rm -f occ-core/parser/ptree-generated.cc
	rm -f `find . -name '*.d'`
	rm -f `find . -name '*.pyc'`
	rm -f `find . -name '*~'`

clean: gcov-clean
	rm -f $(libocc_parser_a_OBJS)
	rm -f $(libocc_analyzer_a_OBJS)
	rm -f $(parser_test_glue_OBJS)
	rm -f $(elaboration_test_glue_OBJS)
	rm -f bin/* 
	rm -f parser-test/*.ps parser-test/*.dot
	
-include $(libocc_parser_a_DEPS)
-include $(libocc_analyzer_a_DEPS)
-include $(parser_test_glue_DEPS)
-include $(elaboration_test_glue_DEPS)



