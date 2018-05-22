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

typedef union{
    long int i; // int, char
    double d; // double
    const char *str; // char[]
}CtVal;
typedef struct{
    Type type; // type of the result
    int isLVal; // if it is a LVal
    int isCtVal; // if it is a constant value (int, real, char, char[])
    CtVal ctVal; // the constat value
}RetVal;

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
int expr(RetVal *rv);
int exprAssign(RetVal *rv);
int exprOr(RetVal *rv);
int exprAnd(RetVal *rv);
int exprEq(RetVal *rv);
int exprRel(RetVal *rv);
int exprAdd(RetVal *rv);
int exprMul(RetVal *rv);
int exprCast(RetVal *rv);
int exprUnary(RetVal *rv);
int exprPostfix(RetVal *rv);
int exprPrimary(RetVal *rv);
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

Type createType(int typeBase,int nElements){
    Type t;
    t.typeBase = typeBase;
    t.nElements = nElements;
    return t;
}

void cast(Type *dst,Type *src) {
    if(src->nElements>-1){
        if(dst->nElements>-1){
            if(src->typeBase!=dst->typeBase)
                tkerr(crtTk,"an array cannot be converted to an array of another type");
        }else{
            tkerr(crtTk,"an array cannot be converted to a non-array");
        }
    }else{
        if(dst->nElements>-1){
            tkerr(crtTk,"a non-array cannot be converted to an array");
        }
    }
    switch(src->typeBase){
        case TB_CHAR:
        case TB_INT:
        case TB_DOUBLE:
            switch(dst->typeBase){
                case TB_CHAR:
                case TB_INT:
                case TB_DOUBLE:
                    return;
            }
        case TB_STRUCT:
            if(dst->typeBase==TB_STRUCT){
                if(src->s!=dst->s)
                    tkerr(crtTk,"a structure cannot be converted to another one");
                return;
            }
    }
    tkerr(crtTk,"incompatible types");
}

Type getArithType(Type *s1,Type *s2){
    //find common type
    Type aux;
    switch(s1->typeBase){
        case TB_INT: {
            switch(s2->typeBase){
                case TB_INT:{
                    aux.typeBase = TB_INT;
                    break;
                }
                case TB_CHAR:{
                    aux.typeBase = TB_INT;
                    break;
                }
                case TB_DOUBLE:{
                    aux.typeBase = TB_DOUBLE;
                    break;
                }
            }
            break;
        }
        case TB_CHAR:{
            switch(s2->typeBase){
                case TB_INT:{
                    aux.typeBase = TB_INT;
                    break;
                }
                case TB_CHAR:{
                    aux.typeBase = TB_CHAR;
                    break;
                }
                case TB_DOUBLE:{
                    aux.typeBase = TB_DOUBLE;
                    break;
                }
            }
            break;
        }
        case TB_DOUBLE:{
            switch(s2->typeBase){
                case TB_INT:{
                    aux.typeBase = TB_DOUBLE;
                    break;
                }
                case TB_CHAR:{
                    aux.typeBase = TB_DOUBLE;
                    break;
                }
                case TB_DOUBLE:{
                    aux.typeBase = TB_DOUBLE;
                    break;
                }
            }
            break;
        }
    }
    aux.nElements = -1;
    return  aux;
}

Symbol *addExtFunc(const char *name,Type type){
    Symbol *s = addSymbol(&symbols,name,CLS_EXTFUNC);
    s->type = type;
    initSymbols(&s->args);
    return s;
}

Symbol *addFuncArg(Symbol *func,const char *name,Type type) {
    Symbol *a = addSymbol(&func->args,name,CLS_VAR);
    a->type = type;
    return a;
}

