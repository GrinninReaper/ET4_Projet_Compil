#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include "tp.h"
#include "tp_y.h"

extern int yyparse();
extern int yylineno;

int evalMain(TreeP tree, VarDeclP decls);
int eval(TreeP tree, VarDeclP decls);

void printAST(TreeP tree, TreeP main);

extern void *strdup();

/* Niveau de 'verbosite'.
 * Par defaut, n'imprime que le resultat et les messages d'erreur
 */
bool verbose = FALSE;

/* Evaluation ou pas. Par defaut, on evalue les expressions */
bool noEval = FALSE;

/* code d'erreur a retourner: liste dans tp.h */
int errorCode = NO_ERROR;

/* yyerror:  fonction importee par Bison et a fournir explicitement.
 * Elle est appelee quand Bison detecte une erreur syntaxique.
 * Ici on se contente d'un message minimal.
 */
void yyerror(char *ignore) {
  fprintf(stderr, "Syntax error on line: %d\n", yylineno);
}


/* mémorise le code d'erreur et s'arrange pour bloquer l'évaluation */
void setError(int code) {
  errorCode = code;
  if (code != NO_ERROR) { noEval = TRUE; }
}


/* Appel:
 *   tp [-option]* programme.txt
 * Les options doivent apparaitre avant le nom du fichier du programme.
 * Options: -[eE] -[vV] -[hH?]
 */
int main(int argc, char **argv) {
  int fi;
  int i, res;

  for(i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      switch (argv[i][1]) {
      case 'v': case 'V':
	verbose = TRUE; continue;
      case 'e': case 'E':
	noEval = TRUE; continue;
      case '?': case 'h': case 'H':
	fprintf(stderr, "Syntax: tp -e -v program.txt\n");
	exit(USAGE_ERROR);
      default:
	fprintf(stderr, "Error: Unknown Option: %c\n", argv[i][1]);
	exit(USAGE_ERROR);
      }
    } else break;
  }

  if (i == argc) {
    fprintf(stderr, "Error: Program file is missing\n");
    exit(USAGE_ERROR);
  }

  if ((fi = open(argv[i++], O_RDONLY)) == -1) {
    fprintf(stderr, "Error: Cannot open %s\n", argv[i-1]);
    exit(USAGE_ERROR);
  }

  /* redirige l'entree standard sur le fichier... */
  close(0); dup(fi); close(fi);

  if (i < argc) { /* fichier dans lequel lire les valeurs pour get() */
    fprintf(stderr, "Error: extra argument: %s\n", argv[i]);
    exit(USAGE_ERROR);
  }

  /* Lance l'analyse syntaxique de tout le source, qui appelle yylex au fur
   * et a mesure. Execute les actions semantiques en parallele avec les
   * reductions.
   * yyparse renvoie 0 si le source est syntaxiquement correct, une valeur
   * differente de 0 en cas d'erreur syntaxique (eventuellement dues a des
   * erreurs lexicales).
   * Comme l'interpretation globale est automatiquement lancee par les actions
   * associees aux reductions, une fois que yyparse a termine il n'y
   * a plus rien a faire (sauf fermer les fichiers)
   * Si le code du programme contient une erreur, on bloque l'evaluation.
   * S'il n'y a que des erreurs contextuelles on essaye de ne pas s'arreter
   * a la premiere mais de continuer l'analyse pour en trovuer d'autres, quand
   * c'est possible.
   */
  res = yyparse();
  if (res == 0 && errorCode == NO_ERROR) return 0;
  else {
    return res ? SYNTAX_ERROR : errorCode;
  }
}


/* Fonction AUXILIAIRE pour la construction d'arbre : renvoie un squelette
 * d'arbre pour 'nbChildren' fils et d'etiquette 'op' donnee. L'appelant
 * doit lui-même stocker ses fils dans la strucutre que MakeNode renvoie
 */
TreeP makeNode(int nbChildren, short op) {
  TreeP tree = NEW(1, Tree);
  tree->op = op; tree->nbChildren = nbChildren;
  tree->u.children = nbChildren > 0 ? NEW(nbChildren, TreeP) : NIL(TreeP);
  return(tree);
}


