
// Compiler implementation of the D programming language
// Copyright (c) 1999-2011 by Digital Mars
// All Rights Reserved
// written by Walter Bright
// http://www.digitalmars.com
// License for redistribution is by either the Artistic License
// in artistic.txt, or the GNU General Public License in gnu.txt.
// See the included readme.txt for details.

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "rmem.h"

#include "statement.h"
#include "expression.h"
#include "cond.h"
#include "init.h"
#include "staticassert.h"
#include "mtype.h"
#include "scope.h"
#include "declaration.h"
#include "aggregate.h"
#include "id.h"
#include "hdrgen.h"
#include "parse.h"
#include "template.h"
#include "attrib.h"

extern int os_critsecsize32();
extern int os_critsecsize64();

/******************************** Statement ***************************/

Statement::Statement(Loc loc)
    : loc(loc)
{
    // If this is an in{} contract scope statement (skip for determining
    //  inlineStatus of a function body for header content)
    incontract = 0;
}

Statement *Statement::syntaxCopy()
{
    assert(0);
    return NULL;
}

void Statement::print()
{
    fprintf(stdmsg, "%s\n", toChars());
    fflush(stdmsg);
}

char *Statement::toChars()
{   OutBuffer *buf;
    HdrGenState hgs;

    buf = new OutBuffer();
    toCBuffer(buf, &hgs);
    return buf->toChars();
}

void Statement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->printf("Statement::toCBuffer()");
    buf->writenl();
}

Statement *Statement::semantic(Scope *sc)
{
    return this;
}

Statement *Statement::semanticNoScope(Scope *sc)
{
    //printf("Statement::semanticNoScope() %s\n", toChars());
    Statement *s = this;
    if (!s->isCompoundStatement() && !s->isScopeStatement())
    {
        s = new CompoundStatement(loc, this);           // so scopeCode() gets called
    }
    s = s->semantic(sc);
    return s;
}

// Same as semanticNoScope(), but do create a new scope

Statement *Statement::semanticScope(Scope *sc, Statement *sbreak, Statement *scontinue)
{
    Scope *scd = sc->push();
    if (sbreak)
        scd->sbreak = sbreak;
    if (scontinue)
        scd->scontinue = scontinue;
    Statement *s = semanticNoScope(scd);
    scd->pop();
    return s;
}

void Statement::error(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    ::verror(loc, format, ap);
    va_end( ap );
}

void Statement::warning(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    ::vwarning(loc, format, ap);
    va_end( ap );
}

int Statement::hasBreak()
{
    //printf("Statement::hasBreak()\n");
    return FALSE;
}

int Statement::hasContinue()
{
    return FALSE;
}

// TRUE if statement uses exception handling

int Statement::usesEH()
{
    return FALSE;
}

/* Only valid after semantic analysis
 * If 'mustNotThrow' is true, generate an error if it throws
 */
int Statement::blockExit(bool mustNotThrow)
{
    printf("Statement::blockExit(%p)\n", this);
    printf("%s\n", toChars());
    assert(0);
    return BEany;
}

// TRUE if statement 'comes from' somewhere else, like a goto

int Statement::comeFrom()
{
    //printf("Statement::comeFrom()\n");
    return FALSE;
}

// Return TRUE if statement has no code in it
int Statement::isEmpty()
{
    //printf("Statement::isEmpty()\n");
    return FALSE;
}

Statement *Statement::last()
{
    return this;
}

/****************************************
 * If this statement has code that needs to run in a finally clause
 * at the end of the current scope, return that code in the form of
 * a Statement.
 * Output:
 *      *sentry         code executed upon entry to the scope
 *      *sexception     code executed upon exit from the scope via exception
 *      *sfinally       code executed in finally block
 */

Statement *Statement::scopeCode(Scope *sc, Statement **sentry, Statement **sexception, Statement **sfinally)
{
    //printf("Statement::scopeCode()\n");
    //print();
    *sentry = NULL;
    *sexception = NULL;
    *sfinally = NULL;
    return this;
}

/*********************************
 * Flatten out the scope by presenting the statement
 * as an array of statements.
 * Returns NULL if no flattening necessary.
 */

Statements *Statement::flatten(Scope *sc)
{
    return NULL;
}


/******************************** PeelStatement ***************************/

PeelStatement::PeelStatement(Statement *s)
    : Statement(s->loc)
{
    this->s = s;
}

Statement *PeelStatement::semantic(Scope *sc)
{
    /* "peel" off this wrapper, and don't run semantic()
     * on the result.
     */
    return s;
}

/******************************** ExpStatement ***************************/

ExpStatement::ExpStatement(Loc loc, Expression *exp)
    : Statement(loc)
{
    this->exp = exp;
}

ExpStatement::ExpStatement(Loc loc, Dsymbol *declaration)
    : Statement(loc)
{
    this->exp = new DeclarationExp(loc, declaration);
}

Statement *ExpStatement::syntaxCopy()
{
    Expression *e = exp ? exp->syntaxCopy() : NULL;
    ExpStatement *es = new ExpStatement(loc, e);
    return es;
}

void ExpStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    if (exp)
    {   exp->toCBuffer(buf, hgs);
        if (exp->op != TOKdeclaration)
        {   buf->writeByte(';');
            if (!hgs->FLinit.init)
                buf->writenl();
        }
    }
    else
    {
        buf->writeByte(';');
        if (!hgs->FLinit.init)
            buf->writenl();
    }
}

Statement *ExpStatement::semantic(Scope *sc)
{
    if (exp)
    {
        //printf("ExpStatement::semantic() %s\n", exp->toChars());

#if 0   // Doesn't work because of difficulty dealing with things like a.b.c!(args).Foo!(args)
        // See if this should be rewritten as a TemplateMixin
        if (exp->op == TOKdeclaration)
        {   DeclarationExp *de = (DeclarationExp *)exp;
            Dsymbol *s = de->declaration;

            printf("s: %s %s\n", s->kind(), s->toChars());
            VarDeclaration *v = s->isVarDeclaration();
            if (v)
            {
                printf("%s, %d\n", v->type->toChars(), v->type->ty);
            }
        }
#endif

        exp = exp->semantic(sc);
        exp = exp->addDtorHook(sc);
        exp = resolveProperties(sc, exp);
        exp->discardValue();
        exp = exp->optimize(0);
    }
    return this;
}

int ExpStatement::blockExit(bool mustNotThrow)
{   int result = BEfallthru;

    if (exp)
    {
        if (exp->op == TOKhalt)
            return BEhalt;
        if (exp->op == TOKassert)
        {   AssertExp *a = (AssertExp *)exp;

            if (a->e1->isBool(FALSE))   // if it's an assert(0)
                return BEhalt;
        }
        if (exp->canThrow(mustNotThrow))
            result |= BEthrow;
    }
    return result;
}

int ExpStatement::isEmpty()
{
    return exp == NULL;
}

Statement *ExpStatement::scopeCode(Scope *sc, Statement **sentry, Statement **sexception, Statement **sfinally)
{
    //printf("ExpStatement::scopeCode()\n");
    //print();

    *sentry = NULL;
    *sexception = NULL;
    *sfinally = NULL;

    if (exp)
    {
        if (exp->op == TOKdeclaration)
        {
            DeclarationExp *de = (DeclarationExp *)(exp);
            VarDeclaration *v = de->declaration->isVarDeclaration();
            if (v && !v->noscope && !v->isDataseg())
            {
                Expression *e = v->edtor;
                if (e)
                {
                    //printf("dtor is: "); e->print();
#if 0
                    if (v->type->toBasetype()->ty == Tstruct)
                    {   /* Need a 'gate' to turn on/off destruction,
                         * in case v gets moved elsewhere.
                         */
                        Identifier *id = Lexer::uniqueId("__runDtor");
                        ExpInitializer *ie = new ExpInitializer(loc, new IntegerExp(1));
                        VarDeclaration *rd = new VarDeclaration(loc, Type::tint32, id, ie);
                        *sentry = new ExpStatement(loc, rd);
                        v->rundtor = rd;

                        /* Rewrite e as:
                         *  rundtor && e
                         */
                        Expression *ve = new VarExp(loc, v->rundtor);
                        e = new AndAndExp(loc, ve, e);
                        e->type = Type::tbool;
                    }
#endif
                    *sfinally = new DtorExpStatement(loc, e, v);
                }
                v->noscope = 1;         // don't add in dtor again
            }
        }
    }
    return this;
}


/******************************** DtorExpStatement ***************************/

DtorExpStatement::DtorExpStatement(Loc loc, Expression *exp, VarDeclaration *v)
    : ExpStatement(loc, exp)
{
    this->var = v;
}

Statement *DtorExpStatement::syntaxCopy()
{
    Expression *e = exp ? exp->syntaxCopy() : NULL;
    DtorExpStatement *es = new DtorExpStatement(loc, e, var);
    return es;
}

/******************************** CompileStatement ***************************/

CompileStatement::CompileStatement(Loc loc, Expression *exp)
    : Statement(loc)
{
    this->exp = exp;
}

Statement *CompileStatement::syntaxCopy()
{
    Expression *e = exp->syntaxCopy();
    CompileStatement *es = new CompileStatement(loc, e);
    return es;
}

void CompileStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("mixin(");
    exp->toCBuffer(buf, hgs);
    buf->writestring(");");
    if (!hgs->FLinit.init)
        buf->writenl();
}

Statements *CompileStatement::flatten(Scope *sc)
{
    //printf("CompileStatement::flatten() %s\n", exp->toChars());
    exp = exp->semantic(sc);
    exp = resolveProperties(sc, exp);
    exp = exp->optimize(WANTvalue | WANTinterpret);
    if (exp->op == TOKerror)
        return NULL;
    StringExp *se = exp->toString();
    if (!se)
    {   error("argument to mixin must be a string, not (%s)", exp->toChars());
        return NULL;
    }
    se = se->toUTF8(sc);
    Parser p(sc->module, (unsigned char *)se->string, se->len, 0);
    p.loc = loc;
    p.nextToken();

    Statements *a = new Statements();
    while (p.token.value != TOKeof)
    {
        Statement *s = p.parseStatement(PSsemi | PScurlyscope);
        if (s)                  // if no parsing errors
            a->push(s);
    }
    return a;
}

Statement *CompileStatement::semantic(Scope *sc)
{
    //printf("CompileStatement::semantic() %s\n", exp->toChars());
    Statements *a = flatten(sc);
    if (!a)
        return NULL;
    Statement *s = new CompoundStatement(loc, a);
    return s->semantic(sc);
}

int CompileStatement::blockExit(bool mustNotThrow)
{
    assert(global.errors);
    return BEfallthru;
}


/******************************** CompoundStatement ***************************/

CompoundStatement::CompoundStatement(Loc loc, Statements *s)
    : Statement(loc)
{
    statements = s;
}

CompoundStatement::CompoundStatement(Loc loc, Statement *s1, Statement *s2)
    : Statement(loc)
{
    statements = new Statements();
    statements->reserve(2);
    statements->push(s1);
    statements->push(s2);
}

CompoundStatement::CompoundStatement(Loc loc, Statement *s1)
    : Statement(loc)
{
    statements = new Statements();
    statements->push(s1);
}

Statement *CompoundStatement::syntaxCopy()
{
    Statements *a = new Statements();
    a->setDim(statements->dim);
    for (size_t i = 0; i < statements->dim; i++)
    {   Statement *s = (*statements)[i];
        if (s)
            s = s->syntaxCopy();
        (*a)[i] = s;
    }
    CompoundStatement *cs = new CompoundStatement(loc, a);
    return cs;
}


Statement *CompoundStatement::semantic(Scope *sc)
{   Statement *s;

    //printf("CompoundStatement::semantic(this = %p, sc = %p)\n", this, sc);

#if 0
    for (size_t i = 0; i < statements->dim; i++)
    {
        s = (*statements)[i];
        if (s)
            printf("[%d]: %s", i, s->toChars());
    }
#endif

    for (size_t i = 0; i < statements->dim; )
    {
        s = (*statements)[i];
        if (s)
        {   Statements *a = s->flatten(sc);

            if (a)
            {
                statements->remove(i);
                statements->insert(i, a);
                continue;
            }
            s = s->semantic(sc);
            (*statements)[i] = s;
            if (s)
            {
                Statement *sentry;
                Statement *sexception;
                Statement *sfinally;

                (*statements)[i] = s->scopeCode(sc, &sentry, &sexception, &sfinally);
                if (sentry)
                {
                    sentry = sentry->semantic(sc);
                    statements->insert(i, sentry);
                    i++;
                }
                if (sexception)
                    sexception = sexception->semantic(sc);
                if (sexception)
                {
                    if (i + 1 == statements->dim && !sfinally)
                    {
                    }
                    else
                    {
                        /* Rewrite:
                         *      s; s1; s2;
                         * As:
                         *      s;
                         *      try { s1; s2; }
                         *      catch (Object __o)
                         *      { sexception; throw __o; }
                         */
                        Statements *a = new Statements();
                        for (size_t j = i + 1; j < statements->dim; j++)
                        {
                            a->push((*statements)[j]);
                        }
                        Statement *body = new CompoundStatement(0, a);
                        body = new ScopeStatement(0, body);

                        Identifier *id = Lexer::uniqueId("__o");

                        Statement *handler = sexception;
                        if (sexception->blockExit(FALSE) & BEfallthru)
                        {   handler = new ThrowStatement(0, new IdentifierExp(0, id));
                            handler = new CompoundStatement(0, sexception, handler);
                        }

                        Catches *catches = new Catches();
                        Catch *ctch = new Catch(0, NULL, id, handler);
                        catches->push(ctch);
                        s = new TryCatchStatement(0, body, catches);

                        if (sfinally)
                            s = new TryFinallyStatement(0, s, sfinally);
                        s = s->semantic(sc);
                        statements->setDim(i + 1);
                        statements->push(s);
                        break;
                    }
                }
                else if (sfinally)
                {
                    if (0 && i + 1 == statements->dim)
                    {
                        statements->push(sfinally);
                    }
                    else
                    {
                        /* Rewrite:
                         *      s; s1; s2;
                         * As:
                         *      s; try { s1; s2; } finally { sfinally; }
                         */
                        Statements *a = new Statements();
                        for (size_t j = i + 1; j < statements->dim; j++)
                        {
                            a->push((*statements)[j]);
                        }
                        Statement *body = new CompoundStatement(0, a);
                        s = new TryFinallyStatement(0, body, sfinally);
                        s = s->semantic(sc);
                        statements->setDim(i + 1);
                        statements->push(s);
                        break;
                    }
                }
            }
        }
        i++;
    }
    if (statements->dim == 1)
    {
        return (*statements)[0];
    }
    return this;
}

Statements *CompoundStatement::flatten(Scope *sc)
{
    return statements;
}

ReturnStatement *CompoundStatement::isReturnStatement()
{
    ReturnStatement *rs = NULL;

    for (size_t i = 0; i < statements->dim; i++)
    {   Statement *s = (*statements)[i];
        if (s)
        {
            rs = s->isReturnStatement();
            if (rs)
                break;
        }
    }
    return rs;
}

Statement *CompoundStatement::last()
{
    Statement *s = NULL;

    for (size_t i = statements->dim; i; --i)
    {   s = (*statements)[i - 1];
        if (s)
        {
            s = s->last();
            if (s)
                break;
        }
    }
    return s;
}

void CompoundStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    for (size_t i = 0; i < statements->dim; i++)
    {   Statement *s = (*statements)[i];
        if (s)
            s->toCBuffer(buf, hgs);
    }
}

int CompoundStatement::usesEH()
{
    for (size_t i = 0; i < statements->dim; i++)
    {   Statement *s = (*statements)[i];
        if (s && s->usesEH())
            return TRUE;
    }
    return FALSE;
}

int CompoundStatement::blockExit(bool mustNotThrow)
{
    //printf("CompoundStatement::blockExit(%p) %d\n", this, statements->dim);
    int result = BEfallthru;
    Statement *slast = NULL;
    for (size_t i = 0; i < statements->dim; i++)
    {   Statement *s = (*statements)[i];
        if (s)
        {
            //printf("result = x%x\n", result);
            //printf("%s\n", s->toChars());
            if (global.params.warnings && result & BEfallthru && slast)
            {
                slast = slast->last();
                if (slast && (slast->isCaseStatement() || slast->isDefaultStatement()) &&
                             (s->isCaseStatement() || s->isDefaultStatement()))
                {
                    // Allow if last case/default was empty
                    CaseStatement *sc = slast->isCaseStatement();
                    DefaultStatement *sd = slast->isDefaultStatement();
                    if (sc && sc->statement->isEmpty())
                        ;
                    else if (sd && sd->statement->isEmpty())
                        ;
                    else
                        s->error("switch case fallthrough - use 'goto %s;' if intended",
                            s->isCaseStatement() ? "case" : "default");
                }
            }

            if (!(result & BEfallthru) && !s->comeFrom())
            {
                if (s->blockExit(mustNotThrow) != BEhalt && !s->isEmpty())
                    s->warning("statement is not reachable");
            }
            else
            {
                result &= ~BEfallthru;
                result |= s->blockExit(mustNotThrow);
            }
            slast = s;
        }
    }
    return result;
}

