%token If Then Else Begin End ADD SUB MUL DIV AFFECT
%token <S> ID
%token <I> CST
%token <C> RELOP

/* On realise la version simple dans laquelle les declarations sont
 * stockees dans l'AST comme le reste du programme
 */
%type <T> expr bexpr decl declLOpt

/* indications de precedence (en ordre croissant) et d'associativite. Les
 * operateurs sur une meme ligne (separes par un espace) ont la meme priorite.
 */
%right Else
%left ADD SUB
%left MUL DIV
%nonassoc unary

%{
#include "tp.h"     /* les definition des types et les etiquettes des noeuds */

extern int yylex();	/* fournie par Flex */
extern void yyerror();  /* definie dans tp.c */
%}

%%
programme : declLOpt Begin expr End
{ /* $1 represente le sous-arbre avec toutes les declarations,
   * S3 represente l'arbre pour l'expression finale.
   * lvar va etre la liste des couples (variable, valeur) resultant de
   * l'evaluation des declarations.
   */
  VarDeclP lvar = genDecls($1);
    genMain($3, lvar);
}
;

declLOpt :	  { $$ = NIL(Tree); }
|  declLOpt decl  { $$ = makeTree(LIST, 2, $1, $2); }
;


decl: ID AFFECT expr ';'
    { $$ = makeTree(DECL, 2, makeLeafStr(ID, $1), $3); }
;

expr : If bexpr Then expr Else expr
                        { $$ = makeTree(ITE, 3, $2, $4, $6); }
| expr ADD expr 	{ $$ = makeTree(Eadd, 2, $1, $3); }
| expr SUB expr 	{ $$ = makeTree(Eminus, 2, $1, $3); }
| expr MUL expr         { $$ = makeTree(Emult, 2, $1, $3); }
| expr DIV expr	        { $$ = makeTree(Ediv, 2, $1, $3); }
| ADD expr %prec unary  { $$ = $2; }
/* Pour l'AST on traite le - unaire comme un - binaire , comme cela on ne
 * s'en soucie plus dans la suite
 */
| SUB expr %prec unary  { $$ = makeTree(Eminus, 2, makeLeafInt(CONST, 0), $2); }
| CST		        { $$ = makeLeafInt(CONST, $1); }
| ID 			{ $$ = makeLeafStr(IDVAR, $1); }
| '(' expr ')'		{ $$ = $2; }
;

/* Expression booleenne seulement presente dans un IF */
bexpr : expr RELOP expr { $$ = makeTree($2, 2, $1, $3); }
| '(' bexpr ')'		{ $$ = $2; }
;

