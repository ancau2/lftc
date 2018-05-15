#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#define MAX 30000
typedef struct _Token{
    int code; // codul (numele)
    union{
        char *text; // folosit pentru ID, CT_STRING (alocat dinamic)
        long int i; // folosit pentru CT_INT, CT_CHAR
        double r; // folosit pentru CT_REAL
    };
    int line; // linia din fisierul de intrare
    struct _Token *next; // inlantuire la urmatorul AL
}Token;

int line = 1;

Token *lastToken, *tokens,*consumedTK,*crtTk;
void err(const char *fmt,...){
    va_list va;
    va_start(va,fmt);
    fprintf(stderr,"error: ");
    vfprintf(stderr,fmt,va);
    fputc('\n',stderr);
    va_end(va);
    exit(-1);
}
void tkerr(const Token *tk,const char *fmt,...){
    va_list va;
    va_start(va,fmt);
    fprintf(stderr,"error in line %d: ",tk->line);
    vfprintf(stderr,fmt,va);
    fputc('\n',stderr);
    va_end(va);
    exit(-1);
}
#define SAFEALLOC(var,Type) if((var=(Type*)malloc(sizeof(Type)))==NULL) err("not enough memory");
Token *addTk(int code){
    Token *tk;
    SAFEALLOC(tk,Token)
    tk->code = code;
    tk->line = line;
    tk->next = NULL;
    if(lastToken){
        lastToken->next=tk;
    }else{
        tokens = tk;
    }
    lastToken = tk;
    return tk;
}
enum{
    ID, END,
    BREAK, CHAR, DOUBLE, ELSE, FOR, IF, INT, RETURN, STRUCT, VOID, WHILE,
    CT_INT, CT_REAL, CT_CHAR, CT_STRING,
    SEMICOLON, COMMA, LPAR, RPAR, LBRACKET, RBRACKET, LACC, RACC,
    ADD, SUB, MUL, DIV, DOT, AND, OR, NOT, ASSIGN, EQUAL, NOTEQ, LESS, LESSEQ, GREATER, GREATEREQ
};
enum{TB_INT,TB_DOUBLE,TB_CHAR,TB_STRUCT,TB_VOID};
enum{CLS_VAR,CLS_FUNC,CLS_EXTFUNC,CLS_STRUCT};
enum{MEM_GLOBAL,MEM_ARG,MEM_LOCAL};
struct _Symbol;
typedef struct _Symbol Symbol;
typedef struct{
    int typeBase; // TB_*
    Symbol *s; // struct definition for TB_STRUCT
    int nElements; // >0 array of given size, 0=array without size, <0 non array
}Type;
typedef struct{
    Symbol **begin; // the beginning of the symbols, or NULL
    Symbol **end; // the position after the last symbol
    Symbol **after; // the position after the allocated space
}Symbols;
typedef struct _Symbol{
    const char *name; // a reference to the name stored in a token
    int cls; // CLS_*
    int mem; // MEM_*
    Type type;
    int depth; // 0-global, 1-in function, 2... - nested blocks in function
    union{
        Symbols args; // used only of functions
        Symbols members; // used only for structs
    };
}Symbol;
Symbols symbols;

int crtDepth = 0;
Symbol *crtFunc = NULL;
Symbol *crtStruct = NULL;


void initSymbols(Symbols *symbols){
    symbols->begin = NULL;
    symbols->end = NULL;
    symbols->after = NULL;
}

Symbol *findSymbol(Symbols *symbols,const char *name){
    Symbol **s = symbols->end-1;
    while(s != symbols->begin-1){

        if(strcmp((*s)->name,name) == 0){
            return *s;
        }
        s = s-1;
    }
    return NULL;
}

Symbol *addSymbol(Symbols *symbols,const char *name,int cls) {
    Symbol *s;
    if(symbols->end == symbols->after){ // create more room
        int count = symbols->after - symbols->begin;
        int n = count*2; // double the room
        if(n == 0)
            n = 1;
        symbols->begin = (Symbol**)realloc(symbols->begin, n*sizeof(Symbol*));
        if(symbols->begin == NULL)
            err("not enough memory");
        symbols->end = symbols->begin+count;
        symbols->after = symbols->begin+n;
    }
    SAFEALLOC(s,Symbol)
    *symbols->end++=s;
    s->name = strdup(name);
    s->cls = cls;
    s->depth = crtDepth;
    return s;
}

void deleteSymbolsAfter(Symbols *symbols,Symbol *after){
    Symbol **s = symbols->end-1;
    while(*s != after && s != symbols->begin){
       // printf("!%s!\n",(*s)->name);
        free(*s);
        s--;

    }
    symbols->end = s+1;
}

char *getClass(int code){
    if(code == 0)
        return "CLS_VAR";
    if(code == 1)
        return "CLS_FUNC";
    if(code == 2)
        return "CLS_EXTFUNC";
    if(code == 3)
        return "CLS_STRUCT";
    return NULL;
}

char *getMem(int code){
    if(code == 0)
        return "MEM_GLOBAL";
    if(code == 1)
        return "MEM_ARG";
    if(code == 2)
        return "MEM_LOCAL";
    return NULL;
}