int CompoundStatement::comeFrom()
{   int comefrom = FALSE;

    //printf("CompoundStatement::comeFrom()\n");
    for (size_t i = 0; i < statements->dim; i++)
    {   Statement *s = (*statements)[i];

        if (!s)
            continue;

        comefrom |= s->comeFrom();
    }
    return comefrom;
}

int CompoundStatement::isEmpty()
{
    for (size_t i = 0; i < statements->dim; i++)
    {   Statement *s = (*statements)[i];
        if (s && !s->isEmpty())
            return FALSE;
    }
    return TRUE;
}


/******************************** CompoundDeclarationStatement ***************************/

CompoundDeclarationStatement::CompoundDeclarationStatement(Loc loc, Statements *s)
    : CompoundStatement(loc, s)
{
    statements = s;
}

Statement *CompoundDeclarationStatement::syntaxCopy()
{
    Statements *a = new Statements();
    a->setDim(statements->dim);
    for (size_t i = 0; i < statements->dim; i++)
    {   Statement *s = (*statements)[i];
        if (s)
            s = s->syntaxCopy();
        (*a)[i] = s;
    }
    CompoundDeclarationStatement *cs = new CompoundDeclarationStatement(loc, a);
    return cs;
}

void CompoundDeclarationStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    int nwritten = 0;
    for (size_t i = 0; i < statements->dim; i++)
    {   Statement *s = (*statements)[i];
        ExpStatement *ds;
        if (s &&
            (ds = s->isExpStatement()) != NULL &&
            ds->exp->op == TOKdeclaration)
        {
            DeclarationExp *de = (DeclarationExp *)ds->exp;
            Declaration *d = de->declaration->isDeclaration();
            assert(d);
            VarDeclaration *v = d->isVarDeclaration();
            if (v)
            {
                /* This essentially copies the part of VarDeclaration::toCBuffer()
                 * that does not print the type.
                 * Should refactor this.
                 */
                if (nwritten)
                {
                    buf->writeByte(',');
                    buf->writestring(v->ident->toChars());
                }
                else
                {
                    StorageClassDeclaration::stcToCBuffer(buf, v->storage_class);
                    if (v->type)
                        v->type->toCBuffer(buf, v->ident, hgs);
                    else
                        buf->writestring(v->ident->toChars());
                }

                if (v->init)
                {   buf->writestring(" = ");
#if DMDV2
                    ExpInitializer *ie = v->init->isExpInitializer();
                    if (ie && (ie->exp->op == TOKconstruct || ie->exp->op == TOKblit))
                        ((AssignExp *)ie->exp)->e2->toCBuffer(buf, hgs);
                    else
#endif
                        v->init->toCBuffer(buf, hgs);
                }
            }
            else
                d->toCBuffer(buf, hgs);
            nwritten++;
        }
    }
    buf->writeByte(';');
    if (!hgs->FLinit.init)
        buf->writenl();
}

/**************************** UnrolledLoopStatement ***************************/

UnrolledLoopStatement::UnrolledLoopStatement(Loc loc, Statements *s)
    : Statement(loc)
{
    statements = s;
}

Statement *UnrolledLoopStatement::syntaxCopy()
{
    Statements *a = new Statements();
    a->setDim(statements->dim);
    for (size_t i = 0; i < statements->dim; i++)
    {   Statement *s = (*statements)[i];
        if (s)
            s = s->syntaxCopy();
        (*a)[i] = s;
    }
    UnrolledLoopStatement *cs = new UnrolledLoopStatement(loc, a);
    return cs;
}


Statement *UnrolledLoopStatement::semantic(Scope *sc)
{
    //printf("UnrolledLoopStatement::semantic(this = %p, sc = %p)\n", this, sc);

    sc->noctor++;
    Scope *scd = sc->push();
    scd->sbreak = this;
    scd->scontinue = this;

    for (size_t i = 0; i < statements->dim; i++)
    {
        Statement *s = (*statements)[i];
        if (s)
        {
            //printf("[%d]: %s\n", i, s->toChars());
            s = s->semantic(scd);
            (*statements)[i] = s;
        }
    }

    scd->pop();
    sc->noctor--;
    return this;
}

void UnrolledLoopStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("unrolled {");
    buf->writenl();

    for (size_t i = 0; i < statements->dim; i++)
    {   Statement *s;

        s = (*statements)[i];
        if (s)
            s->toCBuffer(buf, hgs);
    }

    buf->writeByte('}');
    buf->writenl();
}

int UnrolledLoopStatement::hasBreak()
{
    return TRUE;
}

int UnrolledLoopStatement::hasContinue()
{
    return TRUE;
}

int UnrolledLoopStatement::usesEH()
{
    for (size_t i = 0; i < statements->dim; i++)
    {   Statement *s = (*statements)[i];
        if (s && s->usesEH())
            return TRUE;
    }
    return FALSE;
}

int UnrolledLoopStatement::blockExit(bool mustNotThrow)
{
    int result = BEfallthru;
    for (size_t i = 0; i < statements->dim; i++)
    {   Statement *s = (*statements)[i];
        if (s)
        {
            int r = s->blockExit(mustNotThrow);
            result |= r & ~(BEbreak | BEcontinue);
        }
    }
    return result;
}


int UnrolledLoopStatement::comeFrom()
{   int comefrom = FALSE;

    //printf("UnrolledLoopStatement::comeFrom()\n");
    for (size_t i = 0; i < statements->dim; i++)
    {   Statement *s = (*statements)[i];

        if (!s)
            continue;

        comefrom |= s->comeFrom();
    }
    return comefrom;
}


/******************************** ScopeStatement ***************************/

ScopeStatement::ScopeStatement(Loc loc, Statement *s)
    : Statement(loc)
{
    this->statement = s;
}

Statement *ScopeStatement::syntaxCopy()
{
    Statement *s;

    s = statement ? statement->syntaxCopy() : NULL;
    s = new ScopeStatement(loc, s);
    return s;
}


Statement *ScopeStatement::semantic(Scope *sc)
{   ScopeDsymbol *sym;

    //printf("ScopeStatement::semantic(sc = %p)\n", sc);
    if (statement)
    {   Statements *a;

        sym = new ScopeDsymbol();
        sym->parent = sc->scopesym;
        sc = sc->push(sym);

        a = statement->flatten(sc);
        if (a)
        {
            statement = new CompoundStatement(loc, a);
        }

        statement = statement->semantic(sc);
        if (statement)
        {
            Statement *sentry;
            Statement *sexception;
            Statement *sfinally;

            statement = statement->scopeCode(sc, &sentry, &sexception, &sfinally);
            assert(!sentry);
            assert(!sexception);
            if (sfinally)
            {
                //printf("adding sfinally\n");
                sfinally = sfinally->semantic(sc);
                statement = new CompoundStatement(loc, statement, sfinally);
            }
        }

        sc->pop();
    }
    return this;
}

int ScopeStatement::hasBreak()
{
    //printf("ScopeStatement::hasBreak() %s\n", toChars());
    return statement ? statement->hasBreak() : FALSE;
}

int ScopeStatement::hasContinue()
{
    return statement ? statement->hasContinue() : FALSE;
}

int ScopeStatement::usesEH()
{
    return statement ? statement->usesEH() : FALSE;
}

int ScopeStatement::blockExit(bool mustNotThrow)
{
    //printf("ScopeStatement::blockExit(%p)\n", statement);
    return statement ? statement->blockExit(mustNotThrow) : BEfallthru;
}


int ScopeStatement::comeFrom()
{
    //printf("ScopeStatement::comeFrom()\n");
    return statement ? statement->comeFrom() : FALSE;
}

int ScopeStatement::isEmpty()
{
    //printf("ScopeStatement::isEmpty() %d\n", statement ? statement->isEmpty() : TRUE);
    return statement ? statement->isEmpty() : TRUE;
}

void ScopeStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writeByte('{');
    buf->writenl();

    if (statement)
        statement->toCBuffer(buf, hgs);

    buf->writeByte('}');
    buf->writenl();
}

/******************************** WhileStatement ***************************/

WhileStatement::WhileStatement(Loc loc, Expression *c, Statement *b)
    : Statement(loc)
{
    condition = c;
    body = b;
}

Statement *WhileStatement::syntaxCopy()
{
    WhileStatement *s = new WhileStatement(loc, condition->syntaxCopy(), body ? body->syntaxCopy() : NULL);
    return s;
}


Statement *WhileStatement::semantic(Scope *sc)
{
    /* Rewrite as a for(;condition;) loop
     */

    Statement *s = new ForStatement(loc, NULL, condition, NULL, body);
    s = s->semantic(sc);
    return s;
}

int WhileStatement::hasBreak()
{
    return TRUE;
}

int WhileStatement::hasContinue()
{
    return TRUE;
}

int WhileStatement::usesEH()
{
    assert(0);
    return body ? body->usesEH() : 0;
}

int WhileStatement::blockExit(bool mustNotThrow)
{
    assert(0);
    //printf("WhileStatement::blockExit(%p)\n", this);

    int result = BEnone;
    if (condition->canThrow(mustNotThrow))
        result |= BEthrow;
    if (condition->isBool(TRUE))
    {
        if (body)
        {   result |= body->blockExit(mustNotThrow);
            if (result & BEbreak)
                result |= BEfallthru;
        }
    }
    else if (condition->isBool(FALSE))
    {
        result |= BEfallthru;
    }
    else
    {
        if (body)
            result |= body->blockExit(mustNotThrow);
        result |= BEfallthru;
    }
    result &= ~(BEbreak | BEcontinue);
    return result;
}


int WhileStatement::comeFrom()
{
    assert(0);
    if (body)
        return body->comeFrom();
    return FALSE;
}

void WhileStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("while (");
    condition->toCBuffer(buf, hgs);
    buf->writebyte(')');
    buf->writenl();
    if (body)
        body->toCBuffer(buf, hgs);
}

/******************************** DoStatement ***************************/

DoStatement::DoStatement(Loc loc, Statement *b, Expression *c)
    : Statement(loc)
{
    body = b;
    condition = c;
}

Statement *DoStatement::syntaxCopy()
{
    DoStatement *s = new DoStatement(loc, body ? body->syntaxCopy() : NULL, condition->syntaxCopy());
    return s;
}


Statement *DoStatement::semantic(Scope *sc)
{
    sc->noctor++;
    if (body)
        body = body->semanticScope(sc, this, this);
    sc->noctor--;
    condition = condition->semantic(sc);
    condition = resolveProperties(sc, condition);
    condition = condition->optimize(WANTvalue);

    condition = condition->checkToBoolean(sc);

    return this;
}

int DoStatement::hasBreak()
{
    return TRUE;
}

int DoStatement::hasContinue()
{
    return TRUE;
}

int DoStatement::usesEH()
{
    return body ? body->usesEH() : 0;
}

int DoStatement::blockExit(bool mustNotThrow)
{   int result;

    if (body)
    {   result = body->blockExit(mustNotThrow);
        if (result == BEbreak)
            return BEfallthru;
        if (result & BEcontinue)
            result |= BEfallthru;
    }
    else
        result = BEfallthru;
    if (result & BEfallthru)
    {
        if (condition->canThrow(mustNotThrow))
            result |= BEthrow;
        if (!(result & BEbreak) && condition->isBool(TRUE))
            result &= ~BEfallthru;
    }
    result &= ~(BEbreak | BEcontinue);
    return result;
}


int DoStatement::comeFrom()
{
    if (body)
        return body->comeFrom();
    return FALSE;
}

void DoStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("do");
    buf->writenl();
    if (body)
        body->toCBuffer(buf, hgs);
    buf->writestring("while (");
    condition->toCBuffer(buf, hgs);
    buf->writestring(");");
}

/******************************** ForStatement ***************************/

ForStatement::ForStatement(Loc loc, Statement *init, Expression *condition, Expression *increment, Statement *body)
    : Statement(loc)
{
    this->init = init;
    this->condition = condition;
    this->increment = increment;
    this->body = body;
}

Statement *ForStatement::syntaxCopy()
{
    Statement *i = NULL;
    if (init)
        i = init->syntaxCopy();
    Expression *c = NULL;
    if (condition)
        c = condition->syntaxCopy();
    Expression *inc = NULL;
    if (increment)
        inc = increment->syntaxCopy();
    ForStatement *s = new ForStatement(loc, i, c, inc, body->syntaxCopy());
    return s;
}

Statement *ForStatement::semantic(Scope *sc)
{
    ScopeDsymbol *sym = new ScopeDsymbol();
    sym->parent = sc->scopesym;
    sc = sc->push(sym);
    if (init)
        init = init->semantic(sc);
    sc->noctor++;
    if (condition)
    {
        condition = condition->semantic(sc);
        condition = resolveProperties(sc, condition);
        condition = condition->optimize(WANTvalue);
        condition = condition->checkToBoolean(sc);
    }
    if (increment)
    {   increment = increment->semantic(sc);
        increment = resolveProperties(sc, increment);
        increment = increment->optimize(0);
    }

    sc->sbreak = this;
    sc->scontinue = this;
    if (body)
        body = body->semanticNoScope(sc);
    sc->noctor--;

    sc->pop();
    return this;
}

Statement *ForStatement::scopeCode(Scope *sc, Statement **sentry, Statement **sexception, Statement **sfinally)
{
    //printf("ForStatement::scopeCode()\n");
    //print();
    if (init)
        init = init->scopeCode(sc, sentry, sexception, sfinally);
    else
        Statement::scopeCode(sc, sentry, sexception, sfinally);
    return this;
}

int ForStatement::hasBreak()
{
    //printf("ForStatement::hasBreak()\n");
    return TRUE;
}

int ForStatement::hasContinue()
{
    return TRUE;
}

int ForStatement::usesEH()
{
    return (init && init->usesEH()) || body->usesEH();
}

int ForStatement::blockExit(bool mustNotThrow)
{   int result = BEfallthru;

    if (init)
    {   result = init->blockExit(mustNotThrow);
        if (!(result & BEfallthru))
            return result;
    }
    if (condition)
    {   if (condition->canThrow(mustNotThrow))
            result |= BEthrow;
        if (condition->isBool(TRUE))
            result &= ~BEfallthru;
        else if (condition->isBool(FALSE))
            return result;
    }
    else
        result &= ~BEfallthru;  // the body must do the exiting
    if (body)
    {   int r = body->blockExit(mustNotThrow);
        if (r & (BEbreak | BEgoto))
            result |= BEfallthru;
        result |= r & ~(BEfallthru | BEbreak | BEcontinue);
    }
    if (increment && increment->canThrow(mustNotThrow))
        result |= BEthrow;
    return result;
}


int ForStatement::comeFrom()
{
    //printf("ForStatement::comeFrom()\n");
    if (body)
    {   int result = body->comeFrom();
        //printf("result = %d\n", result);
        return result;
    }
    return FALSE;
}

void ForStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("for (");
    if (init)
    {
        hgs->FLinit.init++;
        init->toCBuffer(buf, hgs);
        hgs->FLinit.init--;
    }
    else
        buf->writebyte(';');
    if (condition)
    {   buf->writebyte(' ');
        condition->toCBuffer(buf, hgs);
    }
    buf->writebyte(';');
    if (increment)
    {   buf->writebyte(' ');
        increment->toCBuffer(buf, hgs);
    }
    buf->writebyte(')');
    buf->writenl();
    buf->writebyte('{');
    buf->writenl();
    body->toCBuffer(buf, hgs);
    buf->writebyte('}');
    buf->writenl();
}

/******************************** ForeachStatement ***************************/

ForeachStatement::ForeachStatement(Loc loc, enum TOK op, Parameters *arguments,
        Expression *aggr, Statement *body)
    : Statement(loc)
{
    this->op = op;
    this->arguments = arguments;
    this->aggr = aggr;
    this->body = body;

    this->key = NULL;
    this->value = NULL;

    this->func = NULL;

    this->cases = NULL;
    this->gotos = NULL;
}

Statement *ForeachStatement::syntaxCopy()
{
    Parameters *args = Parameter::arraySyntaxCopy(arguments);
    Expression *exp = aggr->syntaxCopy();
    ForeachStatement *s = new ForeachStatement(loc, op, args, exp,
        body ? body->syntaxCopy() : NULL);
    return s;
}