void addExtFuncs(){
    Symbol *s;
    s = addExtFunc("put_s",createType(TB_VOID,-1));
    addFuncArg(s,"s",createType(TB_CHAR,0));
    s = addExtFunc("get_s",createType(TB_VOID,-1));
    addFuncArg(s,"s",createType(TB_CHAR,0));
    s = addExtFunc("put_i",createType(TB_VOID,-1));
    addFuncArg(s,"i",createType(TB_INT,-1));
    s = addExtFunc("get_i",createType(TB_INT,-1));
    s = addExtFunc("put_d",createType(TB_VOID,-1));
    addFuncArg(s,"d",createType(TB_DOUBLE,-1));
    s = addExtFunc("get_d",createType(TB_DOUBLE,-1));
    s = addExtFunc("put_c",createType(TB_VOID,-1));
    addFuncArg(s,"s",createType(TB_CHAR,-1));
    s = addExtFunc("get_c",createType(TB_CHAR,0));
    s = addExtFunc("seconds",createType(TB_DOUBLE,-1));
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
//         printf("consume ->");
//         if(getEnum(crtTk->code) == 5){
//         	printf("  %s",crtTk->text);
//         }
//         printf("\n");
        consumedTK = crtTk;
        crtTk = crtTk->next;
        return 1;
    }else
        return 0;
}