char *getType(int code){
    if(code == 0)
        return "TB_INT";
    if(code == 1)
        return "TB_DOUBLE";
    if(code == 2)
        return "TB_CHAR";
    if(code == 3)
        return "TB_STRUCT";
    if(code == 4)
        return "TB_VOID";
    return NULL;
}

void showSymbols(Symbols *symbols){
    Symbol **s = symbols->begin;
    Type t;
    while(s != symbols->end){
        t = (*s)->type;
        printf("%s cls: %s  mem: %s type %s\n",(*s)->name,getClass((*s)->cls),getMem((*s)->mem),getType(t.typeBase));
        s++;
    }

}

int unit();
int declStruct();
int declVar(Type *ret);
int typeBase(Type *ret);
int arrayDecl(Type *ret);
int typeName(Type *ret);
int declFunc();
int funcArg();
int declFuncAux(Type t);
int stm();
int stmCompound();
int expr();
int exprAssign();
int exprOr();
int exprAnd();
int exprEq();
int exprRel();
int exprAdd();
int exprMul();
int exprCast();
int exprUnary();
int exprPostfix();
int exprPrimary();
char inbuf[MAX];
char *pch;

void addVar(Token *tkName,Type *t){
    Symbol *s;
    if(crtStruct){
        if(findSymbol(&crtStruct->members,tkName->text))
            tkerr(crtTk,"symbol redefinition: %s",tkName->text);
        s = addSymbol(&crtStruct->members,tkName->text,CLS_VAR);
    }
    else if(crtFunc){
        s = findSymbol(&symbols,tkName->text);
        if(s&&s->depth==crtDepth)
            tkerr(crtTk,"symbol redefinition: %s",tkName->text);
        s = addSymbol(&symbols,tkName->text,CLS_VAR);
        s->mem = MEM_LOCAL;
    }
    else{
        if(findSymbol(&symbols,tkName->text))
            tkerr(crtTk,"symbol redefinition: %s",tkName->text);
        s = addSymbol(&symbols,tkName->text,CLS_VAR);
        s->mem = MEM_GLOBAL;
    }
    s->type = *t;
}

int getEnum(int v){
    switch(v){
        case 0:
            printf( "ID");
            return 5;
            break;
        case 1:
            printf( "END");
            break;
        case 2:
            printf( "BREAK");
            break;
        case 3:
            printf( "CHAR");
            break;
        case 4:
            printf("DOUBLE");
            break;
        case 5:
            printf( "ELSE");
            break;
        case 6:
            printf( "FOR");
            break;
        case 7:
            printf( "IF");
            break;
        case 8:
            printf("INT");
            break;
        case 9:
            printf( "RETURN");
            break;
        case 10:
            printf( "STRUCT");
            break;
        case 11:
            printf( "VOID");
            break;
        case 12:
            printf( "WHILE");
            break;
        case 13:
            printf( "CT_INT");
            return 1;
            break;
        case 14:
            printf( "CT_REAL");
            return 2;
            break;
        case 15:
            printf( "CT_CHAR");
            return 3;
            break;
        case 16:
            printf( "CT_STRING");
            return 4;
            break;
        case 17:
            printf( "SEMICOLON");
            break;
        case 18:
            printf( "COMMA");
            break;
        case 19:
            printf( "LPAR");
            break;
        case 20:
            printf( "RPAR");
            break;
        case 21:
            printf( "LBRACKET");
            break;
        case 22:
            printf( "RBRACKET");
            break;
        case 23:
            printf( "LACC");
            break;
        case 24:
            printf( "RACC");
            break;
        case 25:
            printf( "ADD");
            break;
        case 26:
            printf( "SUB");
            break;
        case 27:
            printf( "MUL");
            break;
        case 28:
            printf( "DIV");
            break;
        case 29:
            printf( "DOT");
            break;
        case 30:
            printf( "AND");
            break;
        case 31:
            printf( "OR");
            break;
        case 32:
            printf( "NOT");
            break;
        case 33:
            printf( "ASSIGN");
            break;
        case 34:
            printf( "EQUAL");
            break;
        case 35:
            printf( "NOTEQ");
            break;
        case 36:
            printf( "LESS");
            break;
        case 37:
            printf( "LESSEQ");
            break;
        case 38:
            printf( "GREATER");
            break;
        case 39:
            printf( "GREATEREQ");
            break;
        default:
            printf("STARE INEXISTENTA\n");
            exit(1);
    }
    return 0;
}

char exChar(char ch){
    switch(ch){
        case 'n':
            return '\n';
        case 't':
            return '\t';
        case 'r':
            return '\r';
        case 'a':
            return '\a';
        case 'v':
            return '\v';
        case 'b':
            return '\b';
        case 'f':
            return '\f';
        case '?':
            return '\?';
        case '0':
            return '\0';
        case '"':
            return '\"';
        default:
            return ch;
    }
}

char *createString(char *start,char *end){
    int len = end - start + 1;
    char *stri = (char *)malloc(sizeof(char)*len);
    char *c = start;
    int i = 0;
    while(c<end){
        char a = *c;
        stri[i] = a;
        c++;
        i++;
    }
    stri[i] = '\0';
    return stri;
}

