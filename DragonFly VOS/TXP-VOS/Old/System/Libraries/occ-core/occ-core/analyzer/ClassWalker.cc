//@beginlicenses@
//@license{chiba-tokyo}{}@
//
//  Copyright (C) 1997-2001 Shigeru Chiba, Tokyo Institute of Technology.
//
//  Permission to use, copy, distribute and modify this software and
//  its documentation for any purpose is hereby granted without fee,
//  provided that the above copyright notice appears in all copies and that
//  both that copyright notice and this permission notice appear in
//  supporting documentation.
//
//  Shigeru Chiba makes no representations about the suitability of this
//  software for any purpose.  It is provided "as is" without express or
//  implied warranty.
//
//@endlicenses@

#include <occ-core/analyzer/ClassWalker.h>
#include <occ-core/analyzer/ClassBodyWalker.h>
#include <occ-core/analyzer/Class.h>
#include <occ-core/analyzer/TemplateClass.h>
#include <occ-core/parser/ptreeAll.h>
#include <occ-core/analyzer/MemberFunction.h>
#include <occ-core/analyzer/Environment.h>
#include <occ-core/analyzer/TypeInfo.h>
#include <occ-core/analyzer/memberAll.h>
#include <occ-core/parser/Encoding.h>
#include <occ-core/parser/PtreeClassSpec.h>
#include <occ-core/parser/PtreeTemplateInstantiation.h>
#include <occ-core/parser/token-names.h>
#include <occ-core/parser/PtreeArray.h>
#include <occ-core/parser/PtreeDeclaration.h>
#include <occ-core/parser/PtreeDeclarator.h>
#include <occ-core/parser/PtreeAccessSpec.h>
#include <occ-core/parser/PtreeTemplateDecl.h>
#include <occ-core/parser/PtreeBlock.h>
#include <occ-core/parser/CerrErrorLog.h>
#include <occ-core/parser/ErrorLog.h>
#include <occ-core/parser/TheErrorLog.h>
#include <occ-core/parser/MopMsg.h>

using namespace std;

