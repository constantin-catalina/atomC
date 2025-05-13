#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include "parser.h"
#include "utils.h"
#include "ad.h"
#include "at.h"
#include "gc.h"

#define DEBUG 0

#if DEBUG
#define DEBUG_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...)
#endif

#define CONSUME_DEBUG 0

#if CONSUME_DEBUG
#define CONSUME_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define CONSUME_PRINT(fmt, ...)
#endif

Token *iTk;			  // the iterator in the tokens list
Token *consumedTk;	  // the last consumed token
Symbol *owner = NULL; // the last consumed symbol

const char *tkCodeName(int code)
{
	switch (code)
	{
	case ID:
		return "ID";
	case TYPE_CHAR:
		return "TYPE_CHAR";
	case TYPE_DOUBLE:
		return "TYPE_DOUBLE";
	case ELSE:
		return "ELSE";
	case IF:
		return "IF";
	case TYPE_INT:
		return "TYPE_INT";
	case RETURN:
		return "RETURN";
	case STRUCT:
		return "STRUCT";
	case VOID:
		return "VOID";
	case WHILE:
		return "WHILE";

	case INT:
		return "INT";
	case DOUBLE:
		return "DOUBLE";
	case CHAR:
		return "CHAR";
	case STRING:
		return "STRING";

	case COMMA:
		return "COMMA";
	case SEMICOLON:
		return "SEMICOLON";
	case LPAR:
		return "LPAR";
	case RPAR:
		return "RPAR";
	case LBRACKET:
		return "LBRACKET";
	case RBRACKET:
		return "RBRACKET";
	case LACC:
		return "LACC";
	case RACC:
		return "RACC";
	case END:
		return "END";

	case ADD:
		return "ADD";
	case SUB:
		return "SUB";
	case MUL:
		return "MUL";
	case DIV:
		return "DIV";
	case DOT:
		return "DOT";
	case AND:
		return "AND";
	case OR:
		return "OR";
	case NOT:
		return "NOT";
	case ASSIGN:
		return "ASSIGN";
	case EQUAL:
		return "EQUAL";
	case NOTEQ:
		return "NOTEQ";
	case LESS:
		return "LESS";
	case LESSEQ:
		return "LESSEQ";
	case GREATER:
		return "GREATER";
	case GREATEREQ:
		return "GREATEREQ";

	default:
		return "UNKNOWN";
	}
}

void tkerr(const char *fmt, ...)
{
	fprintf(stderr, "error in line %d: ", iTk->line);
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
	fprintf(stderr, "\n");
	exit(EXIT_FAILURE);
}

bool consume(int code)
{
	CONSUME_PRINT("consume(%s)", tkCodeName(code));
	if (iTk->code == code)
	{
		consumedTk = iTk;
		iTk = iTk->next;
		CONSUME_PRINT(" => consumed\n");
		return true;
	}
	CONSUME_PRINT(" => found %s\n", tkCodeName(iTk->code));
	return false;
}

// unit: ( structDef | fnDef | varDef )* END
bool unit()
{
	DEBUG_PRINT("Parsing unit\n");
	for (;;)
	{
		if (structDef())
		{
		}
		else if (fnDef())
		{
		}
		else if (varDef())
		{
		}
		else
			break;
	}
	if (consume(END))
	{
		return true;
	}
	return false;
}

// structDef: STRUCT ID LACC varDef* RACC SEMICOLON
bool structDef()
{
	DEBUG_PRINT("Parsing structDef\n");
	Token *start = iTk;
	if (consume(STRUCT))
	{
		if (consume(ID))
		{
			Token *tkName = consumedTk;
			if (consume(LACC))
			{
				Symbol *s = findSymbolInDomain(symTable, tkName->value.text);
				if (s)
				{
					tkerr("symbol redefinition: %s", tkName->value.text);
				}
				s = addSymbolToDomain(symTable, newSymbol(tkName->value.text, SK_STRUCT));
				s->type.tb = TB_STRUCT;
				s->type.s = s;
				s->type.n = -1;
				pushDomain();
				owner = s;
				for (;;)
				{
					if (varDef())
						;
					else
						break;
				}
				if (consume(RACC))
				{
					if (consume(SEMICOLON))
					{
						owner = NULL;
						dropDomain();
						return true;
					}
					else
					{
						tkerr("missing ';'");
					}
				}
				else
				{
					tkerr("invalid expression between {...} or missing '}'");
				}
			}
			else if (varDef() || consume(RACC))
			{
				tkerr("missing '{' or invalid expression between {...}");
			}
		}
		else
		{
			tkerr("missing struct identifier");
		}
	}

	iTk = start;
	return false;
}