/* Construction d'un arbre a nbChildren branches, passees en parametres
 * 'op' est une etiquette symbolique qui permet de memoriser la construction
 * dans le programme source qui est representee par cet arbre.
 * Une liste preliminaire d'etiquettes est dans tp.h; il faut l'enrichir selon
 * vos besoins.
 * Cette fonction prend un nombre variable d'arguments: au moins deux.
 * Les eventuels arguments supplementaires doivent etre de type TreeP
 * (defini dans tp.h)
 */
TreeP makeTree(short op, int nbChildren, ...) {
  va_list args;
  int i;
  TreeP tree = makeNode(nbChildren, op); 
  va_start(args, nbChildren);
  for (i = 0; i < nbChildren; i++) { 
    tree->u.children[i] = va_arg(args, TreeP);
  }
  va_end(args);
  return(tree);
}


/* Retourne le i-ieme fils d'un arbre (de 0 a n-1) */
TreeP getChild(TreeP tree, int i) {
  return tree->u.children[i];
}


/* Constructeur de feuille dont la valeur est un entier */
TreeP makeLeafInt(short op, int val) {
  TreeP tree = makeNode(0, op); 
  tree->u.val = val;
  return(tree);
}


/* Constructeur de feuille dont la valeur est une chaine de caracteres.
 * Construit un doublet pour la future variable et stocke son nom dedans.
 */
TreeP makeLeafStr(short op, char *str) {
  TreeP tree = makeNode(0, op); 
  tree->u.str = str;
  return(tree);
}



/* Verifie que nouv n'apparait pas deja dans list. l'ajoute en tete et
 * renvoie la nouvelle liste
 */
VarDeclP addToScope(VarDeclP list, VarDeclP nouv) {
  VarDeclP p;
  for(p=list; p != NIL(VarDecl); p = p->next) {
    if (! strcmp(p->name, nouv->name)) {
      fprintf(stderr, "Error: Multiple declaration in the same scope of %s\n",
	      p->name);
      setError(CONTEXT_ERROR);
      break;
    }
  }
  /* On continue meme en cas de double declaration, pour pouvoir eventuellement
   * detecter plus d'une erreur
   */
  nouv->next=list;
  nouv->rang = list == NIL(VarDecl) ? 0 : 1+list->rang;
  return nouv;
}


/**
 * 	A partir d'ici les fonctions ont besoin d'etre modifiees/completees
 **/

void printOpBinaire(char op) {
  switch(op) {
  case EQ:    printf("="); break;
  case NE:    printf("<>"); break;
  case GT:    printf("<"); break;
  case GE:    printf(">="); break;
  case LT:    printf("<"); break;
  case LE:    printf("<="); break;
  case Eadd:  printf("+"); break;
  case Eminus:printf("-"); break;
  case Emult: printf("*"); break;
  case Ediv:  printf("/"); break;
  default:
    fprintf(stderr, "Unexpected binary operator of code: %d\n", op);
    exit(UNEXPECTED);
  }
}

void printExpr(TreeP tree) {
  switch (tree->op) {
  case IDVAR :
    printf("%s", tree->u.str); break;
  case CONST:
    printf("%d", tree->u.val); break;
  case ITE:
    printf("[ITE "); printExpr(getChild(tree, 0)); /* la condition */
    printf(", "); printExpr(getChild(tree, 1)); /* la partie 'then' */
    printf(", "); printExpr(getChild(tree, 2)); /* la partie 'else' */
    printf("]");
    break;
  case EQ:
  case NE:
  case GT:
  case GE:
  case LT:
  case LE:
  case Eadd:
  case Eminus:
  case Emult:
  case Ediv:
    printf("(");
    printOpBinaire(tree->op);
    printf(" "); printExpr(getChild(tree, 0));
    printf(" "); printExpr(getChild(tree, 1));
    printf(")"); break;
  default:
    fprintf(stderr, "Erreur! etiquette indefinie: %d\n", tree->op);
    exit(UNEXPECTED);
  }
}

void printDecls(TreeP decls) {
  TreeP gauche, droite;
  if (decls == NIL(Tree)) { return; }
  if (decls->op != LIST) {
    fprintf(stderr, "Mauvais format dans les declarations\n");
    exit(UNEXPECTED);
  }
  gauche = getChild(decls, 0); droite = getChild(decls, 1);
  printDecls(gauche);
  printf("%s := ", getChild(droite, 0)->u.str);
  printExpr(getChild(droite, 1));
  printf("\n");
}


void printAST(TreeP decls, TreeP main) {
  printDecls(decls);
  printExpr(main);
}