Statement *ForeachStatement::semantic(Scope *sc)
{
    //printf("ForeachStatement::semantic() %p\n", this);
    ScopeDsymbol *sym;
    Statement *s = this;
    size_t dim = arguments->dim;
    TypeAArray *taa = NULL;
    Dsymbol *sapply = NULL;

    Type *tn = NULL;
    Type *tnv = NULL;

    func = sc->func;
    if (func->fes)
        func = func->fes->func;

    if (!inferAggregate(sc, sapply))
    {
        error("invalid foreach aggregate %s", aggr->toChars());
        return this;
    }

    /* Check for inference errors
     */
    if (!inferApplyArgTypes(sc, sapply))
    {
        //printf("dim = %d, arguments->dim = %d\n", dim, arguments->dim);
        error("cannot uniquely infer foreach argument types");
        return this;
    }

    Type *tab = aggr->type->toBasetype();

    if (tab->ty == Ttuple)      // don't generate new scope for tuple loops
    {
        if (dim < 1 || dim > 2)
        {
            error("only one (value) or two (key,value) arguments for tuple foreach");
            return s;
        }

        TypeTuple *tuple = (TypeTuple *)tab;
        Statements *statements = new Statements();
        //printf("aggr: op = %d, %s\n", aggr->op, aggr->toChars());
        size_t n;
        TupleExp *te = NULL;
        Expression *prelude = NULL;
        if (aggr->op == TOKtuple)       // expression tuple
        {   te = (TupleExp *)aggr;
            n = te->exps->dim;

            if (te->exps->dim > 0 && (*te->exps)[0]->op == TOKdotvar &&
                ((DotVarExp *)(*te->exps)[0])->e1->isTemp())
            {
                CommaExp *ce = (CommaExp *)((DotVarExp *)(*te->exps)[0])->e1;

                                prelude = ce->e1;
                                ((DotVarExp *)(*te->exps)[0])->e1 = ce->e2;
            }
        }
        else if (aggr->op == TOKtype)   // type tuple
        {
            n = Parameter::dim(tuple->arguments);
        }
        else
            assert(0);
        for (size_t j = 0; j < n; j++)
        {   size_t k = (op == TOKforeach) ? j : n - 1 - j;
            Expression *e;
            Type *t;
            if (te)
                e = (*te->exps)[k];
            else
                t = Parameter::getNth(tuple->arguments, k)->type;
            Parameter *arg = (*arguments)[0];
            Statements *st = new Statements();

            if (dim == 2)
            {   // Declare key
                if (arg->storageClass & (STCout | STCref | STClazy))
                    error("no storage class for key %s", arg->ident->toChars());
                arg->type = arg->type->semantic(loc, sc);
                TY keyty = arg->type->ty;
                if (keyty != Tint32 && keyty != Tuns32)
                {
                    if (global.params.is64bit)
                    {
                        if (keyty != Tint64 && keyty != Tuns64)
                            error("foreach: key type must be int or uint, long or ulong, not %s", arg->type->toChars());
                    }
                    else
                        error("foreach: key type must be int or uint, not %s", arg->type->toChars());
                }
                Initializer *ie = new ExpInitializer(0, new IntegerExp(k));
                VarDeclaration *var = new VarDeclaration(loc, arg->type, arg->ident, ie);
                var->storage_class |= STCmanifest;
                DeclarationExp *de = new DeclarationExp(loc, var);
                st->push(new ExpStatement(loc, de));
                arg = (*arguments)[1];  // value
            }
            // Declare value
            if (arg->storageClass & (STCout | STClazy) ||
                arg->storageClass & STCref && !te)
                error("no storage class for value %s", arg->ident->toChars());
            Dsymbol *var;
            if (te)
            {   Type *tb = e->type->toBasetype();
                Dsymbol *s = NULL;
                if ((tb->ty == Tfunction || tb->ty == Tsarray) && e->op == TOKvar)
                    s = ((VarExp *)e)->var;
                else if (e->op == TOKtemplate)
                    s =((TemplateExp *)e)->td;
                else if (e->op == TOKimport)
                    s =((ScopeExp *)e)->sds;

                if (s)
                {
                    var = new AliasDeclaration(loc, arg->ident, s);
                    if (arg->storageClass & STCref)
                        error("symbol %s cannot be ref", s->toChars());
                }
                else if (e->op == TOKtype)
                {
                    var = new AliasDeclaration(loc, arg->ident, e->type);
                }
                else
                {
                    arg->type = e->type;
                    Initializer *ie = new ExpInitializer(0, e);
                    VarDeclaration *v = new VarDeclaration(loc, arg->type, arg->ident, ie);
                    if (arg->storageClass & STCref)
                        v->storage_class |= STCref | STCforeach;
                    if (e->isConst() || e->op == TOKstring)
                    {   if (v->storage_class & STCref)
                            error("constant value %s cannot be ref", ie->toChars());
                        else
                            v->storage_class |= STCmanifest;
                    }
                    var = v;
                }
            }
            else
            {
                var = new AliasDeclaration(loc, arg->ident, t);
            }
            DeclarationExp *de = new DeclarationExp(loc, var);
            st->push(new ExpStatement(loc, de));

            st->push(body->syntaxCopy());
            s = new CompoundStatement(loc, st);
            s = new ScopeStatement(loc, s);
            statements->push(s);
        }

        s = new UnrolledLoopStatement(loc, statements);
        if (prelude)
            s = new CompoundStatement(loc,
                    new ExpStatement(prelude->loc, prelude), s);
        s = s->semantic(sc);
        return s;
    }

    sym = new ScopeDsymbol();
    sym->parent = sc->scopesym;
    sc = sc->push(sym);

    sc->noctor++;

Lagain:
    switch (tab->ty)
    {
        case Tarray:
        case Tsarray:
            if (!checkForArgTypes())
                return this;

            if (dim < 1 || dim > 2)
            {
                error("only one or two arguments for array foreach");
                break;
            }

            /* Look for special case of parsing char types out of char type
             * array.
             */
            tn = tab->nextOf()->toBasetype();
            if (tn->ty == Tchar || tn->ty == Twchar || tn->ty == Tdchar)
            {   Parameter *arg;

                int i = (dim == 1) ? 0 : 1;     // index of value
                arg = (*arguments)[i];
                arg->type = arg->type->semantic(loc, sc);
                tnv = arg->type->toBasetype();
                if (tnv->ty != tn->ty &&
                    (tnv->ty == Tchar || tnv->ty == Twchar || tnv->ty == Tdchar))
                {
                    if (arg->storageClass & STCref)
                        error("foreach: value of UTF conversion cannot be ref");
                    if (dim == 2)
                    {   arg = (*arguments)[0];
                        if (arg->storageClass & STCref)
                            error("foreach: key cannot be ref");
                    }
                    goto Lapply;
                }
            }

            for (size_t i = 0; i < dim; i++)
            {   // Declare args
                Parameter *arg = (*arguments)[i];
                Type *argtype = arg->type->semantic(loc, sc);
                VarDeclaration *var;

                var = new VarDeclaration(loc, argtype, arg->ident, NULL);
                var->storage_class |= STCforeach;
                var->storage_class |= arg->storageClass & (STCin | STCout | STCref | STC_TYPECTOR);
                if (var->storage_class & (STCref | STCout))
                    var->storage_class |= STCnodtor;
                if (dim == 2 && i == 0)
                {   key = var;
                    //var->storage_class |= STCfinal;
                }
                else
                {
                    value = var;
                    /* Reference to immutable data should be marked as const
                     */
                    if (var->storage_class & STCref && !tn->isMutable())
                    {
                        var->storage_class |= STCconst;
                    }
                }
#if 0
                DeclarationExp *de = new DeclarationExp(loc, var);
                de->semantic(sc);
#endif
            }

#if 1
        {
             /* Convert to a ForStatement
              *   foreach (key, value; a) body =>
              *   for (T[] tmp = a[], size_t key; key < tmp.length; ++key)
              *   { T value = tmp[k]; body }
              *
              *   foreach_reverse (key, value; a) body =>
              *   for (T[] tmp = a[], size_t key = tmp.length; key--; )
              *   { T value = tmp[k]; body }
              */
            Identifier *id = Lexer::uniqueId("__aggr");
            ExpInitializer *ie = new ExpInitializer(loc, new SliceExp(loc, aggr, NULL, NULL));
            VarDeclaration *tmp = new VarDeclaration(loc, tab->nextOf()->arrayOf(), id, ie);

            Expression *tmp_length = new DotIdExp(loc, new VarExp(loc, tmp), Id::length);

            if (!key)
            {
                Identifier *idkey = Lexer::uniqueId("__key");
                key = new VarDeclaration(loc, Type::tsize_t, idkey, NULL);
            }
            if (op == TOKforeach_reverse)
                key->init = new ExpInitializer(loc, tmp_length);
            else
                key->init = new ExpInitializer(loc, new IntegerExp(0));

            Statements *cs = new Statements();
            cs->push(new ExpStatement(loc, tmp));
            cs->push(new ExpStatement(loc, key));
            Statement *forinit = new CompoundDeclarationStatement(loc, cs);

            Expression *cond;
            if (op == TOKforeach_reverse)
                // key--
                cond = new PostExp(TOKminusminus, loc, new VarExp(loc, key));
            else
                // key < tmp.length
                cond = new CmpExp(TOKlt, loc, new VarExp(loc, key), tmp_length);

            Expression *increment = NULL;
            if (op == TOKforeach)
                // key += 1
                increment = new AddAssignExp(loc, new VarExp(loc, key), new IntegerExp(1));

            // T value = tmp[key];
            value->init = new ExpInitializer(loc, new IndexExp(loc, new VarExp(loc, tmp), new VarExp(loc, key)));
            Statement *ds = new ExpStatement(loc, value);

            body = new CompoundStatement(loc, ds, body);

            s = new ForStatement(loc, forinit, cond, increment, body);
            s = s->semantic(sc);
            break;
        }
#else
            if (tab->nextOf()->implicitConvTo(value->type) < MATCHconst)
            {
                if (aggr->op == TOKstring)
                    aggr = aggr->implicitCastTo(sc, value->type->arrayOf());
                else
                    error("foreach: %s is not an array of %s",
                        tab->toChars(), value->type->toChars());
            }

            if (key)
            {
                if (key->type->ty != Tint32 && key->type->ty != Tuns32)
                {
                    if (global.params.is64bit)
                    {
                        if (key->type->ty != Tint64 && key->type->ty != Tuns64)
                            error("foreach: key type must be int or uint, long or ulong, not %s", key->type->toChars());
                    }
                    else
                        error("foreach: key type must be int or uint, not %s", key->type->toChars());
                }

                if (key->storage_class & (STCout | STCref))
                    error("foreach: key cannot be out or ref");
            }

            sc->sbreak = this;
            sc->scontinue = this;
            body = body->semantic(sc);
            break;
#endif

        case Taarray:
            if (!checkForArgTypes())
                return this;

            taa = (TypeAArray *)tab;
            if (dim < 1 || dim > 2)
            {
                error("only one or two arguments for associative array foreach");
                break;
            }
#if SARRAYVALUE
            /* This only works if Key or Value is a static array.
             */
            tab = taa->getImpl()->type;
            goto Lagain;
#else
            if (op == TOKforeach_reverse)
            {
                error("no reverse iteration on associative arrays");
            }
            goto Lapply;
#endif
        case Tclass:
        case Tstruct:
#if DMDV2
            /* Prefer using opApply, if it exists
             */
            if (sapply)
                goto Lapply;

        {   /* Look for range iteration, i.e. the properties
             * .empty, .next, .retreat, .head and .rear
             *    foreach (e; aggr) { ... }
             * translates to:
             *    for (auto __r = aggr[]; !__r.empty; __r.next)
             *    {   auto e = __r.head;
             *        ...
             *    }
             */
            AggregateDeclaration *ad = (tab->ty == Tclass)
                        ? (AggregateDeclaration *)((TypeClass  *)tab)->sym
                        : (AggregateDeclaration *)((TypeStruct *)tab)->sym;
            Identifier *idhead;
            Identifier *idnext;
            if (op == TOKforeach)
            {   idhead = Id::Ffront;
                idnext = Id::FpopFront;
            }
            else
            {   idhead = Id::Fback;
                idnext = Id::FpopBack;
            }
            Dsymbol *shead = search_function(ad, idhead);
            if (!shead)
                goto Lapply;

            /* Generate a temporary __r and initialize it with the aggregate.
             */
            Identifier *id = Identifier::generateId("__r");
            VarDeclaration *r = new VarDeclaration(loc, NULL, id, new ExpInitializer(loc, aggr));
            Statement *init = new ExpStatement(loc, r);

            // !__r.empty
            Expression *e = new VarExp(loc, r);
            e = new DotIdExp(loc, e, Id::Fempty);
            Expression *condition = new NotExp(loc, e);

            // __r.next
            e = new VarExp(loc, r);
            Expression *increment = new CallExp(loc, new DotIdExp(loc, e, idnext));

            /* Declaration statement for e:
             *    auto e = __r.idhead;
             */
            e = new VarExp(loc, r);
            Expression *einit = new DotIdExp(loc, e, idhead);
            Statement *makeargs, *forbody;
            if (dim == 1)
            {
                Parameter *arg = (*arguments)[0];
                VarDeclaration *ve = new VarDeclaration(loc, arg->type, arg->ident, new ExpInitializer(loc, einit));
                ve->storage_class |= STCforeach;
                ve->storage_class |= arg->storageClass & (STCin | STCout | STCref | STC_TYPECTOR);

                DeclarationExp *de = new DeclarationExp(loc, ve);
                makeargs = new ExpStatement(loc, de);
            }
            else
            {
                Identifier *id = Lexer::uniqueId("__front");
                ExpInitializer *ei = new ExpInitializer(loc, einit);
                VarDeclaration *vd = new VarDeclaration(loc, NULL, id, ei);
                vd->storage_class |= STCctfe | STCref | STCforeach;

                Expression *de = new DeclarationExp(loc, vd);
                makeargs = new ExpStatement(loc, de);

                Expression *ve = new VarExp(loc, vd);
                ve->type = shead->isDeclaration()->type;
                if (ve->type->toBasetype()->ty == Tfunction)
                    ve->type = ve->type->toBasetype()->nextOf();
                if (!ve->type || ve->type->ty == Terror)
                    goto Lrangeerr;

                // Resolve inout qualifier of front type
                ve->type = ve->type->substWildTo(tab->mod);

                Expressions *exps = new Expressions();
                exps->push(ve);
                int pos = 0;
                while (exps->dim < dim)
                {
                    pos = expandAliasThisTuples(exps, pos);
                    if (pos == -1)
                        break;
                }
                if (exps->dim != dim)
                    goto Lrangeerr;

                for (size_t i = 0; i < dim; i++)
                {
                    Parameter *arg = (*arguments)[i];
                    Expression *exp = (*exps)[i];
                #if 0
                    printf("[%d] arg = %s %s, exp = %s %s\n", i,
                            arg->type ? arg->type->toChars() : "?", arg->ident->toChars(),
                            exp->type->toChars(), exp->toChars());
                #endif
                    if (arg->type && !exp->implicitConvTo(arg->type))
                        goto Lrangeerr;
                    if (!arg->type)
                        arg->type = exp->type;

                    VarDeclaration *var = new VarDeclaration(loc, arg->type, arg->ident, new ExpInitializer(loc, exp));
                    var->storage_class |= STCctfe | STCref | STCforeach;
                    DeclarationExp *de = new DeclarationExp(loc, var);
                    makeargs = new CompoundStatement(loc, makeargs, new ExpStatement(loc, de));
                }

            }

            forbody = new CompoundStatement(loc,
                makeargs, this->body);

            s = new ForStatement(loc, init, condition, increment, forbody);
#if 0
            printf("init: %s\n", init->toChars());
            printf("condition: %s\n", condition->toChars());
            printf("increment: %s\n", increment->toChars());
            printf("body: %s\n", forbody->toChars());
#endif
            s = s->semantic(sc);
            break;

        Lrangeerr:
            error("cannot infer argument types");
            break;
        }
#endif
        case Tdelegate:
        Lapply:
        {
            Expression *ec;
            Expression *e;

            if (!checkForArgTypes())
            {   body = body->semanticNoScope(sc);
                return this;
            }

            Type *tret = func->type->nextOf();

            // Need a variable to hold value from any return statements in body.
            if (!sc->func->vresult && tret && tret != Type::tvoid)
            {
                VarDeclaration *v = new VarDeclaration(loc, tret, Id::result, NULL);
                v->noscope = 1;
                v->semantic(sc);
                if (!sc->insert(v))
                    assert(0);
                v->parent = sc->func;
                sc->func->vresult = v;
            }

            TypeFunction *tfld = NULL;
            if (sapply)
            {   FuncDeclaration *fdapply = sapply->isFuncDeclaration();
                if (fdapply)
                {   assert(fdapply->type && fdapply->type->ty == Tfunction);
                    tfld = (TypeFunction *)fdapply->type->semantic(loc, sc);
                    goto Lget;
                }
                else if (tab->ty == Tdelegate)
                {
                    tfld = (TypeFunction *)tab->nextOf();
                Lget:
                    //printf("tfld = %s\n", tfld->toChars());
                    if (tfld->parameters->dim == 1)
                    {
                        Parameter *p = Parameter::getNth(tfld->parameters, 0);
                        if (p->type && p->type->ty == Tdelegate)
                        {   Type *t = p->type->semantic(loc, sc);
                            assert(t->ty == Tdelegate);
                            tfld = (TypeFunction *)t->nextOf();
                        }
                    }
                }
            }

            /* Turn body into the function literal:
             *  int delegate(ref T arg) { body }
             */
            Parameters *args = new Parameters();
            for (size_t i = 0; i < dim; i++)
            {   Parameter *arg = (*arguments)[i];
                StorageClass stc = STCref;
                Identifier *id;

                arg->type = arg->type->semantic(loc, sc);
                if (tfld)
                {   Parameter *prm = Parameter::getNth(tfld->parameters, i);
                    //printf("\tprm = %s%s\n", (prm->storageClass&STCref?"ref ":""), prm->ident->toChars());
                    stc = prm->storageClass & STCref;
                    id = arg->ident;    // argument copy is not need.
                    if ((arg->storageClass & STCref) != stc)
                    {   if (!stc)
                            error("foreach: cannot make %s ref", arg->ident->toChars());
                        goto LcopyArg;
                    }
                }
                else if (arg->storageClass & STCref)
                {   // default delegate parameters are marked as ref, then
                    // argument copy is not need.
                    id = arg->ident;
                }
                else
                {   // Make a copy of the ref argument so it isn't
                    // a reference.
                LcopyArg:
                    id = Lexer::uniqueId("__applyArg", i);

                    Initializer *ie = new ExpInitializer(0, new IdentifierExp(0, id));
                    VarDeclaration *v = new VarDeclaration(0, arg->type, arg->ident, ie);
                    s = new ExpStatement(0, v);
                    body = new CompoundStatement(loc, s, body);
                }
                args->push(new Parameter(stc, arg->type, id, NULL));
            }
            tfld = new TypeFunction(args, Type::tint32, 0, LINKd);
            cases = new Statements();
            gotos = new CompoundStatements();
            FuncLiteralDeclaration *fld = new FuncLiteralDeclaration(loc, 0, tfld, TOKdelegate, this);
            fld->fbody = body;
            Expression *flde = new FuncExp(loc, fld);
            flde = flde->semantic(sc);
            fld->tookAddressOf = 0;

            // Resolve any forward referenced goto's
            for (size_t i = 0; i < gotos->dim; i++)
            {   CompoundStatement *cs = (*gotos)[i];
                GotoStatement *gs = (GotoStatement *)(*cs->statements)[0];

                if (!gs->label->statement)
                {   // 'Promote' it to this scope, and replace with a return
                    cases->push(gs);
                    s = new ReturnStatement(0, new IntegerExp(cases->dim + 1));
                    (*cs->statements)[0] = s;
                }
            }

            if (taa)
            {
                // Check types
                Parameter *arg = (*arguments)[0];
                if (dim == 2)
                {
                    if (arg->storageClass & STCref)
                        error("foreach: index cannot be ref");
                    if (!arg->type->equals(taa->index))
                        error("foreach: index must be type %s, not %s", taa->index->toChars(), arg->type->toChars());
                    arg = (*arguments)[1];
                }
                if (!arg->type->equals(taa->nextOf()))
                    error("foreach: value must be type %s, not %s", taa->nextOf()->toChars(), arg->type->toChars());

                /* Call:
                 *      _aaApply(aggr, keysize, flde)
                 */
                FuncDeclaration *fdapply;
                if (dim == 2)
                    fdapply = FuncDeclaration::genCfunc(Type::tindex, "_aaApply2");
                else
                    fdapply = FuncDeclaration::genCfunc(Type::tindex, "_aaApply");
                ec = new VarExp(0, fdapply);
                Expressions *exps = new Expressions();
                exps->push(aggr);
                size_t keysize = taa->index->size();
                keysize = (keysize + (PTRSIZE-1)) & ~(PTRSIZE-1);
                exps->push(new IntegerExp(0, keysize, Type::tsize_t));
                exps->push(flde);
                e = new CallExp(loc, ec, exps);
                e->type = Type::tindex; // don't run semantic() on e
            }
            else if (tab->ty == Tarray || tab->ty == Tsarray)
            {
                /* Call:
                 *      _aApply(aggr, flde)
                 */
                static char fntab[9][3] =
                { "cc","cw","cd",
                  "wc","cc","wd",
                  "dc","dw","dd"
                };
                char fdname[7+1+2+ sizeof(dim)*3 + 1];
                int flag;

                switch (tn->ty)
                {
                    case Tchar:         flag = 0; break;
                    case Twchar:        flag = 3; break;
                    case Tdchar:        flag = 6; break;
                    default:            assert(0);
                }
                switch (tnv->ty)
                {
                    case Tchar:         flag += 0; break;
                    case Twchar:        flag += 1; break;
                    case Tdchar:        flag += 2; break;
                    default:            assert(0);
                }
                const char *r = (op == TOKforeach_reverse) ? "R" : "";
                int j = sprintf(fdname, "_aApply%s%.*s%llu", r, 2, fntab[flag], (ulonglong)dim);
                assert(j < sizeof(fdname));
                FuncDeclaration *fdapply = FuncDeclaration::genCfunc(Type::tindex, fdname);

                ec = new VarExp(0, fdapply);
                Expressions *exps = new Expressions();
                if (tab->ty == Tsarray)
                   aggr = aggr->castTo(sc, tn->arrayOf());
                exps->push(aggr);
                exps->push(flde);
                e = new CallExp(loc, ec, exps);
                e->type = Type::tindex; // don't run semantic() on e
            }
            else if (tab->ty == Tdelegate)
            {
                /* Call:
                 *      aggr(flde)
                 */
                Expressions *exps = new Expressions();
                exps->push(flde);
                if (aggr->op == TOKdelegate &&
                    ((DelegateExp *)aggr)->func->isNested())
                    // See Bugzilla 3560
                    e = new CallExp(loc, ((DelegateExp *)aggr)->e1, exps);
                else
                    e = new CallExp(loc, aggr, exps);
                e = e->semantic(sc);
                if (e->type != Type::tint32)
                    error("opApply() function for %s must return an int", tab->toChars());
            }
            else
            {
                assert(tab->ty == Tstruct || tab->ty == Tclass);
                Expressions *exps = new Expressions();
                assert(sapply);
#if 0
                TemplateDeclaration *td;
                if (sapply &&
                    (td = sapply->isTemplateDeclaration()) != NULL)
                {   /* Call:
                     *  aggr.apply!(fld)()
                     */
                    Objects *tiargs = new Objects();
                    tiargs->push(fld);
                    ec = new DotTemplateInstanceExp(loc, aggr, sapply->ident, tiargs);
                }
                else
#endif
                {
                    /* Call:
                     *  aggr.apply(flde)
                     */
                    ec = new DotIdExp(loc, aggr, sapply->ident);
                    exps->push(flde);
                }
                e = new CallExp(loc, ec, exps);
                e = e->semantic(sc);
                if (e->type != Type::tint32)
                    error("opApply() function for %s must return an int", tab->toChars());
            }

            if (!cases->dim)
                // Easy case, a clean exit from the loop
                s = new ExpStatement(loc, e);
            else
            {   // Construct a switch statement around the return value
                // of the apply function.
                Statements *a = new Statements();

                // default: break; takes care of cases 0 and 1
                s = new BreakStatement(0, NULL);
                s = new DefaultStatement(0, s);
                a->push(s);

                // cases 2...
                for (size_t i = 0; i < cases->dim; i++)
                {
                    s = (*cases)[i];
                    s = new CaseStatement(0, new IntegerExp(i + 2), s);
                    a->push(s);
                }

                s = new CompoundStatement(loc, a);
                s = new SwitchStatement(loc, e, s, FALSE);
            }
            s = s->semantic(sc);
            break;
        }
        case Terror:
            s = NULL;
            break;

        default:
            error("foreach: %s is not an aggregate type", aggr->type->toChars());
            s = NULL;   // error recovery
            break;
    }
    sc->noctor--;
    sc->pop();
    return s;
}

