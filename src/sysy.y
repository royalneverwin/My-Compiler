%code requires {
  #include "AST.h"
}

%{

#include "AST.h"

int yylex();
void yyerror(std::unique_ptr<BaseAST> &ast, const char *s);

using namespace std;

%}


// 定义 parser 函数和错误处理函数的附加参数
// Bison 生成的 parser 函数返回类型一定是 int, 所以我们没办法通过返回值返回 AST, 所以只能通过参数来返回 AST
// 我们需要返回一个 AST, 所以我们把附加参数定义成AST的智能指针
// 解析完成后, 我们要手动修改这个参数, 把它设置成解析得到的字符串
%parse-param { std::unique_ptr<BaseAST> &ast }


// yylval 的定义, 我们把它定义成了一个联合体 (union)
// 如果你想让属性值栈可以存放多种类型的属性值，此时，你可以用%union来定义它
// 因为 token 的值有的是字符串指针, 有的是整数
// 之前我们在 lexer 中用到的 str_val 和 int_val 就是在这里被定义的
// 至于为什么要用字符串指针而不直接用 string 或者 unique_ptr<string>?
// c++标准决定，c++联合体中不允许定义有自定义构造函数的类，同理，不允许定义自定义了拷贝构造函数、析构函数的类。
%union {
  std::string *str_val;
  int int_val;
  BaseAST *ast_val;
}


// lexer 返回的所有 token 种类的声明
// %token定义文法中使用了的终结符
// 注意 IDENT 和 INT_CONST 会返回 token 的值, 分别对应 str_val 和 int_val
%token INT RETURN OR AND CONST IF ELSE WHILE BREAK CONTINUE VOID
%token <str_val> IDENT POSITIVE NEGATIVE OPPOSITE MULTIPLY DIVISION MOD EQUAL NEQUAL LESS GREATER EQLESS EQGREATER
%token <int_val> INT_CONST 

// 非终结符的类型定义
%type <ast_val> CompUnit FuncDef Block Stmt Exp PrimaryExp UnaryExp AddExp MulExp LOrExp LAndExp EqExp RelExp BlockItem Decl ConstDecl ConstDef ConstInitVal ConstExp LVal BlockItems ConstDefs VarDecl VarDef VarDefs InitVal MatchedStmt OpenStmt OtherStmt FuncFParams FuncFParam OtherFuncFParams FuncRParams Exps Type AllDef ArrayConstExps ConstInitVals InitVals ArrayExps
%type <int_val> Number
%type <str_val> UnaryOp AddOp MulOp EqOp RelOp

%%
// 开始符, CompUnit ::= FuncDef, 大括号后声明了解析完成后 parser 要做的事情
// 之前我们定义了 FuncDef 会返回一个 str_val, 也就是字符串指针
// 而 parser 一旦解析完 CompUnit, 就说明所有的 token 都被解析了, 即解析结束了
// 此时我们应该把 FuncDef 返回的结果收集起来, 作为 AST 传给调用 parser 的函数
// $1 指代规则里第一个符号的返回值, 也就是 FuncDef 的返回值
Result
  : CompUnit {
    // // cout << "Result -> CompUnit" << endl;
    auto result = make_unique<ResultAST>();
    result->comp_unit = unique_ptr<BaseAST>($1);
    ast = move(result);
  }
  ;