int getNextToken(){
    int s=0;
    char ch;
    char stri[2];
    char *pStartCh;
    Token *tk;
    int nCh;
    int base = 10;
    while(1){
        ch=*pch;
        //printf("#starea %d cu ~%c~(%d)\n",s,ch,ch);
        switch(s){
            case 0:
                if((isdigit(ch))&&(ch=='0')){
                    pStartCh = pch;
                    s = 3;
                    pch++;
                }else if((isdigit(ch))&&((ch>='1')&&(ch<='9'))){
                    s = 2;
                    pStartCh = pch;
                    pch++;
                }else if((isalpha(ch))||(ch=='_')){
                    s = 46;
                    pStartCh = pch;
                    pch++;
                }else if(ch=='\''){
                    s = 14;
                    pStartCh = pch;
                    pch++;
                }else if(ch=='\"'){
                    s = 15;
                    pStartCh = pch;
                    pch++;
                }else if((ch==' ')||(ch=='\n')||(ch=='\r')||(ch=='\t')){
                    if(ch=='\n')
                        line++;
                    pch++;
                }else if(ch=='/'){
                    pch++;
                    s = 22;
                }else if(ch==','){
                    s = 48;
                    pch++;
                }else if(ch==';'){
                    s = 49;
                    pch++;
                }else if(ch=='('){
                    s = 50;
                    pch++;
                }else if(ch==')'){
                    s = 51;
                    pch++;
                }else if(ch=='['){
                    s = 52;
                    pch++;
                }else if(ch==']'){
                    s = 53;
                    pch++;
                }else if(ch=='{'){
                    s = 54;
                    pch++;
                }else if(ch=='}'){
                    s = 55;
                    pch++;
                }else if(ch=='+'){
                    s = 26;
                    pch++;
                }else if(ch=='-'){
                    s = 27;
                    pch++;
                }else if(ch=='*'){
                    s = 28;
                    pch++;
                }else if(ch=='.'){
                    s = 29;
                    pch++;
                }else if(ch=='&'){
                    s = 30;
                    pch++;
                }else if(ch=='|'){
                    s = 32;
                    pch++;
                }else if(ch=='!'){
                    s = 34;
                    pch++;
                }else if(ch=='='){
                    s = 35;
                    pch++;
                }else if(ch=='<'){
                    s = 40;
                    pch++;
                }else if(ch=='>'){
                    s = 43;
                    pch++;
                }else if(ch == 0){
                    addTk(END);
                    return END;
                }else{
                    tkerr(addTk(END),"caracter invalid");
                    printf("Eroare:caracter invalid\n");
                }
                break;
            case 2:
                if(isdigit(ch)){
                    pch++;
                }else if(ch=='.'){
                    s = 8;
                    pch++;
                }else if((isalpha(ch))&&((ch=='e')||(ch=='E'))){
                    s = 10;
                    pch++;
                }else{
                    s = 7;
                }
                break;
            case 3:
                if((isdigit(ch))&&((ch<='7')||(ch>='0'))){
                    s = 4;
                    pch++;
                }else if(ch=='.'){
                    s = 8;
                    pch++;
                }else if((isalpha(ch))&&((ch=='e')||ch=='E')){
                    s = 10;
                    pch++;
                }else if((isalpha(ch))&&(ch=='x')){
                    s = 5;
                    pch++;
                }else {
                    s = 7;
                }
                break;
            case 4:
                if((isdigit(ch))&&((ch<='7')||(ch>='0'))){
                    pch++;
                    base = 8;
                }else if(ch=='.'){
                    pch++;
                    s = 8;
                }else if((isalpha(ch))&&((ch=='e'||ch=='E'))){
                    s = 10;
                    pch++;
                }else if((isdigit(ch))&&((ch=='9')||(ch=='8'))){
                    s = 56;
                    pch++;
                }else{
                    s = 7;
                }
                break;
            case 5:
                if((isdigit(ch))||((isalpha(ch))&&(((ch<='F')&&(ch>='A'))||((ch<='f')&&(ch>='a'))))){
                    s = 6;
                    base = 0;
                    pch++;
                }else
                { printf("Eroare caracter invalid 5\n"); return 0;}

                break;
            case 6:
                if((isdigit(ch))||((isalpha(ch))&&(((ch<='F')&&(ch>='A'))||((ch<='f')&&(ch>='a'))))){
                    pch++;
                }else s = 7;
                break;
            case 7:
                //STARE FINALA
                tk = addTk(CT_INT);
                nCh = pch - pStartCh;
                char *nr = createString(pStartCh,pch);
                long l =  strtol(nr,NULL,base);
                tk->i = l;
                return CT_INT;
            case 8:
                if(isdigit(ch)){
                    s = 9;
                    pch++;
                }
                break;
            case 9:
                if(isdigit(ch)){
                    pch++;
                }else if((ch=='e')||(ch=='E')){
                    s = 10;
                    pch++;
                }else
                    s = 13;
                break;
            case 10:
                if((ch=='+')||(ch=='-')){
                    s = 11;
                    pch++;
                }else{
                    s = 11;
                }
                break;
            case 11:
                if(isdigit(ch)){
                    s = 12;
                    pch++;
                }else { printf("Eroare caracter invalid 11\n"); return 0;}
                break;
            case 12:
                if(isdigit(ch)){
                    pch++;
                }else{
                    s = 13;
                }
                break;
            case 13:
                //STARE FINALA
                tk=addTk(CT_REAL);
                char* str=createString(pStartCh,pch);
                tk->r=atof(str);
                return CT_REAL;
            case 14:
                if(ch=='\\'){
                    s = 16;
                    char buf[MAX];
                    strcpy(buf,pch+1);
                    strcpy(pch,buf);
                }else if ((ch!='\'')&&(ch!='\\')){
                    s = 18;
                    pch++;
                }
                break;
            case 15:
                if(ch=='\"'){
                    s = 59;
                    pch++;
                }else if(ch == '\\'){
                    s = 17;
                    char buf[MAX];
                    strcpy(buf,pch+1);
                    strcpy(pch,buf);
                }else if((ch != '\"') && (ch != '\\')){
                    s = 58;
                    pch++;
                }
                break;
            case 16:
                stri[0] = ch;
                stri[1] = '\0';
                if(strstr("abnfrtv\'?\\\"\0",stri)!=NULL){
                    s = 18;
                    pch[0] = exChar(pch[0]);
                    pch++;
                }else
                { printf("Eroare caracter invalid 16\n"); return 0;}
                break;
            case 17:
                stri[0] = ch;
                stri[1] = '\0';
                if(strstr("abnfrtv'?\"\0",stri)!=NULL){
                    s = 58;
                    pch[0] = exChar(pch[0]);
                    pch++;
                }else { printf("Eroare caracter invalid 17\n"); return 0;}
                break;
            case 18:
                if(ch=='\''){
                    s = 19;
                    pch++;
                }else { printf("Eroare caracter invalid 18\n"); return 0;}
                break;
            case 19:
                //STARE FINALA
                tk=addTk(CT_CHAR);
                char* character=createString(pStartCh,pch-1);
                tk->i=character[1];

                return CT_CHAR;
            case 21:
                stri[0] = ch;
                stri[1] = '\0';
                if(strstr("\n\r\0",stri)==NULL){
                    pch++;
                }else{
                    s=0;
                }
                break;
            case 22:
                if(ch == '*'){
                    s = 23;
                    pch++;
                }else if(ch=='/'){
                    s = 21;
                    pch++;
                }else
                    s = 25;
                break;
            case 23:
                if(ch != '*'){
                    pch++;
                } else{
                    s = 24;
                    pch++;
                }
                break;
            case 24:
                if(ch == '/'){
                    s = 0;
                    pch++;
                }else if(ch != '*'){
                    s = 23;
                    pch++;
                } else if(ch == '*'){
                    pch++;
                }
                break;
            case 25:
                //STARE FINALA
                addTk(DIV);
                return DIV;
            case 26:
                //STARE FINALA
                addTk(ADD);
                return ADD;
            case 27:
                //STARE FINALA
                addTk(SUB);
                return SUB;
            case 28:
                //STARE FINALA
                addTk(MUL);
                return MUL;
            case 29:
                //STARE FINALA
                addTk(DOT);
                return DOT;
            case 30:
                if(ch=='&'){
                    s = 31;
                    pch++;
                }else {
                    printf("Eroare caracter invalid 30\n");
                    return 0;
                }
                break;
            case 31:
                //STARE FINALA
                addTk(AND);
                return AND;
            case 32:
                if(ch=='|'){
                    s = 33;
                    pch++;
                }else {
                    printf("Eroare caracter invalid32\n");
                    return 0;
                }
                break;
            case 33:
                //STARE FINALA
                addTk(OR);
                return OR;
            case 34:
                if(ch == '='){
                    s = 39;
                    pch++;
                }else{
                    s = 38;
                }
                break;
            case 35:
                if(ch == '='){
                    s = 37;
                    pch++;
                }else{
                    s = 36;
                }
                break;
            case 36:
                //STARE FINALA
                addTk(ASSIGN);
                return ASSIGN;
            case 37:
                //STARE FINALA
                addTk(EQUAL);
                return EQUAL;
            case 38:
                //STARE FINALA
                addTk(NOT);
                return NOT;
            case 39:
                //STARE FINALA
                addTk(NOTEQ);
                return NOTEQ;
            case 40:
                if(ch == '='){
                    s = 42;
                    pch++;
                }else
                    s = 41;
                break;
            case 41:
                //STARE FINALA
                addTk(LESS);
                return LESS;
            case 42:
                //STARE FINALA
                addTk(LESSEQ);
                return LESSEQ;
            case 43:
                if(ch == '='){
                    s = 45;
                    pch++;
                }else
                    s = 44;
                break;
            case 44:
                //STARE FINALA
                addTk(GREATER);
                return GREATER;
            case 45:
                //STARE FINALA
                addTk(GREATEREQ);
                return GREATEREQ;
            case 46:
                if(isalpha(ch)||isdigit(ch)||(ch == '_')){
                    pch++;
                }else
                    s = 47;
                break;
            case 47:
                nCh = pch - pStartCh;
                if((nCh == 5) && (!memcmp(pStartCh,"break",5)))
                    tk = addTk(BREAK);
                else if((nCh == 4) && (!memcmp(pStartCh,"char",4)))
                    tk = addTk(CHAR);
                else if((nCh == 6) && (!memcmp(pStartCh,"double",6)))
                    tk = addTk(DOUBLE);
                else if((nCh == 4) && (!memcmp(pStartCh,"else",4)))
                    tk = addTk(ELSE);
                else if((nCh == 3) && (!memcmp(pStartCh,"for",3)))
                    tk = addTk(FOR);
                else if((nCh == 2) && (!memcmp(pStartCh,"if",2)))
                    tk = addTk(IF);
                else if((nCh == 3) && (!memcmp(pStartCh,"int",3)))
                    tk = addTk(INT);
                else if((nCh == 6) && (!memcmp(pStartCh,"return",6)))
                    tk = addTk(RETURN);
                else if((nCh == 6) && (!memcmp(pStartCh,"struct",6)))
                    tk = addTk(STRUCT);
                else if((nCh == 4) && (!memcmp(pStartCh,"void",4)))
                    tk = addTk(VOID);
                else if((nCh == 5) && (!memcmp(pStartCh,"while",5)))
                    tk = addTk(WHILE);
                else{
                    tk = addTk(ID);
                    tk->text = createString(pStartCh,pch);
                }
                s = 0;
                return tk->code;
            case 48:
                //STARE FINALA
                addTk(COMMA);
                return COMMA;
            case 49:
                //STARE FINALA
                addTk(SEMICOLON);
                return SEMICOLON;
            case 50:
                //STARE FINALA
                addTk(LPAR);
                return LPAR;
            case 51:
                //STARE FINALA
                addTk(RPAR);
                return RPAR;
            case 52:
                //STARE FINALA
                addTk(LBRACKET);
                return LBRACKET;
            case 53:
                //STARE FINALA
                addTk(RBRACKET);
                return RBRACKET;
            case 54:
                //STARE FINALA
                addTk(LACC);
                return LACC;
            case 55:
                //STARE FINALA
                addTk(RACC);
                return RACC;
            case 56:
                if(isdigit(ch)){
                    pch++;
                }else if((ch == 'e')||(ch == 'E')){
                    s = 10;
                    pch++;
                }else if(ch == '.'){
                    s = 8;
                    pch++;
                }else printf("Caracter invalid\n");
                break;
            case 58:
                if(ch == '\"'){
                    s = 59;
                    pch++;
                }else{
                    s = 15;
                    //pch++;
                }
                break;
            case 59:
                //STARE FINALA
                tk=addTk(CT_STRING);
                tk->text=createString(pStartCh+1,pch-1);
                return CT_STRING;
            default:
                printf("stare netratata %d\n",s);
                break;
        }
    }
}

