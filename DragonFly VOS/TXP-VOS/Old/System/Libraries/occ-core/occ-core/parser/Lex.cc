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

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <occ-core/parser/Lex.h>
#include <occ-core/parser/token-names.h>
#include <occ-core/parser/Token.h>
#include <occ-core/parser/HashTable.h>
#include <occ-core/parser/Program.h>
#include <occ-core/parser/auxil.h>
#include <occ-core/parser/GC.h>
// #include <occ-core/driver.h>

using namespace std;

namespace Opencxx
{

static void InitializeOtherKeywords(bool);

// class Lex

HashTable* Lex::user_keywords = 0;
std::string Lex::comments;

Lex::Lex(Program* aFile, bool wchars, bool recognizeOccExtensions) 
    : file(aFile)
    , wcharSupport(wchars)
{
    file->Rewind();
    last_token = '\n';
    tokenp = 0;
    token_len = 0;

    InitializeOtherKeywords(recognizeOccExtensions);
    char* pos;
    int len;
    int kind;
    do
    {
        kind = ReadToken(pos, len);
        tokens_.push_back(Token(kind, pos, len));
    }
    while (kind);
    currentToken_ = tokens_.begin();
}

Lex::TokenIterator Lex::Save() const
{
    return currentToken_;
}

void Lex::Restore(Lex::TokenIterator pos)
{
    currentToken_ = pos;
}

unsigned Lex::LineNumber(char* pos, char*& ptr, int& len)
{
    return file->LineNumber(pos, ptr, len);
}

int Lex::GetToken(Token& t)
{
    t = *currentToken_;
    ++currentToken_;
    return t.GetKind();
}

int Lex::LookAhead(int offset)
{
    return (currentToken_ + offset)->GetKind();
}

int Lex::LookAhead(int offset, Token& t)
{
    t = *(currentToken_ + offset);
    return t.GetKind();
}

char* Lex::TokenPosition()
{
    return (char*)file->Read(Tokenp());
}

char Lex::Ref(unsigned i)
{
    return file->Ref(i);
}

void Lex::Rewind(char* p)
{
    file->Rewind(p - file->Read(0));
}

bool Lex::RecordKeyword(char* keyword, int token)
{
    int index;
    char* str;

    if(keyword == 0)
	return false;

    str = new(GC) char[strlen(keyword) + 1];
    strcpy(str, keyword);

    if(user_keywords == 0)
	user_keywords = new HashTable;

    if(user_keywords->AddEntry(str, (HashTable::Value)token, &index) >= 0)
	return true;
    else
	return bool(user_keywords->Peek(index) == (HashTable::Value)token);
}

bool Lex::Reify(char* p, int len, unsigned int& value)
{
    value = 0;
    if(len > 2 && *p == '0' && is_xletter(p[1])){
	for(int i = 2; i < len; ++i){
	    char c = p[i];
	    if(is_digit(c))
		value = value * 0x10 + (c - '0');
	    else if('A' <= c && c <= 'F')
		value = value * 0x10 + (c - 'A' + 10);
	    else if('a' <= c && c <= 'f')
		value = value * 0x10 + (c - 'a' + 10);
	    else if(is_int_suffix(c))
		break;
	    else
		return false;
	}

	return true;
    }
    else if(len > 0 && is_digit(*p)){
	for(int i = 0; i < len; ++i){
	    char c = p[i];
	    if(is_digit(c))
		value = value * 10 + c - '0';
	    else if(is_int_suffix(c))
		break;
	    else
		return false;
	}

	return true;
    }
    else
	return false;
}

// Reify() doesn't interpret an escape character.

bool Lex::Reify(char* p, int length, char*& str)
{
    if(*p != '"')
	return false;
    else{
	str = new(GC) char[length];
	char* sp = str;
	for(int i = 1; i < length; ++i)
	    if(p[i] != '"'){
		*sp++ = p[i];
		if(p[i] == '\\' && i + 1 < length)
		    *sp++ = p[++i];
	    }
	    else
		while(++i < length && p[i] != '"')
		    ;

	*sp = '\0';
	return true;
    }
}


/*
  Lexical Analyzer
*/

int Lex::ReadToken(char*& ptr, int& len)
{
    int t;

    for(;;){
	t = ReadLine();

        if(t == Ignore)
	    continue;

	last_token = t;

#if (defined __GNUC__) || (defined _GNUC_SYNTAX)
	if(t == ATTRIBUTE){
	    SkipAttributeToken();
	    continue;
	}
	else if(t == EXTENSION){
	    t = SkipExtensionToken(ptr, len);
	    if(t == Ignore)
		continue;
	    else
		return t;
	}
#endif
#if defined(_MSC_VER)
        if(t == ASM){
            SkipAsmToken();
	    continue;
	}
        else if(t == DECLSPEC){
	    SkipDeclspecToken();
	    continue;
	}
#endif
	if(t != '\n')
	    break;
    }

    ptr = TokenPosition();
    len = TokenLen();
    return t;
}

//   SkipAttributeToken() skips __attribute__(...), __asm__(...), ...

void Lex::SkipAttributeToken()
{
    char c;

    do{
	c = file->Get();
    }while(c != '(' && c != '\0');

    int i = 1;
    do{
	c = file->Get();
	if(c == '(')
	    ++i;
	else if(c == ')')
	    --i;
	else if(c == '\0')
	    break;
    } while(i > 0);
}

// SkipExtensionToken() skips __extension__(...).

int Lex::SkipExtensionToken(char*& ptr, int& len)
{
    ptr = TokenPosition();
    len = TokenLen();

    char c;

    do{
	c = file->Get();
    }while(is_blank(c) || c == '\n');

    if(c != '('){
	file->Unget();
	return Ignore;		// if no (..) follows, ignore __extension__
    }

    int i = 1;
    do{
	c = file->Get();
	if(c == '(')
	    ++i;
	else if(c == ')')
	    --i;
	else if(c == '\0')
	    break;
    } while(i > 0);

    return Identifier;	// regards it as the identifier __extension__
}

#if defined(_MSC_VER)

#define CHECK_END_OF_INSTRUCTION(C, EOI) \
	if (C == '\0') return; \
	if (strchr(EOI, C)) { \
	    this->file->Unget(); \
	    return; \
	}

/* SkipAsmToken() skips __asm ...
   You can have the following :

   Just count the '{' and '}' and it should be ok
   __asm { mov ax,1
           mov bx,1 }

   Stop when EOL found. Note that the first ';' after
   an __asm instruction is an ASM comment !
   int v; __asm mov ax,1 __asm mov bx,1; v=1;

   Stop when '}' found
   if (cond) {__asm mov ax,1 __asm mov bx,1}

   and certainly more...
*/
void Lex::SkipAsmToken()
{
    char c;

    do{
	c = file->Get();
	CHECK_END_OF_INSTRUCTION(c, "");
    }while(is_blank(c) || c == '\n');

    if(c == '{'){
        int i = 1;
        do{
	    c = file->Get();
	    CHECK_END_OF_INSTRUCTION(c, "");
	    if(c == '{')
	        ++i;
	    else if(c == '}')
	        --i;
        } while(i > 0);
    }
    else{
        for(;;){
	    CHECK_END_OF_INSTRUCTION(c, "}\n");
	    c = file->Get();
        }
    }
}

//   SkipDeclspecToken() skips __declspec(...).

void Lex::SkipDeclspecToken()
{
    char c;

    do{
	c = file->Get();
	CHECK_END_OF_INSTRUCTION(c, "");
    }while(is_blank(c));

    if (c == '(') {
        int i = 1;
        do{
	    c = file->Get();
	    CHECK_END_OF_INSTRUCTION(c, "};");
	    if(c == '(')
	        ++i;
	    else if(c == ')')
	        --i;
        }while(i > 0);
    }
}

#undef CHECK_END_OF_INSTRUCTION

#endif /* _MSC_VER */

char Lex::GetNextNonWhiteChar()
{
    char c;

    for(;;){
        do{
	    c = file->Get();
        }while(is_blank(c));

        if(c != '\\')
	    break;

        c = file->Get();
        if(c != '\n' && c!= '\r') {
	    file->Unget();
	    break;
	}
    }

    return c;
}

int Lex::ReadLine()
{
    char c;
    unsigned top;

    c = GetNextNonWhiteChar();

    tokenp = top = file->GetCurPos();
    if(c == '\0'){
	file->Unget();
	return '\0';
    }
    else if(c == '\n')
	return '\n';
    else if(c == '#' && last_token == '\n'){
	if(ReadLineDirective())
	    return '\n';
	else{
	    file->Rewind(top + 1);
	    token_len = 1;
	    return SingleCharOp(c);
	}
    }
    else if(c == '\'' || c == '"'){
	if(c == '\''){
	    if(ReadCharConst(top))
		return CharConst;
	}
	else{
	    if(ReadStrConst(top))
		return StringL;
	}

	file->Rewind(top + 1);
	token_len = 1;
	return SingleCharOp(c);
    }
    else if(is_digit(c))
	return ReadNumber(c, top);
    else if(c == '.'){
	c = file->Get();
	if(is_digit(c))
	    return ReadFloat(top);
	else{
	    file->Unget();
	    return ReadSeparator('.', top);
	}
    }

#if 1  // for wchar constants !!! 

    else if(is_letter(c)) {

      if (c == 'L') {
	c = file->Get();
	if (c == '\'' || c == '"') {
	  if (c == '\'') {
	    if (ReadCharConst(top+1)) {
	      // cout << "WideCharConst" << endl;
	      return WideCharConst;
	    }
	  } else {
	    if(ReadStrConst(top+1)) {
	      // cout << "WideStringL" << endl;	      
	      return WideStringL;
	    }
	  }
	}
	file->Rewind(top);
      }
      
      return ReadIdentifier(top);

    } else

      return ReadSeparator(c, top);
    
#else
    
    else if(is_letter(c)) 
	return ReadIdentifier(top);
    else
	return ReadSeparator(c, top);

#endif
}

bool Lex::ReadCharConst(unsigned top)
{
    char c;

    for(;;){
	c = file->Get();
	if(c == '\\'){
	    c = file->Get();
	    if(c == '\0')
		return false;
	}
	else if(c == '\''){
	    token_len = int(file->GetCurPos() - top + 1);
	    return true;
	}
	else if(c == '\n' || c == '\0')
	    return false;
    }
}

/*
  If text is a sequence of string constants like:
	"string1" "string2"
  then the string constants are delt with as a single constant.
*/
bool Lex::ReadStrConst(unsigned top)
{
    char c;

    for(;;){
	c = file->Get();
	if(c == '\\'){
	    c = file->Get();
	    if(c == '\0')
		return false;
	}
	else if(c == '"'){
	    unsigned pos = file->GetCurPos() + 1;
	    int nline = 0;
	    do{
		c = file->Get();
		if(c == '\n')
		    ++nline;
	    } while(is_blank(c) || c == '\n');

	    if(c == '"')
		/* line_number += nline; */ ;
	    else{
		token_len = int(pos - top);
		file->Rewind(pos);
		return true;
	    }
	}
	else if(c == '\n' || c == '\0')
	    return false;
    }
}

int Lex::ReadNumber(char c, unsigned top)
{
    char c2 = file->Get();

    if(c == '0' && is_xletter(c2)){
	do{
	    c = file->Get();
	} while(is_hexdigit(c));
	while(is_int_suffix(c))
	    c = file->Get();

	file->Unget();
	token_len = int(file->GetCurPos() - top + 1);
	return Constant;
    }

    while(is_digit(c2))
	c2 = file->Get();

    if(is_int_suffix(c2))
	do{
	    c2 = file->Get();
	}while(is_int_suffix(c2));
    else if(c2 == '.')
	return ReadFloat(top);
    else if(is_eletter(c2)){
	file->Unget();
	return ReadFloat(top);
    }

    file->Unget();
    token_len = int(file->GetCurPos() - top + 1);
    return Constant;
}

int Lex::ReadFloat(unsigned top)
{
    char c;

    do{
	c = file->Get();
    }while(is_digit(c));
    if(is_float_suffix(c))
	do{
	    c = file->Get();
	}while(is_float_suffix(c));
    else if(is_eletter(c)){
	unsigned p = file->GetCurPos();
	c = file->Get();
	if(c == '+' || c == '-'){
	     c = file->Get();
	     if(!is_digit(c)){
		file->Rewind(p);
		token_len = int(p - top);
		return Constant;
	    }
	}
	else if(!is_digit(c)){
	    file->Rewind(p);
	    token_len = int(p - top);
	    return Constant;
	}

	do{
	    c = file->Get();
	}while(is_digit(c));

	while(is_float_suffix(c))
	    c = file->Get();
    }

    file->Unget();
    token_len = int(file->GetCurPos() - top + 1);
    return Constant;
}

// ReadLineDirective() simply ignores a line beginning with '#'

bool Lex::ReadLineDirective()
{
    char c;

    do{
	c = file->Get();
    }while(c != '\n' && c != '\0');
    return true;
}

int Lex::ReadIdentifier(unsigned top)
{
    char c;

    do{
	c = file->Get();
    }while(is_letter(c) || is_digit(c));

    if (c) {  // WARNING: Get() does not advance position if EOF is reached
        file->Unget();
    }

    unsigned len = 1 + file->GetCurPos() - top;
    token_len = int(len);

    return Screening((char*)file->Read(top), int(len));
}

/*
  This table is a list of reserved key words.
  Note: alphabetical order!
*/
static struct rw_table {
    char*	name;
    long	value;
} table[] = {
#if (defined __GNUC__) || (defined _GNUC_SYNTAX)
    { "__alignof__", SIZEOF },
    { "__asm__", ATTRIBUTE },
    { "__attribute__", ATTRIBUTE },
    { "__const",	CONST },
    { "__extension__", EXTENSION },
    { "__inline", INLINE },
    { "__inline__", INLINE },
    { "__noreturn__", Ignore }, 
    { "__restrict", Ignore },
    { "__restrict__", Ignore },
    { "__signed", SIGNED },
    { "__signed__", SIGNED },
    { "__typeof",	TYPEOF },
    { "__typeof__",	TYPEOF },
    { "__unused__", Ignore },
    { "__vector", Ignore },
#endif
    { "asm",		ATTRIBUTE },
    { "auto",		AUTO },
#if !defined(_MSC_VER) || (_MSC_VER >= 1100)
    { "bool",		BOOLEAN },
#endif
    { "break",		BREAK },
    { "case",		CASE },
    { "catch",		CATCH },
    { "char",		CHAR },
    { "class",		CLASS },
    { "const",		CONST },
    { "continue",	CONTINUE },
    { "default",	DEFAULT },
    { "delete",		DELETE },
    { "do",			DO },
    { "double",		DOUBLE },
    { "else",		ELSE },
    { "enum",		ENUM },
    { "extern",		EXTERN },
    { "float",		FLOAT },
    { "for",		FOR },
    { "friend",		FRIEND },
    { "goto",		GOTO },
    { "if",		IF },
    { "inline",		INLINE },
    { "int",		INT },
    { "long",		LONG },
    { "metaclass",	METACLASS },	// OpenC++
    { "mutable",	MUTABLE },
    { "namespace",	NAMESPACE },
    { "new",		NEW },
#if (defined __GNUC__) || (defined _GNUC_SYNTAX)
    { "noreturn", Ignore },
#endif 
    { "operator",	OPERATOR },
    { "private",	PRIVATE },
    { "protected",	PROTECTED },
    { "public",		PUBLIC },
    { "register",	REGISTER },
    { "return",		RETURN },
    { "short",		SHORT },
    { "signed",		SIGNED },
    { "sizeof",		SIZEOF },
    { "static",		STATIC },
    { "struct",		STRUCT },
    { "switch",		SWITCH },
    { "template",	TEMPLATE },
    { "this",		THIS },
    { "throw",		THROW },
    { "try",		TRY },
    { "typedef",	TYPEDEF },
    { "typeid",         TYPEID },
    { "typename",	CLASS },	// it's not identical to class, but...
    { "union",		UNION },
    { "unsigned",	UNSIGNED },
    { "using",		USING },
    { "virtual",	VIRTUAL },
    { "void",		VOID },
    { "volatile",		VOLATILE },
    { "while",		WHILE },
    /* NULL slot */
};

#ifndef NDEBUG

class rw_table_sanity_check
{
public:
    rw_table_sanity_check(const rw_table table[])
    {
    	 unsigned n = (sizeof table)/(sizeof table[0]);
        
        if (n < 2) return;

        for (const char* old = (table++)->name; --n; old = (table++)->name)
            if (strcmp(old, table->name) >= 0) {
                cerr << "FAILED: '" << old << "' < '"
                     << table->name << "'" << endl;
                assert(! "invalid order in presorted array");
        }
    }
};

rw_table_sanity_check rw_table_sanity_check_instance(table);

#endif

static void InitializeOtherKeywords(bool recognizeOccExtensions)
{
    static bool done = false;

    if(done)
	return;
    else
	done = true;

    if (! recognizeOccExtensions)
	for(unsigned int i = 0; i < sizeof(table) / sizeof(table[0]); ++i)
	    if(table[i].value == METACLASS){
		table[i].value = Identifier;
		break;
	    }

#if defined(_MSC_VER)

// by JCAB
#define verify(c) do { const bool cond = (c); assert(cond); } while (0)

    verify(Lex::RecordKeyword("cdecl", Ignore));
    verify(Lex::RecordKeyword("_cdecl", Ignore));
    verify(Lex::RecordKeyword("__cdecl", Ignore));

    verify(Lex::RecordKeyword("_fastcall", Ignore));
    verify(Lex::RecordKeyword("__fastcall", Ignore));
    
    verify(Lex::RecordKeyword("_based", Ignore));
    verify(Lex::RecordKeyword("__based", Ignore));

    verify(Lex::RecordKeyword("_asm", ASM));
    verify(Lex::RecordKeyword("__asm", ASM));

    verify(Lex::RecordKeyword("_inline", INLINE));
    verify(Lex::RecordKeyword("__inline", INLINE));
    verify(Lex::RecordKeyword("__forceinline", INLINE));

    verify(Lex::RecordKeyword("_stdcall", Ignore));
    verify(Lex::RecordKeyword("__stdcall", Ignore));

    verify(Lex::RecordKeyword("__declspec", DECLSPEC));

    verify(Lex::RecordKeyword("__int8",  CHAR));
    verify(Lex::RecordKeyword("__int16", SHORT));
    verify(Lex::RecordKeyword("__int32", INT));
    verify(Lex::RecordKeyword("__int64",  INT64));
#endif
}

int Lex::Screening(char *identifier, int len)
{
    struct rw_table	*low, *high, *mid;
    int			c, token;

    if (wcharSupport && !strncmp("wchar_t", identifier, len))
    	return WCHAR;

    low = table;
    high = &table[sizeof(table) / sizeof(table[0]) - 1];
    while(low <= high){
	mid = low + (high - low) / 2;
	if((c = strncmp(mid->name, identifier, len)) == 0)
	    if(mid->name[len] == '\0')
		return mid->value;
	    else
		high = mid - 1;
	else if(c < 0)
	    low = mid + 1;
	else
	    high = mid - 1;
    }

    if(user_keywords == 0)
	user_keywords = new HashTable;

    if(user_keywords->Lookup(identifier, len, (HashTable::Value*)&token))
	return token;

    return Identifier;
}

int Lex::ReadSeparator(char c, unsigned top)
{
    char c1 = file->Get();

    token_len = 2;
    if(c1 == '='){
	switch(c){
	case '*' :
	case '/' :
	case '%' :
	case '+' :
	case '-' :
	case '&' :
	case '^' :
	case '|' :
	    return AssignOp;
	case '=' :
	case '!' :
	    return EqualOp;
	case '<' :
	case '>' :
	    return RelOp;
	default :
	    file->Unget();
	    token_len = 1;
	    return SingleCharOp(c);
	}
    }
    else if(c == c1){
	switch(c){
	case '<' :
	case '>' :
	    if(file->Get() != '='){
		file->Unget();
		return ShiftOp;
	    }
	    else{
		token_len = 3;
		return AssignOp;
	    }
	case '|' :
	    return LogOrOp;
	case '&' :
	    return LogAndOp;
	case '+' :
	case '-' :
	    return IncOp;
	case ':' :
	    return Scope;
	case '.' :
	    if(file->Get() == '.'){
		token_len = 3;
		return Ellipsis;
	    }
	    else
		file->Unget();
	case '/' :
	    return ReadComment(c1, top);
	default :
	    file->Unget();
	    token_len = 1;
	    return SingleCharOp(c);
	}
    }
    else if(c == '.' && c1 == '*')
	return PmOp;
    else if(c == '-' && c1 == '>')
	if(file->Get() == '*'){
	    token_len = 3;
	    return PmOp;
	}
	else{
	    file->Unget();
	    return ArrowOp;
	}
    else if(c == '/' && c1 == '*')
	return ReadComment(c1, top);
    else{
	file->Unget();
	token_len = 1;
	return SingleCharOp(c);
    }

    cerr << "*** An invalid character has been found! ("
	 << (int)c << ',' << (int)c1 << ")\n";
    return BadToken;
}

int Lex::SingleCharOp(unsigned char c)
{
			/* !"#$%&'()*+,-./0123456789:;<=>? */
    static char valid[] = "x   xx xxxxxxxx          xxxxxx";

    if('!' <= c && c <= '?' && valid[c - '!'] == 'x')
	return c;
    else if(c == '[' || c == ']' || c == '^')
	return c;
    else if('{' <= c && c <= '~')
	return c;
    else
	return BadToken;
}

int Lex::ReadComment(char c, unsigned top) {
    unsigned len = 0;
    if (c == '*') {	// a nested C-style comment is prohibited.
	do {
	    c = file->Get();
	    if (c == '*') {
		c = file->Get();
		if (c == '/') {
		    len = 1;
		    break;
		}
		else {
		    file->Unget();
		}
	    }
        } 
        while(c != '\0');
    }
    else {
        assert(c == '/');
	do {
	    c = file->Get();
	}
	while(c != '\n' && c != '\0');
    }
    len += file->GetCurPos() - top;
    token_len = int(len);
    char* pos = (char*)(file->Read(top));
    comments += string(pos, pos + int(len));
    return Ignore;
}

const string& Lex::GetComments2() {
    return comments;
}

string Lex::GetComments() {
    string s(GetComments2());
    comments = "";
    return s;
}
}