bool ForeachStatement::checkForArgTypes()
{   bool result = TRUE;

    for (size_t i = 0; i < arguments->dim; i++)
    {   Parameter *arg = (*arguments)[i];
        if (!arg->type)
        {
            error("cannot infer type for %s", arg->ident->toChars());
            arg->type = Type::terror;
            result = FALSE;
        }
    }
    return result;
}

int ForeachStatement::hasBreak()
{
    return TRUE;
}

int ForeachStatement::hasContinue()
{
    return TRUE;
}

int ForeachStatement::usesEH()
{
    return body->usesEH();
}

int ForeachStatement::blockExit(bool mustNotThrow)
{   int result = BEfallthru;

    if (aggr->canThrow(mustNotThrow))
        result |= BEthrow;

    if (body)
    {
        result |= body->blockExit(mustNotThrow) & ~(BEbreak | BEcontinue);
    }
    return result;
}


int ForeachStatement::comeFrom()
{
    if (body)
        return body->comeFrom();
    return FALSE;
}

void ForeachStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring(Token::toChars(op));
    buf->writestring(" (");
    for (size_t i = 0; i < arguments->dim; i++)
    {
        Parameter *a = (*arguments)[i];
        if (i)
            buf->writestring(", ");
        if (a->storageClass & STCref)
            buf->writestring((global.params.Dversion == 1)
                ? (char*)"inout " : (char*)"ref ");
        if (a->type)
            a->type->toCBuffer(buf, a->ident, hgs);
        else
            buf->writestring(a->ident->toChars());
    }
    buf->writestring("; ");
    aggr->toCBuffer(buf, hgs);
    buf->writebyte(')');
    buf->writenl();
    buf->writebyte('{');
    buf->writenl();
    if (body)
        body->toCBuffer(buf, hgs);
    buf->writebyte('}');
    buf->writenl();
}

/**************************** ForeachRangeStatement ***************************/

#if DMDV2

ForeachRangeStatement::ForeachRangeStatement(Loc loc, enum TOK op, Parameter *arg,
        Expression *lwr, Expression *upr, Statement *body)
    : Statement(loc)
{
    this->op = op;
    this->arg = arg;
    this->lwr = lwr;
    this->upr = upr;
    this->body = body;

    this->key = NULL;
}

Statement *ForeachRangeStatement::syntaxCopy()
{
    ForeachRangeStatement *s = new ForeachRangeStatement(loc, op,
        arg->syntaxCopy(),
        lwr->syntaxCopy(),
        upr->syntaxCopy(),
        body ? body->syntaxCopy() : NULL);
    return s;
}

Statement *ForeachRangeStatement::semantic(Scope *sc)
{
    //printf("ForeachRangeStatement::semantic() %p\n", this);
    Statement *s = this;

    lwr = lwr->semantic(sc);
    lwr = resolveProperties(sc, lwr);
    lwr = lwr->optimize(WANTvalue);
    if (!lwr->type)
    {
        error("invalid range lower bound %s", lwr->toChars());
        return this;
    }

    upr = upr->semantic(sc);
    upr = resolveProperties(sc, upr);
    upr = upr->optimize(WANTvalue);
    if (!upr->type)
    {
        error("invalid range upper bound %s", upr->toChars());
        return this;
    }

    if (arg->type)
    {
        arg->type = arg->type->semantic(loc, sc);
        lwr = lwr->implicitCastTo(sc, arg->type);
        upr = upr->implicitCastTo(sc, arg->type);
    }
    else
    {
        /* Must infer types from lwr and upr
         */
        Type *tlwr = lwr->type->toBasetype();
        if (tlwr->ty == Tstruct || tlwr->ty == Tclass)
        {
            /* Just picking the first really isn't good enough.
             */
            arg->type = lwr->type->mutableOf();
        }
        else
        {
            AddExp ea(loc, lwr, upr);
            Expression *e = ea.typeCombine(sc);
            arg->type = ea.type->mutableOf();
            lwr = ea.e1;
            upr = ea.e2;
        }
    }
#if 1
    /* Convert to a for loop:
     *  foreach (key; lwr .. upr) =>
     *  for (auto key = lwr, auto tmp = upr; key < tmp; ++key)
     *
     *  foreach_reverse (key; lwr .. upr) =>
     *  for (auto tmp = lwr, auto key = upr; key-- > tmp;)
     */

    ExpInitializer *ie = new ExpInitializer(loc, (op == TOKforeach) ? lwr : upr);
    key = new VarDeclaration(loc, arg->type, arg->ident, ie);

    Identifier *id = Lexer::uniqueId("__limit");
    ie = new ExpInitializer(loc, (op == TOKforeach) ? upr : lwr);
    VarDeclaration *tmp = new VarDeclaration(loc, arg->type, id, ie);

    Statements *cs = new Statements();
    // Keep order of evaluation as lwr, then upr
    if (op == TOKforeach)
    {
        cs->push(new ExpStatement(loc, key));
        cs->push(new ExpStatement(loc, tmp));
    }
    else
    {
        cs->push(new ExpStatement(loc, tmp));
        cs->push(new ExpStatement(loc, key));
    }
    Statement *forinit = new CompoundDeclarationStatement(loc, cs);

    Expression *cond;
    if (op == TOKforeach_reverse)
    {
        cond = new PostExp(TOKminusminus, loc, new VarExp(loc, key));
        if (arg->type->isscalar())
            // key-- > tmp
            cond = new CmpExp(TOKgt, loc, cond, new VarExp(loc, tmp));
        else
            // key-- != tmp
            cond = new EqualExp(TOKnotequal, loc, cond, new VarExp(loc, tmp));
    }
    else
    {
        if (arg->type->isscalar())
            // key < tmp
            cond = new CmpExp(TOKlt, loc, new VarExp(loc, key), new VarExp(loc, tmp));
        else
            // key != tmp
            cond = new EqualExp(TOKnotequal, loc, new VarExp(loc, key), new VarExp(loc, tmp));
    }

    Expression *increment = NULL;
    if (op == TOKforeach)
        // key += 1
        //increment = new AddAssignExp(loc, new VarExp(loc, key), new IntegerExp(1));
        increment = new PreExp(TOKpreplusplus, loc, new VarExp(loc, key));

    ForStatement *fs = new ForStatement(loc, forinit, cond, increment, body);
    s = fs->semantic(sc);
    return s;
#else
    if (!arg->type->isscalar())
        error("%s is not a scalar type", arg->type->toChars());

    sym = new ScopeDsymbol();
    sym->parent = sc->scopesym;
    sc = sc->push(sym);

    sc->noctor++;

    key = new VarDeclaration(loc, arg->type, arg->ident, NULL);
    DeclarationExp *de = new DeclarationExp(loc, key);
    de->semantic(sc);

    if (key->storage_class)
        error("foreach range: key cannot have storage class");

    sc->sbreak = this;
    sc->scontinue = this;
    body = body->semantic(sc);

    sc->noctor--;
    sc->pop();
    return s;
#endif
}

int ForeachRangeStatement::hasBreak()
{
    return TRUE;
}

int ForeachRangeStatement::hasContinue()
{
    return TRUE;
}

int ForeachRangeStatement::usesEH()
{
    assert(0);
    return body->usesEH();
}

int ForeachRangeStatement::blockExit(bool mustNotThrow)
{
    assert(0);
    int result = BEfallthru;

    if (lwr && lwr->canThrow(mustNotThrow))
        result |= BEthrow;
    else if (upr && upr->canThrow(mustNotThrow))
        result |= BEthrow;

    if (body)
    {
        result |= body->blockExit(mustNotThrow) & ~(BEbreak | BEcontinue);
    }
    return result;
}


int ForeachRangeStatement::comeFrom()
{
    assert(0);
    if (body)
        return body->comeFrom();
    return FALSE;
}

void ForeachRangeStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring(Token::toChars(op));
    buf->writestring(" (");

    if (arg->type)
        arg->type->toCBuffer(buf, arg->ident, hgs);
    else
        buf->writestring(arg->ident->toChars());

    buf->writestring("; ");
    lwr->toCBuffer(buf, hgs);
    buf->writestring(" .. ");
    upr->toCBuffer(buf, hgs);
    buf->writebyte(')');
    buf->writenl();
    buf->writebyte('{');
    buf->writenl();
    if (body)
        body->toCBuffer(buf, hgs);
    buf->writebyte('}');
    buf->writenl();
}

#endif

/******************************** IfStatement ***************************/

IfStatement::IfStatement(Loc loc, Parameter *arg, Expression *condition, Statement *ifbody, Statement *elsebody)
    : Statement(loc)
{
    this->arg = arg;
    this->condition = condition;
    this->ifbody = ifbody;
    this->elsebody = elsebody;
    this->match = NULL;
}

Statement *IfStatement::syntaxCopy()
{
    Statement *i = NULL;
    if (ifbody)
        i = ifbody->syntaxCopy();

    Statement *e = NULL;
    if (elsebody)
        e = elsebody->syntaxCopy();

    Parameter *a = arg ? arg->syntaxCopy() : NULL;
    IfStatement *s = new IfStatement(loc, a, condition->syntaxCopy(), i, e);
    return s;
}

