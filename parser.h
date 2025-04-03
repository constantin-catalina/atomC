#pragma once

#include <stdbool.h>
#include "lexer.h"
#include "ad.h"

const char *tkCodeName(int code);
void tkerr(const char *fmt,...);
void parse(Token *tokens);
bool consume(int code);

bool unit();
bool structDef();
bool varDef();
bool typeBase(Type *t);
bool arrayDecl(Type *t);
bool fnDef(); 
bool fnParam();
bool stm();
bool stmCompound(bool newDomain);
bool expr();
bool exprAssign();
bool exprOr();
bool exprAnd();
bool exprEq();
bool exprRel();
bool exprAdd();
bool exprMul();
bool exprCast();
bool exprUnary();
bool exprPostfix();
bool exprPrimary();

bool exprOrPrim();
bool exprAndPrim();
bool exprEqPrim();
bool exprRelPrim();
bool exprAddPrim();
bool exprMulPrim();
bool exprPostfixPrim();