// varDef: typeBase ID arrayDecl? SEMICOLON
bool varDef()
{
	DEBUG_PRINT("Parsing varDef\n");
	Token *start = iTk;
	Type t;
	if (typeBase(&t))
	{
		if (consume(ID))
		{
			Token *tkName = consumedTk;
			if (arrayDecl(&t))
			{
				if (t.n == 0)
				{
					tkerr("a vector variable must have a specified dimension");
				}
			}
			if (consume(SEMICOLON))
			{
				Symbol *var = findSymbolInDomain(symTable, tkName->value.text);
				if (var)
					tkerr("symbol redefinition: %s", tkName->value.text);
				var = newSymbol(tkName->value.text, SK_VAR);
				var->type = t;
				var->owner = owner;
				addSymbolToDomain(symTable, var);
				if (owner)
				{
					switch (owner->kind)
					{
					case SK_FN:
						var->varIdx = symbolsLen(owner->fn.locals);
						addSymbolToList(&owner->fn.locals, dupSymbol(var));
						break;
					case SK_STRUCT:
						var->varIdx = typeSize(&owner->type);
						addSymbolToList(&owner->structMembers, dupSymbol(var));
						break;
					default:
						break;
					}
				}
				else
				{
					var->varMem = safeAlloc(typeSize(&t));
				}
				return true;
			}
			else
			{
				tkerr("missing ';'");
			}
		}
		else
		{
			tkerr("missing or invalid identifier");
		}
	}
	iTk = start;
	return false;
}

// typeBase: TYPE_INT | TYPE_DOUBLE | TYPE_CHAR | STRUCT ID
bool typeBase(Type *t)
{
	t->n = -1;
	DEBUG_PRINT("Parsing typeBase\n");
	Token *start = iTk;
	if (consume(TYPE_INT))
	{
		t->tb = TB_INT;
		return true;
	}
	if (consume(TYPE_DOUBLE))
	{
		t->tb = TB_DOUBLE;
		return true;
	}
	if (consume(TYPE_CHAR))
	{
		t->tb = TB_CHAR;
		return true;
	}
	if (consume(STRUCT))
	{
		if (consume(ID))
		{
			Token *tkName = consumedTk;
			t->tb = TB_STRUCT;
			t->s = findSymbol(tkName->value.text);
			if (!t->s)
			{
				tkerr("undefined struct type: %s", tkName->value.text);
			}
			return true;
		}
		else
		{
			tkerr("missing struct identifier");
		}
	}
	iTk = start;
	return false;
}

// arrayDecl: LBRACKET INT? RBRACKET
bool arrayDecl(Type *t)
{
	DEBUG_PRINT("Parsing arrayDecl\n");
	Token *start = iTk;
	if (consume(LBRACKET))
	{
		if (consume(INT))
		{
			Token *tkSize = consumedTk;
			t->n = tkSize->value.i;
		}
		else
		{
			t->n = 0; // array fara dimensiune: int v[]
		}
		if (consume(RBRACKET))
		{
			return true;
		}
		else
		{
			tkerr("invalid expression between [...] or missing ']'");
		}
	}
	iTk = start;
	return false;
}

// fnDef: (typeBase | VOID) ID LPAR (fnParam (COMMA fnParam)*)? RPAR stmCompound
bool fnDef()
{
	DEBUG_PRINT("Parsing fnDef\n");
	Token *start = iTk;
	Instr *startInstr = owner ? lastInstr(owner->fn.instr) : NULL;
	Type t;
	bool consumedVoidTk = false;
	if (typeBase(&t) || (consumedVoidTk = consume(VOID)))
	{
		if (consumedVoidTk)
		{
			t.tb = TB_VOID;
		}
		if (consume(ID))
		{
			Token *tkName = consumedTk;
			if (consume(LPAR))
			{
				Symbol *fn = findSymbolInDomain(symTable, tkName->value.text);
				if (fn)
					tkerr("symbol redefinition: %s", tkName->value.text);
				fn = newSymbol(tkName->value.text, SK_FN);
				fn->type = t;
				addSymbolToDomain(symTable, fn);
				owner = fn;
				pushDomain();
				if (fnParam())
				{
					while (consume(COMMA))
					{
						if (fnParam())
						{
						}
						else
						{
							tkerr("missing or invalid additional function parameter after ','");
						}
					}
				}
				if (consume(RPAR))
				{
					addInstr(&fn->fn.instr, OP_ENTER);
					if (stmCompound(false))
					{
						fn->fn.instr->arg.i = symbolsLen(fn->fn.locals);
						if (fn->type.tb == TB_VOID)
						{
							addInstrWithInt(&fn->fn.instr, OP_RET_VOID, symbolsLen(fn->fn.params));
						}
						dropDomain();
						owner = NULL;
						return true;
					}
					else
					{
						tkerr("missing function body");
					}
				}
				else
				{
					tkerr("invalid function parameters or missing ')'");
				}
			}
		}
		else
		{
			if (consume(LPAR))
			{
				tkerr("missing function identifier");
			}
		}
	}
	iTk = start;
	if (owner)
		delInstrAfter(startInstr);
	return false;
}

// fnParam: typeBase ID arrayDecl?
bool fnParam()
{
	DEBUG_PRINT("Parsing fnParam\n");
	Token *start = iTk;
	Type t;
	if (typeBase(&t))
	{
		if (consume(ID))
		{
			Token *tkName = consumedTk;
			if (arrayDecl(&t))
			{
				t.n = 0;
			}
			Symbol *param = findSymbolInDomain(symTable, tkName->value.text);
			if (param)
				tkerr("symbol redefinition: %s", tkName->value.text);
			param = newSymbol(tkName->value.text, SK_PARAM);
			param->type = t;
			param->owner = owner;
			param->paramIdx = symbolsLen(owner->fn.params);
			addSymbolToDomain(symTable, param);
			addSymbolToList(&owner->fn.params, dupSymbol(param));
			return true;
		}
		else
		{
			tkerr("missing function parameter identifier");
		}
	}
	iTk = start;
	return false;
}