Statement *IfStatement::semantic(Scope *sc)
{
    // Evaluate at runtime
    unsigned cs0 = sc->callSuper;
    unsigned cs1;

    Scope *scd;
    if (arg)
    {   /* Declare arg, which we will set to be the
         * result of condition.
         */
        ScopeDsymbol *sym = new ScopeDsymbol();
        sym->parent = sc->scopesym;
        scd = sc->push(sym);

        match = new VarDeclaration(loc, arg->type, arg->ident, new ExpInitializer(loc, condition));
        match->parent = sc->func;

        DeclarationExp *de = new DeclarationExp(loc, match);
        VarExp *ve = new VarExp(0, match);
        condition = new CommaExp(loc, de, ve);
        condition = condition->semantic(scd);

       if (match->edtor)
       {
            Statement *sdtor = new ExpStatement(loc, match->edtor);
            sdtor = new OnScopeStatement(loc, TOKon_scope_exit, sdtor);
            ifbody = new CompoundStatement(loc, sdtor, ifbody);
            match->noscope = 1;
       }
    }
    else
    {
        condition = condition->semantic(sc);
        condition = condition->addDtorHook(sc);
        condition = resolveProperties(sc, condition);
        scd = sc->push();
    }

    // Convert to boolean after declaring arg so this works:
    //  if (S arg = S()) {}
    // where S is a struct that defines opCast!bool.
    condition = condition->checkToBoolean(sc);

    // If we can short-circuit evaluate the if statement, don't do the
    // semantic analysis of the skipped code.
    // This feature allows a limited form of conditional compilation.
    condition = condition->optimize(WANTflags);
    ifbody = ifbody->semanticNoScope(scd);
    scd->pop();

    cs1 = sc->callSuper;
    sc->callSuper = cs0;
    if (elsebody)
        elsebody = elsebody->semanticScope(sc, NULL, NULL);
    sc->mergeCallSuper(loc, cs1);

    return this;
}

int IfStatement::usesEH()
{
    return (ifbody && ifbody->usesEH()) || (elsebody && elsebody->usesEH());
}

int IfStatement::blockExit(bool mustNotThrow)
{
    //printf("IfStatement::blockExit(%p)\n", this);

    int result = BEnone;
    if (condition->canThrow(mustNotThrow))
        result |= BEthrow;
    if (condition->isBool(TRUE))
    {
        if (ifbody)
            result |= ifbody->blockExit(mustNotThrow);
        else
            result |= BEfallthru;
    }
    else if (condition->isBool(FALSE))
    {
        if (elsebody)
            result |= elsebody->blockExit(mustNotThrow);
        else
            result |= BEfallthru;
    }
    else
    {
        if (ifbody)
            result |= ifbody->blockExit(mustNotThrow);
        else
            result |= BEfallthru;
        if (elsebody)
            result |= elsebody->blockExit(mustNotThrow);
        else
            result |= BEfallthru;
    }
    //printf("IfStatement::blockExit(%p) = x%x\n", this, result);
    return result;
}


void IfStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("if (");
    if (arg)
    {
        if (arg->type)
            arg->type->toCBuffer(buf, arg->ident, hgs);
        else
        {   buf->writestring("auto ");
            buf->writestring(arg->ident->toChars());
        }
        buf->writestring(" = ");
    }
    condition->toCBuffer(buf, hgs);
    buf->writebyte(')');
    buf->writenl();
    ifbody->toCBuffer(buf, hgs);
    if (elsebody)
    {   buf->writestring("else");
        buf->writenl();
        elsebody->toCBuffer(buf, hgs);
    }
}

/******************************** ConditionalStatement ***************************/

ConditionalStatement::ConditionalStatement(Loc loc, Condition *condition, Statement *ifbody, Statement *elsebody)
    : Statement(loc)
{
    this->condition = condition;
    this->ifbody = ifbody;
    this->elsebody = elsebody;
}

Statement *ConditionalStatement::syntaxCopy()
{
    Statement *e = NULL;
    if (elsebody)
        e = elsebody->syntaxCopy();
    ConditionalStatement *s = new ConditionalStatement(loc,
                condition->syntaxCopy(), ifbody->syntaxCopy(), e);
    return s;
}

Statement *ConditionalStatement::semantic(Scope *sc)
{
    //printf("ConditionalStatement::semantic()\n");

    // If we can short-circuit evaluate the if statement, don't do the
    // semantic analysis of the skipped code.
    // This feature allows a limited form of conditional compilation.
    if (condition->include(sc, NULL))
    {
        DebugCondition *dc = condition->isDebugCondition();
        if (dc)
        {
            sc = sc->push();
            sc->flags |= SCOPEdebug;
            ifbody = ifbody->semantic(sc);
            sc->pop();
        }
        else
            ifbody = ifbody->semantic(sc);
        return ifbody;
    }
    else
    {
        if (elsebody)
            elsebody = elsebody->semantic(sc);
        return elsebody;
    }
}

Statements *ConditionalStatement::flatten(Scope *sc)
{
    Statement *s;

    //printf("ConditionalStatement::flatten()\n");
    if (condition->include(sc, NULL))
    {
        DebugCondition *dc = condition->isDebugCondition();
        if (dc)
            s = new DebugStatement(loc, ifbody);
        else
            s = ifbody;
    }
    else
        s = elsebody;

    Statements *a = new Statements();
    a->push(s);
    return a;
}

int ConditionalStatement::usesEH()
{
    return (ifbody && ifbody->usesEH()) || (elsebody && elsebody->usesEH());
}

int ConditionalStatement::blockExit(bool mustNotThrow)
{
    int result = ifbody->blockExit(mustNotThrow);
    if (elsebody)
        result |= elsebody->blockExit(mustNotThrow);
    return result;
}

void ConditionalStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    condition->toCBuffer(buf, hgs);
    buf->writenl();
    buf->writeByte('{');
    buf->writenl();
    if (ifbody)
        ifbody->toCBuffer(buf, hgs);
    buf->writeByte('}');
    buf->writenl();
    if (elsebody)
    {
        buf->writestring("else");
        buf->writenl();
        buf->writeByte('{');
        buf->writenl();
        elsebody->toCBuffer(buf, hgs);
        buf->writeByte('}');
        buf->writenl();
    }
    buf->writenl();
}


/******************************** PragmaStatement ***************************/

PragmaStatement::PragmaStatement(Loc loc, Identifier *ident, Expressions *args, Statement *body)
    : Statement(loc)
{
    this->ident = ident;
    this->args = args;
    this->body = body;
}

Statement *PragmaStatement::syntaxCopy()
{
    Statement *b = NULL;
    if (body)
        b = body->syntaxCopy();
    PragmaStatement *s = new PragmaStatement(loc,
                ident, Expression::arraySyntaxCopy(args), b);
    return s;
}

Statement *PragmaStatement::semantic(Scope *sc)
{   // Should be merged with PragmaDeclaration
    //printf("PragmaStatement::semantic() %s\n", toChars());
    //printf("body = %p\n", body);
    if (ident == Id::msg)
    {
        if (args)
        {
            for (size_t i = 0; i < args->dim; i++)
            {
                Expression *e = (*args)[i];

                e = e->semantic(sc);
                e = e->optimize(WANTvalue | WANTinterpret);
                StringExp *se = e->toString();
                if (se)
                {
                    fprintf(stdmsg, "%.*s", (int)se->len, (char *)se->string);
                }
                else
                    fprintf(stdmsg, "%s", e->toChars());
            }
            fprintf(stdmsg, "\n");
        }
    }
    else if (ident == Id::lib)
    {
#if 1
        /* Should this be allowed?
         */
        error("pragma(lib) not allowed as statement");
#else
        if (!args || args->dim != 1)
            error("string expected for library name");
        else
        {
            Expression *e = (*args)[0];

            e = e->semantic(sc);
            e = e->optimize(WANTvalue | WANTinterpret);
            (*args)[0] = e;
            StringExp *se = e->toString();
            if (!se)
                error("string expected for library name, not '%s'", e->toChars());
            else if (global.params.verbose)
            {
                char *name = (char *)mem.malloc(se->len + 1);
                memcpy(name, se->string, se->len);
                name[se->len] = 0;
                printf("library   %s\n", name);
                mem.free(name);
            }
        }
#endif
    }
#if DMDV2
    else if (ident == Id::startaddress)
    {
        if (!args || args->dim != 1)
            error("function name expected for start address");
        else
        {
            Expression *e = (*args)[0];
            e = e->semantic(sc);
            e = e->optimize(WANTvalue | WANTinterpret);
            (*args)[0] = e;
            Dsymbol *sa = getDsymbol(e);
            if (!sa || !sa->isFuncDeclaration())
                error("function name expected for start address, not '%s'", e->toChars());
            if (body)
            {
                body = body->semantic(sc);
            }
            return this;
        }
    }
#endif
    else
        error("unrecognized pragma(%s)", ident->toChars());

    if (body)
    {
        body = body->semantic(sc);
    }
    return body;
}

int PragmaStatement::usesEH()
{
    return body && body->usesEH();
}

int PragmaStatement::blockExit(bool mustNotThrow)
{
    int result = BEfallthru;
#if 0 // currently, no code is generated for Pragma's, so it's just fallthru
    if (arrayExpressionCanThrow(args))
        result |= BEthrow;
    if (body)
        result |= body->blockExit(mustNotThrow);
#endif
    return result;
}


void PragmaStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("pragma (");
    buf->writestring(ident->toChars());
    if (args && args->dim)
    {
        buf->writestring(", ");
        argsToCBuffer(buf, args, hgs);
    }
    buf->writeByte(')');
    if (body)
    {
        buf->writenl();
        buf->writeByte('{');
        buf->writenl();

        body->toCBuffer(buf, hgs);

        buf->writeByte('}');
        buf->writenl();
    }
    else
    {
        buf->writeByte(';');
        buf->writenl();
    }
}


/******************************** StaticAssertStatement ***************************/

StaticAssertStatement::StaticAssertStatement(StaticAssert *sa)
    : Statement(sa->loc)
{
    this->sa = sa;
}

Statement *StaticAssertStatement::syntaxCopy()
{
    StaticAssertStatement *s = new StaticAssertStatement((StaticAssert *)sa->syntaxCopy(NULL));
    return s;
}

Statement *StaticAssertStatement::semantic(Scope *sc)
{
    sa->semantic2(sc);
    return NULL;
}

int StaticAssertStatement::blockExit(bool mustNotThrow)
{
    return BEfallthru;
}

void StaticAssertStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    sa->toCBuffer(buf, hgs);
}


/******************************** SwitchStatement ***************************/

SwitchStatement::SwitchStatement(Loc loc, Expression *c, Statement *b, bool isFinal)
    : Statement(loc)
{
    this->condition = c;
    this->body = b;
    this->isFinal = isFinal;
    sdefault = NULL;
    tf = NULL;
    cases = NULL;
    hasNoDefault = 0;
    hasVars = 0;
}

Statement *SwitchStatement::syntaxCopy()
{
    SwitchStatement *s = new SwitchStatement(loc,
        condition->syntaxCopy(), body->syntaxCopy(), isFinal);
    return s;
}

Statement *SwitchStatement::semantic(Scope *sc)
{
    //printf("SwitchStatement::semantic(%p)\n", this);
    tf = sc->tf;
    assert(!cases);             // ensure semantic() is only run once
    condition = condition->semantic(sc);
    condition = resolveProperties(sc, condition);
    if (condition->type->isString())
    {
        // If it's not an array, cast it to one
        if (condition->type->ty != Tarray)
        {
            condition = condition->implicitCastTo(sc, condition->type->nextOf()->arrayOf());
        }
        condition->type = condition->type->constOf();
    }
    else
    {   condition = condition->integralPromotions(sc);
        condition->checkIntegral();
    }
    condition = condition->optimize(WANTvalue);

    sc = sc->push();
    sc->sbreak = this;
    sc->sw = this;

    cases = new CaseStatements();
    sc->noctor++;       // BUG: should use Scope::mergeCallSuper() for each case instead
    body = body->semantic(sc);
    sc->noctor--;

    // Resolve any goto case's with exp
    for (size_t i = 0; i < gotoCases.dim; i++)
    {
        GotoCaseStatement *gcs = gotoCases[i];

        if (!gcs->exp)
        {
            gcs->error("no case statement following goto case;");
            break;
        }

        for (Scope *scx = sc; scx; scx = scx->enclosing)
        {
            if (!scx->sw)
                continue;
            for (size_t j = 0; j < scx->sw->cases->dim; j++)
            {
                CaseStatement *cs = (*scx->sw->cases)[j];

                if (cs->exp->equals(gcs->exp))
                {
                    gcs->cs = cs;
                    goto Lfoundcase;
                }
            }
        }
        gcs->error("case %s not found", gcs->exp->toChars());

     Lfoundcase:
        ;
    }

    bool needswitcherror = FALSE;
#if DMDV2
    if (isFinal)
    {   Type *t = condition->type;
        while (t->ty == Ttypedef)
        {   // Don't use toBasetype() because that will skip past enums
            t = ((TypeTypedef *)t)->sym->basetype;
        }
        if (condition->type->ty == Tenum)
        {   TypeEnum *te = (TypeEnum *)condition->type;
            EnumDeclaration *ed = te->toDsymbol(sc)->isEnumDeclaration();
            assert(ed);
            size_t dim = ed->members->dim;
            for (size_t i = 0; i < dim; i++)
            {
                EnumMember *em = (*ed->members)[i]->isEnumMember();
                if (em)
                {
                    for (size_t j = 0; j < cases->dim; j++)
                    {   CaseStatement *cs = (*cases)[j];
                        if (cs->exp->equals(em->value))
                            goto L1;
                    }
                    error("enum member %s not represented in final switch", em->toChars());
                }
              L1:
                ;
            }
        }
        else
            needswitcherror = TRUE;
    }
#endif

    if (!sc->sw->sdefault && (!isFinal || needswitcherror))
    {   hasNoDefault = 1;

        if (!global.params.useDeprecated && !isFinal)
           error("non-final switch statement without a default is deprecated");

        // Generate runtime error if the default is hit
        Statements *a = new Statements();
        CompoundStatement *cs;
        Statement *s;

        if (global.params.useSwitchError)
            s = new SwitchErrorStatement(loc);
        else
        {   Expression *e = new HaltExp(loc);
            s = new ExpStatement(loc, e);
        }

        a->reserve(2);
        sc->sw->sdefault = new DefaultStatement(loc, s);
        a->push(sc->sw->sdefault);
        a->push(body);
        cs = new CompoundStatement(loc, a);
        body = cs;
    }

    sc->pop();
    return this;
}

int SwitchStatement::hasBreak()
{
    return TRUE;
}

int SwitchStatement::usesEH()
{
    return body ? body->usesEH() : 0;
}

int SwitchStatement::blockExit(bool mustNotThrow)
{   int result = BEnone;
    if (condition->canThrow(mustNotThrow))
        result |= BEthrow;

    if (body)
    {   result |= body->blockExit(mustNotThrow);
        if (result & BEbreak)
        {   result |= BEfallthru;
            result &= ~BEbreak;
        }
    }
    else
        result |= BEfallthru;

    return result;
}


void SwitchStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring(isFinal ? "final switch (" : "switch (");
    condition->toCBuffer(buf, hgs);
    buf->writebyte(')');
    buf->writenl();
    if (body)
    {
        if (!body->isScopeStatement())
        {   buf->writebyte('{');
            buf->writenl();
            body->toCBuffer(buf, hgs);
            buf->writebyte('}');
            buf->writenl();
        }
        else
        {
            body->toCBuffer(buf, hgs);
        }
    }
}

/******************************** CaseStatement ***************************/

CaseStatement::CaseStatement(Loc loc, Expression *exp, Statement *s)
    : Statement(loc)
{
    this->exp = exp;
    this->statement = s;
    index = 0;
    cblock = NULL;
}

Statement *CaseStatement::syntaxCopy()
{
    CaseStatement *s = new CaseStatement(loc, exp->syntaxCopy(), statement->syntaxCopy());
    return s;
}

Statement *CaseStatement::semantic(Scope *sc)
{   SwitchStatement *sw = sc->sw;

    //printf("CaseStatement::semantic() %s\n", toChars());
    exp = exp->semantic(sc);
    if (sw)
    {
        exp = exp->implicitCastTo(sc, sw->condition->type);
        exp = exp->optimize(WANTvalue);

        /* This is where variables are allowed as case expressions.
         */
        if (exp->op == TOKvar)
        {   VarExp *ve = (VarExp *)exp;
            VarDeclaration *v = ve->var->isVarDeclaration();
            Type *t = exp->type->toBasetype();
            if (v && (t->isintegral() || t->ty == Tclass))
            {   /* Flag that we need to do special code generation
                 * for this, i.e. generate a sequence of if-then-else
                 */
                sw->hasVars = 1;
                if (sw->isFinal)
                    error("case variables not allowed in final switch statements");
                goto L1;
            }
        }
        else
            exp = exp->optimize(WANTvalue | WANTinterpret);

        if (exp->op != TOKstring && exp->op != TOKint64 && exp->op != TOKerror)
        {
            error("case must be a string or an integral constant, not %s", exp->toChars());
            exp = new IntegerExp(0);
        }

    L1:
        for (size_t i = 0; i < sw->cases->dim; i++)
        {
            CaseStatement *cs = (*sw->cases)[i];

            //printf("comparing '%s' with '%s'\n", exp->toChars(), cs->exp->toChars());
            if (cs->exp->equals(exp))
            {   error("duplicate case %s in switch statement", exp->toChars());
                break;
            }
        }

        sw->cases->push(this);

        // Resolve any goto case's with no exp to this case statement
        for (size_t i = 0; i < sw->gotoCases.dim; i++)
        {
            GotoCaseStatement *gcs = sw->gotoCases[i];

            if (!gcs->exp)
            {
                gcs->cs = this;
                sw->gotoCases.remove(i);        // remove from array
            }
        }

        if (sc->sw->tf != sc->tf)
            error("switch and case are in different finally blocks");
    }
    else
        error("case not in switch statement");
    statement = statement->semantic(sc);
    return this;
}

