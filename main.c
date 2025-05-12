#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lexer.h"
#include "utils.h"
#include "parser.h"
#include "ad.h"
#include "at.h"
#include "vm.h"

int main()
{
    char *inbuf = loadFile("tests/testat.c");
    //puts(inbuf);

    Token *tokens = tokenize(inbuf);
    free(inbuf);

    //showTokens(tokens);
    //printTokens(tokens);
    //compareFiles("my-output.txt", "tests/lista-de-atomi.txt");

    pushDomain();
    vmInit();

    parse(tokens);

    Instr *testCode = genTestProgram();
    run(testCode);

    //showDomain(symTable, "global");
    dropDomain();

    free(testCode);
    free(tokens);
    
    return 0;
}