void afisare(){
    Token *t = tokens;
    while(t){
        int p = getEnum(t->code);
        switch(p){
            case 1:
                printf(" -> %ld",t->i);
                break;
            case 2:
                printf(" -> %f",t->r);
                break;
            case 3:
                printf(" -> %ld",t->i);
                break;
            case 4:
                printf(" -> %s",t->text);
                break;
            case 5:
                printf(" -> %s",t->text);
                break;
            default:
                break;
        }
        printf("\n");
        t = t->next;
    }
}

int consume(int code){
    if(crtTk->code == code){
        // printf("consume ->");
        // if(getEnum(crtTk->code) == 5){
        // 	printf("  %s",crtTk->text);
        // }
        // printf("\n");
        consumedTK = crtTk;
        crtTk = crtTk->next;
        return 1;
    }else
        return 0;
}

int declStruct(){
    // printf("declStruct ->");
    // getEnum(crtTk->code); printf("\n");

    Token *initTk = crtTk;

    if(consume(STRUCT)){
        if(consume(ID)){
            Token *tkName = consumedTK;
            if(consume(LACC)){
                if(findSymbol(&symbols,tkName->text))
                    tkerr(crtTk,"symbol redefinition: %s",tkName->text);
                crtStruct = addSymbol(&symbols,tkName->text,CLS_STRUCT);
                initSymbols(&crtStruct->members);
                while(true){
                    Type t;
                    if(declVar(&t)){}
                    else break;
                }
                if(consume(RACC)){
                    if(consume(SEMICOLON)){
                        crtStruct = NULL;
                        return 1;
                    }else{
                        tkerr(crtTk,"Lipseste ; -> declStruct");
                    }
                }else{
                    tkerr(crtTk,"Lipseste } -> declStruct");
                }
            }
        }else{
            tkerr(crtTk,"Lipseste definitia ID");
        }
    }
    crtTk = initTk;
    return 0;
}

