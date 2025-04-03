#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include "parser.h"
#include "utils.h"
#include "ad.h"

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

Token *iTk;				// the iterator in the tokens list
Token *consumedTk;		// the last consumed token
Symbol *owner = NULL;	// the last consumed symbol	

const char *tkCodeName(int code) {
    switch (code) {
        case ID: return "ID";
        case TYPE_CHAR: return "TYPE_CHAR";
        case TYPE_DOUBLE: return "TYPE_DOUBLE";
        case ELSE: return "ELSE";
        case IF: return "IF";
        case TYPE_INT: return "TYPE_INT";
        case RETURN: return "RETURN";
        case STRUCT: return "STRUCT";
        case VOID: return "VOID";
        case WHILE: return "WHILE";

        case INT: return "INT";
        case DOUBLE: return "DOUBLE";
        case CHAR: return "CHAR";
        case STRING: return "STRING";

        case COMMA: return "COMMA";
        case SEMICOLON: return "SEMICOLON";
        case LPAR: return "LPAR";
        case RPAR: return "RPAR";
        case LBRACKET: return "LBRACKET";
        case RBRACKET: return "RBRACKET";
        case LACC: return "LACC";
        case RACC: return "RACC";
        case END: return "END";

        case ADD: return "ADD";
        case SUB: return "SUB";
        case MUL: return "MUL";
        case DIV: return "DIV";
        case DOT: return "DOT";
        case AND: return "AND";
        case OR: return "OR";
        case NOT: return "NOT";
        case ASSIGN: return "ASSIGN";
        case EQUAL: return "EQUAL";
        case NOTEQ: return "NOTEQ";
        case LESS: return "LESS";
        case LESSEQ: return "LESSEQ";
        case GREATER: return "GREATER";
        case GREATEREQ: return "GREATEREQ";

        default: return "UNKNOWN";
    }
}

void tkerr(const char *fmt,...){
	fprintf(stderr,"error in line %d: ",iTk->line);
	va_list va;
	va_start(va,fmt);
	vfprintf(stderr,fmt,va);
	va_end(va);
	fprintf(stderr,"\n");
	exit(EXIT_FAILURE);
	}

bool consume(int code){
	CONSUME_PRINT("consume(%s)", tkCodeName(code));
	if(iTk->code==code){
		consumedTk=iTk;
		iTk=iTk->next;
		CONSUME_PRINT(" => consumed\n");
		return true;
		}
	CONSUME_PRINT(" => found %s\n", tkCodeName(iTk->code));
	return false;
	}

// unit: ( structDef | fnDef | varDef )* END
bool unit(){
	DEBUG_PRINT("Parsing unit\n");	
	for(;;){
		if(structDef()){}
		else if(fnDef()){}
		else if(varDef()){}
		else break;
	}
	if(consume(END)){
		return true;
	}
	return false;
}

// structDef: STRUCT ID LACC varDef* RACC SEMICOLON
bool structDef(){
	DEBUG_PRINT("Parsing structDef\n");	
	Token *start = iTk;
	if (consume(STRUCT)) {
        if (consume(ID)) {
			Token *tkName = consumedTk;
            if (consume(LACC)) {
				Symbol *s = findSymbolInDomain(symTable, tkName->value.text);
				if(s){
					tkerr("symbol redefinition: %s", tkName->value.text);
				}
				s = addSymbolToDomain(symTable, newSymbol(tkName->value.text, SK_STRUCT));
				s->type.tb = TB_STRUCT;
				s->type.s = s;
				s->type.n = -1;
				pushDomain();
				owner = s;
				for(;;){
					if(varDef());
					else break;
				}
                if (consume(RACC)) {
                    if (consume(SEMICOLON)) {
						owner = NULL;
						dropDomain();
                        return true;
                    }
					else{
						tkerr("missing ';'");
					}
                }
				else{
					tkerr("invalid expression between {...} or missing '}'");
				}
            }
			else if(varDef() || consume(RACC)){
				tkerr("missing '{' or invalid expression between {...}");
			}
        }
		else{
			tkerr("missing struct identifier");
		}
    }
	
    iTk = start;
    return false;
}