int CaseStatement::compare(Object *obj)
{
    // Sort cases so we can do an efficient lookup
    CaseStatement *cs2 = (CaseStatement *)(obj);

    return exp->compare(cs2->exp);
}

int CaseStatement::usesEH()
{
    return statement->usesEH();
}

int CaseStatement::blockExit(bool mustNotThrow)
{
    return statement->blockExit(mustNotThrow);
}


int CaseStatement::comeFrom()
{
    return TRUE;
}

void CaseStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("case ");
    exp->toCBuffer(buf, hgs);
    buf->writebyte(':');
    buf->writenl();
    statement->toCBuffer(buf, hgs);
}

/******************************** CaseRangeStatement ***************************/

#if DMDV2

CaseRangeStatement::CaseRangeStatement(Loc loc, Expression *first,
        Expression *last, Statement *s)
    : Statement(loc)
{
    this->first = first;
    this->last = last;
    this->statement = s;
}

Statement *CaseRangeStatement::syntaxCopy()
{
    CaseRangeStatement *s = new CaseRangeStatement(loc,
        first->syntaxCopy(), last->syntaxCopy(), statement->syntaxCopy());
    return s;
}

Statement *CaseRangeStatement::semantic(Scope *sc)
{   SwitchStatement *sw = sc->sw;

    //printf("CaseRangeStatement::semantic() %s\n", toChars());
    if (sw->isFinal)
        error("case ranges not allowed in final switch");

    first = first->semantic(sc);
    first = first->implicitCastTo(sc, sw->condition->type);
    first = first->optimize(WANTvalue | WANTinterpret);


    last = last->semantic(sc);
    last = last->implicitCastTo(sc, sw->condition->type);
    last = last->optimize(WANTvalue | WANTinterpret);

    if (first->op == TOKerror || last->op == TOKerror)
        return statement ? statement->semantic(sc) : NULL;

    uinteger_t fval = first->toInteger();
    uinteger_t lval = last->toInteger();


    if ( (first->type->isunsigned()  &&  fval > lval) ||
        (!first->type->isunsigned()  &&  (sinteger_t)fval > (sinteger_t)lval))
    {
        error("first case %s is greater than last case %s",
            first->toChars(), last->toChars());
        lval = fval;
    }

    if (lval - fval > 256)
    {   error("had %llu cases which is more than 256 cases in case range", lval - fval);
        lval = fval + 256;
    }

    /* This works by replacing the CaseRange with an array of Case's.
     *
     * case a: .. case b: s;
     *    =>
     * case a:
     *   [...]
     * case b:
     *   s;
     */

    Statements *statements = new Statements();
    for (uinteger_t i = fval; i != lval + 1; i++)
    {
        Statement *s = statement;
        if (i != lval)                          // if not last case
            s = new ExpStatement(loc, (Expression *)NULL);
        Expression *e = new IntegerExp(loc, i, first->type);
        Statement *cs = new CaseStatement(loc, e, s);
        statements->push(cs);
    }
    Statement *s = new CompoundStatement(loc, statements);
    s = s->semantic(sc);
    return s;
}

void CaseRangeStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("case ");
    first->toCBuffer(buf, hgs);
    buf->writestring(": .. case ");
    last->toCBuffer(buf, hgs);
    buf->writebyte(':');
    buf->writenl();
    statement->toCBuffer(buf, hgs);
}

#endif

/******************************** DefaultStatement ***************************/

DefaultStatement::DefaultStatement(Loc loc, Statement *s)
    : Statement(loc)
{
    this->statement = s;
#if IN_GCC
    cblock = NULL;
#endif
}

Statement *DefaultStatement::syntaxCopy()
{
    DefaultStatement *s = new DefaultStatement(loc, statement->syntaxCopy());
    return s;
}

Statement *DefaultStatement::semantic(Scope *sc)
{
    //printf("DefaultStatement::semantic()\n");
    if (sc->sw)
    {
        if (sc->sw->sdefault)
        {
            error("switch statement already has a default");
        }
        sc->sw->sdefault = this;

        if (sc->sw->tf != sc->tf)
            error("switch and default are in different finally blocks");

        if (sc->sw->isFinal)
            error("default statement not allowed in final switch statement");
    }
    else
        error("default not in switch statement");
    statement = statement->semantic(sc);
    return this;
}

int DefaultStatement::usesEH()
{
    return statement->usesEH();
}

int DefaultStatement::blockExit(bool mustNotThrow)
{
    return statement->blockExit(mustNotThrow);
}


int DefaultStatement::comeFrom()
{
    return TRUE;
}

void DefaultStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("default:\n");
    statement->toCBuffer(buf, hgs);
}

/******************************** GotoDefaultStatement ***************************/

GotoDefaultStatement::GotoDefaultStatement(Loc loc)
    : Statement(loc)
{
    sw = NULL;
}

Statement *GotoDefaultStatement::syntaxCopy()
{
    GotoDefaultStatement *s = new GotoDefaultStatement(loc);
    return s;
}

Statement *GotoDefaultStatement::semantic(Scope *sc)
{
    sw = sc->sw;
    if (!sw)
        error("goto default not in switch statement");
    return this;
}

int GotoDefaultStatement::blockExit(bool mustNotThrow)
{
    return BEgoto;
}


void GotoDefaultStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("goto default;\n");
}

/******************************** GotoCaseStatement ***************************/

GotoCaseStatement::GotoCaseStatement(Loc loc, Expression *exp)
    : Statement(loc)
{
    cs = NULL;
    this->exp = exp;
}

Statement *GotoCaseStatement::syntaxCopy()
{
    Expression *e = exp ? exp->syntaxCopy() : NULL;
    GotoCaseStatement *s = new GotoCaseStatement(loc, e);
    return s;
}

Statement *GotoCaseStatement::semantic(Scope *sc)
{
    if (exp)
        exp = exp->semantic(sc);

    if (!sc->sw)
        error("goto case not in switch statement");
    else
    {
        sc->sw->gotoCases.push(this);
        if (exp)
        {
            exp = exp->implicitCastTo(sc, sc->sw->condition->type);
            exp = exp->optimize(WANTvalue);
        }
    }
    return this;
}

int GotoCaseStatement::blockExit(bool mustNotThrow)
{
    return BEgoto;
}


void GotoCaseStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("goto case");
    if (exp)
    {   buf->writebyte(' ');
        exp->toCBuffer(buf, hgs);
    }
    buf->writebyte(';');
    buf->writenl();
}

/******************************** SwitchErrorStatement ***************************/

SwitchErrorStatement::SwitchErrorStatement(Loc loc)
    : Statement(loc)
{
}

int SwitchErrorStatement::blockExit(bool mustNotThrow)
{
    // Switch errors are non-recoverable
    return BEhalt;
}


void SwitchErrorStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("SwitchErrorStatement::toCBuffer()");
    buf->writenl();
}

/******************************** ReturnStatement ***************************/

ReturnStatement::ReturnStatement(Loc loc, Expression *exp)
    : Statement(loc)
{
    this->exp = exp;
}

Statement *ReturnStatement::syntaxCopy()
{
    Expression *e = NULL;
    if (exp)
        e = exp->syntaxCopy();
    ReturnStatement *s = new ReturnStatement(loc, e);
    return s;
}

Statement *ReturnStatement::semantic(Scope *sc)
{
    //printf("ReturnStatement::semantic() %s\n", toChars());

    FuncDeclaration *fd = sc->parent->isFuncDeclaration();
    Scope *scx = sc;
    int implicit0 = 0;
    Expression *eorg = NULL;

    if (fd->fes)
        fd = fd->fes->func;             // fd is now function enclosing foreach

    Type *tret = fd->type->nextOf();
    if (fd->tintro)
        /* We'll be implicitly casting the return expression to tintro
         */
        tret = fd->tintro->nextOf();
    Type *tbret = NULL;

    if (tret)
        tbret = tret->toBasetype();

    // main() returns 0, even if it returns void
    if (!exp && (!tbret || tbret->ty == Tvoid) && fd->isMain())
    {   implicit0 = 1;
        exp = new IntegerExp(0);
    }

    if (sc->incontract || scx->incontract)
        error("return statements cannot be in contracts");
    if (sc->tf || scx->tf)
        error("return statements cannot be in finally, scope(exit) or scope(success) bodies");

    if (fd->isCtorDeclaration())
    {
        // Constructors implicitly do:
        //      return this;
        if (exp && exp->op != TOKthis)
            error("cannot return expression from constructor");
        exp = new ThisExp(0);
        exp->type = tret;
    }

    if (!exp)
        fd->nrvo_can = 0;

    if (exp)
    {
        fd->hasReturnExp |= 1;

        if (exp->op == TOKfunction && tbret)
            ((FuncExp *)exp)->setType(tbret);

        exp = exp->semantic(sc);
        exp = resolveProperties(sc, exp);
        if (!((TypeFunction *)fd->type)->isref)
            exp = exp->optimize(WANTvalue);

        if (fd->nrvo_can && exp->op == TOKvar)
        {   VarExp *ve = (VarExp *)exp;
            VarDeclaration *v = ve->var->isVarDeclaration();

            if (((TypeFunction *)fd->type)->isref)
                // Function returns a reference
                fd->nrvo_can = 0;
            else if (!v || v->isOut() || v->isRef())
                fd->nrvo_can = 0;
//            else if (tbret->ty == Tstruct && ((TypeStruct *)tbret)->sym->dtor)
//                // Struct being returned has destructors
//                fd->nrvo_can = 0;
            else if (fd->nrvo_var == NULL)
            {   if (!v->isDataseg() && !v->isParameter() && v->toParent2() == fd)
                {   //printf("Setting nrvo to %s\n", v->toChars());
                    fd->nrvo_var = v;
                }
                else
                    fd->nrvo_can = 0;
            }
            else if (fd->nrvo_var != v)
                fd->nrvo_can = 0;
        }
        else
            fd->nrvo_can = 0;

        if (fd->inferRetType)
        {   TypeFunction *tf = (TypeFunction *)fd->type;
            assert(tf->ty == Tfunction);
            Type *tfret = tf->nextOf();
            if (tfret)
            {
                if (tfret != Type::terror)
                {
                    if (!exp->type->equals(tfret))
                    {
                        int m1 = exp->type->implicitConvTo(tfret);
                        int m2 = tfret->implicitConvTo(exp->type);
                        //printf("exp->type = %s m2<-->m1 tret %s\n", exp->type->toChars(), tfret->toChars());
                        //printf("m1 = %d, m2 = %d\n", m1, m2);

                        if (m1 && m2)
                            ;
                        else if (!m1 && m2)
                            tf->next = exp->type;
                        else if (m1 && !m2)
                            ;
                        else
                            error("mismatched function return type inference of %s and %s",
                                exp->type->toChars(), tfret->toChars());
                    }
                }

                /* The "refness" is determined by the first return statement,
                 * not all of them. This means:
                 *    return 3; return x;  // ok, x can be a value
                 *    return x; return 3;  // error, 3 is not an lvalue
                 */
            }
            else
            {
                if (tf->isref)
                {   /* Determine "refness" of function return:
                     * if it's an lvalue, return by ref, else return by value
                     */
                    if (exp->isLvalue())
                    {
                        /* Return by ref
                         * (but first ensure it doesn't fail the "check for
                         * escaping reference" test)
                         */
                        unsigned errors = global.startGagging();
                        exp->checkEscapeRef();
                        if (global.endGagging(errors))
                        {   tf->isref = FALSE;  // return by value
                        }
                    }
                    else
                        tf->isref = FALSE;      // return by value
                }
                tf->next = exp->type;
                //fd->type = tf->semantic(loc, sc);     // Removed with 6902
                if (!fd->tintro)
                {   tret = fd->type->nextOf();
                    tbret = tret->toBasetype();
                }
            }
            if (fd->returnLabel)
                eorg = exp;
        }
        else if (tbret->ty != Tvoid)
        {
            if (fd->isPureBypassingInference() == PUREstrong &&
                !exp->type->implicitConvTo(tret) &&
                exp->type->invariantOf()->implicitConvTo(tret))
            {
                exp = exp->castTo(sc, exp->type->invariantOf());
            }
            if (fd->tintro)
                exp = exp->implicitCastTo(sc, fd->type->nextOf());

            // eorg isn't casted to tret (== fd->tintro->nextOf())
            if (fd->returnLabel)
                eorg = exp->copy();
            exp = exp->implicitCastTo(sc, tret);

            if (!((TypeFunction *)fd->type)->isref)
                exp = exp->optimize(WANTvalue);
        }
    }
    else if (fd->inferRetType)
    {
        if (fd->type->nextOf())
        {
            if (fd->type->nextOf()->ty != Tvoid)
                error("mismatched function return type inference of void and %s",
                    fd->type->nextOf()->toChars());
        }
        else
        {
            ((TypeFunction *)fd->type)->next = Type::tvoid;
            //fd->type = fd->type->semantic(loc, sc);   // Remove with7321, same as 6902
            if (!fd->tintro)
            {   tret = Type::tvoid;
                tbret = tret;
            }
        }
    }
    else if (tbret->ty != Tvoid)        // if non-void return
        error("return expression expected");

    if (sc->fes)
    {
        Statement *s;

        if (exp && !implicit0)
        {
            exp = exp->implicitCastTo(sc, tret);
        }
        if (!exp || exp->op == TOKint64 || exp->op == TOKfloat64 ||
            exp->op == TOKimaginary80 || exp->op == TOKcomplex80 ||
            exp->op == TOKthis || exp->op == TOKsuper || exp->op == TOKnull ||
            exp->op == TOKstring)
        {
            sc->fes->cases->push(this);
            // Construct: return cases->dim+1;
            s = new ReturnStatement(0, new IntegerExp(sc->fes->cases->dim + 1));
        }
        else if (fd->type->nextOf()->toBasetype() == Type::tvoid)
        {
            s = new ReturnStatement(0, NULL);
            sc->fes->cases->push(s);

            // Construct: { exp; return cases->dim + 1; }
            Statement *s1 = new ExpStatement(loc, exp);
            Statement *s2 = new ReturnStatement(0, new IntegerExp(sc->fes->cases->dim + 1));
            s = new CompoundStatement(loc, s1, s2);
        }
        else
        {
            // Construct: return vresult;
            if (!fd->vresult)
            {   // Declare vresult
                VarDeclaration *v = new VarDeclaration(loc, tret, Id::result, NULL);
                v->noscope = 1;
                v->storage_class |= STCresult;
                v->semantic(scx);
                if (!scx->insert(v))
                    assert(0);
                v->parent = fd;
                fd->vresult = v;
            }

            s = new ReturnStatement(0, new VarExp(0, fd->vresult));
            sc->fes->cases->push(s);

            // Construct: { vresult = exp; return cases->dim + 1; }
            exp = new ConstructExp(loc, new VarExp(0, fd->vresult), exp);
            exp = exp->semantic(sc);
            Statement *s1 = new ExpStatement(loc, exp);
            Statement *s2 = new ReturnStatement(0, new IntegerExp(sc->fes->cases->dim + 1));
            s = new CompoundStatement(loc, s1, s2);
        }
        return s;
    }

    if (exp)
    {
        if (((TypeFunction *)fd->type)->isref && !fd->isCtorDeclaration())
        {   // Function returns a reference
            if (tret->isMutable())
                exp = exp->modifiableLvalue(sc, exp);
            else
                exp = exp->toLvalue(sc, exp);

            exp->checkEscapeRef();
        }
        else
        {
            //exp->dump(0);
            //exp->print();
            exp->checkEscape();
        }

        if (fd->returnLabel && tbret->ty != Tvoid)
        {
            assert(fd->vresult);
            VarExp *v = new VarExp(0, fd->vresult);

            assert(eorg);
            exp = new ConstructExp(loc, v, eorg);
            exp = exp->semantic(sc);
        }
    }

    /* BUG: need to issue an error on:
     *  this
     *  {   if (x) return;
     *      super();
     *  }
     */

    if (sc->callSuper & CSXany_ctor &&
        !(sc->callSuper & (CSXthis_ctor | CSXsuper_ctor)))
        error("return without calling constructor");

    sc->callSuper |= CSXreturn;

    // See if all returns are instead to be replaced with a goto returnLabel;
    if (fd->returnLabel)
    {
        GotoStatement *gs = new GotoStatement(loc, Id::returnLabel);

        gs->label = fd->returnLabel;
        if (exp)
        {   /* Replace: return exp;
             * with:    exp; goto returnLabel;
             */
            Statement *s = new ExpStatement(0, exp);
            return new CompoundStatement(loc, s, gs);
        }
        return gs;
    }

    if (exp && tbret->ty == Tvoid && !implicit0)
    {
        /* Replace:
         *      return exp;
         * with:
         *      exp; return;
         */
        Statement *s = new ExpStatement(loc, exp);
        s = s->semantic(sc);

        if (exp->type->ty != Tvoid)
        {
            error("cannot return non-void from void function");
        }

        exp = NULL;
        return new CompoundStatement(loc, s, this);
    }

    if (exp)
    {   if (exp->op == TOKcall)
            valueNoDtor(exp);
        else
        {
            Expression *e = exp->isTemp();
            if (e)
                exp = e;                // don't need temporary
        }
    }

    return this;
}