int declStruct(){
//     printf("declStruct ->");
//     getEnum(crtTk->code); printf("\n");

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
//    printf("typeBase ->");
//    getEnum(crtTk->code); printf("\n");
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
            if(s == NULL)
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
//     printf("declVar ->");
//     getEnum(crtTk->code); printf("\n");

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
//    printf("arrayDecl ->");
//    getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;
    RetVal rv;

    if(consume(LBRACKET)){
        if(expr(&rv)){
            ret->nElements = 0; // for now do not compute the real size

            if(!rv.isCtVal)tkerr(crtTk,"the array size is not a constant");
            if(rv.type.typeBase!=TB_INT)tkerr(crtTk,"the array size is not an integer");
            ret->nElements=rv.ctVal.i;
        }
        else{
            ret->nElements=0; /*array without given size*/
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
//     printf("typeName ->");
//     getEnum(crtTk->code); printf("\n");
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
//     printf("unit ->");
//     getEnum(crtTk->code); printf("\n");

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
//     printf("declFuncAux ->");
//     getEnum(crtTk->code); printf("\n");
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
//     printf("declFunc ->");
//     getEnum(crtTk->code); printf("\n");

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
//     printf("funcArg ->");
//     getEnum(crtTk->code); printf("\n");
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
//     printf("stmCompound ->");
//     getEnum(crtTk->code); printf("\n");
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
//     printf("stm ->");
//     getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;
    RetVal rv,rv1,rv2,rv3;

    if(stmCompound())
        return 1;

    if(consume(IF)){
        if(consume(LPAR)){
            if(expr(&rv)){
                if(rv.type.typeBase==TB_STRUCT){
                    tkerr(crtTk,"a structure cannot be logically tested");
                }
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
            if(expr(&rv)){
                if(rv.type.typeBase==TB_STRUCT){
                    tkerr(crtTk,"a structure cannot be logically tested");
                }
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
            expr(&rv1);
            if(consume(SEMICOLON)){
                if(expr(&rv2)){
                    if(rv2.type.typeBase == TB_STRUCT){
                        tkerr(crtTk,"a structure cannot be logically tested");
                    }
                }
                if(consume(SEMICOLON)){
                    expr(&rv3);
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
        if(expr(&rv)){
            if(crtFunc->type.typeBase == TB_VOID){
                tkerr(crtTk,"a void function cannot return a value");
            }
            cast(&crtFunc->type,&rv.type);
        }
        if(consume(SEMICOLON)){
            return 1;
        }else tkerr(crtTk,"Lipseste ; -> stm 4");
    }

    if(expr(&rv)){
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

int exprOrAux(RetVal *rv){
//     printf("exprOrAux ->");
//     getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;
    RetVal rve;

    if(consume(OR)){
        if(exprAnd(&rve)){
            if(rv->type.typeBase == TB_STRUCT || rve.type.typeBase == TB_STRUCT){
                tkerr(crtTk,"a structure cannot be logically tested");
            }
            rv->type = createType(TB_INT,-1);
            rv->isCtVal = rv->isLVal=0;
            if(exprOrAux(rv)){
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

int exprOr(RetVal *rv){
//     printf("exprOr ->");
//     if(getEnum(crtTk->code)==5){
//     	printf("%s\n",crtTk->text );
//     }
//     printf("\n");

    Token *initTk = crtTk;

    if(exprAnd(rv)){
        if(exprOrAux(rv)){
            return 1;
        }else{
            tkerr(crtTk,"Declarare incorecta OR");
        }
    }

    crtTk = initTk;
    return 0;
}

int exprAndAux(RetVal *rv){
//     printf("exprAndAux ->");
//     getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;
    RetVal rve;

    if(consume(AND)){
        if(exprEq(&rve)){
            if(rv->type.typeBase == TB_STRUCT || rve.type.typeBase == TB_STRUCT){
                tkerr(crtTk,"a structure cannot be logically tested");
            }
            rv->type=createType(TB_INT,-1);
            rv->isCtVal=rv->isLVal=0;
            if(exprAndAux(rv)){
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

int exprAnd(RetVal *rv){
//     printf("exprAnd ->");
//     getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(exprEq(rv)){
        if(exprAndAux(rv)){
            return 1;
        }else{
            tkerr(crtTk,"Declarare incorecta AND");
        }
    }

    crtTk = initTk;
    return 0;
}

int exprEqAux(RetVal *rv){
//     printf("exprEqAux ->");
//     getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;
    RetVal rve;
    int tkop;

    if(consume(EQUAL) || consume(NOTEQ)){
        if(exprRel(&rve)){
            if(rv->type.typeBase==TB_STRUCT||rve.type.typeBase==TB_STRUCT){
                tkerr(crtTk,"a structure cannot be compared");
            }
            rv->type=createType(TB_INT,-1);
            rv->isCtVal=rv->isLVal=0;
            if(exprEqAux(rv)){
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

int exprEq(RetVal *rv){
//     printf("exprEq ->");
//     getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(exprRel(rv)){
        if(exprEqAux(rv)){
            return 1;
        }else{
            tkerr(crtTk,"Declarare incorecta exprEq");
        }
    }

    crtTk = initTk;
    return 0;
}

int exprAssign(RetVal *rv){
//     printf("exprAssign ->");
//     getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;
    RetVal rve;

    if(exprUnary(rv)){
        if(consume(ASSIGN)){
            if(exprAssign(&rve)){
                if(!rv->isLVal){
                    tkerr(crtTk,"cannot assign to a non-lval");
                }
                if(rv->type.nElements>-1 || rve.type.nElements>-1){
                    tkerr(crtTk,"the arrays cannot be assigned");
                }
                cast(&rv->type,&rve.type);
                rv->isCtVal=rv->isLVal=0;

                return 1;
            }else{
                tkerr(crtTk,"Lipseste exprAssign");
            }
        }
        crtTk = initTk;
    }

    if(exprOr(rv))
        return 1;

    crtTk = initTk;
    return 0;
}

int exprRelAux(RetVal *rv){
//     printf("exprRelAux ->");
//     getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;
    RetVal rve;

    if(consume(LESS) || consume(LESSEQ) || consume(GREATER) || consume(GREATEREQ)){
        if(exprAdd(&rve)){
            if(rv->type.nElements>-1||rve.type.nElements>-1){
                tkerr(crtTk,"an array cannot be compared");
            }
            if(rv->type.typeBase==TB_STRUCT||rve.type.typeBase==TB_STRUCT){
                tkerr(crtTk,"a structure cannot be compared");
            }
            rv->type=createType(TB_INT,-1);
            rv->isCtVal=rv->isLVal=0;
            if(exprRelAux(rv)){
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

int exprRel(RetVal *rv){
//     printf("exprRel ->");
//     getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(exprAdd(rv)){
        if(exprRelAux(rv)){
            return 1;
        }else{
            tkerr(crtTk,"Declarare incorecta exprRel");
        }
    }

    crtTk = initTk;
    return 0;
}

int exprAddAux(RetVal *rv){
//     printf("exprAddAux ->");
//     getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;
    RetVal rve;

    if(consume(ADD) || consume(SUB)){
        if(exprMul(&rve)){
            if(rv->type.nElements>-1||rve.type.nElements>-1){
                tkerr(crtTk,"an array cannot be added or subtracted");
            }
            if(rv->type.typeBase==TB_STRUCT||rve.type.typeBase==TB_STRUCT){
                tkerr(crtTk,"a structure cannot be added or subtracted");
            }
            rv->type=getArithType(&rv->type,&rve.type);
            rv->isCtVal=rv->isLVal=0;
            if(exprAddAux(rv)){
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

int exprAdd(RetVal *rv){
//     printf("exprAdd ->");
//     getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(exprMul(rv)){
        if(exprAddAux(rv)){
            return 1;
        }else{
            tkerr(crtTk,"Declarare incorecta exprAdd");
        }
    }

    crtTk = initTk;
    return 0;
}

int exprMulAux(RetVal *rv){
//     printf("exprMulAux ->");
//     getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;
    RetVal rve;

    if(consume(MUL) || consume(DIV)){
        if(exprCast(&rve)){
            if(rv->type.nElements>-1||rve.type.nElements>-1){
                tkerr(crtTk,"an array cannot be multiplied or divided");
            }
            if(rv->type.typeBase==TB_STRUCT||rve.type.typeBase==TB_STRUCT){
                tkerr(crtTk,"a structure cannot be multiplied or divided");
            }
            rv->type=getArithType(&rv->type,&rve.type);
            rv->isCtVal=rv->isLVal=0;
            if(exprMulAux(rv)){
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

int exprMul(RetVal *rv){
//     printf("exprMul ->");
//     getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(exprCast(rv)){
        if(exprMulAux(rv)){
            return 1;
        }else{
            tkerr(crtTk,"Declarare incorecta exprMul");
        }
    }

    crtTk = initTk;
    return 0;
}

int exprCast(RetVal *rv){
//     printf("exprCast ->");
//     getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;
    Type t;
    RetVal rve;

    if(consume(LPAR)){
        if(typeName(&t)){
            if(consume(RPAR)){
                if(exprCast(&rve)){
                    cast(&t,&rve.type);
                    rv->type=t;
                    rv->isCtVal=rv->isLVal=0;
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

    if(exprUnary(rv))
        return 1;

    crtTk = initTk;
    return 0;
}

int exprUnary(RetVal *rv){
//     printf("exprUnary ->");
//     getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(consume(SUB)){
        if(exprUnary(rv)){
            if(rv->type.nElements>=0){
                tkerr(crtTk,"unary '-' cannot be applied to an array");
            }
            if(rv->type.typeBase==TB_STRUCT){
                tkerr(crtTk,"unary '-' cannot be applied to a struct");
            }
            return 1;
        }else{
            tkerr(crtTk,"Lipseste exprUnary");
        }
    }else if(consume(NOT)){
        if(exprUnary(rv)){

            if(rv->type.typeBase==TB_STRUCT){
                tkerr(crtTk,"'!' cannot be applied to a struct");
            }
            rv->type=createType(TB_INT,-1);

            return true;
        }
        else{
            tkerr(crtTk, "Lipseste exprUnary");
        }
    }

    if(exprPostfix(rv))
        return 1;

    crtTk = initTk;
    return 0;
}

int exprPostfixAux(RetVal *rv){
//     printf("exprPostfixAux ->");
//     getEnum(crtTk->code);
   // printf("\n");
    Token *initTk = crtTk;
    RetVal rve;
    Token *tkName;

    if(consume(DOT)){
        if(consume(ID)){
            tkName = consumedTK;

            Symbol *sStruct=rv->type.s;
            Symbol *sMember=findSymbol(&sStruct->members,tkName->text);
            if(!sMember){
                tkerr(crtTk,"struct %s does not have a member %s",sStruct->name,tkName->text);
            }
            rv->type=sMember->type;
            rv->isLVal=1;
            rv->isCtVal=0;

            if(exprPostfixAux(rv)){
                return 1;
            }else{
                tkerr(crtTk,"Declarare incorecta exprPostfix");
            }
        }else{
            tkerr(crtTk,"Lipseste ID -> exprPostfixAux");
        }
    }

    if(consume(LBRACKET)){
        if(expr(&rve)){
            if(rv->type.nElements<0){
                tkerr(crtTk,"only an array can be indexed");
            }
            Type typeInt=createType(TB_INT,-1);
            cast(&typeInt,&rve.type);
            rv->type=rv->type;
            rv->type.nElements=-1;
            rv->isLVal=1;
            rv->isCtVal=0;
            if(consume(RBRACKET)){
                if(exprPostfixAux(rv)){
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

int exprPostfix(RetVal *rv){
//     printf("exprPostfix ->");
//     getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;

    if(exprPrimary(rv)){
        if(exprPostfixAux(rv)){
            return 1;
        }else{
            tkerr(crtTk,"Declarare incorecta exprPostfix");
        }
    }

    crtTk = initTk;
    return 0;
}

int exprPrimary(RetVal *rv){
//     printf("exprPrimary ->");
//     getEnum(crtTk->code); printf("\n");
    Token *initTk = crtTk;
    RetVal arg;
    Token *tkName;

    if(consume(ID)){
        tkName = consumedTK;

        Symbol *s=findSymbol(&symbols,tkName->text);
        if(!s){
            tkerr(crtTk,"undefined symbol %s",tkName->text);
        }
        rv->type=s->type;
        rv->isCtVal=0;
        rv->isLVal=1;
        if(consume(LPAR)){
            Symbol **crtDefArg=s->args.begin;
            if(s->cls!=CLS_FUNC&&s->cls!=CLS_EXTFUNC){
                tkerr(crtTk,"call of the non-function %s",tkName->text);
            }
            if(expr(&arg)){
                if(crtDefArg==s->args.end){
                    tkerr(crtTk,"too many arguments in call");
                }
                cast(&(*crtDefArg)->type,&arg.type);
                crtDefArg++;
                while(consume(COMMA)){
                    if(expr(&arg)){
                        if(crtDefArg==s->args.end){
                            tkerr(crtTk,"too many arguments in call");
                        }
                        cast(&(*crtDefArg)->type,&arg.type);
                        crtDefArg++;
                    }
                    else
                        tkerr(crtTk,"Lipseste expr dupa ,");
                }
            }
            if(consume(RPAR)) {
                if(crtDefArg!=s->args.end){
                    tkerr(crtTk,"too few arguments in call");
                }
                rv->type=s->type;
                rv->isCtVal=rv->isLVal=0;
                return 1;
            }
            else
                tkerr(crtTk,"Lipseste ) in apel de func");
        }else{
            if(s->cls==CLS_FUNC||s->cls==CLS_EXTFUNC){
                tkerr(crtTk,"missing call for function %s",tkName->text);
            }
        }
        return 1;
    }

    if(consume(CT_INT)){
        Token *tki = consumedTK;

        rv->type=createType(TB_INT,-1);
        rv->ctVal.i=tki->i;
        rv->isCtVal=1;rv->isLVal=0;
        return 1;
    }

    if(consume(CT_REAL)){
        Token *tkr = consumedTK;

        rv->type=createType(TB_DOUBLE,-1);
        rv->ctVal.d=tkr->r;
        rv->isCtVal=1;rv->isLVal=0;
        return 1;
    }

    if(consume(CT_STRING)){

        Token *tks = consumedTK;

        rv->type=createType(TB_CHAR,0);
        rv->ctVal.str=tks->text;
        rv->isCtVal=1;rv->isLVal=0;
        return 1;
    }

    if(consume(CT_CHAR)){
        Token *tkc = consumedTK;

        rv->type=createType(TB_CHAR,-1);
        rv->ctVal.i=tkc->i;
        rv->isCtVal=1;rv->isLVal=0;
        return 1;
    }

    if(consume(LPAR)){
        if(expr(rv)){
            if(consume(RPAR)){
                return 1;
            }else tkerr(crtTk,"Lipseste )");
        }else tkerr(crtTk,"Lipseste expr");
    }

    crtTk = initTk;
    return 0;
}

int expr(RetVal *rv){
    Token* initTk = crtTk;
//     printf("expr ->");
//     getEnum(crtTk->code);
//     printf("\n");

    if(exprAssign(rv))
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

    addExtFuncs();

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