/* Avant evaluation, verifie si tout identificateur qui apparait dans tree a
 * bien ete declare (dans ce cas il doit apparaitre dans lvar).
 */
bool checkScope(TreeP tree, VarDeclP lvar) {
  VarDeclP p; char *name;
  if (tree == NIL(Tree)) { return TRUE; }
  switch (tree->op) {
  case IDVAR :
    name = tree->u.str;
    for(p=lvar; p != NIL(VarDecl); p = p->next) {
      if (! strcmp(p->name, name)) { return TRUE; }
    }
    fprintf(stderr, "\nError: undeclared variable %s\n", name);
    /* setError met noEval a true de facon à bloquer les evaluations
     * ulterieures, sinon on pourrait chercher la valeur d'une variable qui
     * n'existe pas
     */
    setError(CONTEXT_ERROR); abort();
    return FALSE;
  case CONST:
    return TRUE;
  case ITE:
    return
         checkScope(getChild(tree, 0), lvar) /* la condition */
      && checkScope(getChild(tree, 1), lvar) /* la partie 'then' */
      && checkScope(getChild(tree, 2), lvar); /* la partie 'else' */
  case EQ:
  case NE:
  case GT:
  case GE:
  case LT:
  case LE:
  case Eadd:
  case Eminus:
  case Emult:
  case Ediv:
    return
         checkScope(getChild(tree, 0), lvar)
      && checkScope(getChild(tree, 1), lvar);
  default:
    fprintf(stderr, "Erreur! etiquette indefinie: %d\n", tree->op);
    exit(UNEXPECTED);
  }
}

VarDeclP declVarCode(char *name, TreeP tree, VarDeclP decls){
  VarDeclP pvar = NEW(1, VarDecl), fv;
  pvar->name = name; pvar->next = NIL(VarDecl);
  /* verifie que l'AST ne mentionne pas de variable non declaree */
  checkScope(tree, decls);
  /* puis evalue l'expression en recherchant la valeur des variables dans la
   * liste representee par 'decls'.
   */
genCode(tree, decls);
  fv = addToScope(decls, pvar);
return fv;
  /* if (! noEval) {genCode(tree, decls); } */
  /* ajoute le nouveau couple variable/valeur en tete de la liste et la
   * renvoie en resultat. Verifie que cette variable n'a pas deja ete declaree
   */

}

/* Associe une variable a l'expression qui definit sa valeur, et procede a
 * l'evaluation de cette expression, sauf si on est en mode noEval
 */
VarDeclP declVar(char *name, TreeP tree, VarDeclP decls) {
  VarDeclP pvar = NEW(1, VarDecl);
  pvar->name = name; pvar->next = NIL(VarDecl);
  /* verifie que l'AST ne mentionne pas de variable non declaree */
  checkScope(tree, decls);
  /* puis evalue l'expression en recherchant la valeur des variables dans la
   * liste representee par 'decls'.
   */
  if (! noEval) { /*pvar->val = eval(tree, decls);*/ }
  /* ajoute le nouveau couple variable/valeur en tete de la liste et la
   * renvoie en resultat. Verifie que cette variable n'a pas deja ete declaree
   */
  return addToScope(decls, pvar);
}


VarDeclP genAffCode (TreeP tree, VarDeclP decls) {
  if (tree == NIL(Tree) || tree->op != DECL) {
    exit(UNEXPECTED);
  } 
    else {
        return declVarCode(getChild(tree, 0)->u.str, getChild(tree, 1), decls) ;
  }
}



/* Traite une declaration representee par l'AST 'tree'.
 * 'decls' represente la liste des couples (variable, valeur) definis par les
 * declarations qui precedaient la declaration courante dans le programme,
 * donc les variables qui peuvent apparaitre aux feuilles de 'tree'
*/
VarDeclP evalAff (TreeP tree, VarDeclP decls) {
  if (tree == NIL(Tree) || tree->op != DECL) {
    exit(UNEXPECTED);
  } else {
    return declVar(getChild(tree, 0)->u.str, getChild(tree, 1), decls) ;
  }
}

VarDeclP genDecls(TreeP tree) {
  if (tree == NIL(Tree)) { return NIL(VarDecl); }
  else {
    TreeP g, d; VarDeclP res;
    g = getChild(tree, 0);
    d = getChild(tree, 1);
    res = genDecls(g);
    return genAffCode(d, res);
  }
}