// varDef: typeBase ID arrayDecl? SEMICOLON
bool varDef(){
	DEBUG_PRINT("Parsing varDef\n");	
	Token *start = iTk;
	Type t;
	if(typeBase(&t)){
		if(consume(ID)){
			Token *tkName = consumedTk;
			if(arrayDecl(&t)){
				if(t.n==0){
					tkerr("a vector variable must have a specified dimension");
				}
			}
			if(consume(SEMICOLON)){
				Symbol *var = findSymbolInDomain(symTable, tkName->value.text);
				if (var) tkerr("symbol redefinition: %s", tkName->value.text);
				var = newSymbol(tkName->value.text, SK_VAR);
				var->type = t;
				var->owner = owner;
				addSymbolToDomain(symTable, var);
				if(owner){
					switch (owner->kind) {
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
				else{
					var->varMem = safeAlloc(typeSize(&t));
				}
				return true;
			}
			else{
				tkerr("missing ';'");
			}
		}
		else{
			tkerr("missing or invalid identifier");
		}
	}
	iTk = start;
    return false;
}

// typeBase: TYPE_INT | TYPE_DOUBLE | TYPE_CHAR | STRUCT ID
bool typeBase(Type *t){
	t->n = -1;
	DEBUG_PRINT("Parsing typeBase\n");	
	Token *start=iTk;
	if(consume(TYPE_INT)){
		t->tb = TB_INT;
		return true;
	}
	if(consume(TYPE_DOUBLE)){
		t->tb = TB_DOUBLE;
		return true;
	}
	if(consume(TYPE_CHAR)){
		t->tb = TB_CHAR;
		return true;
	}
	if(consume(STRUCT)){
		if(consume(ID)){
			Token *tkName = consumedTk;
			t->tb = TB_STRUCT;
			t->s = findSymbol(tkName->value.text);
			if(!t->s){
				tkerr("undefined struct type: %s", tkName->value.text);
			}
			return true;
		}
		else{
			tkerr("missing struct identifier");
		}
	}
	iTk=start;
	return false;
}

// arrayDecl: LBRACKET INT? RBRACKET
bool arrayDecl(Type *t){
	DEBUG_PRINT("Parsing arrayDecl\n");	
	Token *start=iTk;
	if(consume(LBRACKET)){
		if(consume(INT)){
			Token *tkSize = consumedTk;
			t->n = tkSize->value.i;
		}else{
			t->n=0; // array fara dimensiune: int v[]
		}
		if(consume(RBRACKET)){
			return true;
		}
		else{
			tkerr("invalid expression between [...] or missing ']'");
		}
	}
	iTk=start;
	return false;
}

// fnDef: (typeBase | VOID) ID LPAR (fnParam (COMMA fnParam)*)? RPAR stmCompound
bool fnDef(){
	DEBUG_PRINT("Parsing fnDef\n");	
	Token *start = iTk;
	Type t;
	bool consumedVoidTk = false;
    if(typeBase(&t) || (consumedVoidTk = consume(VOID))){
		if(consumedVoidTk){
			t.tb = TB_VOID;
		}
		if(consume(ID)){
			Token *tkName = consumedTk;
			if(consume(LPAR)){
				Symbol *fn = findSymbolInDomain(symTable, tkName->value.text);
				if (fn) tkerr("symbol redefinition: %s", tkName->value.text);
				fn = newSymbol(tkName->value.text, SK_FN);
				fn->type = t;
				addSymbolToDomain(symTable, fn);
				owner = fn;
				pushDomain();
				if(fnParam()){
					while(consume(COMMA))
					{
						if(fnParam()){}
						else{
							tkerr("missing or invalid additional function parameter after ','");
						}
					}
				}
				if(consume(RPAR)){
					if(stmCompound(false)){
						dropDomain();
						owner = NULL;
						return true;
					}
					else{
						tkerr("missing function body");
					}
				}
				else{
					tkerr("invalid function parameters or missing ')'");
				}
			}
		}
		else {
			if(consume(LPAR)){
				tkerr("missing function identifier");
			}
		}
	}
    iTk = start;
    return false;
}

// fnParam: typeBase ID arrayDecl?
bool fnParam(){
	DEBUG_PRINT("Parsing fnParam\n");	
	Token *start=iTk;
	Type t;
	if(typeBase(&t)){
		if(consume(ID)){
			Token *tkName = consumedTk;
			if(arrayDecl(&t)){
				t.n = 0;
			}
			Symbol *param = findSymbolInDomain(symTable, tkName->value.text);
			if (param) tkerr("symbol redefinition: %s", tkName->value.text);
			param = newSymbol(tkName->value.text, SK_PARAM);
			param->type = t;
			param->owner = owner;
			param->paramIdx = symbolsLen(owner->fn.params);
			addSymbolToDomain(symTable, param);
			addSymbolToList(&owner->fn.params, dupSymbol(param));
			return true;
		}
		else{
			tkerr("missing function parameter identifier");
		}
	}
	iTk=start;
	return false;
}

// stm: stmCompound 
//		| IF LPAR expr RPAR stm ( ELSE stm )? 
//		| WHILE LPAR expr RPAR stm 
//		| RETURN expr? SEMICOLON 
//		| expr? SEMICOLON
bool stm(){
	DEBUG_PRINT("Parsing stm\n");	
	Token *start = iTk;
	if(stmCompound(true)){
		return true;
	}
	if(consume(IF)){
		if(consume(LPAR)){
			if(expr()){
				if(consume(RPAR)){
					if(stm()){
						if(consume(ELSE)){
							if(stm()){}
							else{
								tkerr("missing or invalid statement after 'else'");
							}
						}
						return true;
					}
					else{
						tkerr("missing statement after 'if'");
					}
				}
				else{
					tkerr("invalid condition for 'if' or missing ')'");
				}
			}
			else{
				tkerr("missing condition for 'if'");
			}
		}
		else{
			tkerr("missing '(' after 'if'");
		}
	}
	if(consume(WHILE)){
		if(consume(LPAR)){
			if(expr()){
				if(consume(RPAR)){
					if(stm()){
						return true;
					}
					else{
						tkerr("missing 'while' function body");
					}
				}
				else{
					tkerr("invalid condition for 'while' or missing ')'");
				}
			}
			else{
				tkerr("missing condition for 'while'");
			}
		}
		else{
			tkerr("missing '(' after 'while'");
		}
	}
	if(consume(RETURN)){
		if(expr()){}
		if(consume(SEMICOLON)){
			return true;
		}
		else{
			tkerr("missing ';' after 'return'");
		}
	}
	if(expr()){
		if(consume(SEMICOLON)){
			return true;
		}
		else{
			tkerr("missing ';' after expression");
		}
	}
	if(consume(SEMICOLON)){
		return true;
	}
	iTk = start;
    return false;
}

// stmCompound: LACC (varDef | stm)* RACC
bool stmCompound(bool newDomain){
	DEBUG_PRINT("Parsing stmCompound\n");	
	Token *start = iTk;
	if(consume(LACC)){
		if(newDomain){
			pushDomain();		
		}
		for(;;){
			if(varDef()){}
			else if(stm()){}
			else break;
		}
		if(consume(RACC)){
			if(newDomain){
				dropDomain();
			}
			return true;
		}
		else{
			tkerr("missing '}'");
		}
	}
	iTk = start;
    return false;
}

// expr: exprAssign
bool expr(){
	DEBUG_PRINT("Parsing expr\n");	
	if(exprAssign()){
		return true;
	}
    return false;
}

// exprAssign: exprUnary ASSIGN exprAssign | exprOr
bool exprAssign(){
	DEBUG_PRINT("Parsing exprAssign\n");	
	Token *start = iTk;
	if(exprUnary()){
		if(consume(ASSIGN)){
			if(exprAssign()){
				return true;
			}
			else{
				tkerr("invalid or missing expression after '=' operator");
			}
		}
	}
	iTk = start;
	if(exprOr()){
		return true;
	}
	iTk = start;
    return false;
}

// exprOr: exprOr OR exprAnd | exprAnd
// devine exprOr: exprAnd exprOrPrim
bool exprOr(){
	DEBUG_PRINT("Parsing exprOr\n");	
	Token *start = iTk;
	if(exprAnd()){
		if(exprOrPrim()){
			return true;
		}
	}
	iTk = start;
    return false;
}

// exprOrPrim: OR exprAnd exprOrPrim | ε
// echivalent cu exprOrPrim: ( OR exprAnd exprOrPrim )?
bool exprOrPrim(){
	DEBUG_PRINT("Parsing exprOrPrim\n");	
	Token *start = iTk;
	if(consume(OR)){
		if(exprAnd()){
			if(exprOrPrim()){
				return true;
			}
		}
		else{
			tkerr("missing or invalid expression after \"||\" operator");
		}
	}
	iTk = start;
	return true;
}

// exprAnd: exprAnd AND exprEq | exprEq
// devine exprAnd: exprEq exprAndPrim
bool exprAnd(){
	DEBUG_PRINT("Parsing exprAdd\n");	
	Token *start = iTk;
	if(exprEq()){
		if(exprAndPrim()){
			return true;
		}
	}
	iTk = start;
    return false;
}

// exprAndPrim: AND exprEq exprAndPrim | ε
// echivalent cu exprAnd: ( AND exprEq exprAndPrim )?
bool exprAndPrim(){
	DEBUG_PRINT("Parsing exprAddPrim\n");
	Token *start = iTk;	
	if(consume(AND)){
		if(exprEq()){
			if(exprAndPrim()){
				return true;
			}
		}
		else{
			tkerr("missing or invalid expression after \"&&\" operator");
		}
	}
	iTk = start;
	return true;
}

// exprEq: exprEq ( EQUAL | NOTEQ ) exprRel | exprRel
// devine exprEq: exprRel exprEqPrim
bool exprEq(){
	DEBUG_PRINT("Parsing exprEq\n");	
	Token *start = iTk;
	if(exprRel()){
		if(exprEqPrim()){
			return true;
		}
	}
	iTk = start;
    return false;
}

// exprEqPrim: ( EQUAL | NOTEQ ) exprRel exprEqPrim | ε
// echivalent cu (( EQUAL | NOTEQ ) exprRel exprEqPrim)?
bool exprEqPrim(){
	DEBUG_PRINT("Parsing exprEqPrim\n");
	Token *start = iTk;	
	if(consume(EQUAL)){
		if(exprRel()){
			if(exprEqPrim()){
				return true;
			}
		}
		else{
			tkerr("missing or invalid expression after '==' operator");
		}
	}
	if(consume(NOTEQ)){
		if(exprRel()){
			if(exprEqPrim()){
				return true;
			}
		}
		else{
			tkerr("missing or invalid expression after '!=' operator");
		}
	}
	iTk = start;
	return true;
}

// exprRel: exprRel ( LESS | LESSEQ | GREATER | GREATEREQ ) exprAdd | exprAdd
// devine exprRel: exprAdd exprRelPrim
bool exprRel(){
	DEBUG_PRINT("Parsing exprRel\n");	
	Token *start = iTk;
	if(exprAdd()){
		if(exprRelPrim()){
			return true;
		}
	}
	iTk = start;
    return false;
}

// exprRelPrim:  ( LESS | LESSEQ | GREATER | GREATEREQ ) exprAdd exprRelPrim | ε
// echivalent cu ( ( LESS | LESSEQ | GREATER | GREATEREQ ) exprAdd exprRelPrim)?
bool exprRelPrim(){
	DEBUG_PRINT("Parsing exprRelPrim\n");	
	if(consume(LESS)){
		if(exprAdd()){
			if(exprRelPrim()){
				return true;
			}
		}
		else{
			tkerr("missing or invalid expression after '<' operator");
		}
	}
	if(consume(LESSEQ)){
		if(exprAdd()){
			if(exprRelPrim()){
				return true;
			}
		}
		else{
			tkerr("missing or invalid expression after '<=' operator");
		}
	}
	if(consume(GREATER)){
		if(exprAdd()){
			if(exprRelPrim()){
				return true;
			}
		}
		else{
			tkerr("missing or invalid expression after '>' operator");
		}
	}
	if(consume(GREATEREQ)){
		if(exprAdd()){
			if(exprRelPrim()){
				return true;
			}
		}
		else{
			tkerr("missing or invalid expression after '>=' operator");
		}
	}
	return true;
}

// exprAdd: exprAdd ( ADD | SUB ) exprMul | exprMul
// devine exprAdd: exprMul exprAddPrim
bool exprAdd(){
	DEBUG_PRINT("Parsing exprAdd\n");	
	Token *start = iTk;
	if(exprMul()){
		if(exprAddPrim()){
			return true;
		}
	}
	iTk = start;
    return false;
}

// exprAddPrim: ( ADD | SUB ) exprMul exprAddPrim | ε
// echivalent cu (( ADD | SUB ) exprMul exprAddPrim)?
bool exprAddPrim(){
	DEBUG_PRINT("Parsing exprAddPrim\n");	
	if(consume(ADD)){
		if(exprMul()){
			if(exprAddPrim()){
				return true;
			}
		}
		else{
			tkerr("missing or invalid expression after '+' operator");
		}
	}
	if(consume(SUB)){
		if(exprMul()){
			if(exprAddPrim()){
				return true;
			}
		}
		else{
			tkerr("missing or invalid expression after '-' operator");
		}
	}
	return true;
}

// exprMul: exprMul ( MUL | DIV ) exprCast | exprCast
// devine exprMul: exprCast exprMulPrim
bool exprMul(){
	DEBUG_PRINT("Parsing exprMul\n");	
	Token *start = iTk;
	if(exprCast()){
		if(exprMulPrim()){
			return true;
		}
	}
	iTk = start;
    return false;
}

// exprMulPrim: ( MUL | DIV ) exprCast exprMulPrim | ε
// echivalent cu (( MUL | DIV ) exprCast exprMulPrim)?
bool exprMulPrim(){
	DEBUG_PRINT("Parsing exprMulPrim\n");	
	if(consume(MUL)){
		if(exprCast()){
			if(exprMulPrim()){
				return true;
			}
		}
		else{
			tkerr("missing or invalid expression after '*' operator");
		}
	}
	if(consume(DIV)){
		if(exprCast()){
			if(exprMulPrim()){
				return true;
			}
		}
		else{
			tkerr("missing or invalid expression after '/' operator");
		}
	}
	return true;
}

// exprCast: LPAR typeBase arrayDecl? RPAR exprCast | exprUnary
bool exprCast(){
	DEBUG_PRINT("Parsing exprCast\n");	
	Token *start = iTk;
	if(consume(LPAR)){
		Type t;
		if(typeBase(&t)){
			if(arrayDecl(&t)){}
			if(consume(RPAR)){
				if(exprCast()){
					return true;
				}
				else{
					tkerr("missing or invalid expression to be casted");
				}
			}
			else{
				tkerr("missing ')' after cast type");
			}
		}
		else{
			tkerr("missing cast type");
		}
	}
	iTk = start;
	if(exprUnary()){
		return true;
	}
	iTk = start;
    return false;
}

// exprUnary: ( SUB | NOT ) exprUnary | exprPostfix
bool exprUnary(){
	DEBUG_PRINT("Parsing exprUnary\n");	
	Token *start = iTk;
	if(consume(SUB)){
		if(exprUnary()){
			return true;
		}
		else{
			tkerr("missing or invalid expression after '-' operator");
		}
	}
	if(consume(NOT)){
		if(exprUnary()){
			return true;
		}
		else{
			tkerr("missing or invalid expression after '!' operator");
		}
	}
	iTk = start;
	if(exprPostfix()){
		return true;
	}
	iTk = start;
    return false;
}

// exprPostfix: exprPostfix LBRACKET expr RBRACKET | exprPostfix DOT ID | exprPrimary
// devine exprPostfix: exprPrimary exprPostfixPrim
bool exprPostfix(){
	DEBUG_PRINT("Parsing exprPostfix\n");	
	Token *start = iTk;
	if(exprPrimary()){
		if(exprPostfixPrim()){
			return true;
		}
	}
	iTk = start;
    return false;
}

// exprPostfixPrim: LBRACKET expr RBRACKET exprPostfixPrim | DOT ID exprPostfixPrim | ε
// echivalent cu (LBRACKET expr RBRACKET exprPostfixPrim | DOT ID exprPostfixPrim)?
bool exprPostfixPrim(){
	DEBUG_PRINT("Parsing exprPostfixPrim\n");
	Token *start = iTk;	
	if(consume(LBRACKET)){
		if(expr()){
			if(consume(RBRACKET)){
				if(exprPostfixPrim()){
					return true;
				}
			}
			else{
				tkerr("invalid expression between [...] or missing ']'");
			}
		}
	}
	iTk = start;
	if(consume(DOT)){
		if(consume(ID)){
			if(exprPostfixPrim()){
				return true;
			}
		}
		else{
			tkerr("invalid or missing identifier after '.'");
		}
	}
	iTk = start;
	return true;
}

// exprPrimary: ID ( LPAR ( expr ( COMMA expr )* )? RPAR )? | INT | DOUBLE | CHAR | STRING | LPAR expr RPAR
bool exprPrimary(){
	DEBUG_PRINT("Parsing exprPrimary\n");	
	Token *start = iTk;
	if(consume(ID)){
		if(consume(LPAR)){
			if(expr()){
				while(consume(COMMA)){
					if(expr()){}
					else{
						tkerr("missing or invalid additional expression after ','");					
					}
				}
			}
			if(consume(RPAR)){
				return true;
			}
			else{
				tkerr("missing ')' or invalid expression between \"(...)\"");
			}
		}
		return true;
	}
	if(consume(INT)){
		return true;
	}
	if(consume(DOUBLE)){
		return true;
	}
	if(consume(CHAR)){
		return true;
	}
	if(consume(STRING)){
		return true;
	}
	if(consume(LPAR)){
		if(expr()){
			if(consume(RPAR)){
				return true;
			}
			else{
				tkerr("missing or invalid expression between \"(...)\" or missing ')'");
			}
		}
	}
	iTk = start;
    return false;
}

void parse(Token *tokens){
	iTk=tokens;
	if(!unit()){
		tkerr("Syntax error");
	}
}