int typeBase(Type *ret){
    // printf("typeBase ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(consume(INT)){
        ret->typeBase = TB_INT;
        return 1;
    }
    if(consume(DOUBLE)){
        ret->typeBase = TB_DOUBLE;
        return 1;

    }
    if(consume(CHAR)){
        ret->typeBase = TB_CHAR;
        return 1;
    }

    if(consume(STRUCT)){
        if(consume(ID)){
            Token *tkName = consumedTK;
            Symbol *s = findSymbol(&symbols,tkName->text);
            if(s==NULL)
                tkerr(crtTk,"undefined symbol: %s",tkName->text);
            if(s->cls != CLS_STRUCT)
                tkerr(crtTk,"%s is not a struct",tkName->text);
            ret->typeBase = TB_STRUCT;
            ret->s = s;
            return 1;
        }
        else
            tkerr(crtTk,"Lipseste ID -> typeBase");
    }

    crtTk = initTk;
    return 0;
}

int declVar(Type *ret){
    // printf("declVar ->");
     //getEnum(crtTk->code); printf("\n");

    Token *initTk = crtTk;
    Type t;
    Token *tkName;
    if(typeBase(&t)){
        if(consume(ID)){
            tkName = consumedTK;
            if(!arrayDecl(&t))
                t.nElements = -1;
            addVar(tkName,&t);
            while(true){
                if(consume(COMMA)){
                    if(consume(ID)){
                        tkName = consumedTK;
                        fflush(stdout);
                        if(!arrayDecl(&t))
                            t.nElements = -1;
                    }else{
                        tkerr(crtTk,"Lipseste ID -> declVar 1");
                    }
                    addVar(tkName,&t);
                }else break;
            }
            if(consume(SEMICOLON)){
                return 1;
            }else{
                tkerr(crtTk,"Lipseste ; -> declVar");
            }
        }
    }

    crtTk = initTk;
    return 0;
}

