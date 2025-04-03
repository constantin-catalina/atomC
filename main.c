#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lexer.h"
#include "utils.h"
#include "parser.h"
#include "ad.h"
#include "vm.h"

int main()
{
    char *inbuf = loadFile("tests/testad.c");
    //puts(inbuf);

    Token *tokens = tokenize(inbuf);
    free(inbuf);

    //showTokens(tokens);
    //printTokens(tokens);
    //compareFiles("my-output.txt", "tests/lista-de-atomi.txt");

    pushDomain();

    parse(tokens);

    showDomain(symTable, "global");
    dropDomain();

    return 0;
}