int ReturnStatement::blockExit(bool mustNotThrow)
{   int result = BEreturn;

    if (exp && exp->canThrow(mustNotThrow))
        result |= BEthrow;
    return result;
}


void ReturnStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->printf("return ");
    if (exp)
        exp->toCBuffer(buf, hgs);
    buf->writeByte(';');
    buf->writenl();
}

/******************************** BreakStatement ***************************/

BreakStatement::BreakStatement(Loc loc, Identifier *ident)
    : Statement(loc)
{
    this->ident = ident;
}

Statement *BreakStatement::syntaxCopy()
{
    BreakStatement *s = new BreakStatement(loc, ident);
    return s;
}

Statement *BreakStatement::semantic(Scope *sc)
{
    //printf("BreakStatement::semantic()\n");
    // If:
    //  break Identifier;
    if (ident)
    {
        Scope *scx;
        FuncDeclaration *thisfunc = sc->func;

        for (scx = sc; scx; scx = scx->enclosing)
        {
            LabelStatement *ls;

            if (scx->func != thisfunc)  // if in enclosing function
            {
                if (sc->fes)            // if this is the body of a foreach
                {
                    /* Post this statement to the fes, and replace
                     * it with a return value that caller will put into
                     * a switch. Caller will figure out where the break
                     * label actually is.
                     * Case numbers start with 2, not 0, as 0 is continue
                     * and 1 is break.
                     */
                    Statement *s;
                    sc->fes->cases->push(this);
                    s = new ReturnStatement(0, new IntegerExp(sc->fes->cases->dim + 1));
                    return s;
                }
                break;                  // can't break to it
            }

            ls = scx->slabel;
            if (ls && ls->ident == ident)
            {
                Statement *s = ls->statement;

                if (!s->hasBreak())
                    error("label '%s' has no break", ident->toChars());
                if (ls->tf != sc->tf)
                    error("cannot break out of finally block");
                return this;
            }
        }
        error("enclosing label '%s' for break not found", ident->toChars());
    }
    else if (!sc->sbreak)
    {
        if (sc->fes)
        {   Statement *s;

            // Replace break; with return 1;
            s = new ReturnStatement(0, new IntegerExp(1));
            return s;
        }
        error("break is not inside a loop or switch");
    }
    return this;
}

int BreakStatement::blockExit(bool mustNotThrow)
{
    //printf("BreakStatement::blockExit(%p) = x%x\n", this, ident ? BEgoto : BEbreak);
    return ident ? BEgoto : BEbreak;
}


void BreakStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("break");
    if (ident)
    {   buf->writebyte(' ');
        buf->writestring(ident->toChars());
    }
    buf->writebyte(';');
    buf->writenl();
}

/******************************** ContinueStatement ***************************/

ContinueStatement::ContinueStatement(Loc loc, Identifier *ident)
    : Statement(loc)
{
    this->ident = ident;
}

Statement *ContinueStatement::syntaxCopy()
{
    ContinueStatement *s = new ContinueStatement(loc, ident);
    return s;
}

Statement *ContinueStatement::semantic(Scope *sc)
{
    //printf("ContinueStatement::semantic() %p\n", this);
    if (ident)
    {
        Scope *scx;
        FuncDeclaration *thisfunc = sc->func;

        for (scx = sc; scx; scx = scx->enclosing)
        {
            LabelStatement *ls;

            if (scx->func != thisfunc)  // if in enclosing function
            {
                if (sc->fes)            // if this is the body of a foreach
                {
                    for (; scx; scx = scx->enclosing)
                    {
                        ls = scx->slabel;
                        if (ls && ls->ident == ident && ls->statement == sc->fes)
                        {
                            // Replace continue ident; with return 0;
                            return new ReturnStatement(0, new IntegerExp(0));
                        }
                    }

                    /* Post this statement to the fes, and replace
                     * it with a return value that caller will put into
                     * a switch. Caller will figure out where the break
                     * label actually is.
                     * Case numbers start with 2, not 0, as 0 is continue
                     * and 1 is break.
                     */
                    Statement *s;
                    sc->fes->cases->push(this);
                    s = new ReturnStatement(0, new IntegerExp(sc->fes->cases->dim + 1));
                    return s;
                }
                break;                  // can't continue to it
            }

            ls = scx->slabel;
            if (ls && ls->ident == ident)
            {
                Statement *s = ls->statement;

                if (!s->hasContinue())
                    error("label '%s' has no continue", ident->toChars());
                if (ls->tf != sc->tf)
                    error("cannot continue out of finally block");
                return this;
            }
        }
        error("enclosing label '%s' for continue not found", ident->toChars());
    }
    else if (!sc->scontinue)
    {
        if (sc->fes)
        {   Statement *s;

            // Replace continue; with return 0;
            s = new ReturnStatement(0, new IntegerExp(0));
            return s;
        }
        error("continue is not inside a loop");
    }
    return this;
}

int ContinueStatement::blockExit(bool mustNotThrow)
{
    return ident ? BEgoto : BEcontinue;
}


void ContinueStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("continue");
    if (ident)
    {   buf->writebyte(' ');
        buf->writestring(ident->toChars());
    }
    buf->writebyte(';');
    buf->writenl();
}

/******************************** SynchronizedStatement ***************************/

SynchronizedStatement::SynchronizedStatement(Loc loc, Expression *exp, Statement *body)
    : Statement(loc)
{
    this->exp = exp;
    this->body = body;
    this->esync = NULL;
}

SynchronizedStatement::SynchronizedStatement(Loc loc, elem *esync, Statement *body)
    : Statement(loc)
{
    this->exp = NULL;
    this->body = body;
    this->esync = esync;
}

Statement *SynchronizedStatement::syntaxCopy()
{
    Expression *e = exp ? exp->syntaxCopy() : NULL;
    SynchronizedStatement *s = new SynchronizedStatement(loc, e, body ? body->syntaxCopy() : NULL);
    return s;
}

Statement *SynchronizedStatement::semantic(Scope *sc)
{
    if (exp)
    {
        exp = exp->semantic(sc);
        exp = resolveProperties(sc, exp);
        if (exp->op == TOKerror)
            goto Lbody;
        ClassDeclaration *cd = exp->type->isClassHandle();
        if (!cd)
            error("can only synchronize on class objects, not '%s'", exp->type->toChars());
        else if (cd->isInterfaceDeclaration())
        {   /* Cast the interface to an object, as the object has the monitor,
             * not the interface.
             */
            Type *t = new TypeIdentifier(0, Id::Object);

            t = t->semantic(0, sc);
            exp = new CastExp(loc, exp, t);
            exp = exp->semantic(sc);
        }

#if 1
        /* Rewrite as:
         *  auto tmp = exp;
         *  _d_monitorenter(tmp);
         *  try { body } finally { _d_monitorexit(tmp); }
         */
        Identifier *id = Lexer::uniqueId("__sync");
        ExpInitializer *ie = new ExpInitializer(loc, exp);
        VarDeclaration *tmp = new VarDeclaration(loc, exp->type, id, ie);

        Statements *cs = new Statements();
        cs->push(new ExpStatement(loc, tmp));

        FuncDeclaration *fdenter = FuncDeclaration::genCfunc(Type::tvoid, Id::monitorenter);
        Expression *e = new CallExp(loc, new VarExp(loc, fdenter), new VarExp(loc, tmp));
        e->type = Type::tvoid;                  // do not run semantic on e
        cs->push(new ExpStatement(loc, e));

        FuncDeclaration *fdexit = FuncDeclaration::genCfunc(Type::tvoid, Id::monitorexit);
        e = new CallExp(loc, new VarExp(loc, fdexit), new VarExp(loc, tmp));
        e->type = Type::tvoid;                  // do not run semantic on e
        Statement *s = new ExpStatement(loc, e);
        s = new TryFinallyStatement(loc, body, s);
        cs->push(s);

        s = new CompoundStatement(loc, cs);
        return s->semantic(sc);
#endif
    }
#if 1
    else
    {   /* Generate our own critical section, then rewrite as:
         *  __gshared byte[CriticalSection.sizeof] critsec;
         *  _d_criticalenter(critsec.ptr);
         *  try { body } finally { _d_criticalexit(critsec.ptr); }
         */
        Identifier *id = Lexer::uniqueId("__critsec");
        Type *t = new TypeSArray(Type::tint8, new IntegerExp(PTRSIZE + (global.params.is64bit ? os_critsecsize64() : os_critsecsize32())));
        VarDeclaration *tmp = new VarDeclaration(loc, t, id, NULL);
        tmp->storage_class |= STCgshared | STCstatic;

        Statements *cs = new Statements();
        cs->push(new ExpStatement(loc, tmp));

        FuncDeclaration *fdenter = FuncDeclaration::genCfunc(Type::tvoid, Id::criticalenter);
        Expression *e = new DotIdExp(loc, new VarExp(loc, tmp), Id::ptr);
        e = e->semantic(sc);
        e = new CallExp(loc, new VarExp(loc, fdenter), e);
        e->type = Type::tvoid;                  // do not run semantic on e
        cs->push(new ExpStatement(loc, e));

        FuncDeclaration *fdexit = FuncDeclaration::genCfunc(Type::tvoid, Id::criticalexit);
        e = new DotIdExp(loc, new VarExp(loc, tmp), Id::ptr);
        e = e->semantic(sc);
        e = new CallExp(loc, new VarExp(loc, fdexit), e);
        e->type = Type::tvoid;                  // do not run semantic on e
        Statement *s = new ExpStatement(loc, e);
        s = new TryFinallyStatement(loc, body, s);
        cs->push(s);

        s = new CompoundStatement(loc, cs);
        return s->semantic(sc);
    }
#endif
Lbody:
    if (body)
        body = body->semantic(sc);
    return this;
}

int SynchronizedStatement::hasBreak()
{
    return FALSE; //TRUE;
}

int SynchronizedStatement::hasContinue()
{
    return FALSE; //TRUE;
}

int SynchronizedStatement::usesEH()
{
    return TRUE;
}

int SynchronizedStatement::blockExit(bool mustNotThrow)
{
    return body ? body->blockExit(mustNotThrow) : BEfallthru;
}


void SynchronizedStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("synchronized");
    if (exp)
    {   buf->writebyte('(');
        exp->toCBuffer(buf, hgs);
        buf->writebyte(')');
    }
    if (body)
    {
        buf->writebyte(' ');
        body->toCBuffer(buf, hgs);
    }
}

/******************************** WithStatement ***************************/

WithStatement::WithStatement(Loc loc, Expression *exp, Statement *body)
    : Statement(loc)
{
    this->exp = exp;
    this->body = body;
    wthis = NULL;
}

Statement *WithStatement::syntaxCopy()
{
    WithStatement *s = new WithStatement(loc, exp->syntaxCopy(), body ? body->syntaxCopy() : NULL);
    return s;
}

Statement *WithStatement::semantic(Scope *sc)
{   ScopeDsymbol *sym;
    Initializer *init;

    //printf("WithStatement::semantic()\n");
    exp = exp->semantic(sc);
    exp = resolveProperties(sc, exp);
    if (exp->op == TOKerror)
        return NULL;
    if (exp->op == TOKimport)
    {   ScopeExp *es = (ScopeExp *)exp;

        sym = es->sds;
    }
    else if (exp->op == TOKtype)
    {   TypeExp *es = (TypeExp *)exp;

        Dsymbol *s = es->type->toDsymbol(sc);
        sym = s ? s->isScopeDsymbol() : NULL;
        if (!sym)
        {   error("with type %s has no members", es->toChars());
            if (body)
                body = body->semantic(sc);
            return this;
        }
    }
    else
    {   Type *t = exp->type;

        assert(t);
        t = t->toBasetype();
        if (t->isClassHandle())
        {
            init = new ExpInitializer(loc, exp);
            wthis = new VarDeclaration(loc, exp->type, Id::withSym, init);
            wthis->semantic(sc);

            sym = new WithScopeSymbol(this);
            sym->parent = sc->scopesym;
        }
        else if (t->ty == Tstruct)
        {
            if (!exp->isLvalue())
            {
                init = new ExpInitializer(loc, exp);
                wthis = new VarDeclaration(loc, exp->type, Lexer::uniqueId("__withtmp"), init);
                exp = new CommaExp(loc, new DeclarationExp(loc, wthis), new VarExp(loc, wthis));
                exp = exp->semantic(sc);
            }
            Expression *e = exp->addressOf(sc);
            init = new ExpInitializer(loc, e);
            wthis = new VarDeclaration(loc, e->type, Id::withSym, init);
            wthis->semantic(sc);
            sym = new WithScopeSymbol(this);
            sym->parent = sc->scopesym;
        }
        else
        {   error("with expressions must be class objects, not '%s'", exp->type->toChars());
            return NULL;
        }
    }
    sc = sc->push(sym);

    if (body)
        body = body->semantic(sc);

    sc->pop();

    return this;
}

void WithStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("with (");
    exp->toCBuffer(buf, hgs);
    buf->writestring(")\n");
    if (body)
        body->toCBuffer(buf, hgs);
}

int WithStatement::usesEH()
{
    return body ? body->usesEH() : 0;
}

int WithStatement::blockExit(bool mustNotThrow)
{
    int result = BEnone;
    if (exp->canThrow(mustNotThrow))
        result = BEthrow;
    if (body)
        result |= body->blockExit(mustNotThrow);
    else
        result |= BEfallthru;
    return result;
}


/******************************** TryCatchStatement ***************************/

TryCatchStatement::TryCatchStatement(Loc loc, Statement *body, Catches *catches)
    : Statement(loc)
{
    this->body = body;
    this->catches = catches;
}

Statement *TryCatchStatement::syntaxCopy()
{
    Catches *a = new Catches();
    a->setDim(catches->dim);
    for (size_t i = 0; i < a->dim; i++)
    {   Catch *c;

        c = (*catches)[i];
        c = c->syntaxCopy();
        (*a)[i] = c;
    }
    TryCatchStatement *s = new TryCatchStatement(loc, body->syntaxCopy(), a);
    return s;
}

Statement *TryCatchStatement::semantic(Scope *sc)
{
    body = body->semanticScope(sc, NULL /*this*/, NULL);

    /* Even if body is NULL, still do semantic analysis on catches
     */
    for (size_t i = 0; i < catches->dim; i++)
    {   Catch *c = (*catches)[i];
        c->semantic(sc);

        // Determine if current catch 'hides' any previous catches
        for (size_t j = 0; j < i; j++)
        {   Catch *cj = (*catches)[j];
            char *si = c->loc.toChars();
            char *sj = cj->loc.toChars();

            if (c->type->toBasetype()->implicitConvTo(cj->type->toBasetype()))
                error("catch at %s hides catch at %s", sj, si);
        }
    }

    if (!body || body->isEmpty())
    {
        return NULL;
    }
    return this;
}

int TryCatchStatement::hasBreak()
{
    return FALSE; //TRUE;
}

int TryCatchStatement::usesEH()
{
    return TRUE;
}

int TryCatchStatement::blockExit(bool mustNotThrow)
{
    assert(body);
    int result = body->blockExit(false);

    int catchresult = 0;
    for (size_t i = 0; i < catches->dim; i++)
    {
        Catch *c = (*catches)[i];
        if (c->type == Type::terror)
            continue;

        catchresult |= c->blockExit(mustNotThrow);

        /* If we're catching Object, then there is no throwing
         */
        Identifier *id = c->type->toBasetype()->isClassHandle()->ident;
        if (id == Id::Object || id == Id::Throwable || id == Id::Exception)
        {
            result &= ~BEthrow;
        }
    }
    if (mustNotThrow && (result & BEthrow))
    {
        body->blockExit(mustNotThrow); // now explain why this is nothrow
    }
    return result | catchresult;
}


void TryCatchStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("try");
    buf->writenl();
    if (body)
        body->toCBuffer(buf, hgs);
    for (size_t i = 0; i < catches->dim; i++)
    {
        Catch *c = (*catches)[i];
        c->toCBuffer(buf, hgs);
    }
}

