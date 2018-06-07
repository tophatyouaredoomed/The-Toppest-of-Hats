#ifndef guard_occ_core_parser_Lex_h
#define guard_occ_core_parser_Lex_h

//@beginlicenses@
//@license{chiba_tokyo}{}@
//@license{xerox}{}@
//@license{contributors}{}@
//
//  Permission to use, copy, distribute and modify this software and its  
//  documentation for any purpose is hereby granted without fee, provided that
//  the above copyright notice appears in all copies and that both that copyright
//  notice and this permission notice appear in supporting documentation.
// 
//  1997-2001 Shigeru Chiba, Tokyo Institute of Technology. make(s) no representations about the suitability of this
//  software for any purpose. It is provided "as is" without express or implied
//  warranty.
//  
//  Copyright (C)  1997-2001 Shigeru Chiba, Tokyo Institute of Technology.
//
//  -----------------------------------------------------------------
//
//
//  Copyright (c) 1995, 1996 Xerox Corporation.
//  All Rights Reserved.
//
//  Use and copying of this software and preparation of derivative works
//  based upon this software are permitted. Any copy of this software or
//  of any derivative work must include the above copyright notice of   
//  Xerox Corporation, this paragraph and the one after it.  Any
//  distribution of this software or derivative works must comply with all
//  applicable United States export control laws.
//
//  This software is made available AS IS, and XEROX CORPORATION DISCLAIMS
//  ALL WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE  
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR    
//  PURPOSE, AND NOTWITHSTANDING ANY OTHER PROVISION CONTAINED HEREIN, ANY
//  LIABILITY FOR DAMAGES RESULTING FROM THE SOFTWARE OR ITS USE IS
//  EXPRESSLY DISCLAIMED, WHETHER ARISING IN CONTRACT, TORT (INCLUDING
//  NEGLIGENCE) OR STRICT LIABILITY, EVEN IF XEROX CORPORATION IS ADVISED
//  OF THE POSSIBILITY OF SUCH DAMAGES.
//
//  -----------------------------------------------------------------
//
//  Permission to use, copy, distribute and modify this software and its  
//  documentation for any purpose is hereby granted without fee, provided that
//  the above copyright notice appears in all copies and that both that copyright
//  notice and this permission notice appear in supporting documentation.
// 
//  Other Contributors (see file AUTHORS) make(s) no representations about the suitability of this
//  software for any purpose. It is provided "as is" without express or implied
//  warranty.
//  
//  Copyright (C)  Other Contributors (see file AUTHORS)
//
//@endlicenses@

#include <occ-core/parser/GC.h>
#include <string>
#include <vector>
#include <occ-core/parser/Token.h>

namespace Opencxx
{

class Program;
class HashTable;

class Lex : public Object 
{
private:
    typedef std::vector<Token> TokenContainer;

public:
    typedef TokenContainer::iterator TokenIterator;
    
    Lex(Program*, bool wchars = false, bool recognizeOpencxxExtensions = true);
    int GetToken(Token&);
    int LookAhead(int);
    int LookAhead(int, Token&);

    TokenIterator Save() const;
    void Restore(TokenIterator);

    std::string GetComments();
    const std::string& GetComments2();

    unsigned LineNumber(char*, char*&, int&);

    static bool RecordKeyword(char*, int);
    static bool Reify(char* ptr, int len, unsigned int&);
    static bool Reify(char* ptr, int len, char*&);

private:
    unsigned Tokenp() { return tokenp; }
    int TokenLen() { return token_len; }
    char* TokenPosition();
    char Ref(unsigned i);
    void Rewind(char*);

    int ReadToken(char*&, int&);
    void SkipAttributeToken();
    int SkipExtensionToken(char*&, int&);

#if defined(_MSC_VER) || defined(_PARSE_VCC)
    void SkipAsmToken();
    void SkipDeclspecToken();
#endif

    char GetNextNonWhiteChar();
    int ReadLine();
    bool ReadCharConst(unsigned top);
    bool ReadStrConst(unsigned top);
    int ReadNumber(char c, unsigned top);
    int ReadFloat(unsigned top);
    bool ReadLineDirective();
    int ReadIdentifier(unsigned top);
    int Screening(char *identifier, int len);
    int ReadSeparator(char c, unsigned top);
    int SingleCharOp(unsigned char c);
    int ReadComment(char c, unsigned top);
        
private:
    Program* file;
    unsigned tokenp;
    int token_len;
    int last_token;

    static HashTable* user_keywords;
    static std::string comments;
    
    bool wcharSupport;
    TokenContainer tokens_;
    TokenIterator  currentToken_;
};

}

#endif /* ! guard_occ_core_parser_Lex_h */