/* Ici 'tree' correspond a l'AST pour une liste de declarations.
 * D'apres la forme des regles utilisée dans tp.y, si 'tree' n'est pas
 * vide, son etiquette doit etre LIST, son fils droit est l'AST d'une
 * declaration et le fils gauche est l'AST des declarations qui precedent
 * la declaration courante. Il faut donc d'abord faire un appel recursif sur
 * le fils gauche avant de traiter le fils droit.
 * A chaque niveau on renvoie la liste des couples (variable, valeur) pour
 * l'AST de 'tree' (i.e. la declaration courante et celles qui la precedent).
 * La toute premiere declaration ne peut référencer aucune variable, les
 * suivantes peuvent référencer celles qui precedent. Les appels recursifs
 * remontent progressivement les listes de variables referencables
 */
VarDeclP evalDecls (TreeP tree) {
  if (tree == NIL(Tree)) { return NIL(VarDecl); }
  else {
    TreeP g, d; VarDeclP res;
    g = getChild(tree, 0);
    d = getChild(tree, 1);
    res = evalDecls(g);
    res = evalAff(d, res);
    return res;
  }
}

char *genEti() {
static char buf[5];
static int last = 0;
sprintf(buf, "eti%d", last++);
return strdup(buf);

}

void genIfCode(TreeP tree, VarDeclP decls) {
char *etiElse = genEti();
char *etiFin = genEti();

    genCode(getChild(tree, 0), decls);
    printf("JZ %s\n", etiElse);
    genCode(getChild(tree, 1), decls);
    printf("JUMP %s\n", etiFin);
    printf("%s: ", etiElse);
    genCode(getChild(tree, 2), decls);
    printf("%s: NOP\n", etiFin);
}


/* Evaluation d'un if then else
 * le premier fils represente la condition,
 * les deux autres fils correspondent respectivement aux parties then et else.
 * Attention a n'evaluer qu'un seul de ces deux sous-arbres !
 */
int evalIf(TreeP tree, VarDeclP decls) {
  if (eval(getChild(tree, 0), decls)) {
    return eval(getChild(tree, 1), decls);
  } else { return eval(getChild(tree, 2), decls); }
}


/* retourne la valeur d'une variable: 'tree' correspond a une feuille qui
 * represente un identificateur. decls est la liste courante des couples
 * (variable, valeur). On est suppose avoir deja verifie que l'identificateur
 * etait bien declare, donc on doit trouver sa valeur.
 */
int getValue(TreeP tree, VarDeclP decls) {
  char *name = tree->u.str;
  while (decls != NIL(VarDecl)) {
    if (! strcmp(decls->name, name)) return(decls->val);
    decls = decls->next;
  }
  /* Ne peut arriver si on a bien fait les verifications contextuelles.
   * Si le programme contient une variabme non declaree, la fonction
   * checkScope a positionne errorCode a ERROR.
   */
  if (errorCode == NO_ERROR) {
    fprintf(stderr, "Unexpected error: Undeclared variable %s\n", name);
    exit(UNEXPECTED);
  } else {
    /* Code mort ais evite que gcc se plaigne de pouvoir atteindre la fin
     * de la fonction sans executer de return.
     */
    return -1;
  }
}

int getRang(char *name, VarDeclP decls) {
VarDeclP l = decls; 
while(l != NIL(VarDecl))  {
if (strcmp(name, l->name) == 0) { return l->rang; }
else { l = l->next; }
}
fprintf(stderr, "OOPS\n"); abort();
}