#ifdef TEST
#include <stdio.h>
#include <occ-core/parser/ProgramFromStdin.h>

using namespace Opencxx;

int main()
{
    int   i = 0;
    Token token;

    Lex lex(new ProgramFromStdin);
    for(;;){
//	int t = lex.GetToken(token);
	int t = lex.LookAhead(i++, token);
	if(t == 0)
	    break;
	else if(t < 128)
	    printf("%c (%x): ", t, t);
	else
	    printf("%-10.10s (%x): ", (char*)t, t);

	putchar('"');
	while(token.len-- > 0)
	    putchar(*token.ptr++);

	puts("\"");
    };
}
#endif

/*

line directive:
^"#"{blank}*{digit}+({blank}+.*)?\n

pragma directive:
^"#"{blank}*"pragma".*\n

Constant	{digit}+{int_suffix}*
		"0"{xletter}{hexdigit}+{int_suffix}*
		{digit}*\.{digit}+{float_suffix}*
		{digit}+\.{float_suffix}*
		{digit}*\.{digit}+"e"("+"|"-")*{digit}+{float_suffix}*
		{digit}+\."e"("+"|"-")*{digit}+{float_suffix}*
		{digit}+"e"("+"|"-")*{digit}+{float_suffix}*

CharConst	\'([^'\n]|\\[^\n])\'
WideCharConst	L\'([^'\n]|\\[^\n])\'    !!! new

StringL		\"([^"\n]|\\["\n])*\"
WideStringL	L\"([^"\n]|\\["\n])*\"   !!! new

Identifier	{letter}+({letter}|{digit})*

AssignOp	*= /= %= += -= &= ^= <<= >>=

EqualOp		== !=

RelOp		<= >=

ShiftOp		<< >>

LogOrOp		||

LogAndOp	&&

IncOp		++ --

Scope		::

Ellipsis	...

PmOp		.* ->*

ArrowOp		->

others		!%^&*()-+={}|~[];:<>?,./

BadToken	others

*/