// stm: stmCompound
//		| IF LPAR expr RPAR stm ( ELSE stm )?
//		| WHILE LPAR expr RPAR stm
//		| RETURN expr? SEMICOLON
//		| expr? SEMICOLON
bool stm()
{
	DEBUG_PRINT("Parsing stm\n");
	Token *start = iTk;
	Instr *startInstr = owner ? lastInstr(owner->fn.instr) : NULL;
	Ret rCond, rExpr;
	if (stmCompound(true))
	{
		return true;
	}
	if (consume(IF))
	{
		if (consume(LPAR))
		{
			if (expr(&rCond))
			{
				if (!canBeScalar(&rCond))
				{
					tkerr("the 'if' condition must be a scalar value");
				}
				if (consume(RPAR))
				{
					addRVal(&owner->fn.instr, rCond.lval, &rCond.type);
					Type intType = {TB_INT, NULL, -1};
					insertConvIfNeeded(lastInstr(owner->fn.instr), &rCond.type, &intType);
					Instr *ifJF = addInstr(&owner->fn.instr, OP_JF);

					if (stm())
					{
						if (consume(ELSE))
						{
							Instr *ifJMP = addInstr(&owner->fn.instr, OP_JMP);
							ifJF->arg.instr = addInstr(&owner->fn.instr, OP_NOP);

							if (stm())
							{
								ifJMP->arg.instr = addInstr(&owner->fn.instr, OP_NOP);
							}
							else
							{
								tkerr("missing or invalid statement after 'else'");
							}
						}
						else
						{
							ifJF->arg.instr = addInstr(&owner->fn.instr, OP_NOP);
						}
						return true;
					}
					else
					{
						tkerr("missing statement after 'if'");
					}
				}
				else
				{
					tkerr("invalid condition for 'if' or missing ')'");
				}
			}
			else
			{
				tkerr("missing condition for 'if'");
			}
		}
		else
		{
			tkerr("missing '(' after 'if'");
		}
	}
	if (consume(WHILE))
	{
		Instr *beforeWhileCond = lastInstr(owner->fn.instr);
		if (consume(LPAR))
		{
			if (expr(&rCond))
			{
				if (!canBeScalar(&rCond))
				{
					tkerr("the 'while' condition must be a scalar value");
				}
				if (consume(RPAR))
				{
					addRVal(&owner->fn.instr, rCond.lval, &rCond.type);
					Type intType = {TB_INT, NULL, -1};
					insertConvIfNeeded(lastInstr(owner->fn.instr), &rCond.type, &intType);
					Instr *whileJF = addInstr(&owner->fn.instr, OP_JF);
					if (stm())
					{
						addInstr(&owner->fn.instr, OP_JMP)->arg.instr = beforeWhileCond->next;
						whileJF->arg.instr = addInstr(&owner->fn.instr, OP_NOP);
						return true;
					}
					else
					{
						tkerr("missing 'while' function body");
					}
				}
				else
				{
					tkerr("invalid condition for 'while' or missing ')'");
				}
			}
			else
			{
				tkerr("missing condition for 'while'");
			}
		}
		else
		{
			tkerr("missing '(' after 'while'");
		}
	}
	if (consume(RETURN))
	{
		if (expr(&rExpr))
		{
			if (owner->type.tb == TB_VOID)
			{
				tkerr("a void function cannot return a value");
			}
			if (!canBeScalar(&rExpr))
			{
				tkerr("the return value must be a scalar value");
			}
			if (!convTo(&rExpr.type, &owner->type))
			{
				tkerr("cannot convert the return expression type to the function return type");
			}
			addRVal(&owner->fn.instr, rExpr.lval, &rExpr.type);
			insertConvIfNeeded(lastInstr(owner->fn.instr), &rExpr.type, &owner->type);
			addInstrWithInt(&owner->fn.instr, OP_RET, symbolsLen(owner->fn.params));
		}
		else
		{
			if (owner->type.tb != TB_VOID)
			{
				tkerr("a non-void function must return a value");
			}
			addInstr(&owner->fn.instr, OP_RET_VOID);
		}
		if (consume(SEMICOLON))
		{
			return true;
		}
		else
		{
			tkerr("missing ';' after 'return'");
		}
	}
	if (expr(&rExpr))
	{
		if (rExpr.type.tb != TB_VOID)
		{
			addInstr(&owner->fn.instr, OP_DROP);
		}
		if (consume(SEMICOLON))
		{
			return true;
		}
		else
		{
			tkerr("missing ';' after expression");
		}
	}
	if (consume(SEMICOLON))
	{
		return true;
	}
	iTk = start;
	if (owner)
		delInstrAfter(startInstr);
	return false;
}