void genCode(TreeP tree, VarDeclP decls){
    if (tree == NIL(Tree)) 
        exit(UNEXPECTED);
      switch (tree->op) {
          case IDVAR:
            printf("PUSHG %d\n", getRang(tree->u.str, decls));
            break;
          case CONST:
            printf("PUSHI %d\n", tree->u.val);
            break;
          case EQ:
            genCode(getChild(tree, 0), decls);
            genCode(getChild(tree, 1), decls);
            printf("EQUAL\n");
            break;
          case NE:
            genCode(getChild(tree, 0), decls);
            genCode(getChild(tree, 1), decls);
            printf("EQUAL\n");
            printf("NOT\n");
            break;
          case GT:
            genCode(getChild(tree, 0), decls);
            genCode(getChild(tree, 1), decls);
            printf("SUP\n");
            break;
          case GE:
            genCode(getChild(tree, 0), decls);
            genCode(getChild(tree, 1), decls);
            printf("SUPEQ\n");
            break;
          case LT:
            genCode(getChild(tree, 0), decls);
            genCode(getChild(tree, 1), decls);
            printf("INF\n");
            break;
          case LE:
            genCode(getChild(tree, 0), decls);
            genCode(getChild(tree, 1), decls);
            printf("INFEQ\n");
            break;
          case Eadd:
            genCode(getChild(tree, 0), decls);
            genCode(getChild(tree, 1), decls);
            printf("ADD\n");
            break;
          case Eminus:
            genCode(getChild(tree, 0), decls);
            genCode(getChild(tree, 1), decls);
            printf("SUB\n");
            break;
          case Emult:
            genCode(getChild(tree, 0), decls);
            genCode(getChild(tree, 1), decls);
            printf("MUL\n");
            break;
          case Ediv:
            genCode(getChild(tree, 0), decls);
            genCode(getChild(tree, 1), decls);
            printf("DIV\n");
            break;
          case ITE:
             genIfCode(tree, decls);
            break;
          default: 
            fprintf(stderr, "Erreur! etiquette indefinie: %d\n", tree->op);
            exit(UNEXPECTED);
      }
}


/* eval: parcours recursif de l'AST d'une expression en cherchant dans
 * l'environnement la valeur des variables referencee
 * tree: l'AST d'une expression
 * decls: la liste des variables deja declarées avec leur valeur.
 */
int eval(TreeP tree, VarDeclP decls) {
  if (tree == NIL(Tree)) { exit(UNEXPECTED); }
  switch (tree->op) {
  case IDVAR:
    return getValue(tree, decls);
  case CONST:
    return(tree->u.val);
  case EQ:
    return (eval(getChild(tree, 0), decls) == eval(getChild(tree, 1), decls));
  case NE:
    return (eval(getChild(tree, 0), decls) != eval(getChild(tree, 1), decls));
  case GT:
    return (eval(getChild(tree, 0), decls) > eval(getChild(tree, 1), decls));
  case GE:
    return (eval(getChild(tree, 0), decls) >= eval(getChild(tree, 1), decls));
  case LT:
    return (eval(getChild(tree, 0), decls) < eval(getChild(tree, 1), decls));
  case LE:
    return (eval(getChild(tree, 0), decls) <= eval(getChild(tree, 1), decls));
  case Eadd:
    return (eval(getChild(tree, 0), decls) + eval(getChild(tree, 1), decls));
  case Eminus:
    return (eval(getChild(tree, 0), decls) - eval(getChild(tree, 1), decls));
  case Emult:
    return (eval(getChild(tree, 0), decls) * eval(getChild(tree, 1), decls));
  case Ediv:
    { int res, res2;
      res = eval(getChild(tree, 0), decls);
      res2 = eval(getChild(tree, 1), decls);
      /* verifier que la division est bien definie, sinon raler ! */
      if (res2 == 0) {
        fprintf(stderr, "Error: Division by zero\n"); exit(EVAL_ERROR);
      } else { return (res / res2); }
    }
  case ITE:
    return evalIf(tree, decls);
  default: 
    fprintf(stderr, "Erreur! etiquette indefinie: %d\n", tree->op);
    exit(UNEXPECTED);
  }
}

void genMain(TreeP tree, VarDeclP decls) {
  /*int res;*/
  /* verifie les ident utilises dans l'expression finale */
  checkScope(tree, decls);
  if (noEval) {
    fprintf(stderr, "Skipping evaluation step.\n");
  } 
  else {
    if (errorCode == NO_ERROR) {
      printf(" --- Le code généré est:\n");
      genCode(tree, decls);
printf("PUSHS \"le resultat est: \"\nWRITES\nWRITEI\nPUSHS \"\\n\"\nWRITES\n");

printf("STOP\n");
    } else { }
  }
}

/* evalMain: evaluation de l'expression finale.
 * tree: AST de l'expression comprise entre le BEGIN et le END
 * decls: l'environnement forme par les variables declarees
 */
int evalMain(TreeP tree, VarDeclP decls) {
  int res;
  /* verifie les ident utilises dans l'expression finale */
  checkScope(tree, decls);
  if (noEval) {
    fprintf(stderr, "Skipping evaluation step.\n");
  } else {
    if (errorCode == NO_ERROR) {
      res = eval(tree, decls);
      printf("\nResultat global: %d\n", res);
    } else { }
  }
  return errorCode;
}