namespace Opencxx
{

/**
 *  Obtains the metaobject of the class given the type of this class.
 *
 *  Returns @c 0 for non-object types.
 */
Class* ClassWalker::GetClassMetaobject(TypeInfo& tinfo)
{
    Class* c;
    if(tinfo.IsClass(c))
	return c;
    else if(tinfo.IsReferenceType()){
	tinfo.Dereference();
	if(tinfo.IsClass(c))
	    return c;
    }

    return 0;
}

bool ClassWalker::IsClassWalker()
{
    return true;
}

void ClassWalker::InsertBeforeStatement(Ptree* p)
{
    before_statement.Append(p);
}

void ClassWalker::AppendAfterStatement(Ptree* p)
{
    after_statement.Append(p);
}

void ClassWalker::InsertBeforeToplevel(Ptree* p)
{
    before_toplevel.Append(p);
}

void ClassWalker::AppendAfterToplevel(Ptree* p)
{
    after_toplevel.Append(p);
}

bool ClassWalker::InsertDeclaration(Ptree* d, Class* metaobject, Ptree* key,
				    void* data)
{
    inserted_declarations.Append(d);
    if(metaobject == 0 || key == 0)
	return true;
    else if(LookupClientData(metaobject, key))
	return false;
    else{
	ClientDataLink* entry = new ClientDataLink;
	entry->next = client_data;
	entry->metaobject = metaobject;
	entry->key = key;
	entry->data = data;
	client_data = entry;
	return true;
    }
}

void* ClassWalker::LookupClientData(Class* metaobject, Ptree* key)
{
    for(ClientDataLink* c = client_data; c != 0; c = c->next)
	if(c->metaobject == metaobject && PtreeUtil::Eq(key, c->key))
	    return c->data;

    return 0;
}

Ptree* ClassWalker::GetInsertedPtree()
{
    Ptree* result = 0;
    if(before_toplevel.Number() > 0)
	result = PtreeUtil::Nconc(result, before_toplevel.All());

    if(inserted_declarations.Number() > 0)
	result = PtreeUtil::Nconc(result, inserted_declarations.All());

    if(before_statement.Number() > 0)
	result = PtreeUtil::Nconc(result, before_statement.All());

    before_statement.Clear();
    inserted_declarations.Clear();
    client_data = 0;
    before_toplevel.Clear();
    return result;
}

Ptree* ClassWalker::GetAppendedPtree()
{
    Ptree* result = 0;
    if(after_statement.Number() > 0)
	result = PtreeUtil::Nconc(result, after_statement.All());

    if(after_toplevel.Number() > 0)
	result = PtreeUtil::Nconc(result, after_toplevel.All());

    after_statement.Clear();
    after_toplevel.Clear();
    return result;
}

Ptree* ClassWalker::TranslateMetaclassDecl(Ptree* decl)
{
    env->RecordMetaclassName(decl);
    return 0;
}

Ptree* ClassWalker::TranslateClassSpec(Ptree* spec, Ptree* userkey,
				       Ptree* class_def, Class* metaobject)
{
    using namespace PtreeUtil;
    
    if(metaobject != 0){
	// the class body is given.
	Ptree* bases = PtreeUtil::Third(class_def);
	PtreeArray* tspec_list = RecordMembers(class_def, bases, metaobject);
	//metaobject->TranslateClass(env);
	//metaobject->TranslateClassHasFinished();
	if(metaobject->removed)
	    return 0;

	ClassBodyWalker w(this, tspec_list);
	Ptree* body = Nth(class_def, 3);
	Ptree* body2 = w.TranslateClassBody(body, PtreeUtil::Third(class_def),
					    metaobject);
	Ptree* bases2 = metaobject->GetBaseClasses();
	Ptree* cspec = metaobject->GetClassSpecifier();
	Ptree* name2 = metaobject->GetNewName();
	if(bases != bases2 || body != body2 || cspec != 0 || name2 != 0){
	    if(name2 == 0)
		name2 = PtreeUtil::Second(class_def);

	    Ptree* rest = PtreeUtil::List(name2, bases2, body2);
	    if(cspec != 0)
		rest = PtreeUtil::Cons(cspec, rest);
	    return new PtreeClassSpec(class_def->Car(), rest, 0,
				      spec->GetEncodedName());
	}
    }

    if(userkey == 0)
	return spec;
    else
	return new PtreeClassSpec(class_def->Car(), class_def->Cdr(),
				  0, spec->GetEncodedName());
}

Ptree* ClassWalker::TranslateTemplateInstantiation(Ptree* inst_spec,
			Ptree* userkey, Ptree* class_spec, Class* metaobject)
{
    Ptree* class_spec2;
    class_spec2 = class_spec;

    if(userkey == 0)
	return inst_spec;
    else if (class_spec == class_spec2)
	return inst_spec;
    else
	return new PtreeTemplateInstantiation(class_spec);
}

Ptree* ClassWalker::ConstructClass(Class* metaobject)
{
    using namespace PtreeUtil;
    Ptree* def = metaobject->Definition();
    Ptree* def2;

    //metaobject->TranslateClassHasFinished();
    ClassBodyWalker w(this, 0);
    Ptree* body = Nth(def, 3);
    Ptree* body2 = w.TranslateClassBody(body, 0, metaobject);
    Ptree* bases2 = metaobject->GetBaseClasses();
    Ptree* cspec2 = metaobject->GetClassSpecifier();
    Ptree* name2 = metaobject->GetNewName();
    if(body == body2 && bases2 == 0 && cspec2 == 0 && name2 == 0)
	def2 = def;
    else{
	if(name2 == 0)
	    name2 = PtreeUtil::Second(def);

	Ptree* rest = PtreeUtil::List(name2, bases2, body2);
	if(cspec2 != 0)
	    rest = PtreeUtil::Cons(cspec2, rest);

	def2 = new PtreeClassSpec(def->Car(), rest,
				  0, def->GetEncodedName());
    }

    return new PtreeDeclaration(0, PtreeUtil::List(def2, Class::semicolon_t));
}

PtreeArray* ClassWalker::RecordMembers(Ptree* class_def, Ptree* bases,
				       Class* metaobject)
{
    Ptree *tspec, *tspec2;

    NewScope(metaobject);
    RecordBaseclassEnv(bases);

    PtreeArray* tspec_list = new PtreeArray();

    Ptree* rest = PtreeUtil::Second(PtreeUtil::Nth(class_def, 3));
    while(rest != 0){
	Ptree* mem = rest->Car();
	switch(mem->What()){
	case ntTypedef :
	    tspec = PtreeUtil::Second(mem);
	    tspec2 = TranslateTypespecifier(tspec);
	    env->RecordTypedefName(PtreeUtil::Third(mem));
	    if(tspec != tspec2){
		tspec_list->Append(tspec);
		tspec_list->Append(tspec2);
	    }

	    break;
	case ntMetaclassDecl :
	    env->RecordMetaclassName(mem);
	    break;
	case ntDeclaration :
	    RecordMemberDeclaration(mem, tspec_list);
	    break;
	case ntTemplateDecl :
	case ntTemplateInstantiation :
	case ntUsing :
	default :
	    break;
	}

	rest = rest->Cdr();
    }

    if(tspec_list->Number() == 0){
	delete tspec_list;
	tspec_list = 0;
    }
	
    ExitScope();
    return tspec_list;
}

//  RecordMemberDeclaration() is derived from TranslateDeclaration().

void ClassWalker::RecordMemberDeclaration(Ptree* mem,
					  PtreeArray* tspec_list)
{
    Ptree *tspec, *tspec2, *decls;

    tspec = PtreeUtil::Second(mem);
    tspec2 = TranslateTypespecifier(tspec);
    decls = PtreeUtil::Third(mem);
    if(decls->IsA(ntDeclarator))	// if it is a function
	env->RecordDeclarator(decls);
    else if(!decls->IsLeaf())		// not a null declaration.
	while(decls != 0){
	    Ptree* d = decls->Car();
	    if(d->IsA(ntDeclarator))
		env->RecordDeclarator(d);

	    decls = decls->Cdr();
	    if(decls != 0)
		decls = decls->Cdr();
	}

    if(tspec != tspec2){
	tspec_list->Append(tspec);
	tspec_list->Append(tspec2);
    }
}

Ptree* ClassWalker::ConstructAccessSpecifier(int access)
{
    Ptree* lf;
    switch(access){
    case Class::Protected :
        lf = Class::protected_t;
	break;
    case Class::Private :
	lf = Class::private_t;
	break;
    case Class::Public :
    default :
	lf = Class::public_t;
	break;
     }

     return new PtreeAccessSpec(lf, PtreeUtil::List(Class::colon_t));
}

Ptree* ClassWalker::ConstructMember(void* ptr)
{
    using namespace PtreeUtil;
    ChangedMemberList::Cmem* m = (ChangedMemberList::Cmem*)ptr;
    Ptree* def = m->def;
    Ptree* def2;

    if(Third(def)->IsA(ntDeclarator)){
	// function implementation
	if(m->body == 0){
	    NameScope old_env;
	    Environment* fenv = env->DontRecordDeclarator(m->declarator);
	    if(fenv != 0)
		old_env = ChangeScope(fenv);

	    NewScope();
	    def2 = MakeMemberDeclarator(true, m,
					(PtreeDeclarator*)m->declarator);
	    def2 = List(def2, TranslateFunctionBody(Nth(def,3)));
	    ExitScope();
	    if(fenv != 0)
		RestoreScope(old_env);
	}
	else{
	    def2 = MakeMemberDeclarator(false, m,
					(PtreeDeclarator*)m->declarator);
	    def2 = List(def2, m->body);
	}
    }
    else{
	// declaration
	def2 = MakeMemberDeclarator(false, m,
				    (PtreeDeclarator*)m->declarator);
	if(m->body == 0)
	    def2 = List(List(def2), Class::semicolon_t);
	else
	    def2 = List(def2, m->body);
    }

    def2 = 
        new PtreeDeclaration(
	    TranslateStorageSpecifiers(
	        First(def))
	  , Cons(
	        TranslateTypespecifier(
	            Second(def))
	      , def2));
    return def2;
}

Ptree* ClassWalker::TranslateStorageSpecifiers(Ptree* spec)
{
    return TranslateStorageSpecifiers2(spec);
}

Ptree* ClassWalker::TranslateStorageSpecifiers2(Ptree* rest)
{
    if(rest == 0)
	return 0;
    else{
	Ptree* h = rest->Car();
	Ptree* t = rest->Cdr();
	Ptree* t2 = TranslateStorageSpecifiers2(t);
	if(h->IsA(ntUserdefKeyword))
	    return t2;
	else if(t == t2)
	    return rest;
	else
	    return PtreeUtil::Cons(h, t2);
    }
}

Ptree* ClassWalker::TranslateTemplateFunction(Ptree* temp_def, Ptree* impl)
{
    using namespace PtreeUtil;
    Environment* fenv = env->RecordTemplateFunction(temp_def, impl);
    if (fenv != 0) {
	Class* metaobject = fenv->IsClassEnvironment();
	if(metaobject != 0){
	    NameScope old_env = ChangeScope(fenv);
	    NewScope();

	    ChangedMemberList::Cmem m;
	    Ptree* decl = Third(impl);
	    MemberFunction mem(metaobject, impl, decl);
	    //metaobject->TranslateMemberFunction(env, mem);
	    ChangedMemberList::Copy(&mem, &m, Class::Undefined);
	    Ptree* decl2
		= MakeMemberDeclarator(true, &m, (PtreeDeclarator*)decl);

	    ExitScope();
	    RestoreScope(old_env);
	    if(decl != decl2) {
		Ptree* pt = List(Second(impl), decl2, Nth(impl, 3));
		pt = new PtreeDeclaration(First(impl), pt);
		pt = List(Second(temp_def), Third(temp_def),
				 Nth(temp_def, 3), pt);
		return new PtreeTemplateDecl(First(temp_def), pt);
	    }
	}
    }

    return temp_def;
}

Ptree* ClassWalker::TranslateFunctionImplementation(Ptree* impl)
{
    using namespace PtreeUtil;
    Ptree* sspec = First(impl);
    Ptree* sspec2 = TranslateStorageSpecifiers(sspec);
    Ptree* tspec = Second(impl);
    Ptree* decl = Third(impl);
    Ptree* body = Nth(impl, 3);
    Ptree* decl2;
    Ptree *body2;

    Ptree* tspec2 = TranslateTypespecifier(tspec);
    Environment* fenv = env->RecordDeclarator(decl);

    if(fenv == 0){
	// reach here if resolving the qualified name fails. error?
	NewScope();
	decl2 = TranslateDeclarator(true, (PtreeDeclarator*)decl);
	body2 = TranslateFunctionBody(body);
	ExitScope();
    }
    else{
	Class* metaobject = fenv->IsClassEnvironment();
	NameScope old_env = ChangeScope(fenv);
	NewScope();

	if (metaobject == 0 && Class::metaclass_for_c_functions != 0)
	    metaobject = MakeMetaobjectForCfunctions();

	if(metaobject == 0){
	    decl2 = TranslateDeclarator(true, (PtreeDeclarator*)decl);
	    body2 = TranslateFunctionBody(body);
	}
	else{
	    ChangedMemberList::Cmem m;
	    MemberFunction mem(metaobject, impl, decl);
	    //metaobject->TranslateMemberFunction(env, mem);
	    ChangedMemberList::Copy(&mem, &m, Class::Undefined);
	    decl2 = MakeMemberDeclarator(true, &m, (PtreeDeclarator*)decl);
	    if(m.body != 0)
		body2 = m.body;
	    else
		body2 = TranslateFunctionBody(body);
	}

	ExitScope();
	RestoreScope(old_env);
    }

    if(sspec == sspec2 && tspec == tspec2 && decl == decl2 && body == body2)
	return impl;
    else
	return new PtreeDeclaration(sspec2,
				    PtreeUtil::List(tspec2, decl2, body2));
}

Class* ClassWalker::MakeMetaobjectForCfunctions() {
    if (Class::for_c_functions == 0) {
	TheErrorLog().Report(
	    MopMsg(Msg::Fatal, 
		string("the metaclass for C functions cannot be loaded: ")
		+Class::metaclass_for_c_functions));
    }

    return Class::for_c_functions;
}

Ptree* ClassWalker::MakeMemberDeclarator(bool record, void* ptr,
					 PtreeDeclarator* decl)
{
    using namespace PtreeUtil;
    
    Ptree *args, *args2, *name, *name2, *init, *init2;

    //  Since g++ cannot parse the nested-class declaration:
    //     class ChangedMemberList::Cmem;
    //  MakeMemberDeclarator() takes a void* pointer and convert the type
    //  into ChangedMemberList::Cmem*.
    ChangedMemberList::Cmem* m = (ChangedMemberList::Cmem*)ptr;

    if(m->removed)
	return 0;

    if(PtreeUtil::GetArgDeclList(decl, args))
	if(m->args == 0)
	    args2 = TranslateArgDeclList2(record, env, true,
					  m->arg_name_filled, 0, args);
	else{
	    args2 = m->args;
	    // we may need to record the arguments.
	    TranslateArgDeclList2(record, env, false, false, 0, args);
	}
    else
	args = args2 = 0;

    name = decl->Name();
    if(m->name != 0)
	name2 = m->name;
    else
	name2 = name;

    if(m->init == 0)
	init = init2 = 0;
    else{
	init2 = m->init;
	init = Last(decl)->Car();
	if(init->IsLeaf() || !Eq(init->Car(), ':'))
	    init = 0;
    }

    if(args == args2 && name == name2 && init == init2)
	return decl;
    else{
	Ptree* rest;
	if(init == 0 && init2 != 0){
	    rest = PtreeUtil::Subst(args2, args, name2, name, decl->Cdr());
	    rest = PtreeUtil::Append(rest, init2);
	}
	else
	    rest = PtreeUtil::Subst(args2, args, name2, name,
				init2, init, decl->Cdr());

	if(decl->Car() == name)
	    return new PtreeDeclarator(decl, name2, rest);
	else
	    return new PtreeDeclarator(decl, decl->Car(), rest);
    }
}

Ptree* ClassWalker::RecordArgsAndTranslateFbody(Class* c, Ptree* args,
						Ptree* body)
{
    NameScope old_env;
    Environment* fenv = c->GetEnvironment();

    if(fenv != 0)
	old_env = ChangeScope(fenv);

    NewScope();
    TranslateArgDeclList2(true, env, false, false, 0, args);
    Ptree* body2 = TranslateFunctionBody(body);
    ExitScope();

    if(fenv != 0)
	RestoreScope(old_env);

    return body2;
}

Ptree* ClassWalker::TranslateFunctionBody(Ptree* body)
{
    Ptree* body2;

    inserted_declarations.Clear();
    client_data = 0;
    body = Translate(body);
    if(body == 0 || body->IsLeaf() || inserted_declarations.Number() <= 0)
	body2 = body;
    else{
	Ptree* decls = inserted_declarations.All();
	body2 = new PtreeBlock(PtreeUtil::First(body),
			      PtreeUtil::Nconc(decls, PtreeUtil::Second(body)),
			      PtreeUtil::Third(body));
    }

    inserted_declarations.Clear();
    client_data = 0;
    return body2;
}

Ptree* ClassWalker::TranslateBlock(Ptree* block)
{
    Ptree* block2;

    NewScope();

    PtreeArray array;
    bool changed = false;
    Ptree* body = PtreeUtil::Second(block);
    Ptree* rest = body;
    while(rest != 0){
	unsigned i, n;
	Ptree* p = rest->Car();
	Ptree* q = Translate(p);

	n = before_statement.Number();
	if(n > 0){
	    changed = true;
	    for(i = 0; i < n; ++i)
		array.Append(before_statement[i]);
	}

	array.Append(q);
	if(p != q)
	    changed = true;

	n = after_statement.Number();
	if(n > 0){
	    changed = true;
	    for(i = 0; i < n; ++i)
		array.Append(after_statement[i]);
	}

	before_statement.Clear();
	after_statement.Clear();
	rest = rest->Cdr();
    }

    if(changed)
	block2 = new PtreeBlock(PtreeUtil::First(block), array.All(),
				PtreeUtil::Third(block));
    else
	block2 = block;

    ExitScope();
    return block2;
}

Ptree* ClassWalker::TranslateArgDeclList(bool record, Ptree*, Ptree* args)
{
    return TranslateArgDeclList2(record, env, true, false, 0, args);
}

Ptree* ClassWalker::TranslateInitializeArgs(PtreeDeclarator* decl, Ptree* init)
{
    TypeInfo tinfo;
    env->Lookup(decl, tinfo);
    Class* metaobject = tinfo.ClassMetaobject();
    return TranslateArguments(init);
}

Ptree* ClassWalker::TranslateAssignInitializer(PtreeDeclarator* decl,
					       Ptree* init)
{
    TypeInfo tinfo;
    env->Lookup(decl, tinfo);
    Class* metaobject = tinfo.ClassMetaobject();
    Ptree* exp = PtreeUtil::Second(init);
    Ptree* exp2 = Translate(exp);
    if(exp == exp2)
	return init;
    else
	return PtreeUtil::List(init->Car(), exp2);
}

Ptree* ClassWalker::TranslateUserAccessSpec(Ptree*)
{
    return 0;
}

Ptree* ClassWalker::TranslateAssign(Ptree* exp)
{
    using namespace PtreeUtil;
    
    Ptree *left, *left2, *right, *right2, *exp2;
    Environment* scope;
    Class* metaobject;
    TypeInfo type;

    left = First(exp);
    right = Third(exp);
    if (left->IsA(ntDotMemberExpr) || left->IsA(ntArrowMemberExpr)) {
	Ptree* object = First(left);
	Ptree* op = Second(left);
	Ptree* member = Third(left);
	Ptree* assign_op = Second(exp);
	Typeof(object, type);
	if(!Eq(op, '.'))
	    type.Dereference();

	metaobject = GetClassMetaobject(type);
    }
    else if((scope = env->IsMember(left)) != 0){
	metaobject = scope->IsClassEnvironment();
    }
    else{
	Typeof(left, type);
	metaobject = GetClassMetaobject(type);
    }

    left2 = Translate(left);
    right2 = Translate(right);
    if(left == left2 && right == right2)
	return exp;
    else
	return new PtreeAssignExpr(left2, List(Second(exp), right2));
}

Ptree* ClassWalker::CheckMemberEquiv(Ptree* exp, Ptree* exp2)
{
    if(!exp2->IsLeaf()
       && PtreeUtil::Equiv(exp->Car(), exp2->Car())
       && PtreeUtil::Equiv(exp->Cdr(), exp2->Cdr()))
	return exp;
    else
	return exp2;
}

Ptree* ClassWalker::TranslateInfix(Ptree* exp)
{
    using namespace PtreeUtil;
    
    TypeInfo type;

    Ptree* left = First(exp);
    Ptree* right = Third(exp);
    Typeof(right, type);
    Ptree* left2 = Translate(left);
    Ptree* right2 = Translate(right);
    if(left == left2 && right == right2) 
    {
        return exp;
    }
    else
    {
        return new PtreeInfixExpr(
            left2
          , PtreeUtil::List(PtreeUtil::Second(exp)
          , right2)
        );
    }
}

Ptree* ClassWalker::TranslateUnary(Ptree* exp)
{
    using namespace PtreeUtil;
    
    Ptree* unaryop = exp->Car();
    Ptree* right = PtreeUtil::Second(exp);
    Ptree* right2 = Translate(right);
    if(right == right2)
        return exp;
    else
        return new PtreeUnaryExpr(unaryop, PtreeUtil::List(right2));
}

Ptree* ClassWalker::TranslateArray(Ptree* exp)
{
    TypeInfo type;

    Ptree* array = exp->Car();
    Typeof(array, type);
    Class* metaobject = GetClassMetaobject(type);
    if(metaobject != 0){
    }
    else{
	Ptree* index = PtreeUtil::Third(exp);
	Ptree* array2 = Translate(array);
	Ptree* index2 = Translate(index);
	if(array == array2 && index == index2)
	    return exp;
	else
	    return new PtreeArrayExpr(array2,
			PtreeUtil::ShallowSubst(index2, index, exp->Cdr()));
    }
}

Ptree* ClassWalker::TranslatePostfix(Ptree* exp)
{
    TypeInfo type;
    Environment* scope;
    Class* metaobject;
    Ptree* exp2;

    Ptree* left = exp->Car();
    Ptree* postop = PtreeUtil::Second(exp);
    if (left->IsA(ntDotMemberExpr) || left->IsA(ntArrowMemberExpr)) {
	Ptree* object = PtreeUtil::First(left);
	Ptree* op = PtreeUtil::Second(left);
	Typeof(object, type);
	if(!PtreeUtil::Eq(op, '.'))
	    type.Dereference();

	metaobject = GetClassMetaobject(type);
	if(metaobject != 0){
	}
    }    
    else if((scope = env->IsMember(left)) != 0){
	metaobject = scope->IsClassEnvironment();
	if(metaobject != 0){
	}
    }

    Typeof(left, type);
    metaobject = GetClassMetaobject(type);
    Ptree* left2 = Translate(left);
    if(left == left2)
	return exp;
    else
	return new PtreePostfixExpr(left2, exp->Cdr());
}

Ptree* ClassWalker::TranslateFuncall(Ptree* exp)
{
    TypeInfo type;
    Environment* scope;
    Class* metaobject;
    Ptree *fun, *arglist, *exp2;

    fun = exp->Car();
    arglist = exp->Cdr();
    if (fun->IsA(ntDotMemberExpr) || fun->IsA(ntArrowMemberExpr)) {
	Ptree* object = PtreeUtil::First(fun);
	Ptree* op = PtreeUtil::Second(fun);
	Ptree* member = PtreeUtil::Third(fun);
	Typeof(object, type);
	if(!PtreeUtil::Eq(op, '.'))
	    type.Dereference();

	metaobject = GetClassMetaobject(type);
	if(metaobject != 0){
	}
    }
    else if((scope = env->IsMember(fun)) != 0){
	metaobject = scope->IsClassEnvironment();
    }
    else{
	Typeof(fun, type);
	metaobject = GetClassMetaobject(type);
    }

    Ptree* fun2 = Translate(fun);
    Ptree* arglist2 = TranslateArguments(arglist);
    if(fun == fun2 && arglist == arglist2)
	return exp;
    else
	return new PtreeFuncallExpr(fun2, arglist2);
}

Ptree* ClassWalker::TranslateDotMember(Ptree* exp)
{
    TypeInfo type;

    Ptree* left = exp->Car();
    Typeof(left, type);
    Class* metaobject = GetClassMetaobject(type);
    Ptree* left2 = Translate(left);
    if(left == left2)
	return exp;
    else
	return new PtreeDotMemberExpr(left2, exp->Cdr());
}

Ptree* ClassWalker::TranslateArrowMember(Ptree* exp)
{
    TypeInfo type;

    Ptree* left = exp->Car();
    Typeof(left, type);
    type.Dereference();
    Class* metaobject = GetClassMetaobject(type);
    Ptree* left2 = Translate(left);
    if(left == left2)
	return exp;
    else
	return new PtreeArrowMemberExpr(left2, exp->Cdr());
}

Ptree* ClassWalker::TranslateThis(Ptree* exp)
{
    TypeInfo type;
    Typeof(exp, type);
    type.Dereference();
    Class* metaobject = GetClassMetaobject(type);
    return exp;
}

Ptree* ClassWalker::TranslateVariable(Ptree* exp)
{
    Environment* scope = env->IsMember(exp);
    if(scope != 0){
	Class* metaobject = scope->IsClassEnvironment();
    }

    return exp;
}

Ptree* ClassWalker::TranslateUserStatement(Ptree* exp)
{
    TypeInfo type;
    Ptree* object = PtreeUtil::First(exp);
    Ptree* op = PtreeUtil::Second(exp);
    Ptree* keyword = PtreeUtil::Third(exp);
    Ptree* rest = PtreeUtil::ListTail(exp, 3);

    Typeof(object, type);
    if(!PtreeUtil::Eq(op, '.'))
	type.Dereference();

    Class* metaobject = GetClassMetaobject(type);
    ErrorMessage("no complete class specification for: ", object, exp);
    return 0;
}

Ptree* ClassWalker::TranslateStaticUserStatement(Ptree* exp)
{
    bool is_type_name;
    TypeInfo type;
    Ptree* qualifier = PtreeUtil::First(exp);
    Ptree* keyword = PtreeUtil::Third(exp);
    Ptree* rest = PtreeUtil::ListTail(exp, 3);

    if(env->Lookup(qualifier, is_type_name, type))
	if(is_type_name){
	    Class* metaobject = GetClassMetaobject(type);
	}

    ErrorMessage("no complete class specification for: ", qualifier, exp);
    return 0;
}

Ptree* ClassWalker::TranslateNew2(Ptree* exp, Ptree* userkey, Ptree* scope,
				  Ptree* op, Ptree* placement,
				  Ptree* type, Ptree* init)
{
    TypeInfo t;
    using PtreeUtil::Second;

    if (PtreeUtil::Eq(type->Car(), '('))
	t.Set(Second(Second(type))->GetEncodedType(), env);
    else
	t.Set(Second(type)->GetEncodedType(), env);

    Class* metaobject = GetClassMetaobject(t);
    Ptree* placement2 = TranslateArguments(placement);
    Ptree* type2 = TranslateNew3(type);
    Ptree* init2 = TranslateArguments(init);
    if(userkey == 0 && placement == placement2
       && type == type2 && init == init2)
	return exp;
    else{
	if(userkey == 0)
	    return new PtreeNewExpr(exp->Car(),
			    PtreeUtil::ShallowSubst(placement2, placement,
						type2, type,
						init2, init, exp->Cdr()));
	else{
	    ErrorMessage("no complete class specification for: ",
			 type, exp);
	    exp = exp->Cdr();
	    if(placement == placement2 && type == type2 && init == init2)
		return exp;
	    else
		return new PtreeNewExpr(exp->Car(),
			    PtreeUtil::ShallowSubst(placement2, placement,
						type2, type,
						init2, init, exp->Cdr()));
	}

    }
}

Ptree* ClassWalker::TranslateDelete(Ptree* exp)
{
    TypeInfo type;

    Ptree* obj = PtreeUtil::Last(exp)->Car();
    if(PtreeUtil::Length(exp) == 2){	// not ::delete or delete []
	Typeof(obj, type);
	type.Dereference();
	Class* metaobject = GetClassMetaobject(type);
    }

    Ptree* obj2 = Translate(obj);
    if(obj == obj2)
	return exp;
    else
	return new PtreeDeleteExpr(exp->Car(),
				   PtreeUtil::ShallowSubst(obj2, obj,
						       exp->Cdr()));
}

}