// stmCompound: LACC (varDef | stm)* RACC
bool stmCompound(bool newDomain)
{
	DEBUG_PRINT("Parsing stmCompound\n");
	Token *start = iTk;
	if (consume(LACC))
	{
		if (newDomain)
		{
			pushDomain();
		}
		for (;;)
		{
			if (varDef())
			{
			}
			else if (stm())
			{
			}
			else
				break;
		}
		if (consume(RACC))
		{
			if (newDomain)
			{
				dropDomain();
			}
			return true;
		}
		else
		{
			tkerr("missing '}'");
		}
	}
	iTk = start;
	return false;
}

// expr: exprAssign
bool expr(Ret *r)
{
	DEBUG_PRINT("Parsing expr\n");
	if (exprAssign(r))
	{
		return true;
	}
	return false;
}

// exprAssign: exprUnary ASSIGN exprAssign | exprOr
bool exprAssign(Ret *r)
{
	DEBUG_PRINT("Parsing exprAssign\n");
	Token *start = iTk;
	Instr *startInstr = owner ? lastInstr(owner->fn.instr) : NULL;
	Ret rDst;
	if (exprUnary(&rDst))
	{
		if (consume(ASSIGN))
		{
			if (exprAssign(r))
			{
				if (!rDst.lval)
					tkerr("the assign destination must be a left-value");
				if (rDst.ct)
					tkerr("the assign destination cannot be a constant");
				if (!canBeScalar(&rDst))
					tkerr("the assign destination must be a scalar");
				if (!canBeScalar(r))
					tkerr("the assign source must be a scalar");
				if (!convTo(&r->type, &rDst.type))
					tkerr("the assign source cannot be converted to the destination");
				r->lval = false;
				r->ct = true;
				addRVal(&owner->fn.instr, r->lval, &r->type);
				insertConvIfNeeded(lastInstr(owner->fn.instr), &r->type, &rDst.type);
				switch (rDst.type.tb)
				{
				case TB_INT:
					addInstr(&owner->fn.instr, OP_STORE_I);
					break;
				case TB_DOUBLE:
					addInstr(&owner->fn.instr, OP_STORE_F);
					break;
				default:
					break;
				}
				return true;
			}
			else
			{
				tkerr("invalid or missing expression after '=' operator");
			}
		}
	}
	iTk = start;
	if (owner)
	{
		delInstrAfter(startInstr);
	}
	if (exprOr(r))
	{
		return true;
	}
	iTk = start;
	if (owner)
	{
		delInstrAfter(startInstr);
	}
	return false;
}

// exprOr: exprOr OR exprAnd | exprAnd
// devine exprOr: exprAnd exprOrPrim
bool exprOr(Ret *r)
{
	DEBUG_PRINT("Parsing exprOr\n");
	Token *start = iTk;
	if (exprAnd(r))
	{
		if (exprOrPrim(r))
		{
			return true;
		}
	}
	iTk = start;
	return false;
}