int arrayDecl(Type *ret){
    // printf("arrayDecl ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(consume(LBRACKET)){
        if(expr()){
            ret->nElements = 0;
        }
        if(consume(RBRACKET)){
            return 1;
        }else{
            tkerr(crtTk,"Lipseste ] -> arrayDecl");
        }
    }

    crtTk = initTk;
    return 0;
}

int typeName(Type *ret){
    // printf("typeName ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(!typeBase(ret)){
        tkerr(crtTk,"Lipseste typeBase");
        crtTk = initTk;
        return 0;
    }

    if(arrayDecl(ret)){}
    else
        ret->nElements = -1;

    return 1;
}

int unit(){
    // printf("unit ->");
    // getEnum(crtTk->code); printf("\n");

    while(true){
        Type t;
        if(declStruct() || declFunc() || declVar(&t)) {}
        else break;
    }
    if(consume(END)) {
        return 1;
    }
    else{
        tkerr(crtTk, "Lipseste END");
    }

    return 0;
}

int declFuncAux(Type t){
    // printf("declFuncAux ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(consume(ID)){
        Token *tkName = consumedTK;
        if(consume(LPAR)){
            if(findSymbol(&symbols,tkName->text))
                tkerr(crtTk,"symbol redefinition: %s",tkName->text);
            crtFunc = addSymbol(&symbols,tkName->text,CLS_FUNC);
            initSymbols(&crtFunc->args);
            crtFunc->type = t;
            crtDepth++;
            if(funcArg()){
                while(true){
                    if(consume(COMMA)){
                        if(funcArg()){}
                        else{
                            tkerr(crtTk,"Lipseste funcArg");
                        }
                    }else break;
                }
            }
            if(consume(RPAR)){
                crtDepth--;
                if(stmCompound()){
                    deleteSymbolsAfter(&symbols,crtFunc);
                    crtFunc = NULL;
                    return 1;
                }else{
                    tkerr(crtTk,"Lipseste stmCompound");
                }
            }else{
                tkerr(crtTk,"Lipseste )");
            }
        }
    }

    crtTk = initTk;
    return 0;
}

int declFunc(){
    // printf("declFunc ->");
    // getEnum(crtTk->code); printf("\n");

    Token *initTk = crtTk;
    Type t;

    if(typeBase(&t)){
        if(consume(MUL))
            t.nElements = 0;
        else
            t.nElements = -1;
        if(declFuncAux(t))
            return 1;
    } else if (consume(VOID)){
        t.typeBase = TB_VOID;
        if(!declFuncAux(t)){
            tkerr(crtTk,"declaratie invalida de functie");
        }
        return 1;
    }

    crtTk = initTk;
    return 0;
}

int funcArg(){
    // printf("funcArg ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;
    Type t;
    Token *tkName;

    if(typeBase(&t)){
        if(consume(ID)){
            tkName = consumedTK;
            if(!arrayDecl(&t))
                t.nElements=-1;
            Symbol *s = addSymbol(&symbols,tkName->text,CLS_VAR);
            s->mem = MEM_ARG;
            s->type = t;
            s = addSymbol(&crtFunc->args,tkName->text,CLS_VAR);
            s->mem = MEM_ARG;
            s->type = t;
            return 1;
        }else{
            tkerr(crtTk,"Lipseste ID -> funcArg");
        }
    }

    crtTk = initTk;
    return 0 ;
}

int stmCompound(){
    // printf("stmCompound ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;
    Symbol *start = symbols.end[-1];

    if(consume(LACC)){
        crtDepth++;
        while(true){
            Type t;
            if(declVar(&t) || stm()){}
            else break;
        }
        if(consume(RACC)){
            crtDepth--;
            deleteSymbolsAfter(&symbols,start);
            return 1;
        }else{
            tkerr(crtTk,"Lipseste }");
        }
    }

    crtTk = initTk;
    return 0;
}

