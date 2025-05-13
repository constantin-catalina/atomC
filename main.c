#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lexer.h"
#include "utils.h"
#include "parser.h"
#include "ad.h"
#include "at.h"
#include "vm.h"
#include "gc.h"

int main()
{
    char *inbuf = loadFile("tests/testgc.c");
    // puts(inbuf);

    Token *tokens = tokenize(inbuf);
    free(inbuf);

    // showTokens(tokens);
    // printTokens(tokens);
    // compareFiles("my-output.txt", "tests/lista-de-atomi.txt");

    pushDomain();
    vmInit();

    parse(tokens);

    //Instr *testCode = genTestProgram2();
    //run(testCode);

    // showDomain(symTable, "global");

    Symbol *symMain = findSymbolInDomain(symTable, "main");

    if (!symMain) {
        err("missing main function");
    }

    Instr *entryCode = NULL;
    addInstr(&entryCode, OP_CALL)->arg.instr = symMain->fn.instr;
    addInstr(&entryCode, OP_HALT);
    run(entryCode);

    //dropDomain();

    //free(testCode);
    free(tokens);

    return 0;
}