// exprOrPrim: OR exprAnd exprOrPrim | ε
// echivalent cu exprOrPrim: ( OR exprAnd exprOrPrim )?
bool exprOrPrim(Ret *r)
{
	DEBUG_PRINT("Parsing exprOrPrim\n");
	Token *start = iTk;
	if (consume(OR))
	{
		Ret right;
		if (exprAnd(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '||' operator");
			*r = (Ret){{TB_INT, NULL, -1}, false, true};
			if (exprOrPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("missing or invalid expression after \"||\" operator");
		}
	}
	iTk = start;
	return true;
}

// exprAnd: exprAnd AND exprEq | exprEq
// devine exprAnd: exprEq exprAndPrim
bool exprAnd(Ret *r)
{
	DEBUG_PRINT("Parsing exprAdd\n");
	Token *start = iTk;
	if (exprEq(r))
	{
		if (exprAndPrim(r))
		{
			return true;
		}
	}
	iTk = start;
	return false;
}

// exprAndPrim: AND exprEq exprAndPrim | ε
// echivalent cu exprAnd: ( AND exprEq exprAndPrim )?
bool exprAndPrim(Ret *r)
{
	DEBUG_PRINT("Parsing exprAddPrim\n");
	Token *start = iTk;
	if (consume(AND))
	{
		Ret right;
		if (exprEq(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '&&' operator");
			*r = (Ret){{TB_INT, NULL, -1}, false, true};
			if (exprAndPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("missing or invalid expression after \"&&\" operator");
		}
	}
	iTk = start;
	return true;
}

// exprEq: exprEq ( EQUAL | NOTEQ ) exprRel | exprRel
// devine exprEq: exprRel exprEqPrim
bool exprEq(Ret *r)
{
	DEBUG_PRINT("Parsing exprEq\n");
	Token *start = iTk;
	if (exprRel(r))
	{
		if (exprEqPrim(r))
		{
			return true;
		}
	}
	iTk = start;
	return false;
}

// exprEqPrim: ( EQUAL | NOTEQ ) exprRel exprEqPrim | ε
// echivalent cu (( EQUAL | NOTEQ ) exprRel exprEqPrim)?
bool exprEqPrim(Ret *r)
{
	DEBUG_PRINT("Parsing exprEqPrim\n");
	Token *start = iTk;
	if (consume(EQUAL))
	{
		Ret right;
		if (exprRel(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '==' operator");
			*r = (Ret){{TB_INT, NULL, -1}, false, true};
			if (exprEqPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("missing or invalid expression after '==' operator");
		}
	}
	if (consume(NOTEQ))
	{
		Ret right;
		if (exprRel(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '!=' operator");
			*r = (Ret){{TB_INT, NULL, -1}, false, true};
			if (exprEqPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("missing or invalid expression after '!=' operator");
		}
	}
	iTk = start;
	return true;
}

// exprRel: exprRel ( LESS | LESSEQ | GREATER | GREATEREQ ) exprAdd | exprAdd
// devine exprRel: exprAdd exprRelPrim
bool exprRel(Ret *r)
{
	DEBUG_PRINT("Parsing exprRel\n");
	Token *start = iTk;
	Instr *startInstr = owner ? lastInstr(owner->fn.instr) : NULL;
	if (exprAdd(r))
	{
		if (exprRelPrim(r))
		{
			return true;
		}
	}
	iTk = start;
	if (owner)
	{
		delInstrAfter(startInstr);
	}
	return false;
}

// exprRelPrim:  ( LESS | LESSEQ | GREATER | GREATEREQ ) exprAdd exprRelPrim | ε
// echivalent cu ( ( LESS | LESSEQ | GREATER | GREATEREQ ) exprAdd exprRelPrim)?
bool exprRelPrim(Ret *r)
{
	DEBUG_PRINT("Parsing exprRelPrim\n");
	Token *op;
	if (consume(LESS))
	{
		Ret right;
		op = consumedTk;
		Instr *lastLeft = lastInstr(owner->fn.instr);
		addRVal(&owner->fn.instr, r->lval, &r->type);
		if (exprAdd(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '<' operator");
			addRVal(&owner->fn.instr, right.lval, &right.type);
			insertConvIfNeeded(lastLeft, &r->type, &tDst);
			insertConvIfNeeded(lastInstr(owner->fn.instr), &right.type, &tDst);
			switch (op->code)
			{
			case LESS:
				switch (tDst.tb)
				{
				case TB_INT:
					addInstr(&owner->fn.instr, OP_LESS_I);
					break;
				case TB_DOUBLE:
					addInstr(&owner->fn.instr, OP_LESS_F);
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
			*r = (Ret){{TB_INT, NULL, -1}, false, true};
			if (exprRelPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("missing or invalid expression after '<' operator");
		}
	}
	if (consume(LESSEQ))
	{
		Ret right;
		op = consumedTk;
		Instr *lastLeft = lastInstr(owner->fn.instr);
		addRVal(&owner->fn.instr, r->lval, &r->type);
		if (exprAdd(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '<=' operator");
			addRVal(&owner->fn.instr, right.lval, &right.type);
			insertConvIfNeeded(lastLeft, &r->type, &tDst);
			insertConvIfNeeded(lastInstr(owner->fn.instr), &right.type, &tDst);
			switch (op->code)
			{
			case LESS:
				switch (tDst.tb)
				{
				case TB_INT:
					addInstr(&owner->fn.instr, OP_LESS_I);
					break;
				case TB_DOUBLE:
					addInstr(&owner->fn.instr, OP_LESS_F);
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
			*r = (Ret){{TB_INT, NULL, -1}, false, true};
			if (exprRelPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("missing or invalid expression after '<=' operator");
		}
	}
	if (consume(GREATER))
	{
		Ret right;
		op = consumedTk;
		Instr *lastLeft = lastInstr(owner->fn.instr);
		addRVal(&owner->fn.instr, r->lval, &r->type);
		if (exprAdd(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '>' operator");

			addRVal(&owner->fn.instr, right.lval, &right.type);
			insertConvIfNeeded(lastLeft, &r->type, &tDst);
			insertConvIfNeeded(lastInstr(owner->fn.instr), &right.type, &tDst);
			switch (op->code)
			{
			case LESS:
				switch (tDst.tb)
				{
				case TB_INT:
					addInstr(&owner->fn.instr, OP_LESS_I);
					break;
				case TB_DOUBLE:
					addInstr(&owner->fn.instr, OP_LESS_F);
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
			*r = (Ret){{TB_INT, NULL, -1}, false, true};
			if (exprRelPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("missing or invalid expression after '>' operator");
		}
	}
	if (consume(GREATEREQ))
	{
		Ret right;
		op = consumedTk;
		Instr *lastLeft = lastInstr(owner->fn.instr);
		addRVal(&owner->fn.instr, r->lval, &r->type);
		if (exprAdd(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '>=' operator");

			addRVal(&owner->fn.instr, right.lval, &right.type);
			insertConvIfNeeded(lastLeft, &r->type, &tDst);
			insertConvIfNeeded(lastInstr(owner->fn.instr), &right.type, &tDst);
			switch (op->code)
			{
			case LESS:
				switch (tDst.tb)
				{
				case TB_INT:
					addInstr(&owner->fn.instr, OP_LESS_I);
					break;
				case TB_DOUBLE:
					addInstr(&owner->fn.instr, OP_LESS_F);
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
			*r = (Ret){{TB_INT, NULL, -1}, false, true};
			if (exprRelPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("missing or invalid expression after '>=' operator");
		}
	}

	return true;
}

// exprAdd: exprAdd ( ADD | SUB ) exprMul | exprMul
// devine exprAdd: exprMul exprAddPrim
bool exprAdd(Ret *r)
{
	DEBUG_PRINT("Parsing exprAdd\n");
	Token *start = iTk;
	Instr *startInstr = owner ? lastInstr(owner->fn.instr) : NULL;
	if (exprMul(r))
	{
		if (exprAddPrim(r))
		{
			return true;
		}
	}
	iTk = start;
	if (owner)
	{
		delInstrAfter(startInstr);
	}
	return false;
}

// exprAddPrim: ( ADD | SUB ) exprMul exprAddPrim | ε
// echivalent cu (( ADD | SUB ) exprMul exprAddPrim)?
bool exprAddPrim(Ret *r)
{
	DEBUG_PRINT("Parsing exprAddPrim\n");
	if (consume(ADD))
	{
		Ret right;

		Token *op = consumedTk;
		Instr *lastLeft = lastInstr(owner->fn.instr);
		addRVal(&owner->fn.instr, r->lval, &r->type);

		if (exprMul(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '+' operator");
			addRVal(&owner->fn.instr, right.lval, &right.type);
			insertConvIfNeeded(lastLeft, &r->type, &tDst);
			insertConvIfNeeded(lastInstr(owner->fn.instr), &right.type, &tDst);
			switch (op->code)
			{
			case ADD:
				switch (tDst.tb)
				{
				case TB_INT:
					addInstr(&owner->fn.instr, OP_ADD_I);
					break;
				case TB_DOUBLE:
					addInstr(&owner->fn.instr, OP_ADD_F);
					break;
				default:
					break;
				}
				break;
			case SUB:
				switch (tDst.tb)
				{
				case TB_INT:
					addInstr(&owner->fn.instr, OP_SUB_I);
					break;
				case TB_DOUBLE:
					addInstr(&owner->fn.instr, OP_SUB_F);
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
			*r = (Ret){tDst, false, true};
			if (exprAddPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("missing or invalid expression after '+' operator");
		}
	}
	if (consume(SUB))
	{
		Ret right;

		Token *op = consumedTk;
		Instr *lastLeft = lastInstr(owner->fn.instr);
		addRVal(&owner->fn.instr, r->lval, &r->type);

		if (exprMul(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '-' operator");
			addRVal(&owner->fn.instr, right.lval, &right.type);
			insertConvIfNeeded(lastLeft, &r->type, &tDst);
			insertConvIfNeeded(lastInstr(owner->fn.instr), &right.type, &tDst);
			switch (op->code)
			{
			case ADD:
				switch (tDst.tb)
				{
				case TB_INT:
					addInstr(&owner->fn.instr, OP_ADD_I);
					break;
				case TB_DOUBLE:
					addInstr(&owner->fn.instr, OP_ADD_F);
					break;
				default:
					break;
				}
				break;
			case SUB:
				switch (tDst.tb)
				{
				case TB_INT:
					addInstr(&owner->fn.instr, OP_SUB_I);
					break;
				case TB_DOUBLE:
					addInstr(&owner->fn.instr, OP_SUB_F);
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
			*r = (Ret){tDst, false, true};
			if (exprAddPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("missing or invalid expression after '-' operator");
		}
	}
	return true;
}

// exprMul: exprMul ( MUL | DIV ) exprCast | exprCast
// devine exprMul: exprCast exprMulPrim
bool exprMul(Ret *r)
{
	DEBUG_PRINT("Parsing exprMul\n");
	Token *start = iTk;
	Instr *startInstr = owner ? lastInstr(owner->fn.instr) : NULL;
	if (exprCast(r))
	{
		if (exprMulPrim(r))
		{
			return true;
		}
	}
	iTk = start;
	if (owner)
	{
		delInstrAfter(startInstr);
	}
	return false;
}

// exprMulPrim: ( MUL | DIV ) exprCast exprMulPrim | ε
// echivalent cu (( MUL | DIV ) exprCast exprMulPrim)?
bool exprMulPrim(Ret *r)
{
	DEBUG_PRINT("Parsing exprMulPrim\n");
	if (consume(MUL))
	{
		Ret right;
		Token *op = consumedTk;
		Instr *lastLeft = lastInstr(owner->fn.instr);
		addRVal(&owner->fn.instr, r->lval, &r->type);
		if (exprCast(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '*' operator");

			addRVal(&owner->fn.instr, right.lval, &right.type);
			insertConvIfNeeded(lastLeft, &r->type, &tDst);
			insertConvIfNeeded(lastInstr(owner->fn.instr), &right.type, &tDst);
			switch (op->code)
			{
			case MUL:
				switch (tDst.tb)
				{
				case TB_INT:
					addInstr(&owner->fn.instr, OP_MUL_I);
					break;
				case TB_DOUBLE:
					addInstr(&owner->fn.instr, OP_MUL_F);
					break;
				default:
					break;
				}
				break;
			case DIV:
				switch (tDst.tb)
				{
				case TB_INT:
					addInstr(&owner->fn.instr, OP_DIV_I);
					break;
				case TB_DOUBLE:
					addInstr(&owner->fn.instr, OP_DIV_F);
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}

			*r = (Ret){tDst, false, true};
			if (exprMulPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("missing or invalid expression after '*' operator");
		}
	}
	if (consume(DIV))
	{
		Ret right;
		Token *op = consumedTk;
		Instr *lastLeft = lastInstr(owner->fn.instr);
		addRVal(&owner->fn.instr, r->lval, &r->type);
		if (exprCast(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '/' operator");
			addRVal(&owner->fn.instr, right.lval, &right.type);
			insertConvIfNeeded(lastLeft, &r->type, &tDst);
			insertConvIfNeeded(lastInstr(owner->fn.instr), &right.type, &tDst);
			switch (op->code)
			{
			case MUL:
				switch (tDst.tb)
				{
				case TB_INT:
					addInstr(&owner->fn.instr, OP_MUL_I);
					break;
				case TB_DOUBLE:
					addInstr(&owner->fn.instr, OP_MUL_F);
					break;
				default:
					break;
				}
				break;
			case DIV:
				switch (tDst.tb)
				{
				case TB_INT:
					addInstr(&owner->fn.instr, OP_DIV_I);
					break;
				case TB_DOUBLE:
					addInstr(&owner->fn.instr, OP_DIV_F);
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
			*r = (Ret){tDst, false, true};
			if (exprMulPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("missing or invalid expression after '/' operator");
		}
	}
	return true;
}

// exprCast: LPAR typeBase arrayDecl? RPAR exprCast | exprUnary
bool exprCast(Ret *r)
{
	DEBUG_PRINT("Parsing exprCast\n");
	Token *start = iTk;
	if (consume(LPAR))
	{
		Type t;
		Ret op;
		if (typeBase(&t))
		{
			if (arrayDecl(&t))
			{
			}
			if (consume(RPAR))
			{
				if (exprCast(&op))
				{
					if (t.tb == TB_STRUCT)
						tkerr("cannot convert to a struct type");
					if (op.type.tb == TB_STRUCT)
						tkerr("cannot convert a struct");
					if (op.type.n >= 0 && t.n < 0)
						tkerr("an array can only be converted to another array");
					if (op.type.n < 0 && t.n >= 0)
						tkerr("a scalar can only be converted to another scalar");
					*r = (Ret){t, false, true};
					return true;
				}
				else
				{
					tkerr("missing or invalid expression to be casted");
				}
			}
			else
			{
				tkerr("missing ')' after cast type");
			}
		}
		else
		{
			tkerr("missing cast type");
		}
	}
	iTk = start;
	if (exprUnary(r))
	{
		return true;
	}
	iTk = start;
	return false;
}

// exprUnary: ( SUB | NOT ) exprUnary | exprPostfix
bool exprUnary(Ret *r)
{
	DEBUG_PRINT("Parsing exprUnary\n");
	Token *start = iTk;
	if (consume(SUB))
	{
		if (exprUnary(r))
		{
			if (!canBeScalar(r))
				tkerr("unary '-' operator must have a scalar operand");
			r->lval = false;
			r->ct = true;
			return true;
		}
		else
		{
			tkerr("missing or invalid expression after '-' operator");
		}
	}
	if (consume(NOT))
	{
		if (exprUnary(r))
		{
			if (!canBeScalar(r))
				tkerr("unary '!' operator must have a scalar operand");
			r->lval = false;
			r->ct = true;
			return true;
		}
		else
		{
			tkerr("missing or invalid expression after '!' operator");
		}
	}
	iTk = start;
	if (exprPostfix(r))
	{
		return true;
	}
	iTk = start;
	return false;
}

// exprPostfix: exprPostfix LBRACKET expr RBRACKET | exprPostfix DOT ID | exprPrimary
// devine exprPostfix: exprPrimary exprPostfixPrim
bool exprPostfix(Ret *r)
{
	DEBUG_PRINT("Parsing exprPostfix\n");
	Token *start = iTk;
	if (exprPrimary(r))
	{
		if (exprPostfixPrim(r))
		{
			return true;
		}
	}
	iTk = start;
	return false;
}

// exprPostfixPrim: LBRACKET expr RBRACKET exprPostfixPrim | DOT ID exprPostfixPrim | ε
// echivalent cu (LBRACKET expr RBRACKET exprPostfixPrim | DOT ID exprPostfixPrim)?
bool exprPostfixPrim(Ret *r)
{
	DEBUG_PRINT("Parsing exprPostfixPrim\n");
	Token *start = iTk;
	if (consume(LBRACKET))
	{
		Ret idx;
		if (expr(&idx))
		{
			if (consume(RBRACKET))
			{
				if (r->type.n < 0)
					tkerr("only an array can be indexed");
				Type tInt = {TB_INT, NULL, -1};
				if (!convTo(&idx.type, &tInt))
					tkerr("the array index is not convertible to int");
				r->type.n = -1;
				r->lval = true;
				r->ct = false;
				if (exprPostfixPrim(r))
				{
					return true;
				}
			}
			else
			{
				tkerr("invalid expression between [...] or missing ']'");
			}
		}
	}
	iTk = start;
	if (consume(DOT))
	{
		if (consume(ID))
		{
			Token *tkName = consumedTk;
			if (r->type.tb != TB_STRUCT)
				tkerr("a field can only be selected from a struct");
			Symbol *s = findSymbolInList(r->type.s->structMembers, tkName->value.text);
			if (!s)
				tkerr("the struct %s does not have a field %s", r->type.s->name, tkName->value.text);
			*r = (Ret){s->type, true, s->type.n >= 0};
			if (exprPostfixPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("invalid or missing identifier after '.'");
		}
	}
	iTk = start;
	return true;
}

// exprPrimary: ID ( LPAR ( expr ( COMMA expr )* )? RPAR )? | INT | DOUBLE | CHAR | STRING | LPAR expr RPAR
bool exprPrimary(Ret *r)
{
	DEBUG_PRINT("Parsing exprPrimary\n");
	Token *start = iTk;
	Instr *startInstr = owner ? lastInstr(owner->fn.instr) : NULL;
	if (consume(ID))
	{
		Token *tkName = consumedTk;
		Symbol *s = findSymbol(tkName->value.text);
		if (!s)
		{
			tkerr("undefined identifier: %s", tkName->value.text);
		}
		if (consume(LPAR))
		{
			if (s->kind != SK_FN)
				tkerr("only a function can be called");
			Ret rArg;
			Symbol *param = s->fn.params;

			if (expr(&rArg))
			{
				if (!param)
					tkerr("too many arguments in function call");
				if (!convTo(&rArg.type, &param->type))
					tkerr("cannot convert the argument type to the parameter type during function call");
				addRVal(&owner->fn.instr, rArg.lval, &rArg.type);
				insertConvIfNeeded(lastInstr(owner->fn.instr), &rArg.type, &param->type);
				param = param->next;
				while (consume(COMMA))
				{
					if (expr(&rArg))
					{
						if (!param)
							tkerr("too many arguments in function call");
						if (!convTo(&rArg.type, &param->type))
							tkerr("cannot convert the argument type to the parameter type during function call");
						addRVal(&owner->fn.instr, rArg.lval, &rArg.type);
						insertConvIfNeeded(lastInstr(owner->fn.instr), &rArg.type, &param->type);

						param = param->next;
					}
					else
					{
						tkerr("missing or invalid additional expression after ','");
					}
				}
			}
			if (consume(RPAR))
			{
				if (param)
					tkerr("too few arguments in function call");
				*r = (Ret){s->type, false, true};
				if (s->fn.extFnPtr)
				{
					addInstr(&owner->fn.instr, OP_CALL_EXT)->arg.extFnPtr = s->fn.extFnPtr;
				}
				else
				{
					addInstr(&owner->fn.instr, OP_CALL)->arg.instr = s->fn.instr;
				}
				return true;
			}
			else
			{
				tkerr("missing ')' or invalid expression between \"(...)\"");
			}
		}
		else
		{
			if (s->kind == SK_FN)
			{
				tkerr("A function can only be called");
			}

			*r = (Ret){s->type, true, s->type.n >= 0};

			if (s->kind == SK_VAR)
			{
				if (s->owner == NULL)
				{ // global variables
					addInstr(&owner->fn.instr, OP_ADDR)->arg.p = s->varMem;
				}
				else
				{ // local variables
					switch (s->type.tb)
					{
					case TB_INT:
						addInstrWithInt(&owner->fn.instr, OP_FPADDR_I, s->varIdx + 1);
						break;
					case TB_DOUBLE:
						addInstrWithInt(&owner->fn.instr, OP_FPADDR_F, s->varIdx + 1);
						break;
					default:
						break;
					}
				}
			}

			if (s->kind == SK_PARAM)
			{
				switch (s->type.tb)
				{
				case TB_INT:
					addInstrWithInt(&owner->fn.instr, OP_FPADDR_I, s->paramIdx - symbolsLen(s->owner->fn.params) - 1);
					break;
				case TB_DOUBLE:
					addInstrWithInt(&owner->fn.instr, OP_FPADDR_F, s->paramIdx - symbolsLen(s->owner->fn.params) - 1);
					break;
				default:
					break;
				}
			}
		}
		return true;
	}

	if (consume(INT))
	{
		*r = (Ret){{TB_INT, NULL, -1}, false, true};
		Token *ct = consumedTk;
		addInstrWithInt(&owner->fn.instr, OP_PUSH_I, ct->value.i);
		return true;
	}
	if (consume(DOUBLE))
	{
		*r = (Ret){{TB_DOUBLE, NULL, -1}, false, true};
		Token *ct = consumedTk;
		addInstrWithDouble(&owner->fn.instr, OP_PUSH_F, ct->value.d);
		return true;
	}
	if (consume(CHAR))
	{
		*r = (Ret){{TB_CHAR, NULL, -1}, false, true};
		return true;
	}
	if (consume(STRING))
	{
		*r = (Ret){{TB_CHAR, NULL, 0}, false, true};
		return true;
	}
	if (consume(LPAR))
	{
		if (expr(r))
		{
			if (consume(RPAR))
			{
				return true;
			}
			else
			{
				tkerr("missing or invalid expression between \"(...)\" or missing ')'");
			}
		}
	}
	iTk = start;
	if (owner)
	{
		delInstrAfter(startInstr);
	}
	return false;
}

void parse(Token *tokens)
{
	iTk = tokens;
	if (!unit())
	{
		tkerr("Syntax error");
	}
}