int stm(){
    // printf("stm ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(stmCompound())
        return 1;

    if(consume(IF)){
        if(consume(LPAR)){
            if(expr()){
                if(consume(RPAR)){
                    if(stm()){
                        if(consume(ELSE)){
                            if(stm()){}
                        }
                        return 1;
                    }else tkerr(crtTk,"Lipseste stm");
                }else tkerr(crtTk,"Lipseste )");
            }else tkerr(crtTk,"Lipseste expr");
        }else tkerr(crtTk,"Lipseste (");
    }

    if(consume(WHILE)){
        if(consume(LPAR)){
            if(expr()){
                if(consume(RPAR)){
                    if(stm()){
                        return 1;
                    }else tkerr(crtTk,"Lipseste stm");
                }else tkerr(crtTk,"Lipseste )");
            }else tkerr(crtTk,"Lipseste expr");
        } else tkerr(crtTk,"Lipseste (");
    }

    if(consume(FOR)){
        if(consume(LPAR)){
            expr();
            if(consume(SEMICOLON)){
                expr();
                if(consume(SEMICOLON)){
                    expr();
                    if(consume(RPAR)){
                        if(stm()){
                            return 1;
                        }else tkerr(crtTk,"Lipseste stm");
                    }else tkerr(crtTk,"Lipseste )");
                }else tkerr(crtTk,"Lipseste ; -> stm 1");
            }else tkerr(crtTk,"Lipseste ; -> stm 2");
        }else tkerr(crtTk,"Lipseste ( -> for");
    }

    if(consume(BREAK)){
        if(consume(SEMICOLON)){
            return 1;
        }else tkerr(crtTk,"Lipseste ; -> stm 3");
    }

    if(consume(RETURN)){
        expr();
        if(consume(SEMICOLON)){
            return 1;
        }else tkerr(crtTk,"Lipseste ; -> stm 4");
    }

    if(expr()){
        if(consume(SEMICOLON)){
            return 1;
        }else tkerr(crtTk,"Lipseste ; -> stm 5");
    }else{
        if(consume(SEMICOLON)){
            return 1;
        }
    }

    crtTk = initTk;
    return 0;
}

int exprOrAux(){
    // printf("exprOrAux ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(consume(OR)){
        if(exprAnd()){
            if(exprOrAux()){
                return 1;
            }else{
                tkerr(crtTk,"Declarare incorecta OR");
            }
        }else{
            tkerr(crtTk,"Lipseste exprAnd");
        }
    }

    crtTk = initTk;
    return 1;
}

int exprOr(){
    // printf("exprOr ->");
    // if(getEnum(crtTk->code)==5){
    // 	printf("%s\n",crtTk->text );
    // }

    // printf("\n");
    Token *initTk = crtTk;

    if(exprAnd()){
        if(exprOrAux()){
            return 1;
        }else{
            tkerr(crtTk,"Declarare incorecta OR");
        }
    }

    crtTk = initTk;
    return 0;
}

int exprAndAux(){
    // printf("exprAndAux ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(consume(AND)){
        if(exprEq()){
            if(exprAndAux()){
                return 1;
            }else{
                tkerr(crtTk,"Declarare incorecta AND");
            }
        }else {
            tkerr(crtTk,"Lipseste exprEq");
        }
    }

    crtTk = initTk;
    return 1;
}

int exprAnd(){
    // printf("exprAnd ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(exprEq()){
        if(exprAndAux()){
            return 1;
        }else{
            tkerr(crtTk,"Declarare incorecta AND");
        }
    }

    crtTk = initTk;
    return 0;
}

int exprEqAux(){
    // printf("exprEqAux ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(consume(EQUAL) || consume(NOTEQ)){
        if(exprRel()){
            if(exprEqAux()){
                return 1;
            } else{
                tkerr(crtTk,"Declarare incorecta EQ");
            }
        } else{
            tkerr(crtTk,"Lipseste exprRel()");
        }
    }

    crtTk = initTk;
    return 1;
}

int exprEq(){
    // printf("exprEq ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(exprRel()){
        if(exprEqAux()){
            return 1;
        }else{
            tkerr(crtTk,"Declarare incorecta exprEq");
        }
    }

    crtTk = initTk;
    return 0;
}

int exprAssign(){
    // printf("exprAssign ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(exprUnary()){
        if(consume(ASSIGN)){
            if(exprAssign()){
                return 1;
            }else{
                tkerr(crtTk,"Lipseste exprAssign");
            }
        }
        crtTk = initTk;
    }

    if(exprOr())
        return 1;

    crtTk = initTk;
    return 0;
}

int exprRelAux(){
    // printf("exprRelAux ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(consume(LESS) || consume(LESSEQ) || consume(GREATER) || consume(GREATEREQ)){
        if(exprAdd()){
            if(exprRelAux()){
                return 1;
            }else{
                tkerr(crtTk,"Declarare incorecta exprRel");
            }
        }else{
            tkerr(crtTk,"Lipseste exprAnd");
        }
    }

    crtTk = initTk;
    return 1;
}

int exprRel(){
    // printf("exprRel ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(exprAdd()){
        if(exprRelAux()){
            return 1;
        }else{
            tkerr(crtTk,"Declarare incorecta exprRel");
        }
    }

    crtTk = initTk;
    return 0;
}