CompUnit
  : Type AllDef {
    // cout << "CompUnit -> Type AllDef" << endl;
    auto ast = new CompUnitAST();
    ast->kind = 0;
    ast->all_def = unique_ptr<BaseAST>($2);
    ast->type = unique_ptr<BaseAST>($1);
    ast->comp_unit = NULL;
    ast->const_decl = NULL;
    $$ = ast;
  }
  | ConstDecl {
    // cout << "CompUnit -> ConstDecl" << endl;
    auto ast = new CompUnitAST();
    ast->kind = 1;
    ast->all_def = NULL;
    ast->type = NULL;
    ast->comp_unit = NULL;
    ast->const_decl = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | CompUnit Type AllDef {
    // cout << "CompUnit -> CompUnit Type AllDef" << endl;
    auto ast = new CompUnitAST();
    ast->kind = 0;
    ast->all_def = unique_ptr<BaseAST>($3);
    ast->type = unique_ptr<BaseAST>($2);
    ast->comp_unit = unique_ptr<BaseAST>($1);
    ast->const_decl = NULL;
    $$ = ast;
  }
  | CompUnit ConstDecl {
    // cout << "CompUnit -> CompUnit ConstDecl" << endl;
    auto ast = new CompUnitAST();
    ast->kind = 1;
    ast->all_def = NULL;
    ast->type = NULL;
    ast->comp_unit = unique_ptr<BaseAST>($1);
    ast->const_decl = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
  ;

AllDef
  : FuncDef {
    // cout << "AllDef -> FuncDef" << endl;
    auto ast = new AllDefAST();
    ast->kind = 1;
    ast->func_def = unique_ptr<BaseAST>($1);
    ast->var_decl = NULL;
    $$ = ast;
  }
  | VarDecl {
    // cout << "AllDef -> VarDecl" << endl;
    auto ast = new AllDefAST();
    ast->kind = 0;
    ast->var_decl = unique_ptr<BaseAST>($1);
    ast->func_def = NULL;
    $$ = ast;
  }
  ;

Type
  : INT {
    // cout << "Type -> INT" << endl;
    auto ast = new TypeAST();
    ast->type = "int";
    $$ = ast;
  }
  | VOID {
    // cout << "Type -> VOID" << endl;
    auto ast = new TypeAST();
    ast->type = "void";
    $$ = ast;
  }
  ;
// FuncDef ::= FuncType IDENT '(' ')' Block;
// 我们这里可以直接写 '(' 和 ')', 因为之前在 lexer 里已经处理了单个字符的情况
// 解析完成后, 把这些符号的结果收集起来, 然后拼成一个新的字符串, 作为结果返回
// $$ 表示非终结符的返回值, 我们可以通过给这个符号赋值的方法来返回结果
// 你可能会问, FuncType, IDENT 之类的结果已经是字符串指针了
// 为什么还要用 unique_ptr 接住它们, 然后再解引用, 把它们拼成另一个字符串指针呢
// 因为所有的字符串指针都是我们 new 出来的, new 出来的内存一定要 delete
// 否则会发生内存泄漏, 而 unique_ptr 这种智能指针可以自动帮我们 delete
// 虽然此处你看不出用 unique_ptr 和手动 delete 的区别, 但当我们定义了 AST 之后
// 这种写法会省下很多内存管理的负担
FuncDef
  : IDENT '(' ')' Block {
    // cout << "FunDef ->  IDENT () Block" << endl;
    auto ast = new FuncDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->block = unique_ptr<BaseAST>($4);
    ast->func_f_params = NULL;
    $$ = ast;
  }
  | IDENT '(' FuncFParams ')' Block {
    // cout << "FunDef ->  IDENT ( FuncFParams ) Block" << endl;
    auto ast = new FuncDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->block = unique_ptr<BaseAST>($5);
    ast->func_f_params =  unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

FuncFParams
  : FuncFParam OtherFuncFParams {
    // cout << "FuncFParams -> FuncFParam OtherFuncFParams" << endl;
    auto ast = new FuncFParamsAST();
    ast->func_f_param.emplace_back(unique_ptr<BaseAST>($1));
    auto funcfparams = $2;
    while(funcfparams){ // 非空
      ast->func_f_param.emplace_back(unique_ptr<BaseAST>(funcfparams->GetItem()));
      funcfparams = funcfparams->GetNextItem();
    }
    $$ = ast;
  }
  ;

OtherFuncFParams
  : ',' FuncFParam OtherFuncFParams {
    // cout << "OtherFuncFParams -> ',' FuncFParam OtherFuncFParams" << endl;
    auto ast = new OtherFuncFParamsAST();
    ast->func_f_params = $3;
    ast->func_f_param = $2;
    $$ = ast;
  }
  | {
    // cout << "OtherFuncFParams -> 空" << endl;
    $$ = NULL;
  }
  ;

FuncFParam
  : Type IDENT {
    // cout << "FuncFParam -> Type IDENT" << endl;
    auto ast = new FuncFParamAST();
    ast->kind = 0;
    ast->type = unique_ptr<BaseAST>($1);
    ast->ident = *unique_ptr<string>($2);
    $$ = ast;
  }
  | Type IDENT '[' ']' ArrayConstExps { // ArrayConstExps at least have one const_exp
    // cout << "FuncFParam -> Type IDENT '[' ']' ArrayConstExps" << endl;
    auto ast = new FuncFParamAST();
    ast->kind = 1;
    ast->type = unique_ptr<BaseAST>($1);
    ast->ident = *unique_ptr<string>($2);
    auto constexps = $5;
    while(constexps) { // 非空
      ast->const_exps.emplace_back(unique_ptr<BaseAST>(constexps->GetItem()));
      constexps = constexps->GetNextItem();
    }
    $$ = ast;
  }
  | Type IDENT '[' ']' {
    // cout << "FuncFParam -> Type IDENT '[' ']' ArrayConstExps" << endl;
    auto ast = new FuncFParamAST();
    ast->kind = 1;
    ast->type = unique_ptr<BaseAST>($1);
    ast->ident = *unique_ptr<string>($2);
    $$ = ast;
  }
  ;

FuncRParams
  : Exp Exps {
    // cout << "FuncRParams -> Exp Exps" << endl;
    auto ast = new FuncRParamsAST();
    ast->exp.emplace_back(unique_ptr<BaseAST>($1));
    auto exps = $2;
    while(exps) {
      ast->exp.emplace_back(unique_ptr<BaseAST>(exps->GetItem()));
      exps = exps->GetNextItem();
    }
    $$ = ast;
  }
  ;

// 同上, 不再解释

Block
  : '{' BlockItems '}' {
    // cout << "Block -> { BlockItems }" << endl;
    auto ast = new BlockAST();
    auto blockitems = $2;
    while(blockitems){ // 非空
      ast->block_items.emplace_back(unique_ptr<BaseAST>(blockitems->GetItem()));
      blockitems = blockitems->GetNextItem();
    }
    $$ = ast;
  }
  ;

BlockItems
  : BlockItem BlockItems {
    // cout << "BlockItems -> BlockItem BlockItems" << endl;
    auto ast = new BlockItemsAST();
    ast->block_item = $1;
    ast->block_items = $2;
    $$ = ast;
  }
  | {
    // cout << "BlockItems -> 空" << endl;
    $$ = NULL;
  }
  ;

BlockItem
  : Decl {
    // cout << "BlockItem -> Decl" << endl;
    auto ast = new BlockItemAST();
    ast->kind = 0;
    ast->stmt = NULL;
    ast->decl = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | Stmt {
    // cout << "BlockItem -> Stmt" << endl;
    auto ast = new BlockItemAST();
    ast->kind = 1;
    ast->stmt = unique_ptr<BaseAST>($1);
    ast->decl = NULL;
    $$ = ast;
  }
  ;

Decl
  : ConstDecl {
    // cout << "Decl -> ConstDecl" << endl;
    auto ast = new DeclAST();
    ast->kind = 1;
    ast->const_decl = unique_ptr<BaseAST>($1);
    ast->var_decl = NULL;
    ast->type = NULL;
    $$ = ast;
  }
  | Type VarDecl {
    // cout << "Decl->Type VarDecl" << endl;
    auto ast = new DeclAST();
    ast->kind = 0;
    ast->type = unique_ptr<BaseAST>($1);
    ast->var_decl = unique_ptr<BaseAST>($2);
    ast->const_decl = NULL;
    $$ = ast;
  }
  ;

VarDecl
  : VarDef VarDefs ';' {
    // cout << "VarDecl -> VarDef VarDefs ';'" << endl;
    auto ast = new VarDeclAST();
    ast->var_def.emplace_back(unique_ptr<BaseAST>($1));  
    auto vardefs = $2;
    while(vardefs){ // 非空
      ast->var_def.emplace_back(unique_ptr<BaseAST>(vardefs->GetItem()));
      vardefs = vardefs->GetNextItem();
    }
    $$ = ast;
  }
  ;

VarDefs
  : ',' VarDef VarDefs {
    // cout << "VarDefs -> ',' VarDef VarDefs" << endl;
    auto ast = new VarDefsAST();
    ast->var_defs = $3;
    ast->var_def = $2;
    $$ = ast;
  }
  | {
    // cout << "VarDefs -> 空" << endl;
    $$ = NULL;
  }
  ;

VarDef
  : IDENT {
    // cout << "VarDef -> IDENT" << endl;
    auto ast = new VarDefAST();
    ast->kind = 0;
    ast->ident = *unique_ptr<string>($1);
    ast->init_val = NULL;
    $$ = ast;
  }
  | IDENT ArrayConstExps {
    // cout << "VarDef -> IDENT [ConstExp]" << endl;
    auto ast = new VarDefAST();
    ast->kind = 0;
    ast->ident = *unique_ptr<string>($1);
    ast->init_val = NULL;
    auto constexps = $2;
    while(constexps) { // 非空
      ast->const_exps.emplace_back(unique_ptr<BaseAST>(constexps->GetItem()));
      constexps = constexps->GetNextItem();
    }
    $$ = ast;
  }
  | IDENT '=' InitVal {
    // cout << "VarDef -> IDENT = InitVal" << endl;
    auto ast = new VarDefAST();
    ast->kind = 1;
    ast->ident = *unique_ptr<string>($1);
    ast->init_val = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | IDENT ArrayConstExps '=' InitVal {
    // cout << "VarDef -> IDENT [ConstExp] = InitVal" << endl;
    auto ast = new VarDefAST();
    ast->kind = 1;
    ast->ident = *unique_ptr<string>($1);
    ast->init_val = unique_ptr<BaseAST>($4);
    auto constexps = $2;
    while(constexps) { // 非空
      ast->const_exps.emplace_back(unique_ptr<BaseAST>(constexps->GetItem()));
      constexps = constexps->GetNextItem();
    }
    $$ = ast;
  }
  ;


InitVals
  : ',' InitVal InitVals {
    // cout << "InitVals -> , InitVal InitVals" << endl;
    auto ast = new InitValsAST();
    ast->init_val = $2;
    ast->init_vals = $3;
    $$ = ast;
  }
  | {
    // cout << "InitVals -> 空" << endl;
    $$ = NULL;
  }
  ;

InitVal
  : Exp {
    // cout << "InitVal -> Exp" << endl;
    auto ast = new InitValAST();
    ast->exp = unique_ptr<BaseAST>($1);
    ast->kind = 0;
    $$ = ast;
  }
  | '{' '}' {
    // cout << "InitVal -> {}" << endl;
    auto ast = new InitValAST();
    ast->exp = NULL;
    ast->kind = 1;
    $$ = ast;
  }
  | '{' InitVal InitVals '}' {
    // cout << "InitVal -> {  InitVal InitVals }" << endl;
    auto ast = new InitValAST();
    ast->exp = NULL;
    ast->kind = 1;
    ast->init_vals.emplace_back(unique_ptr<BaseAST>($2));
    auto initvals = $3;
    while(initvals) { // 非空
      ast->init_vals.emplace_back(unique_ptr<BaseAST>(initvals->GetItem()));
      initvals = initvals->GetNextItem();
    }
    $$ = ast;
  }


ConstDecl
  : CONST Type ConstDef ConstDefs ';' {
    // cout << "ConstDecl -> ConstDef ConstDefs ';'" << endl;
    auto ast = new ConstDeclAST();
    ast->type = unique_ptr<BaseAST>($2);
    ast->const_def.emplace_back(unique_ptr<BaseAST>($3));
    auto constdefs = $4;
    while(constdefs){ // 非空
      ast->const_def.emplace_back(unique_ptr<BaseAST>(constdefs->GetItem()));
      constdefs = constdefs->GetNextItem();
    }
    $$ = ast;
  }
  ;

ConstDefs
  : ',' ConstDef ConstDefs {
    // cout << "ConstDefs -> ConstDef ConstDefs" << endl;
    auto ast = new ConstDefsAST();
    ast->const_defs = $3;
    ast->const_def = $2;
    $$ = ast;
  }
  | {
    // cout << "ConstDefs -> 空" << endl;
    $$ = NULL;
  }
  ;

ConstDef
  : IDENT ArrayConstExps '=' ConstInitVal {
    // cout << "ConstDef -> IDENT ArrayConstExps = ConstInitVal" << endl;
    auto ast = new ConstDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->const_init_val = unique_ptr<BaseAST>($4);
    auto constexps = $2;
    while(constexps) { // 非空
      ast->const_exps.emplace_back(unique_ptr<BaseAST>(constexps->GetItem()));
      constexps = constexps->GetNextItem();
    }
    $$ = ast;
  }
  | IDENT '=' ConstInitVal {
    // cout << "ConstDef -> IDENT = ConstInitVal" << endl;
    auto ast = new ConstDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->const_init_val = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

ConstInitVals
  : ',' ConstInitVal ConstInitVals {
    // cout << "ConstInitVals -> , ConstInitVal ConstInitVals" << endl;
    auto ast = new ConstInitValsAST();
    ast->const_init_val = $2;
    ast->const_init_vals = $3;
    $$ = ast;
  }
  | {
    // cout << "ConstInitVals -> 空" << endl;
    $$ = NULL;
  }
  ;

ConstInitVal 
  : ConstExp {
    // cout << "ConstInitVal -> ConstExp" << endl;
    auto ast = new ConstInitValAST();
    ast->kind = 0;
    ast->const_exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | '{''}' {
    // cout << "ConstInitVal -> {}" << endl;
    auto ast = new ConstInitValAST();
    ast->kind = 1;
    ast->const_exp = NULL;
    $$ = ast;
  }
  | '{' ConstInitVal ConstInitVals '}' {
    // cout << "ConstInitVal -> { ConstInitVal ConstInitVals }" << endl;
    auto ast = new ConstInitValAST();
    ast->kind = 1;
    ast->const_init_vals.emplace_back(unique_ptr<BaseAST>($2));
    auto constinitvals = $3;
    while(constinitvals) { // 非空
      ast->const_init_vals.emplace_back(unique_ptr<BaseAST>(constinitvals->GetItem()));
      constinitvals = constinitvals->GetNextItem();
    }
    $$ = ast;
  }
  ;

Stmt
  : MatchedStmt {
    // cout << "Stmt -> MatchedStmt" << endl;
    auto ast = new StmtAST();
    ast->kind = 0;
    ast->matched_stmt = unique_ptr<BaseAST>($1);
    ast->open_stmt = NULL;
    $$ = ast;
  }
  | OpenStmt {
    // cout << "Stmt -> OpenStmt" << endl;
    auto ast = new StmtAST();
    ast->kind = 1;
    ast->matched_stmt = NULL;
    ast->open_stmt = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

OtherStmt
  : RETURN Exp ';' {
    // cout << "OtherStmt -> RETURN Exp; " << endl;
    auto ast = new OtherStmtAST();
    ast->exp = unique_ptr<BaseAST>($2);
    ast->kind = 1;
    ast->lval = NULL;
    ast->block = NULL;
    ast->stmt = NULL;
    $$ = ast;
  }
  | RETURN ';' {
    // cout << "OtherStmt -> RETURN ; " << endl;
    auto ast = new OtherStmtAST();
    ast->exp = NULL;
    ast->kind = 1;
    ast->lval = NULL;
    ast->block = NULL;
    ast->stmt = NULL;
    $$ = ast; 
  }
  | LVal '=' Exp ';' {
    // cout << "OtherStmt -> LVal '=' Exp;" << endl;
    auto ast = new OtherStmtAST();
    ast->exp = unique_ptr<BaseAST>($3);
    ast->kind = 0;
    ast->lval = unique_ptr<BaseAST>($1);
    ast->block = NULL;
    ast->stmt = NULL;

    $$ = ast;
  }
  | ';' {
    // cout << "OtherStmt -> ;" << endl;
    auto ast = new OtherStmtAST();
    ast->exp = NULL;
    ast->kind = 2;
    ast->lval = NULL;
    ast->stmt = NULL;

    ast->block = NULL;
    $$ = ast;
  }
  | Exp ';' {
    // cout << "OtherStmt -> Exp ;" << endl;
    auto ast = new OtherStmtAST();
    ast->exp =  unique_ptr<BaseAST>($1);
    ast->kind = 2;
    ast->lval = NULL;
    ast->block = NULL;
    ast->stmt = NULL;

    $$ = ast;
  }
  | Block {
    // cout << "OtherStmt -> Block" << endl;
    auto ast = new OtherStmtAST();
    ast->exp =  NULL;
    ast->kind = 3;
    ast->lval = NULL;
    ast->block = unique_ptr<BaseAST>($1);
    ast->stmt = NULL;

    $$ = ast;
  }
  | WHILE '(' Exp ')' Stmt {
    // cout << "OtherStmt -> WHILE '(' Exp ')' Stmt" << endl;
    auto ast = new OtherStmtAST();
    ast->exp =  unique_ptr<BaseAST>($3);
    ast->kind = 4;
    ast->lval = NULL;
    ast->block = NULL;
    ast->stmt = unique_ptr<BaseAST>($5);

    $$ = ast;  
  }
  | BREAK ';' {
    // cout << "OtherStmt -> BREAK ;" << endl;
    auto ast = new OtherStmtAST();
    ast->exp =  NULL;
    ast->kind = 5;
    ast->lval = NULL;
    ast->block = NULL;
    ast->stmt = NULL;

    $$ = ast;  
  }
  | CONTINUE ';' {
    // cout << "OtherStmt -> CONTINUE ;" << endl;
    auto ast = new OtherStmtAST();
    ast->exp =  NULL;
    ast->kind = 6;
    ast->lval = NULL;
    ast->block = NULL;
    ast->stmt = NULL;

    $$ = ast;  
  }
  ;

MatchedStmt
  : IF '(' Exp ')' MatchedStmt ELSE MatchedStmt {
    // cout << "MatchedStmt -> if (Exp) MatchedStmt else MatchedStmt" << endl;
    auto ast = new MatchedStmtAST();
    ast->kind = 0;
    ast->other_stmt = NULL;
    ast->exp = unique_ptr<BaseAST>($3);
    ast->if_matched_stmt = unique_ptr<BaseAST>($5);
    ast->else_matched_stmt = unique_ptr<BaseAST>($7);
    $$ = ast;
  }
  | OtherStmt {
    // cout << "MatchedStmt -> OtherStmt" << endl;
    auto ast = new MatchedStmtAST();
    ast->kind = 1;
    ast->other_stmt = unique_ptr<BaseAST>($1);
    ast->exp = NULL;
    ast->if_matched_stmt = NULL;
    ast->else_matched_stmt = NULL;
    $$ = ast;
  }
  ;

OpenStmt
  : IF '(' Exp ')' Stmt {
    // cout << "OpenStmt -> if (Exp) Stmt" << endl;
    auto ast = new OpenStmtAST();
    ast->kind = 0;
    ast->exp = unique_ptr<BaseAST>($3);
    ast->stmt = unique_ptr<BaseAST>($5);
    ast->matched_stmt = NULL;
    ast->open_stmt = NULL;
    $$ = ast;
  }
  | IF '(' Exp ')' MatchedStmt ELSE OpenStmt {
    // cout << "OpenStmt -> if (Exp) MatchedStmt else OpenStmt" << endl;
    auto ast = new OpenStmtAST();
    ast->kind = 1;
    ast->exp = unique_ptr<BaseAST>($3);
    ast->stmt = NULL;
    ast->matched_stmt = unique_ptr<BaseAST>($5);
    ast->open_stmt = unique_ptr<BaseAST>($7);
    $$ = ast;
  }
  ;


// Exp
ArrayConstExps
  : '[' ConstExp ']' ArrayConstExps {
    // cout << "ArrayConstExps -> [ ConstExp ] ArrayConstExps" << endl;
    auto ast = new ConstExpsAST();
    ast->const_exp = $2;
    ast->const_exps = $4;
    $$ = ast;
  }
  | '[' ConstExp ']' {
    // cout << "ArrayConstExps -> [ ConstExp ]" << endl;
    auto ast = new ConstExpsAST();
    ast->const_exp = $2;
    ast->const_exps = NULL;
    $$ = ast;
  }
  ;

/* ConstExps
  : ',' ConstExp ConstExps {
    // cout << "ConstExps -> , ConstExp ConstExps" << endl;
    auto ast = new ConstExpsAST();
    ast->const_exp = $2;
    ast->const_exps = $3;
    $$ = ast;
  }
  | {
    // cout << "ConstExps -> 空" << endl;
    $$ = NULL;
  }
  ; */

ConstExp
  : Exp {
    // cout << "ConstExp -> Exp" << endl;
    auto ast = new ConstExpAST();
    ast->exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;
ArrayExps
  : '[' Exp ']' ArrayExps {
    // cout << "ArrayExps -> [ Exp ] ArrayExps" << endl;
    auto ast = new ExpsAST();
    ast->exp = $2;
    ast->exps = $4;
    $$ = ast;
  }
  | '[' Exp ']' {
    // cout << "ArrayExps -> [ Exp ]" << endl;
    auto ast = new ExpsAST();
    ast->exp = $2;
    ast->exps = NULL;
    $$ = ast;
  }
  ;

Exps
  : ',' Exp Exps {
    // cout << "Exps -> , Exp Exps" << endl;
    auto ast = new ExpsAST();
    ast->exp = $2;
    ast->exps = $3;
    $$ = ast;
  }
  | {
    // cout << "Exps -> 空" << endl;
    $$ = NULL;
  }
Exp
  : LOrExp {
    // cout << "Exp -> LOrExp" << endl;
    auto ast = new ExpAST();
    ast->lor_exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

LOrExp
  : LAndExp {
    // cout << "LOrExp -> LAndExp" << endl;
    auto ast = new LOrExpAST();
    ast->kind = 0;
    ast->lor_exp = NULL;
    ast->land_exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | LOrExp OR LAndExp {
    // cout << "LOrExp -> LOrExp OR LAndExp" << endl;
    auto ast = new LOrExpAST();
    ast->kind = 1;
    ast->lor_exp = unique_ptr<BaseAST>($1);
    ast->land_exp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

LAndExp
  : EqExp { 
    // cout << "LAndExp -> EqExp" << endl;
    auto ast = new LAndExpAST();
    ast->kind = 0;
    ast->land_exp = NULL;
    ast->eq_exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | LAndExp AND EqExp {
    // cout << "LAndExp -> LAndExp AND EqExp" << endl;
    auto ast = new LAndExpAST();
    ast->kind = 1;
    ast->land_exp = unique_ptr<BaseAST>($1);
    ast->eq_exp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

EqExp
  : RelExp {
    // cout << "EqExp -> RelExp" << endl;
    auto ast = new EqExpAST();
    ast->kind = 0;
    ast->eq_exp = NULL;
    ast->eq_op = "";
    ast->rel_exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | EqExp EqOp RelExp {
    // cout << "EqExp -> EqExp EqOp RelExp" << endl;
    auto ast = new EqExpAST();
    ast->kind = 1;
    ast->eq_exp = unique_ptr<BaseAST>($1);
    ast->eq_op = *unique_ptr<string>($2);
    ast->rel_exp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

RelExp 
  : AddExp {
    // cout << "RelExp -> AddExp" << endl;
    auto ast = new RelExpAST();
    ast->kind = 0;
    ast->rel_exp = NULL;
    ast->rel_op = "";
    ast->add_exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | RelExp RelOp AddExp {
    // cout << "RelExp -> RelExp RelOp AddExp" << endl;
    auto ast = new RelExpAST();
    ast->kind = 1;
    ast->rel_exp = unique_ptr<BaseAST>($1);
    ast->rel_op = *unique_ptr<string>($2);
    ast->add_exp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;


AddExp
  : MulExp {
    // cout << "AddExp -> MulExp" << endl;
    auto ast = new AddExpAST();

    ast->kind = 0;
    ast->add_exp = NULL;
    ast->add_op = "";
    ast->mul_exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | AddExp AddOp MulExp {
    // cout << "AddExp -> AddExp AddOp MulExp" << endl;
    auto ast = new AddExpAST();

    ast->kind = 1;
    ast->add_exp = unique_ptr<BaseAST>($1);
    ast->add_op = *unique_ptr<string>($2);
    ast->mul_exp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;


MulExp
  : UnaryExp {
    // cout << "MulExp -> UnaryExp" << endl;
    auto ast = new MulExpAST();

    ast->kind = 0;
    ast->mul_exp = NULL;
    ast->mul_op = "";
    ast->unary_exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | MulExp MulOp UnaryExp {
    // cout << "MulExp -> MulExp MulOp UnaryExp" << endl;
    auto ast = new MulExpAST();

    ast->kind = 1;
    ast->mul_exp = unique_ptr<BaseAST>($1);
    ast->mul_op = *unique_ptr<string>($2);
    ast->unary_exp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;


UnaryExp
  : PrimaryExp {
    // cout << "UnaryExp -> PrimaryExp" << endl;
    auto ast = new UnaryExpAST();
    ast->kind = 1;
    ast->primary_exp = unique_ptr<BaseAST>($1);
    ast->unary_op = "";
    ast->unary_exp = NULL;
    ast->ident = "";
    ast->func_r_params = NULL;
    $$ = ast;
  }
  | UnaryOp UnaryExp {
    // cout << "UnaryExp -> UnaryOp UnaryExp" << endl;
    auto ast = new UnaryExpAST();
    ast->kind = 0;
    ast->primary_exp = NULL;
    ast->unary_op = *unique_ptr<string>($1);
    ast->unary_exp = unique_ptr<BaseAST>($2);
    ast->ident = "";
    ast->func_r_params = NULL;
    $$ = ast;
  }
  | IDENT '(' ')'{
    // cout << "UnaryExp -> IDENT ()" << endl;
    auto ast = new UnaryExpAST();
    ast->kind = 2;
    ast->primary_exp = NULL;
    ast->unary_op = "";
    ast->unary_exp = NULL;
    ast->ident = *unique_ptr<string>($1);
    ast->func_r_params = NULL;
    $$ = ast;
  }
  | IDENT '(' FuncRParams ')' {
    // cout << "UnaryExp -> IDENT ( FuncRParams )" << endl;
    auto ast = new UnaryExpAST();
    ast->kind = 2;
    ast->primary_exp = NULL;
    ast->unary_op = "";
    ast->unary_exp = NULL;
    ast->ident = *unique_ptr<string>($1);
    ast->func_r_params = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

PrimaryExp
  : '(' Exp ')'{
    // cout << "PrimaryExp -> Exp" << endl;
    auto ast = new PrimaryExpAST();
    ast->kind = 0;
    ast->exp = unique_ptr<BaseAST>($2);
    ast->number = 0;
    ast->lval = NULL;
    $$ = ast;
  }
  | Number {
    // cout << "PrimaryExp -> Number" << endl;
    auto ast = new PrimaryExpAST();
    ast->kind = 1;
    ast->exp = NULL;
    ast->number = $1;
    ast->lval = NULL;
    $$ = ast;
  }
  | LVal {
    // cout << "PrimaryExp -> LVal" << endl;
    auto ast = new PrimaryExpAST();
    ast->kind = 2;
    ast->exp = NULL;
    ast->number = 0;
    ast->lval = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

LVal
  : IDENT {
    // cout << "LVal -> IDENT" << endl;
    auto ast = new LValAST();
    ast->ident = *unique_ptr<string>($1);
    $$ = ast;
  }
  | IDENT ArrayExps {
    // cout << "LVal -> IDENT [Exp]" << endl;
    auto ast = new LValAST();
    ast->ident = *unique_ptr<string>($1);
    auto arrayexps = $2;
    while(arrayexps) { // 非空
      ast->exps.emplace_back(unique_ptr<BaseAST>(arrayexps->GetItem()));
      arrayexps = arrayexps->GetNextItem();
    }
    $$ = ast;
  }



// Op

UnaryOp
  : POSITIVE {
    // cout << "UnaryOp -> +" << endl;
    $$ = $1;
  } 
  | NEGATIVE {
    // cout << "UnaryOp -> -" << endl;
    $$ = $1;
  }
  | OPPOSITE {
    // cout << "UnaryOp -> !" << endl;
    $$ = $1;
  }
  ;

AddOp
  : POSITIVE {
    // cout << "AddOp -> +" << endl;
    $$ = $1;
  }
  | NEGATIVE {
    // cout << "AddOp -> -" << endl;
    $$ = $1;
  }
  ;

MulOp
  : MULTIPLY {
    // cout << "MulOp -> *" << endl;
    $$ = $1;
  }
  | DIVISION {
    // cout << "MulOp -> /" << endl;
    $$ = $1;
  }
  | MOD {
    // cout << "MulOp -> %" << endl;
    $$ = $1;
  }
  ;

EqOp
  : EQUAL {
    // cout << "EqOp -> EQUAL" << endl;
    $$ = $1;
  }
  | NEQUAL {
    // cout << "EqOp -> NEQUAL" << endl;
    $$ = $1;
  }
  ;

RelOp
  : LESS {
    // cout << "RelOp -> LESS" << endl;
    $$ = $1;
  }
  | GREATER {
    // cout << "RelOp -> GREATER" << endl;
    $$ = $1;
  }
  | EQLESS {
    // cout << "RelOp -> EQLESS" << endl;
    $$ = $1;
  }
  | EQGREATER {
    // cout << "RelOp -> EQGREATER" << endl;
    $$ = $1;
  }
  ;


Number
  : INT_CONST {
    // cout << "Number -> INT_CONST" << endl;
    $$ = $1;
  }
  ;


%%
// 定义错误处理函数, 其中第二个参数是错误信息
// parser 如果发生错误 (例如输入的程序出现了语法错误), 就会调用这个函数
void yyerror(unique_ptr<BaseAST> &ast, const char *s) {
  cerr << "error: " << s << endl;
}