/******************************** Catch ***************************/

Catch::Catch(Loc loc, Type *t, Identifier *id, Statement *handler)
{
    //printf("Catch(%s, loc = %s)\n", id->toChars(), loc.toChars());
    this->loc = loc;
    this->type = t;
    this->ident = id;
    this->handler = handler;
    var = NULL;
    internalCatch = false;
}

Catch *Catch::syntaxCopy()
{
    Catch *c = new Catch(loc,
        (type ? type->syntaxCopy() : NULL),
        ident,
        (handler ? handler->syntaxCopy() : NULL));
    c->internalCatch = internalCatch;
    return c;
}

void Catch::semantic(Scope *sc)
{   ScopeDsymbol *sym;

    //printf("Catch::semantic(%s)\n", ident->toChars());

#ifndef IN_GCC
    if (sc->tf)
    {
        /* This is because the _d_local_unwind() gets the stack munged
         * up on this. The workaround is to place any try-catches into
         * a separate function, and call that.
         * To fix, have the compiler automatically convert the finally
         * body into a nested function.
         */
        error(loc, "cannot put catch statement inside finally block");
    }
#endif

    sym = new ScopeDsymbol();
    sym->parent = sc->scopesym;
    sc = sc->push(sym);

    if (!type)
        type = new TypeIdentifier(0, Id::Throwable);
    type = type->semantic(loc, sc);
    ClassDeclaration *cd = type->toBasetype()->isClassHandle();
    if (!cd || ((cd != ClassDeclaration::throwable) && !ClassDeclaration::throwable->isBaseOf(cd, NULL)))
    {
        if (type != Type::terror)
        {   error(loc, "can only catch class objects derived from Throwable, not '%s'", type->toChars());
            type = Type::terror;
        }
    }
    else if (sc->func &&
        !sc->intypeof &&
        !internalCatch &&
        cd != ClassDeclaration::exception &&
        !ClassDeclaration::exception->isBaseOf(cd, NULL) &&
        sc->func->setUnsafe())
    {
        error(loc, "can only catch class objects derived from Exception in @safe code, not '%s'", type->toChars());
        type = Type::terror;
    }
    else if (ident)
    {
        var = new VarDeclaration(loc, type, ident, NULL);
        var->parent = sc->parent;
        sc->insert(var);
    }
    handler = handler->semantic(sc);

    sc->pop();
}

int Catch::blockExit(bool mustNotThrow)
{
    return handler ? handler->blockExit(mustNotThrow) : BEfallthru;
}

void Catch::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("catch");
    if (type)
    {   buf->writebyte('(');
        type->toCBuffer(buf, ident, hgs);
        buf->writebyte(')');
    }
    buf->writenl();
    buf->writebyte('{');
    buf->writenl();
    if (handler)
        handler->toCBuffer(buf, hgs);
    buf->writebyte('}');
    buf->writenl();
}

/****************************** TryFinallyStatement ***************************/

TryFinallyStatement::TryFinallyStatement(Loc loc, Statement *body, Statement *finalbody)
    : Statement(loc)
{
    this->body = body;
    this->finalbody = finalbody;
}

Statement *TryFinallyStatement::syntaxCopy()
{
    TryFinallyStatement *s = new TryFinallyStatement(loc,
        body->syntaxCopy(), finalbody->syntaxCopy());
    return s;
}

Statement *TryFinallyStatement::semantic(Scope *sc)
{
    //printf("TryFinallyStatement::semantic()\n");
    body = body->semantic(sc);
    sc = sc->push();
    sc->tf = this;
    sc->sbreak = NULL;
    sc->scontinue = NULL;       // no break or continue out of finally block
    finalbody = finalbody->semanticNoScope(sc);
    sc->pop();
    if (!body)
        return finalbody;
    if (!finalbody)
        return body;
    if (body->blockExit(false) == BEfallthru)
    {   Statement *s = new CompoundStatement(loc, body, finalbody);
        return s;
    }
    return this;
}

void TryFinallyStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->printf("try\n{\n");
    body->toCBuffer(buf, hgs);
    buf->printf("}\nfinally\n{\n");
    finalbody->toCBuffer(buf, hgs);
    buf->writeByte('}');
    buf->writenl();
}

int TryFinallyStatement::hasBreak()
{
    return FALSE; //TRUE;
}

int TryFinallyStatement::hasContinue()
{
    return FALSE; //TRUE;
}

int TryFinallyStatement::usesEH()
{
    return TRUE;
}

int TryFinallyStatement::blockExit(bool mustNotThrow)
{
    if (body)
        return body->blockExit(mustNotThrow);
    return BEfallthru;
}


/****************************** OnScopeStatement ***************************/

OnScopeStatement::OnScopeStatement(Loc loc, TOK tok, Statement *statement)
    : Statement(loc)
{
    this->tok = tok;
    this->statement = statement;
}

Statement *OnScopeStatement::syntaxCopy()
{
    OnScopeStatement *s = new OnScopeStatement(loc,
        tok, statement->syntaxCopy());
    return s;
}

Statement *OnScopeStatement::semantic(Scope *sc)
{
    /* semantic is called on results of scopeCode() */
    return this;
}

int OnScopeStatement::blockExit(bool mustNotThrow)
{   // At this point, this statement is just an empty placeholder
    return BEfallthru;
}

void OnScopeStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring(Token::toChars(tok));
    buf->writebyte(' ');
    statement->toCBuffer(buf, hgs);
}

int OnScopeStatement::usesEH()
{
    return 1;
}

Statement *OnScopeStatement::scopeCode(Scope *sc, Statement **sentry, Statement **sexception, Statement **sfinally)
{
    //printf("OnScopeStatement::scopeCode()\n");
    //print();
    *sentry = NULL;
    *sexception = NULL;
    *sfinally = NULL;
    switch (tok)
    {
        case TOKon_scope_exit:
            *sfinally = statement;
            break;

        case TOKon_scope_failure:
            *sexception = statement;
            break;

        case TOKon_scope_success:
        {
            /* Create:
             *  sentry:   bool x = false;
             *  sexception:    x = true;
             *  sfinally: if (!x) statement;
             */
            Identifier *id = Lexer::uniqueId("__os");

            ExpInitializer *ie = new ExpInitializer(loc, new IntegerExp(0, 0, Type::tbool));
            VarDeclaration *v = new VarDeclaration(loc, Type::tbool, id, ie);
            *sentry = new ExpStatement(loc, v);

            Expression *e = new IntegerExp(0, 1, Type::tbool);
            e = new AssignExp(0, new VarExp(0, v), e);
            *sexception = new ExpStatement(0, e);

            e = new VarExp(0, v);
            e = new NotExp(0, e);
            *sfinally = new IfStatement(0, NULL, e, statement, NULL);

            break;
        }

        default:
            assert(0);
    }
    return NULL;
}

/******************************** ThrowStatement ***************************/

ThrowStatement::ThrowStatement(Loc loc, Expression *exp)
    : Statement(loc)
{
    this->exp = exp;
}

Statement *ThrowStatement::syntaxCopy()
{
    ThrowStatement *s = new ThrowStatement(loc, exp->syntaxCopy());
    return s;
}

Statement *ThrowStatement::semantic(Scope *sc)
{
    //printf("ThrowStatement::semantic()\n");

    FuncDeclaration *fd = sc->parent->isFuncDeclaration();
    fd->hasReturnExp |= 2;

#if DMDV1
    // See bugzilla 3388. Should this be or not?
    if (sc->incontract)
        error("Throw statements cannot be in contracts");
#endif
    exp = exp->semantic(sc);
    exp = resolveProperties(sc, exp);
    if (exp->op == TOKerror)
        return this;
    ClassDeclaration *cd = exp->type->toBasetype()->isClassHandle();
    if (!cd || ((cd != ClassDeclaration::throwable) && !ClassDeclaration::throwable->isBaseOf(cd, NULL)))
        error("can only throw class objects derived from Throwable, not type %s", exp->type->toChars());

    return this;
}

int ThrowStatement::blockExit(bool mustNotThrow)
{
    if (mustNotThrow)
        error("%s is thrown but not caught", exp->type->toChars());
    return BEthrow;  // obviously
}


void ThrowStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->printf("throw ");
    exp->toCBuffer(buf, hgs);
    buf->writeByte(';');
    buf->writenl();
}

/******************************** VolatileStatement **************************/

VolatileStatement::VolatileStatement(Loc loc, Statement *statement)
    : Statement(loc)
{
    this->statement = statement;
}

Statement *VolatileStatement::syntaxCopy()
{
    VolatileStatement *s = new VolatileStatement(loc,
                statement ? statement->syntaxCopy() : NULL);
    return s;
}

Statement *VolatileStatement::semantic(Scope *sc)
{
    if (statement)
        statement = statement->semantic(sc);
    return this;
}

Statements *VolatileStatement::flatten(Scope *sc)
{
    Statements *a;

    a = statement ? statement->flatten(sc) : NULL;
    if (a)
    {   for (size_t i = 0; i < a->dim; i++)
        {   Statement *s = (*a)[i];

            s = new VolatileStatement(loc, s);
            (*a)[i] = s;
        }
    }

    return a;
}

int VolatileStatement::blockExit(bool mustNotThrow)
{
    return statement ? statement->blockExit(mustNotThrow) : BEfallthru;
}


void VolatileStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("volatile");
    if (statement)
    {   if (statement->isScopeStatement())
            buf->writenl();
        else
            buf->writebyte(' ');
        statement->toCBuffer(buf, hgs);
    }
}


/******************************** DebugStatement **************************/

DebugStatement::DebugStatement(Loc loc, Statement *statement)
    : Statement(loc)
{
    this->statement = statement;
}

Statement *DebugStatement::syntaxCopy()
{
    DebugStatement *s = new DebugStatement(loc,
                statement ? statement->syntaxCopy() : NULL);
    return s;
}

Statement *DebugStatement::semantic(Scope *sc)
{
    if (statement)
    {
        sc = sc->push();
        sc->flags |= SCOPEdebug;
        statement = statement->semantic(sc);
        sc->pop();
    }
    return statement;
}

Statements *DebugStatement::flatten(Scope *sc)
{
    Statements *a = statement ? statement->flatten(sc) : NULL;
    if (a)
    {   for (size_t i = 0; i < a->dim; i++)
        {   Statement *s = (*a)[i];

            s = new DebugStatement(loc, s);
            (*a)[i] = s;
        }
    }

    return a;
}

void DebugStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    if (statement)
    {
        statement->toCBuffer(buf, hgs);
    }
}


/******************************** GotoStatement ***************************/

GotoStatement::GotoStatement(Loc loc, Identifier *ident)
    : Statement(loc)
{
    this->ident = ident;
    this->label = NULL;
    this->tf = NULL;
}

Statement *GotoStatement::syntaxCopy()
{
    GotoStatement *s = new GotoStatement(loc, ident);
    return s;
}

Statement *GotoStatement::semantic(Scope *sc)
{   FuncDeclaration *fd = sc->parent->isFuncDeclaration();

    //printf("GotoStatement::semantic()\n");
    tf = sc->tf;
    label = fd->searchLabel(ident);
    if (!label->statement && sc->fes)
    {
        /* Either the goto label is forward referenced or it
         * is in the function that the enclosing foreach is in.
         * Can't know yet, so wrap the goto in a compound statement
         * so we can patch it later, and add it to a 'look at this later'
         * list.
         */
        Statements *a = new Statements();
        CompoundStatement *s;

        a->push(this);
        s = new CompoundStatement(loc, a);
        sc->fes->gotos->push(s);         // 'look at this later' list
        return s;
    }
    if (label->statement && label->statement->tf != sc->tf)
        error("cannot goto in or out of finally block");
    return this;
}

int GotoStatement::blockExit(bool mustNotThrow)
{
    //printf("GotoStatement::blockExit(%p)\n", this);
    return BEgoto;
}


void GotoStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("goto ");
    buf->writestring(ident->toChars());
    buf->writebyte(';');
    buf->writenl();
}

/******************************** LabelStatement ***************************/

LabelStatement::LabelStatement(Loc loc, Identifier *ident, Statement *statement)
    : Statement(loc)
{
    this->ident = ident;
    this->statement = statement;
    this->tf = NULL;
    this->lblock = NULL;
    this->fwdrefs = NULL;
}

Statement *LabelStatement::syntaxCopy()
{
    LabelStatement *s = new LabelStatement(loc, ident, statement->syntaxCopy());
    return s;
}

Statement *LabelStatement::semantic(Scope *sc)
{   LabelDsymbol *ls;
    FuncDeclaration *fd = sc->parent->isFuncDeclaration();

    //printf("LabelStatement::semantic()\n");
    ls = fd->searchLabel(ident);
    if (ls->statement)
        error("Label '%s' already defined", ls->toChars());
    else
        ls->statement = this;
    tf = sc->tf;
    sc = sc->push();
    sc->scopesym = sc->enclosing->scopesym;
    sc->callSuper |= CSXlabel;
    sc->slabel = this;
    if (statement)
        statement = statement->semanticNoScope(sc);
    sc->pop();
    return this;
}

Statements *LabelStatement::flatten(Scope *sc)
{
    Statements *a = NULL;

    if (statement)
    {
        a = statement->flatten(sc);
        if (a)
        {
            if (!a->dim)
            {
                a->push(new ExpStatement(loc, (Expression *)NULL));
            }
            Statement *s = (*a)[0];

            s = new LabelStatement(loc, ident, s);
            (*a)[0] = s;
        }
    }

    return a;
}


int LabelStatement::usesEH()
{
    return statement ? statement->usesEH() : FALSE;
}

int LabelStatement::blockExit(bool mustNotThrow)
{
    //printf("LabelStatement::blockExit(%p)\n", this);
    return statement ? statement->blockExit(mustNotThrow) : BEfallthru;
}


int LabelStatement::comeFrom()
{
    //printf("LabelStatement::comeFrom()\n");
    return TRUE;
}

void LabelStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring(ident->toChars());
    buf->writebyte(':');
    buf->writenl();
    if (statement)
        statement->toCBuffer(buf, hgs);
}


/******************************** LabelDsymbol ***************************/

LabelDsymbol::LabelDsymbol(Identifier *ident)
        : Dsymbol(ident)
{
    statement = NULL;
#if IN_GCC
    asmLabelNum = 0;
#endif
}

LabelDsymbol *LabelDsymbol::isLabel()           // is this a LabelDsymbol()?
{
    return this;
}


/************************ AsmStatement ***************************************/

AsmStatement::AsmStatement(Loc loc, Token *tokens)
    : Statement(loc)
{
    this->tokens = tokens;
    asmcode = NULL;
    asmalign = 0;
    refparam = FALSE;
    naked = FALSE;
    regs = 0;
}

Statement *AsmStatement::syntaxCopy()
{
    return new AsmStatement(loc, tokens);
}



int AsmStatement::comeFrom()
{
    return TRUE;
}

int AsmStatement::blockExit(bool mustNotThrow)
{
    if (mustNotThrow)
        error("asm statements are assumed to throw", toChars());
    // Assume the worst
    return BEfallthru | BEthrow | BEreturn | BEgoto | BEhalt;
}

void AsmStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("asm { ");
    Token *t = tokens;
    while (t)
    {
        buf->writestring(t->toChars());
        if (t->next                         &&
           t->value != TOKmin               &&
           t->value != TOKcomma             &&
           t->next->value != TOKcomma       &&
           t->value != TOKlbracket          &&
           t->next->value != TOKlbracket    &&
           t->next->value != TOKrbracket    &&
           t->value != TOKlparen            &&
           t->next->value != TOKlparen      &&
           t->next->value != TOKrparen      &&
           t->value != TOKdot               &&
           t->next->value != TOKdot)
        {
            buf->writebyte(' ');
        }
        t = t->next;
    }
    buf->writestring("; }");
    buf->writenl();
}

/************************ ImportStatement ***************************************/

ImportStatement::ImportStatement(Loc loc, Dsymbols *imports)
    : Statement(loc)
{
    this->imports = imports;
}

Statement *ImportStatement::syntaxCopy()
{
    Dsymbols *m = new Dsymbols();
    m->setDim(imports->dim);
    for (size_t i = 0; i < imports->dim; i++)
    {   Dsymbol *s = (*imports)[i];
        (*m)[i] = s->syntaxCopy(NULL);
    }
    return new ImportStatement(loc, m);
}

Statement *ImportStatement::semantic(Scope *sc)
{
    for (size_t i = 0; i < imports->dim; i++)
    {   Dsymbol *s = (*imports)[i];
        s->semantic(sc);
        sc->insert(s);
    }
    return this;
}

int ImportStatement::blockExit(bool mustNotThrow)
{
    return BEfallthru;
}

int ImportStatement::isEmpty()
{
    return TRUE;
}

void ImportStatement::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    for (size_t i = 0; i < imports->dim; i++)
    {   Dsymbol *s = (*imports)[i];
        s->toCBuffer(buf, hgs);
    }
}