int exprAddAux(){
    // printf("exprAddAux ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(consume(ADD) || consume(SUB)){
        if(exprMul()){
            if(exprAddAux()){
                return 1;
            }else{
                tkerr(crtTk,"Declarare incorecta exprAdd");
            }
        }else{
            tkerr(crtTk,"Lipseste exprMul");
        }
    }

    crtTk = initTk;
    return 1;
}

int exprAdd(){
    // printf("exprAdd ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(exprMul()){
        if(exprAddAux()){
            return 1;
        }else{
            tkerr(crtTk,"Declarare incorecta exprAdd");
        }
    }

    crtTk = initTk;
    return 0;
}

int exprMulAux(){
    // printf("exprMulAux ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(consume(MUL) || consume(DIV)){
        if(exprCast()){
            if(exprMulAux()){
                return 1;
            }else{
                tkerr(crtTk,"Declarare incorecta exprMul");
            }
        }else{
            tkerr(crtTk,"Lipseste exprMul");
        }
    }

    crtTk = initTk;
    return 1;
}

int exprMul(){
    // printf("exprMul ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(exprCast()){
        if(exprMulAux()){
            return 1;
        }else{
            tkerr(crtTk,"Declarare incorecta exprMul");
        }
    }

    crtTk = initTk;
    return 0;
}

int exprCast(){
    // printf("exprCast ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;
    Type t;

    if(consume(LPAR)){
        if(typeName(&t)){
            if(consume(RPAR)){
                if(exprCast()){
                    return 1;
                }else{
                    tkerr(crtTk,"Lipseste exprCast");
                }
            }else{
                tkerr(crtTk,"Lipseste )");
            }
        }else{
            tkerr(crtTk,"Lipseste typeName");
        }
    }
    crtTk = initTk;

    if(exprUnary())
        return 1;

    crtTk = initTk;
    return 0;
}

int exprUnary(){
    // printf("exprUnary ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(consume(SUB) || consume(NOT)){
        if(exprUnary()){
            return 1;
        }else{
            tkerr(crtTk,"Lipseste exprUnary");
        }
    }

    if(exprPostfix())
        return 1;

    crtTk = initTk;
    return 0;
}

int exprPostfixAux(){
    // printf("exprPostfixAux ->");
    // getEnum(crtTk->code);
    printf("\n");
    Token *initTk = crtTk;

    if(consume(DOT)){
        if(consume(ID)){
            if(exprPostfixAux()){
                return 1;
            }else{
                tkerr(crtTk,"Declarare incorecta exprPostfix");
            }
        }else{
            tkerr(crtTk,"Lipseste ID -> exprPostfixAux");
        }
    }

    if(consume(LBRACKET)){
        if(expr()){
            if(consume(RBRACKET)){
                if(exprPostfixAux()){
                    return 1;
                }else{
                    tkerr(crtTk,"Declarare incorecta exprPostfix");
                }
            }else{
                tkerr(crtTk,"Lipseste } -> exprPostfix");
            }
        }else{
            tkerr(crtTk,"Lipseste expr");
        }
    }

    crtTk = initTk;
    return 1;
}

int exprPostfix(){
    // printf("exprPostfix ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(exprPrimary()){
        if(exprPostfixAux()){
            return 1;
        }else{
            tkerr(crtTk,"Declarare incorecta exprPostfix");
        }
    }

    crtTk = initTk;
    return 0;
}

int exprPrimary(){
    // printf("exprPrimary ->");
    // getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(consume(ID)){
        if(consume(LPAR)){
            if(expr()){
                while(consume(COMMA)){
                    if(expr()){}
                    else
                        tkerr(crtTk,"Lipseste expr dupa ,");
                }
            }
            if(consume(RPAR))
                return 1;
            else
                tkerr(crtTk,"Lipseste ) in apel de func");
        }
        return 1;
    }

    if(consume(CT_INT)){
        return 1;
    }

    if(consume(CT_REAL)){
        return 1;
    }

    if(consume(CT_STRING)){
        return 1;
    }

    if(consume(CT_CHAR)){
        return 1;
    }

    if(consume(LPAR)){
        if(expr()){
            if(consume(RPAR)){
                return 1;
            }else tkerr(crtTk,"Lipseste )");
        }else tkerr(crtTk,"Lipseste expr");
    }

    crtTk = initTk;
    return 0;
}

int expr(){
    Token* initTk = crtTk;
    // printf("expr ->");
    // getEnum(crtTk->code);
    // printf("\n");

    if(exprAssign())
        return 1;

    crtTk = initTk;
    return 0;
}

void terminare(){

    Token *tmp;

    while(tokens != NULL){
        tmp = tokens->next;
        free(tokens);
        tokens = tmp;
    }
}

int main(int argc, char const *argv[]){

    if(argc != 2){
        printf("Calea catre fisierul de intrare nu a fost introdusa.\n");
        exit(EXIT_FAILURE);
    }

    int n;
    FILE *fis = fopen(argv[1],"r");
    n = fread(inbuf,1,MAX,fis);
    inbuf[n] = '\0';
    pch = inbuf;
    while(getNextToken() != END);

    //afisare();
    crtTk = tokens;

    if(unit()){
        printf("Sintaxa corecta\n");
    }else{
        tkerr(crtTk,"Eroare de sintaxa\n");
    }

    showSymbols(&symbols);
    fclose(fis);
    terminare();

    return 0;
}