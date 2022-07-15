#pragma once

#ifndef COMPILER_AST_H
#define COMPILER_AST_H


#include <memory>
#include <string>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <stack>
#include <vector>
#include <cassert>
#include <algorithm>
#include <unordered_map>



extern int cnt; // 记录临时符号命名
extern int ptrCnt; // 记录指针临时符号命名
extern std::stack<std::string> symbol; // 存临时符号
extern void ShowType(std::vector<int>& arrayLens, int index, const std::string& type); // 递归显示类型
extern void AllocInitVal(std::vector<int>& arrayLens, int index, std::vector<int> vals, int &valIndex, std::string prevPtr); // 递归给数组赋值
extern void AllocValForGlobal(std::vector<int> &arrayLens, int index, std::vector<int> vals, int &valIndex); // 递归给全局数组赋值


// ident info
typedef struct {
    int type; // type == 0: const; type == 1: variant; type == 2: function; type == 3: function params(non-array); type == 4: array; type == 5: function params array
    int retType; // retType = 0: void; retType = 1: int; retType = 2: int数组
    std::vector<int> arrayLens;
    std::vector<std::string> paramsName;
    std::unordered_map<std::string, std::vector<int>> paramArrayLens; // 对应参数的数组长度
    std::vector<std::string> paramsType;
    int val;
    std::vector<int> vals;
    int offset;
}  IdentInfo;

// 符号表
typedef struct tables {
    std::unordered_map<std::string, IdentInfo> identTable;
    struct tables *upper;
    struct tables *lower;
    int layer;
    int cnt;
} IdentTables;
extern std::unordered_map<int, int> layerCnt; 
extern IdentTables *identTalbesHead;
extern IdentTables *curIdentTables;


// if else基本块编号和end如何添加
extern int ifCnt;
extern std::stack<bool> ifLast;
extern std::stack<std::string> curEnd; // end名 + layer


// while基本块的编号
extern int whileCnt;
extern std::stack<std::string> whileEntry; // 存放目前while循环的entry
extern std::stack<std::string> whileEnd; // 存放目前while循环的end

extern std::stack<bool> curStatus; // 0是在if_else中 1是在while中
extern std::stack<int> curBlockIndex; // 存放当前if、else或while循环中block嵌套次数

// 每个基本块只能有一个ret、jump或br 其中只需要判断jump前有没有别的jump或br
extern bool ifBlockEnd;

// 判断返回值是否正确
extern std::string curFunc;

// 判断是在全局还是在函数里
extern bool ifGlobal;

// 所有 AST 的基类
class BaseAST {
public:
    virtual ~BaseAST() = default;

    virtual void Dump() const = 0;
    virtual void Dump(int number) {}
    virtual void Dump(std::string type) {}
    virtual int GetConstVal() {
        return 0;
    }
    
    virtual std::string GetIdent() {
        return "";
    }

    virtual int GetLength() {
        return 0;
    }

    virtual BaseAST* GetItem(){
        return NULL;
    }
    virtual BaseAST* GetNextItem(){
        return NULL;
    }
    virtual void GetConstVals(std::vector<int>& vals, std::vector<int>& arrayLens, int index, int expected_len) {
    }
    virtual int GetKind() {
        return 0;
    }
    virtual void GetVals(std::vector<std::string>& vals) {
    }

};

// Result 是 BaseAST 
class ResultAST: public BaseAST {
public:
    std::unique_ptr<BaseAST> comp_unit;
    void Dump() const override {
        std::cout << "decl @getint(): i32" << std::endl;
        std::cout << "decl @getch(): i32" << std::endl; 
        std::cout << "decl @getarray(*i32): i32" << std::endl;
        std::cout << "decl @putint(i32)" << std::endl;
        std::cout << "decl @putch(i32)" << std::endl;
        std::cout << "decl @putarray(i32, *i32)" << std::endl;
        std::cout << "decl @starttime()" << std::endl;
        std::cout << "decl @stoptime()" << std::endl;
        comp_unit->Dump();
    }
};

// CompUnit 是 BaseAST
class CompUnitAST : public BaseAST {
public:
  // 用智能指针管理对象
    int kind; // kind == 0: all_def; kind == 1: const_decl;
    std::unique_ptr<BaseAST> comp_unit;
    std::unique_ptr<BaseAST> type;
    std::unique_ptr<BaseAST> all_def;
    std::unique_ptr<BaseAST> const_decl;
    void Dump() const override {
        if(comp_unit != NULL){
            comp_unit->Dump();
        }

        if(kind == 0) {
            std::string tmp = type->GetIdent();
            all_def->Dump(tmp);
        }
        else if(kind == 1) {
            const_decl->Dump();
        }
        else {
            std::cerr << "Wrong kind of CompUnit" << std::endl;
            assert(false); 
        }
    }
    
};

// type 也是BaseAST
class TypeAST: public BaseAST {
public:
    std::string type;
    void Dump() const override {

    }
    std::string GetIdent() override {
        return type;
    }
};

// AllDef 也是 BaseAST
class AllDefAST: public BaseAST {
public:
    int kind; // kind == 0: var_decl; kind == 1: func_def
    std::unique_ptr<BaseAST> func_def;
    std::unique_ptr<BaseAST> var_decl;
    void Dump() const override {}
    void Dump(std::string type) override {
        if(kind == 0) {
            if(type == "void") {
                std::cerr << "Var Decl can't have type void" << std::endl;
                assert(false);
            }
            var_decl->Dump(type);
        }
        else if(kind == 1) {
            func_def->Dump(type);
        }
        else {
            std::cerr << "Wrong kind of AllDef" << std::endl;
            assert(false);
        }
    }
};

// FuncDef 也是 BaseAST
class FuncDefAST : public BaseAST {
public:
    std::string ident;
    std::unique_ptr<BaseAST> func_f_params;
    std::unique_ptr<BaseAST> block;
    void Dump() const override {}
    void Dump(std::string type)  override {
        // 存储在全局表里
        IdentInfo tmp;
        tmp.type = 2;
        if((identTalbesHead->identTable).count(ident)){ // 已经定义了
            std::cerr << ident << " have declared before." << std::endl;
            assert(false);
        }
        identTalbesHead->identTable[ident] = tmp;

        // 设定当前的函数
        curFunc = ident;

        // 创建新的IdentTables
        IdentTables *preOne = curIdentTables;
        curIdentTables = new IdentTables;
        curIdentTables->identTable = std::unordered_map<std::string, IdentInfo>();
        curIdentTables->upper = preOne;
        curIdentTables->lower = NULL;
        curIdentTables->layer = preOne->layer + 1;
        curIdentTables->cnt = layerCnt[curIdentTables->layer];
        layerCnt[curIdentTables->layer]++;
        preOne->lower = curIdentTables;

        std::cout << "fun ";
        std::cout << "@" << ident << "(";
        if(func_f_params != NULL)
            func_f_params->Dump();
        
        std::cout << ")";
        // 设定function type
        if(type == "int"){
            (identTalbesHead->identTable)[ident].retType = 1;
            std::cout << ": i32 ";
        }
        else if(type == "void"){
            (identTalbesHead->identTable)[ident].retType = 0;
            std::cout << " ";
        }
        else{
            std::cerr <<  "Function type is not INT or VOID.\n";
            assert(false);
        }
        std::cout << "{\n";
        std::cout << "%entry:\n";
        ifBlockEnd = false; // entry的开头
        ifGlobal = false;

        // 在新的IdentTables中存变量
        IdentTables *curTable = curIdentTables;
        for(size_t i = 0; i < (identTalbesHead->identTable)[ident].paramsName.size(); i++){
            std::string curParams = (identTalbesHead->identTable)[ident].paramsName[i];
            std::string paramType = (identTalbesHead->identTable)[ident].paramsType[i];
            if((curTable->identTable).count(curParams)){ // 重复定义
                std::cerr << "Multiple definition of ident: " << curParams << std::endl;
                assert(false);
            }

            IdentInfo tmp;
            if((identTalbesHead->identTable)[ident].paramArrayLens.count(curParams) == 0) { // 不是数组
                std::cout << "  %" << curParams << '_' << curTable->layer << curTable->cnt << " = alloc ";
                if(paramType == "int") {
                    std::cout << "i32" << std::endl;
                }
                std::cout << "  store @" << curParams << '_' << ident << ", %" << curParams << '_' << curTable->layer << curTable->cnt << std::endl;
                tmp.type = 3;
            }
            else { // 是数组
                std::vector<int> paramArrayLens = (identTalbesHead->identTable)[ident].paramArrayLens[curParams];
                std::cout << "  %" << curParams << '_' << curTable->layer << curTable->cnt << " = alloc *";
                if(paramArrayLens.size() == 0) {
                    if(paramType == "int") {
                        std::cout << "i32" << std::endl;
                    }
                }
                else {
                    ShowType(paramArrayLens, 0, paramType); // 递归显示类型
                    std::cout << std::endl;
                }
                std::cout << "  store @" << curParams << '_' << ident << ", %" << curParams << '_' << curTable->layer << curTable->cnt << std::endl;
                tmp.type = 5;
                tmp.arrayLens = paramArrayLens;
            }
            tmp.val = 0; // 不重要
            (curTable->identTable)[curParams] = tmp;
        }

        block->Dump();

        if(!ifBlockEnd) { // 没有ret
            if((identTalbesHead->identTable)[curFunc].retType == 1){
                // std::cerr << "ret int function has no return value" << std::endl;
                // assert(false);
                // TODO: 这里可能根本不需要 但不如直接加一个ret 这样比判断是否需要方便很多
                std::cout << "  ret 0" << std::endl;
                ifBlockEnd = true;
            }
            else{ // 自己加一个ret
                std::cout << "  ret" << std::endl;
                ifBlockEnd = true;
            }
        }

        std::cout << "}" << std::endl;

        ifGlobal = true;

        // 回退到上一个IdentTables
        curIdentTables = curIdentTables->upper;
        delete curIdentTables->lower;
        curIdentTables->lower = NULL;
    }
};

// FuncFParams 也是 BaseAST
class FuncFParamsAST: public BaseAST {
public:
    std::vector<std::unique_ptr<BaseAST>> func_f_param;
    void Dump() const override {
        for(int i = 0; i < func_f_param.size(); i++){
            if(i != 0)
                std::cout << ", ";
            func_f_param[i]->Dump();
        }
    }
};

// FuncFParam 也是 BaseAST
class OtherFuncFParamsAST: public BaseAST {
public:
    BaseAST* func_f_param;
    BaseAST* func_f_params;
    void Dump() const override{

    }
    BaseAST* GetItem() override{
        return func_f_param;
    }
    BaseAST* GetNextItem() override{
        return func_f_params;
    }
};
class FuncFParamAST: public BaseAST {
public:
    int kind; // kind == 0: BType IDENT; kind == 1: BType IDENT [] [ConstExp]
    std::unique_ptr<BaseAST> type;
    std::string ident;
    std::vector<std::unique_ptr<BaseAST>> const_exps;
    void Dump() const override {
        std::string paramType = type->GetIdent();
        // 在全局表里存函数参数
        (identTalbesHead->identTable)[curFunc].paramsType.emplace_back(paramType);
        (identTalbesHead->identTable)[curFunc].paramsName.emplace_back(ident);
        std::vector<int> paramArrayLens(const_exps.size(), 0);

        if(kind == 1) { // 存参数数组长度
            for(size_t i = 0; i < const_exps.size(); i++) {
                paramArrayLens[i] = const_exps[i]->GetConstVal();
            }
            (identTalbesHead->identTable)[curFunc].paramArrayLens[ident] = paramArrayLens;
        }

        std::cout << '@' << ident << '_' << curFunc << ": ";
        if(kind == 0) {
            if(paramType == "int"){
                std::cout << "i32";
            }
        }
        else if(kind == 1) {
            std::cout << "*";
            if(paramArrayLens.size() == 0) {
                if(paramType == "int"){
                    std::cout << "i32";
                }
            }
            else {

                ShowType(paramArrayLens, 0, paramType); // 递归显示类型
            }
        }
    }
};

// FuncRParams 也是 BaseAST
class FuncRParamsAST: public BaseAST {
public:
    std::vector<std::unique_ptr<BaseAST>> exp;
    void Dump() const override {
        for(int i = 0; i < exp.size(); i++){
            exp[i]->Dump();
        }
    }
    int GetLength() override {
        return exp.size();
    }
};

// Block 也是 BaseAST
class BlockAST: public BaseAST {
public:
    std::vector <std::unique_ptr<BaseAST>> block_items;
    void Dump() const override {
        // 创建新的IdentTables
        IdentTables *preOne = curIdentTables;
        curIdentTables = new IdentTables;
        curIdentTables->identTable = std::unordered_map<std::string, IdentInfo>();
        curIdentTables->upper = preOne;
        curIdentTables->lower = NULL;
        curIdentTables->layer = preOne->layer + 1;
        curIdentTables->cnt = layerCnt[curIdentTables->layer];
        layerCnt[curIdentTables->layer]++;
        preOne->lower = curIdentTables;
        
        ifLast.push(0);
        if(!curStatus.empty()) { // 在if else 或while循环里
            assert(!curBlockIndex.empty());
            int curblockindex = curBlockIndex.top();
            curBlockIndex.pop();
            curblockindex++;
            curBlockIndex.push(curblockindex);
        }
        for(int i = 0; i < block_items.size(); i++) {
            if(ifBlockEnd){ //  都blockEnd了后面不应该还有stmt 例如两个ret连续
                // std::cout << "%NotUse" << i << ':' << std::endl;
                break;
            }
            if(i == block_items.size()-1) { // 是最后一个block_item
                ifLast.pop();
                ifLast.push(1);
            }
            block_items[i]->Dump();
        }
        ifLast.pop();

        // 回退到上一个IdentTables
        curIdentTables = curIdentTables->upper;
        delete curIdentTables->lower;
        curIdentTables->lower = NULL;

    }
};


// BlockItem 也是 BaseAST
class BlockItemsAST: public BaseAST {
public:
    BaseAST *block_item;
    BaseAST *block_items;
    void Dump() const override {
    }
    BaseAST* GetItem() override{
        return block_item;
    }
    BaseAST* GetNextItem() override{
        return block_items;
    }

};

class BlockItemAST: public BaseAST {
public:
    bool kind; // kind == 0: Decl; kind == 1: Stmt;
    std::unique_ptr<BaseAST> decl;
    std::unique_ptr<BaseAST> stmt;
    void Dump() const override {
        if(kind) {
            stmt->Dump();
        }
        else {
            decl->Dump();
        }
    }
};

// Decl 也是 BaseAST 
class DeclAST: public BaseAST {
public:
    int kind; // kind == 0: type var_decl; kind == 1; const_decl;
    std::unique_ptr<BaseAST> type;
    std::unique_ptr<BaseAST> var_decl;
    std::unique_ptr<BaseAST> const_decl;
    void Dump() const override {
        if(kind == 0) {
            std::string tmp = type->GetIdent();
            var_decl->Dump(tmp);
        }
        else if(kind == 1) {
            const_decl->Dump();
        }
        else {
            std::cerr << "Wrong kind of decl" << std::endl;
            assert(false);
        }
    }
};

// VarDecl 也是BaseAST
class VarDeclAST: public BaseAST {
public:
    std::vector<std::unique_ptr<BaseAST>> var_def; // >= 1
    void Dump() const override {

    }
    void Dump(std::string type) override {
        for(int i = 0; i < var_def.size(); i++){
            var_def[i]->Dump(type);
        }
    }
};

// VarDef 也是 AST
class VarDefsAST: public BaseAST {
public:
    BaseAST* var_def;
    BaseAST* var_defs;
    void Dump() const override{

    }
    BaseAST* GetItem() override{
        return var_def;
    }
    BaseAST* GetNextItem() override{
        return var_defs;
    }
};

class VarDefAST: public BaseAST {
public:
    int kind; // kind == 0: IDENT; kind == 1: IDENT = InitVal;
    std::string ident;
    std::unique_ptr<BaseAST> init_val;
    std::vector<std::unique_ptr<BaseAST>> const_exps;
    void Dump() const override {}
    void Dump(std::string type)  override {
        IdentTables *curTable = curIdentTables;
        if((curTable->identTable).count(ident)){ // 重复定义
            std::cerr << "Multiple definition of ident: " << ident << std::endl;
            assert(false);
        }

        if(ifGlobal) { // 全局变量
            if(const_exps.size() == 0) { // 不是数组
                if(type == "int") 
                    std::cout << "global @" << ident << '_' << curTable->layer << curTable->cnt << " = alloc i32, ";
                else { // 还没遇到
                    std::cerr << "Not met yet" << std::endl;
                    assert(false);
                }

                IdentInfo tmp;
                tmp.type = 1;
                if(type == "int")
                    tmp.retType = 1;
                
                if(kind){ // 定义了初值
                    int val = init_val->GetConstVal();
                    tmp.val = val;
                    std::cout << val << std::endl;
                }
                else{ // 未定义初值 默认为0
                    std::cout << "zeroinit" << std::endl;
                    tmp.val = 0;
                }

                (curTable->identTable)[ident] = tmp;
            }
            else { // 是数组
                std::vector<int> arrayLens;
                for(size_t i = 0; i < const_exps.size(); i++) {
                    arrayLens.emplace_back(const_exps[i]->GetConstVal());
                }
                int totalLen = 1;
                for(size_t i = 0; i < arrayLens.size(); i++) {
                    totalLen *= arrayLens[i];
                }

                if(type == "int") {
                    std::cout << "global @" << ident << '_' << curTable->layer << curTable->cnt << " = alloc ";
                    // 递归给出类型
                    ShowType(arrayLens, 0, type);
                    std::cout << ", "; // 后面一定给初值
                }
                else { // 还没遇到
                    std::cerr << "Not met yet" << std::endl;
                    assert(false);
                }


                IdentInfo tmp;
                tmp.type = 4;
                tmp.arrayLens = arrayLens;
                if(type == "int")
                    tmp.retType = 2;
                
                if(kind) { // 定义了初值
                    std::vector<int> vals = std::vector<int>();
                    init_val->GetConstVals(vals, arrayLens, 0, totalLen);

                    tmp.vals = vals;
                    int valIndex = 0;
                    AllocValForGlobal(arrayLens, 0, vals, valIndex);// 递归给全局数组赋值
                    std::cout << std::endl;
                }
                else { // 未定义初值 默认为0
                    std::cout << "zeroinit" << std::endl;
                    tmp.vals = std::vector<int>(totalLen, 0);
                }

                (curTable->identTable)[ident] = tmp;
            }
        }

        else {
            if(const_exps.size() == 0) { // 不是数组
                if(type == "int")
                    std::cout << "  @" << ident << '_' << curTable->layer << curTable->cnt << " = alloc i32" << std::endl;
                else {// 还没遇到
                    std::cerr << "Not met yet" << std::endl;
                    assert(false);
                }
                IdentInfo tmp;
                tmp.type = 1;
                if(type == "int")
                    tmp.retType = 1;
                tmp.val = 0; // 不重要
                (curTable->identTable)[ident] = tmp;
                if(kind){ // 定义了初值
                    init_val->Dump();

                    assert(!symbol.empty());
                    std::string top = symbol.top();
                    symbol.pop();
                    std::cout << "  store " << top << ", @" << ident << '_' << curTable->layer << curTable->cnt << std::endl;
                }
                else{ // 未定义初值 默认为0
                    // std::cout << "  store " << 0 << ", @" << ident << '_' << curTable->layer << curTable->cnt << std::endl;
                }
            }
            else {
                std::vector<int> arrayLens;
                for(size_t i = 0; i < const_exps.size(); i++) {
                    arrayLens.emplace_back(const_exps[i]->GetConstVal());
                }
                int totalLen = 1;
                for(size_t i = 0; i < arrayLens.size(); i++) {
                    totalLen *= arrayLens[i];
                }

                if(type == "int") {
                    std::cout << "  @" << ident << '_' << curTable->layer << curTable->cnt << " = alloc ";
                    // 递归给出类型
                    ShowType(arrayLens, 0, type);
                    std::cout << std::endl; 
                }
                else {// 还没遇到
                    std::cerr << "Not met yet" << std::endl;
                    assert(false);
                }

                IdentInfo tmp;
                tmp.type = 4;
                tmp.arrayLens = arrayLens;
                if(type == "int")
                    tmp.retType = 2;

                std::vector<int> vals = std::vector<int>();
                if(kind){ // 定义了初值
                    init_val->GetConstVals(vals, arrayLens, 0, totalLen);
                    int valIndex = 0;
                    AllocInitVal(arrayLens, 0, vals, valIndex, "@" + ident + "_" + std::to_string(curTable->layer) + std::to_string(curTable->cnt));
                }
                else{ // 未定义初值 默认为0
                    vals = std::vector<int>(totalLen, 0);
                }
                tmp.vals = vals;



                (curTable->identTable)[ident] = tmp;
            }
        }
    }
};

// InitVal 也是AST
class InitValsAST: public BaseAST {
public:
    BaseAST* init_val;
    BaseAST* init_vals;
    void Dump() const override{

    }
    BaseAST* GetItem() override{
        return init_val;
    }
    BaseAST* GetNextItem() override{
        return init_vals;
    }
};
class InitValAST: public BaseAST {
public:
    int kind; // kind == 0: Exp; kind == 1: InitVals;
    std::unique_ptr<BaseAST> exp;
    std::vector<std::unique_ptr<BaseAST>> init_vals;

    void Dump() const override {
        if(kind == 0) {
            ifCnt++;
            exp->Dump(ifCnt);
        }
        else if(kind == 1) { // 不可能用得到
            // for(int i = 0; i < init_vals.size(); i++) {
            //     ifCnt++;
            //     init_vals[i]->Dump(ifCnt);
            // }
            std::cerr << "What ?! You could reach this" << std::endl;
            assert(false);
        }
        else {
            std::cerr << "Wrong kind of initVal" << std::endl;
            assert(false);
        }
    }
    int GetConstVal() override {
        if(kind == 1) {
            std::cerr << "Try to get const val from arry init" << std::endl;
            assert(false);
        }
        return exp->GetConstVal();
    }

    void GetConstVals(std::vector<int>& vals, std::vector<int>& arrayLens, int index, int expected_len) override {
        if(kind == 0) {
            vals.emplace_back(exp->GetConstVal());
            
        }
        else {

            // 检查是否可能再出子初始化列表
            if(index == arrayLens.size()) {
                std::cerr << "Already in the last layer, cannot have sub list" << std::endl;
                assert(false);
            }

            for(size_t i = 0; i < init_vals.size(); i++) {
                int kind = init_vals[i]->GetKind();
                if(kind == 0) {
                    init_vals[i]->GetConstVals(vals, arrayLens, arrayLens.size()-1, vals.size() + 1);
                }
                else {
                    // 检查是否可能再出子初始化列表
                    if(index == arrayLens.size() - 1) {
                        std::cerr << "Already in the last layer, cannot have sub list" << std::endl;
                        assert(false);
                    }
                    // 检查当前已经填充的是否是lenn的整倍数
                    int curLen = vals.size();
                    if(curLen % arrayLens[arrayLens.size()-1] != 0) {
                        std::cerr << "Now what have been added in vals cannot mod len-n" << std::endl;
                        assert(false);
                    }
                    // 检查对齐到了哪一个边界
                    int total = 1;
                    for(size_t i = index + 1; i < arrayLens.size(); i++) {
                        total *= arrayLens[i];
                    }
                    int nindex = index + 1;
                    while(curLen % total != 0) {
                        total /= arrayLens[nindex];
                        nindex++;
                    }
                    init_vals[i]->GetConstVals(vals, arrayLens, nindex, vals.size() + total);
                }
            }

            while(vals.size() < expected_len) { // 不够补0
                vals.emplace_back(0);
            }
        }
    }
    int GetKind() override {
        return kind;
    }
};

// ConstDecl 也是BaseAST
class ConstDeclAST: public BaseAST {
public:
    std::unique_ptr<BaseAST> type;
    std::vector<std::unique_ptr<BaseAST>> const_def;
    void Dump() const override {
        std::string tmp = type->GetIdent();
        for(int i = 0; i < const_def.size(); i++){
            const_def[i]->Dump(tmp);
        }
    }
};

// ConstDef 也是 AST
class ConstDefsAST: public BaseAST {
public:
    // 这里不能用unique_ptr! 原因如下：
    // 首先我不能通过BaseAST的指针直接访问const_def等
    // 我只能写一个虚函数 返回const_def 
    // 但函数返回值不能是unique_ptr 因为unique_ptr不是copiable的
    BaseAST * const_def;
    BaseAST * const_defs;
    void Dump() const override {

    }
    BaseAST* GetItem() override{
        return const_def;
    }
    BaseAST* GetNextItem() override{
        return const_defs;
    }
};

class ConstDefAST: public BaseAST {
public:
    std::string ident;
    std::unique_ptr<BaseAST> const_init_val;
    std::vector<std::unique_ptr<BaseAST>> const_exps;

    void Dump() const override {}

    void Dump(std::string type)  override {
        IdentTables *curTable = curIdentTables;
        if((curTable->identTable).count(ident)){ // 重复定义
            std::cerr << "Multiple definition of ident: " << ident << std::endl;
            assert(false);
        }

        if(const_exps.size() == 0) {
            int init_val = const_init_val->GetConstVal();
            IdentInfo tmp;
            tmp.type = 0;
            if(type == "int")
                tmp.retType = 1;
            tmp.val = init_val;
            (curTable->identTable)[ident] = tmp;
        }
        else { // 数组
            std::vector<int> arrayLens;
            for(size_t i = 0; i < const_exps.size(); i++) {
                arrayLens.emplace_back(const_exps[i]->GetConstVal());
            }
            int totalLen = 1;
            for(size_t i = 0; i < arrayLens.size(); i++) {
                totalLen *= arrayLens[i];
            }
            if(ifGlobal) { // 全局数组

                if(type == "int") {
                    std::cout << "global @" << ident << '_' << curTable->layer << curTable->cnt << " = alloc ";
                    // 递归给出类型
                    ShowType(arrayLens, 0, type);
                    std::cout << ", "; // 后面一定给初值
                }
                else { // 还没遇到
                    std::cerr << "Not met yet" << std::endl;
                    assert(false);
                }


                IdentInfo tmp;
                tmp.type = 4;
                tmp.arrayLens = arrayLens;
                if(type == "int")
                    tmp.retType = 2;
                
                std::vector<int> vals = std::vector<int>();
                const_init_val->GetConstVals(vals, arrayLens, 0, totalLen);
                tmp.vals = vals;

                int valIndex = 0;
                AllocValForGlobal(arrayLens, 0, vals, valIndex);// 递归给全局数组赋值
                std::cout << std::endl;

                (curTable->identTable)[ident] = tmp;
            }
            else {
                if(type == "int") {
                    std::cout << "  @" << ident << '_' << curTable->layer << curTable->cnt << " = alloc ";
                    // 递归给出类型
                    ShowType(arrayLens, 0, type);
                    std::cout << std::endl;
                }
                else {// 还没遇到
                    std::cerr << "Not met yet" << std::endl;
                    assert(false);
                }

                IdentInfo tmp;
                tmp.type = 4;
                tmp.arrayLens = arrayLens;
                if(type == "int")
                    tmp.retType = 2;

                std::vector<int> vals = std::vector<int>();
                const_init_val->GetConstVals(vals, arrayLens, 0, totalLen);
                tmp.vals = vals;

                int valIndex = 0;
                AllocInitVal(arrayLens, 0, vals, valIndex, "@" + ident + "_" + std::to_string(curTable->layer) + std::to_string(curTable->cnt));

                (curTable->identTable)[ident] = tmp;
            }
        }
    }
};


// ConstInitVal 也是 AST
class ConstInitValsAST: public BaseAST {
public:
    BaseAST* const_init_val;
    BaseAST* const_init_vals;
    void Dump() const override{

    }
    BaseAST* GetItem() override{
        return const_init_val;
    }
    BaseAST* GetNextItem() override{
        return const_init_vals;
    }
};

class ConstInitValAST: public BaseAST {
public:
    int kind; // kind == 0: CostExp; kind == 1: ConstInitVals;
    std::unique_ptr<BaseAST> const_exp;
    std::vector<std::unique_ptr<BaseAST>> const_init_vals;
    void Dump() const override {
    } 
    int GetConstVal() override {
        if(kind == 1) {
            std::cerr << "Try to get const val from arry init" << std::endl;
            assert(false);
        }
        return const_exp->GetConstVal();
    }
    void GetConstVals(std::vector<int>& vals, std::vector<int>& arrayLens, int index, int expected_len) override {
        if(kind == 0) { // 整数 则是最后一维
            vals.emplace_back(const_exp->GetConstVal());
        }
        else {
            // 检查是否可能再出子初始化列表
            if(index == arrayLens.size()) {
                std::cerr << "Already in the last layer, cannot have sub list" << std::endl;
                assert(false);
            }

            for(size_t i = 0; i < const_init_vals.size(); i++) {
                int kind = const_init_vals[i]->GetKind();
                if(kind == 0) {
                    const_init_vals[i]->GetConstVals(vals, arrayLens, arrayLens.size()-1, vals.size() + 1);
                }
                else {
                    // 检查是否可能再出子初始化列表
                    if(index == arrayLens.size() - 1) {
                        std::cerr << "Already in the last layer, cannot have sub list" << std::endl;
                        assert(false);
                    }
                    // 检查当前已经填充的是否是lenn的整倍数
                    int curLen = vals.size();
                    if(curLen % arrayLens[arrayLens.size()-1] != 0) {
                        std::cerr << "Now what have been added in vals cannot mod len-n" << std::endl;
                        assert(false);
                    }
                    // 检查对齐到了哪一个边界
                    int total = 1;
                    for(size_t i = index + 1; i < arrayLens.size(); i++) {
                        total *= arrayLens[i];
                    }
                    int nindex = index + 1;
                    while(curLen % total != 0) {
                        total /= arrayLens[nindex];
                        nindex++;
                    }
                    const_init_vals[i]->GetConstVals(vals, arrayLens, nindex, vals.size() + total);
                }
            }

            while(vals.size() < expected_len) { // 不够补0
                vals.emplace_back(0);
            }
        }
    }

    int GetKind() override {
        return kind;
    }
};


// ConstExp 也是 AST
class ConstExpsAST: public BaseAST {
public:
    BaseAST* const_exp;
    BaseAST* const_exps;
    void Dump() const override{

    }
    BaseAST* GetItem() override{
        return const_exp;
    }
    BaseAST* GetNextItem() override{
        return const_exps;
    }
};
class ConstExpAST: public BaseAST {
public:
    std::unique_ptr<BaseAST> exp;
    void Dump() const override {

    }
    int GetConstVal() override {
        return exp->GetConstVal();
    }
};




// Stmt 也是 BaseAST
class StmtAST: public BaseAST {
public:
    int kind; // kind == 0: MatchedStmt; kind == 1: OpenStmt
    std::unique_ptr<BaseAST> matched_stmt;
    std::unique_ptr<BaseAST> open_stmt;

    void Dump() const override {

        if(kind == 0) {
            ifCnt++;
            matched_stmt->Dump(ifCnt);
        }
        else if(kind == 1) {
            ifCnt++;
            open_stmt->Dump(ifCnt);
        }
        else
            assert(false);
    }

};

// OtherStmt 是BaseAST: OtherStmt -> LVal = Exp; | return [Exp]; | [Exp]; | Block
class OtherStmtAST: public BaseAST {
public:
    int kind; // kind == 0: LVal = Exp; kind == 1: return [Exp]; kind == 2: [Exp]; kind == 3: Block; kind == 4: while ( Exp ) Stmt; kind == 5: break; kind == 6: continue;
    std::unique_ptr<BaseAST> exp;
    std::unique_ptr<BaseAST> lval;
    std::unique_ptr<BaseAST> block;
    std::unique_ptr<BaseAST> stmt;
    void Dump() const override {
        if(kind == 0){
            std::string ident = lval->GetIdent();
            IdentTables *curTable = curIdentTables;
            while(!(curTable->identTable).count(ident) && curTable->upper != NULL)
                curTable = curTable->upper;
            
    
            if((curTable->identTable).count(ident) && ((curTable->identTable)[ident].type == 1 || (curTable->identTable)[ident].type == 3)){ // 存在一个这样的lval
                ifCnt++;
                exp->Dump(ifCnt);

                assert(!symbol.empty());
                std::string top = symbol.top();
                symbol.pop();

                if((curTable->identTable)[ident].type == 1)
                    std::cout << "  store " << top << ", @" << ident << '_' << curTable->layer << curTable->cnt << std::endl;
                else
                    std::cout << "  store " << top << ", %" << ident << '_' << curTable->layer << curTable->cnt << std::endl;
            }
            else if((curTable->identTable)[ident].type == 4) { // 这是个数组元素
                ifCnt++;
                exp->Dump(ifCnt);

                assert(!symbol.empty());
                std::string top = symbol.top();
                symbol.pop();

                std::vector<std::string> indexArray = std::vector<std::string>();
                lval->GetVals(indexArray);
                std::string prevPtr = "@" + ident + "_" + std::to_string(curTable->layer) + std::to_string(curTable->cnt);
                for(size_t i = 0; i < indexArray.size(); i++) {
                    std::cout << "  %ptr" << ptrCnt << " = getelemptr " << prevPtr << ", " << indexArray[i] << std::endl;
                    prevPtr = "%ptr" + std::to_string(ptrCnt);
                    ptrCnt++;
                }
                std::cout << "  store " << top << ", " << prevPtr << std::endl;
            }
            else if((curTable->identTable)[ident].type == 5) { // 这是个数组元素参数
                ifCnt++;
                exp->Dump(ifCnt);

                assert(!symbol.empty());
                std::string top = symbol.top();
                symbol.pop();

                std::vector<std::string> indexArray = std::vector<std::string>();
                lval->GetVals(indexArray);
                std::string prevPtr = "%" + ident + "_" + std::to_string(curTable->layer) + std::to_string(curTable->cnt);

                std::cout << "  %ptr" << ptrCnt << " = load " << prevPtr << std::endl;
                prevPtr = "%ptr" + std::to_string(ptrCnt);
                ptrCnt++;

                std::cout << "  %ptr" << ptrCnt << " = getptr " << prevPtr << ", " << indexArray[0] << std::endl;
                prevPtr = "%ptr" + std::to_string(ptrCnt);
                ptrCnt++;

                for(size_t i = 1; i < indexArray.size(); i++) {
                    std::cout << "  %ptr" << ptrCnt << " = getelemptr " << prevPtr << ", " << indexArray[i] << std::endl;
                    prevPtr = "%ptr" + std::to_string(ptrCnt);
                    ptrCnt++;
                }
                std::cout << "  store " << top << ", " << prevPtr << std::endl;
            }
            else{ // 符号表中没有这个或是const 出现语义错误
                std::cerr << "not find this ident or it is a const: " << ident << std::endl;
                assert(false);
            }
        }
        else if(kind == 1){
            ifBlockEnd = true;
            if(exp != NULL){
                if ((identTalbesHead->identTable)[curFunc].retType == 0){ // 应该没有返回值才对
                    std::cerr << "Function " << curFunc << " should not have a ret value" << std::endl;
                    assert(false);
                }
                ifCnt++;
                exp->Dump(ifCnt);
                std::string top = symbol.top();
                symbol.pop();
                std::cout << "  ret " << top << std::endl;
            }
            else{
                if ((identTalbesHead->identTable)[curFunc].retType == 1){ // 应该有返回值才对
                    std::cerr << "Function " << curFunc << " should have a ret value" << std::endl;
                    assert(false);
                }
                std::cout << "  ret" << std::endl;
            }
        }
        else if(kind == 2) {
            if(exp != NULL){
                ifCnt++;
                exp->Dump(ifCnt);
            }
        }
        else if(kind == 3) {
            block->Dump();
        }
        else if(kind == 4) {
            int curWhileCnt = whileCnt;
            whileCnt++;

            bool flag = true;
            
            if(!curStatus.empty()) {
                assert(!curBlockIndex.empty());
                int blockIndex = curBlockIndex.top();
                std::vector<bool> ifLastList(blockIndex, 0);
                for(int i = 0; i < blockIndex; i++) {
                    assert(!ifLast.empty());
                    ifLastList[i] = ifLast.top();
                    ifLast.pop();
                }
                for(int i = 0; i < blockIndex; i++) { // 必须要所有block都是last才行
                    if(ifLastList[i] == 0) {
                        flag = false;
                        break;
                    }
                }
                // 再把所有的ifLast放回去
                for(int i = blockIndex - 1; i >= 0; i--) {
                    ifLast.push(ifLastList[i]);
                }
            }
            if(!flag || curEnd.empty()) { // 不是最后一个stmt 或没有curEnd end是下一个stmt 新增end
                curEnd.push("%while_end" + std::to_string(curWhileCnt));
                whileEnd.push("%while_end" + std::to_string(curWhileCnt));
                flag = false; // 当作不是最后一个stmt
            }

            std::cout << "  jump %while_entry" << curWhileCnt << std::endl;

            whileEntry.push("%while_entry" + std::to_string(curWhileCnt)); // while入口
            std::cout << "%while_entry" << curWhileCnt << ':' << std::endl;
            ifCnt++;
            exp->Dump(ifCnt);
            assert(!symbol.empty());
            std::string top = symbol.top();
            symbol.pop();
            // 注意 不一定是end 要看是否有end
            if(!flag)
                std::cout << "  br " << top << ", %while_body" << curWhileCnt << ", " << curEnd.top() << std::endl;
            else{ // jump到上一个循环的entry 或if_else的end
                assert(!whileEntry.empty());
                std::string tmp = whileEntry.top();
                whileEntry.pop();
                if(whileEntry.empty() || (!curStatus.empty() && curStatus.top() == 0)){ 
                    std::cout << "  br " << top << ", %while_body" << curWhileCnt << ", " << curEnd.top() << std::endl;
                }
                else{ // 注意这里我不是whileEntry的top，而是上一个whileEntry的top
                    std::cout << "  br " << top << ", %while_body" << curWhileCnt  << ", " << whileEntry.top() << std::endl;
                }
                whileEntry.push(tmp);
            }

            ifBlockEnd = true;
            std::cout << "%while_body" << curWhileCnt << ':' << std::endl;
            ifBlockEnd = false;
            curStatus.push(1);
            curBlockIndex.push(0);
            stmt->Dump();
            curStatus.pop();
            curBlockIndex.pop();
            
            if(!ifBlockEnd) 
                std::cout << "  jump %while_entry" << curWhileCnt << std::endl;
            ifBlockEnd = true;
            
            if(!flag) {
                std::cout << "%while_end" << curWhileCnt << ':' << std::endl;
                ifBlockEnd = false;
                whileEnd.pop();
                curEnd.pop();
            }
            whileEntry.pop(); // while出口
        }
        else if(kind == 5){
            if(whileEntry.empty()) { // 不在while循环里
                std::cerr << "Not in a while loop" << std::endl;
                assert(false);
            }
            if(!whileEnd.empty() && strcmp(whileEnd.top().c_str() + 10, whileEntry.top().c_str() + 12) == 0) // 有end
                std::cout <<  "  jump " << whileEnd.top() << std::endl;
            else { // 正常end
                assert(!whileEntry.empty());
                std::string tmp = whileEntry.top();
                whileEntry.pop();
                if(whileEntry.empty()){ // 这是第一层循环 则肯定有end
                    std::cout  <<  "  jump " << whileEnd.top() << std::endl;
                }
                else{
                    std::cout  <<  "  jump " << whileEntry.top() << std::endl;
                }
                whileEntry.push(tmp);
            }
            ifBlockEnd = true;
        }
        else if(kind == 6){
            if(whileEntry.empty()){ // 不在while循环里
                std::cerr << "Not in a while loop" << std::endl;
                assert(false);
            }
            std::cout << "  jump " << whileEntry.top() << std::endl;
            ifBlockEnd = true;
        }
        else // 不存在别的情况
            assert(false);
    }
};

// MatchedStmt 是BaseAST: MatchedStmt -> if (Exp) MatchedStmt else MatchedStmt | OtherStmt
class MatchedStmtAST: public BaseAST {
public:
    int kind; // kind == 0: if (Exp) MatchedStmt else MatchedStmt; kind == 1: OtherStmt
    std::unique_ptr<BaseAST> other_stmt;
    std::unique_ptr<BaseAST> exp;
    std::unique_ptr<BaseAST> if_matched_stmt;
    std::unique_ptr<BaseAST>  else_matched_stmt;
    void Dump() const override {

    }


    void Dump(int number) override {
        if(kind == 0) {
            bool flag = true;
            if(!curStatus.empty()) {
                assert(!curBlockIndex.empty());
                int blockIndex = curBlockIndex.top();
                std::vector<bool> ifLastList(blockIndex, 0);
                for(int i = 0; i < blockIndex; i++) {
                    assert(!ifLast.empty());
                    ifLastList[i] = ifLast.top();
                    ifLast.pop();
                }
                for(int i = 0; i < blockIndex; i++) { // 必须要所有block都是last才行
                    if(ifLastList[i] == 0) {
                        flag = false;
                        break;
                    }
                }
                // 再把所有的ifLast放回去
                for(int i = blockIndex - 1; i >= 0; i--) {
                    ifLast.push(ifLastList[i]);
                }
            }

            if(!flag || curEnd.empty()){ // 不是最后一个stmt 或没有curEnd end是下一个stmt 新增end
                curEnd.push("%end" + std::to_string(number));
                flag = false; // 当作不是最后一个stmt
            }

            // 记录当前if else要跳转的end的名字
            if(!curStatus.empty() && curStatus.top() == 1 && flag) { // 要跳转 且要跳转到一个while entry
                curEnd.push(whileEntry.top());
            }

            ifCnt++;
            exp->Dump(ifCnt); // 短路求值
            assert(!symbol.empty());
            std::string top = symbol.top();
            symbol.pop();
            std::cout << "  br " << top << ", %then" << number << ", %else" << number << std::endl;

            ifBlockEnd = true;
            std::cout << "%then" << number << ':' << std::endl;
            ifBlockEnd = false;
            
            curStatus.push(0);
            curBlockIndex.push(0);
            ifCnt++;
            if_matched_stmt->Dump(ifCnt);
            curStatus.pop();
            curBlockIndex.pop();

            if(!ifBlockEnd){
                std::cout << "  jump " << curEnd.top() << std::endl;
            }

            ifBlockEnd = true;
            std::cout << "%else" << number << ':' << std::endl;
            ifBlockEnd = false;

            curStatus.push(0);
            curBlockIndex.push(0);
            ifCnt++;
            else_matched_stmt->Dump(ifCnt);
            curStatus.pop();
            curBlockIndex.pop();

            if(!ifBlockEnd){
                std::cout << "  jump " << curEnd.top() << std::endl;
            }
            ifBlockEnd = true;


            if(!flag){ // 有end
                std::cout << "%end" << number << ':' << std::endl;
                curEnd.pop(); // 这个end已经完成了任务
                ifBlockEnd = false;
            }

            if(!curStatus.empty() && curStatus.top() == 1 && flag) { // 注意在结尾去掉这个end
                curEnd.pop();
            }
        }
        else if(kind == 1){
            // 没用到ifCnt
            ifCnt--;
            other_stmt->Dump();
        }
        else
            assert(false);
    }

};

// OpenStmt 也是BaseAST: OpenStmt -> if (Exp) Stmt | if (Exp) MatchedStmt else OpenStmt
class OpenStmtAST: public BaseAST {
public:
    int kind; // kind == 0: if (Exp) Stmt; kind == 1: if (exp) MatchedStmt else OpenStmt
    std::unique_ptr<BaseAST> exp;
    std::unique_ptr<BaseAST> stmt;
    std::unique_ptr<BaseAST> matched_stmt;
    std::unique_ptr<BaseAST> open_stmt;
    void Dump() const override {

    }
    void Dump(int number) override {
        if(kind == 0){
            bool flag = true;
            
            if(!curStatus.empty()) {
                assert(!curBlockIndex.empty());
                int blockIndex = curBlockIndex.top();
                std::vector<bool> ifLastList(blockIndex, 0);
                for(int i = 0; i < blockIndex; i++) {
                    assert(!ifLast.empty());
                    ifLastList[i] = ifLast.top();
                    ifLast.pop();
                }
                for(int i = 0; i < blockIndex; i++) { // 必须要所有block都是last才行
                    if(ifLastList[i] == 0) {
                        flag = false;
                        break;
                    }
                }
                // 再把所有的ifLast放回去
                for(int i = blockIndex - 1; i >= 0; i--) {
                    ifLast.push(ifLastList[i]);
                }
            }

            if(!flag || curEnd.empty()){ // 不是最后一个stmt end是下一个stmt 新增end
                curEnd.push("%end" + std::to_string(number));
                flag = false;
            }

            // 记录当前if else要跳转的end的名字
            if(!curStatus.empty() && curStatus.top() == 1 && flag) { // 要跳转 且要跳转到一个while entry
                curEnd.push(whileEntry.top());
            }

            ifCnt++;
            exp->Dump(ifCnt); // 短路求值
            assert(!symbol.empty());
            std::string top = symbol.top();
            symbol.pop();

            std::cout << "  br " << top << ", %then" << number << ", " << curEnd.top() << std::endl;

            ifBlockEnd = true;
            std::cout << "%then" << number << ':' << std::endl;
            ifBlockEnd = false;

            curStatus.push(0);
            curBlockIndex.push(0);
            stmt->Dump();
            curStatus.pop();
            curBlockIndex.pop();

            if(!ifBlockEnd){
                std::cout << "  jump " << curEnd.top() << std::endl;
            }
            ifBlockEnd = true;


            if(!flag) { // 有end
                std::cout << "%end" << number << ':' << std::endl;
                curEnd.pop(); // 这个end已经完成了任务

                ifBlockEnd = false;
            }
            if(!curStatus.empty() && curStatus.top() == 1 && flag) { 
                curEnd.pop();
            }
        }
        else if(kind == 1){
            bool flag = true;
            
            if(!curStatus.empty()) {
                assert(!curBlockIndex.empty());
                int blockIndex = curBlockIndex.top();
                std::vector<bool> ifLastList(blockIndex, 0);
                for(int i = 0; i < blockIndex; i++) {
                    assert(!ifLast.empty());
                    ifLastList[i] = ifLast.top();
                    ifLast.pop();
                }
                for(int i = 0; i < blockIndex; i++) { // 必须要所有block都是last才行
                    if(ifLastList[i] == 0) {
                        flag = false;
                        break;
                    }
                }
                // 再把所有的ifLast放回去
                for(int i = blockIndex - 1; i >= 0; i--) {
                    ifLast.push(ifLastList[i]);
                }
            }

            if(!flag || curEnd.empty()){ // 不是最后一个stmt end是下一个stmt 新增end
                curEnd.push("%end" + std::to_string(number));
                flag = false;
            }

            // 记录当前if else要跳转的end的名字
            if(!curStatus.empty() && curStatus.top() == 1 && flag) { // 要跳转 且要跳转到一个while entry
                curEnd.push(whileEntry.top());
            }

            ifCnt++;
            exp->Dump(ifCnt); // 短路求值
            assert(!symbol.empty());
            std::string top = symbol.top();
            symbol.pop();
            std::cout << "  br " << top << ", %then" << number << ", %else" << number << std::endl;
            ifBlockEnd = true;

            std::cout << "%then" << number << ':' << std::endl;
            ifBlockEnd = false;

            curStatus.push(0);
            curBlockIndex.push(0);
            ifCnt++;
            matched_stmt->Dump(ifCnt);
            curStatus.pop();
            curBlockIndex.pop();

            if(!ifBlockEnd){
                std::cout << "  jump " << curEnd.top() << std::endl;
            }
            ifBlockEnd = true;


            std::cout << "%else" << number << ':' << std::endl;
            ifBlockEnd = false;

            curStatus.push(0);
            curBlockIndex.push(0);
            ifCnt++;
            open_stmt->Dump(ifCnt);
            curStatus.pop();
            curBlockIndex.pop();

            if(!ifBlockEnd){
                std::cout << "  jump " << curEnd.top() << std::endl;
            }
            ifBlockEnd = true;


            if(!flag){ // 有end
                std::cout << "%end" << number << ':' << std::endl;
                curEnd.pop(); // 这个end已经完成了任务

                ifBlockEnd = false;
            }

            // 记录当前if else要跳转的end的名字
            if(!curStatus.empty() && curStatus.top() == 1 && flag) { // 要跳转 且要跳转到一个while entry
                curEnd.pop();
            }
        }
        else
            assert(false);
    }
};


//Exp也是 BaseAST
class ExpsAST: public BaseAST {
public:
    BaseAST * exp;
    BaseAST * exps;
    void Dump() const override {

    }
    BaseAST* GetItem() override{
        return exp;
    }
    BaseAST* GetNextItem() override{
        return exps;
    }
};
class ExpAST: public BaseAST {
public:
    std::unique_ptr<BaseAST> lor_exp;
    void Dump() const override {
        lor_exp->Dump();
    }
    void Dump(int number) override {
        lor_exp->Dump(number); // 短路求值
    }
    int GetConstVal() override {
        return lor_exp->GetConstVal();
    }
};

// LOrExp 也是BaseAST
class LOrExpAST: public BaseAST {
public:
    bool kind; // kind == 0: LAndExp; kind == 1: LOrExp "||" LAndExp
    std::unique_ptr<BaseAST> land_exp;
    std::unique_ptr<BaseAST> lor_exp;
    void Dump() const override{
        if(kind){
            lor_exp->Dump();
            land_exp->Dump();
            assert(!symbol.empty());
            std::string land = symbol.top();
            symbol.pop();
            assert(!symbol.empty());
            std::string lor = symbol.top();
            symbol.pop();
            
            std::cout << "  %" << cnt << " = or " << lor << ", " << land << std::endl;
            std::cout << "  %" << cnt+1 << " = ne %" << cnt << ", " << 0 << std::endl;
            symbol.push("%" + std::to_string(cnt+1)); 
            cnt += 2;  
        }      
        else{
            land_exp->Dump();
        }
    }
    
    void Dump(int number) override { // 短路求值 转化成if else语句
        if(kind){
            curEnd.push("%end" + std::to_string(number));

            // result = 1 但是不能写的这么简单
            std::cout << "  @result" << number << " = alloc i32" << std::endl; // mark 不用存符号表 只是个临时变量
            std::cout << "  store 1, @result" << number << std::endl;


            // if (lor_exp == 0)
            ifCnt++;
            lor_exp->Dump(ifCnt);
            assert(!symbol.empty());
            std::string lorExp = symbol.top();
            symbol.pop(); 
            std::cout << "  %" << cnt << " = eq " << lorExp << ", " << 0 << std::endl;
            int lorCnt = cnt;
            cnt++;
            // br
            std::cout << "  br %" << lorCnt << ", %then" << number << ", " << curEnd.top() << std::endl;

            // then: reslut = (land_exp != 0)
            std::cout << "%then" << number << ':' << std::endl;  
            ifCnt++;
            land_exp->Dump(ifCnt);
            assert(!symbol.empty());
            std::string landExp = symbol.top();
            symbol.pop(); 
            std::cout << "  %" << cnt << " = ne " << landExp << ", " << 0 << std::endl;
            int landCnt = cnt;
            cnt++;
            std::cout << "  store" << " %" << landCnt << ", @result" << number << std::endl;


            // jump end
            std::cout << "  jump " << curEnd.top() << std::endl;

            // 下一句就是end
            std::cout << "%end" << number << ':' << std::endl;
            std::cout << "  %" << cnt << " = load @result" << number << std::endl;
            symbol.push("%" + std::to_string(cnt));
            cnt++;

            // 记得把end pop 因为这是临时生成的
            curEnd.pop();
        }
        else{
            land_exp->Dump(number); // 短路求值
        }
    }
    int GetConstVal() override {
        if(kind){
            return lor_exp->GetConstVal() || land_exp->GetConstVal();
        }
        else{
            return land_exp->GetConstVal();
        }
    }

};


// LAndExp 也是 BaseAST
class LAndExpAST: public BaseAST {
public:
    bool kind; // kind == 0: EqExp; kind = =1: LAndExp "&&" EqExp;
    std::unique_ptr<BaseAST> eq_exp;
    std::unique_ptr<BaseAST> land_exp;
    void Dump() const override {
        if(kind){
            land_exp->Dump();
            eq_exp->Dump();
            assert(!symbol.empty());
            std::string eq = symbol.top();
            symbol.pop();
            assert(!symbol.empty());
            std::string land = symbol.top();
            symbol.pop();
            
            std::cout << "  %" << cnt << " = ne " << land << ", " << 0 << std::endl;
            std::cout << "  %" << cnt+1 << " = ne " << eq << ", " << 0 << std::endl;
            std::cout << "  %" << cnt+2 << " = and %" << cnt << ", %" << cnt+1 << std::endl;
            symbol.push("%" + std::to_string(cnt+2)); 
            cnt += 3;  
        }
        else{
            eq_exp->Dump();
        }
    }

    void Dump(int number) override { // 短路求值 转化成if else语句
        if(kind){
            curEnd.push("%end" + std::to_string(number));

            // result = 0 但是不能写的这么简单
            std::cout << "  @result" << number << " = alloc i32" << std::endl; // mark 不用存符号表 只是个临时变量
            std::cout << "  store 0, @result" << number << std::endl;

            // if (land_exp != 0)
            ifCnt++;
            land_exp->Dump(ifCnt);
            assert(!symbol.empty());
            std::string landExp = symbol.top();
            symbol.pop(); 
            std::cout << "  %" << cnt << " = ne " << landExp << ", " << 0 << std::endl;
            int landCnt = cnt;
            cnt++;
            // br
            std::cout << "  br %" << landCnt << ", %then" << number << ", "  << curEnd.top() << std::endl;

            // then: result = (eq_exp != 0);
            std::cout << "%then" << number << ':' << std::endl;  
            eq_exp->Dump();
            assert(!symbol.empty());
            std::string eqExp = symbol.top();
            symbol.pop(); 
            std::cout << "  %" << cnt << " = ne " << eqExp << ", " << 0 << std::endl;
            int eqCnt = cnt;
            cnt++;
            std::cout << "  store" << " %" << eqCnt << ", @result" << number << std::endl;


            // jump end
            std::cout << "  jump " << curEnd.top() << std::endl;

            // 下一句就是end
            std::cout << "%end" << number << ':' << std::endl;
            std::cout << "  %" << cnt << " = load @result" << number << std::endl;
            symbol.push("%" + std::to_string(cnt));
            cnt++;


            // 记得把end pop 因为这是临时生成的
            curEnd.pop();
        }
        else{
            ifCnt--;
            eq_exp->Dump();
        }

    }

    int GetConstVal() override {
        if(kind){
            return land_exp->GetConstVal() && eq_exp->GetConstVal();
        }
        else{
            return eq_exp->GetConstVal();
        }
    }
};


// EqExp 也是 BaseAST
class EqExpAST: public BaseAST {
public:
    bool kind; // kind == 0: RelExp; kind == 1: EqExp "==" | "!=" RelExp;
    std::unique_ptr<BaseAST> rel_exp;
    std::string eq_op;
    std::unique_ptr<BaseAST> eq_exp;
    void Dump() const override {
        if(kind){
            eq_exp->Dump();
            rel_exp->Dump();
            assert(!symbol.empty());
            std::string rel = symbol.top();
            symbol.pop();
            assert(!symbol.empty());
            std::string eq = symbol.top();
            symbol.pop();
            
            if(eq_op == "=="){
                std::cout << "  %" << cnt << " = eq " << eq << ", " << rel << std::endl;
            }
            else if(eq_op == "!="){
                std::cout << "  %" << cnt << " = ne " << eq << ", " << rel << std::endl;
            }
            else{ // 不存在别的可能
                assert(false);
            }
            symbol.push("%" + std::to_string(cnt));
            cnt++;  
        }      
        else{
            rel_exp->Dump();
        }
    }

    int GetConstVal() override {
        if(kind){
            if(eq_op == "=="){
                return eq_exp->GetConstVal() == rel_exp->GetConstVal();
            }
            else if(eq_op == "!="){
                return eq_exp->GetConstVal() != rel_exp->GetConstVal();
            }
            else // 不存在别的可能
                assert(false);
        }
        else{
            return rel_exp->GetConstVal();
        }
    }
};


// RelExp 也是 BaseAST
class RelExpAST: public BaseAST {
public:
    bool kind; // kind == 0: AddExp; kind == 1: RelExp "<" | ">" | "<=" | ">=" AddExp;
    std::unique_ptr<BaseAST> add_exp;
    std::string rel_op;
    std::unique_ptr<BaseAST> rel_exp;
    void Dump() const override {
        if(kind){
            rel_exp->Dump();
            add_exp->Dump();
            assert(!symbol.empty());
            std::string add = symbol.top();
            symbol.pop();
            assert(!symbol.empty());
            std::string rel = symbol.top();
            symbol.pop();
            
            if(rel_op == "<"){
                std::cout << "  %" << cnt << " = lt " << rel << ", " << add << std::endl;
            }
            else if(rel_op == ">"){
                std::cout << "  %" << cnt << " = gt " << rel << ", " << add << std::endl;
            }
            else if(rel_op == "<="){
                std::cout << "  %" << cnt << " = le " << rel << ", " << add << std::endl;
            }
            else if(rel_op == ">="){
                std::cout << "  %" << cnt << " = ge " << rel << ", " << add << std::endl;
            }
            else{ // 不存在别的可能
                assert(false);
            }
            symbol.push("%" + std::to_string(cnt));
            cnt++;  
        }      
        else{
            add_exp->Dump();
        }
    }

    int GetConstVal() override {
        if(kind){
            if(rel_op == "<"){
                return rel_exp->GetConstVal() < add_exp->GetConstVal();
            }
            else if(rel_op == ">"){
                return rel_exp->GetConstVal() > add_exp->GetConstVal();

            }
            else if(rel_op == "<="){
                return rel_exp->GetConstVal() <= add_exp->GetConstVal();

            }
            else if(rel_op == ">="){
                return rel_exp->GetConstVal() >= add_exp->GetConstVal();

            }
            else // 不存在别的可能
                assert(false);
        }
        else{
            return add_exp->GetConstVal();
        }
    }
};


// AddExp 也是 BaseAST
class AddExpAST: public BaseAST {
public:
    bool kind; // kind == 0: MulExp; kind == 1: AddExp ("+" | "-") MulExp
    std::unique_ptr<BaseAST> mul_exp;
    std::unique_ptr<BaseAST> add_exp;
    std::string add_op;

    void Dump() const override {
        if(kind){
            add_exp->Dump();
            mul_exp->Dump();
            assert(!symbol.empty());
            std::string mul = symbol.top();
            symbol.pop();
            assert(!symbol.empty());
            std::string add = symbol.top();
            symbol.pop();
            
            if(add_op == "+"){
                std::cout << "  %" << cnt << " = add " << add << ", " << mul << std::endl;
            }
            else if(add_op == "-"){
                std::cout << "  %" << cnt << " = sub " << add << ", " << mul << std::endl;
            }
            else{ // 不存在别的可能
                assert(false);
            }
            symbol.push("%" + std::to_string(cnt));
            cnt++;  
        }      
        else{
            mul_exp->Dump();
        }
    }

    int GetConstVal() override {
        if(kind){
            if(add_op == "+"){
                return add_exp->GetConstVal() + mul_exp->GetConstVal();
            }
            else if(add_op == "-"){
                return add_exp->GetConstVal() - mul_exp->GetConstVal();
            }
            else // 不存在别的可能
                assert(false);

        }
        else{
            return mul_exp->GetConstVal();
        }
    }
};


// MulExp 也是 BaseAST
class MulExpAST: public BaseAST {
public:
    bool kind; // kind == 0: UnaryExp; kind == 1: MulExp ( "*" | "/" | "%") UnaryExp
    std::unique_ptr<BaseAST> unary_exp;

    std::unique_ptr<BaseAST> mul_exp;

    std::string mul_op;
    void Dump() const override {
        if(kind){
            mul_exp->Dump();
            unary_exp->Dump();
            assert(!symbol.empty());
            std::string unary = symbol.top();
            symbol.pop();
            assert(!symbol.empty());
            std::string mul = symbol.top();
            symbol.pop();
            
            if(mul_op == "*"){
                std::cout << "  %" << cnt << " = mul " << mul << ", " << unary << std::endl;
            }
            else if(mul_op == "/"){
                std::cout << "  %" << cnt << " = div " << mul << ", " << unary << std::endl;
            } 
            else if(mul_op == "%"){
                std::cout << "  %" << cnt << " = mod " << mul << ", " << unary << std::endl;
            }
            else{ // 不存在别的可能
                assert(false);
            }
            symbol.push("%" + std::to_string(cnt));
            cnt++;  
        }
        else{
            unary_exp->Dump();
        }
    }    

    int GetConstVal() override {
        if(kind){
            if(mul_op == "*"){
                return mul_exp->GetConstVal() * unary_exp->GetConstVal();
            }
            else if(mul_op == "/"){
                return mul_exp->GetConstVal() / unary_exp->GetConstVal();
            }
            else if(mul_op == "%"){
                return mul_exp->GetConstVal() % unary_exp->GetConstVal();
            }
            else
                assert(false);
        }
        else{
            return unary_exp->GetConstVal();
        }
    }
};


// UnaryExp 也是BaseAST
class UnaryExpAST: public BaseAST {
public:
    int kind; // kind == 0: unaryOp unaryExp; kind == 1: primaryExp; kind == 2: IDENT ([FuncRParams])

    std::string unary_op;
    std::unique_ptr<BaseAST> unary_exp;
    std::unique_ptr<BaseAST> primary_exp;
    std::string ident;
    std::unique_ptr<BaseAST> func_r_params;
    void Dump() const override {
        if(kind == 1){
            primary_exp->Dump();
        }
        else if(kind == 0){
            unary_exp->Dump();

            if(unary_op == "-"){
                assert(!symbol.empty());
                std::string top = symbol.top();
                symbol.pop();
                std::cout << "  %" << cnt << " = sub 0, " << top << std::endl;
                symbol.push("%" + std::to_string(cnt));
                cnt++;
            }
            else if(unary_op == "!"){
                assert(!symbol.empty());
                std::string top = symbol.top();
                symbol.pop();
                std::cout << "  %" << cnt << " = eq " << top << ", 0"<< std::endl;
                symbol.push("%" + std::to_string(cnt));
                cnt++;
            }
        }
        else if(kind == 2){
            // 读取传递给函数的参数
            int length;
            if(func_r_params == NULL){
                length = 0;
            }
            else{
                length = func_r_params->GetLength();
                func_r_params->Dump();
            }
            if(length != (identTalbesHead->identTable)[ident].paramsType.size()) { // 判断参数是否吻合
                std::cerr << "Function " << ident << " called with wrong params" << std::endl;
                assert(false);
            }
            std::vector<std::string> tmpParams;
            for(int i = 0; i < length; i++) {
                assert(!symbol.empty());
                tmpParams.emplace_back(symbol.top());
                symbol.pop();
            }

            if((identTalbesHead->identTable)[ident].retType == 1){ // int
                std::cout << "  %" << cnt << " = call ";
                symbol.push("%" + std::to_string(cnt));
                cnt++;
            }
            else{
                std::cout << "  call ";
            }
            std::cout << "@" << ident << '(';
            for(int i = length-1; i >= 0; i--) { // 从尾到头添加
                if(i != length-1)
                    std::cout << ", ";
                
                std::cout << tmpParams[i];
            }
            std::cout << ')' << std::endl;
        }
        else{
            std::cerr << "un-known unary_exp type" << std::endl;
            assert(false);            
        }
    } 

    int GetConstVal() override {
        if(kind == 1){
            return primary_exp->GetConstVal();
        }
        else if(kind == 0){
            if(unary_op == "-"){
                return -unary_exp->GetConstVal();
            }
            else if(unary_op == "!"){
                return !unary_exp->GetConstVal();
            }
            else if(unary_op == "+"){
                return unary_exp->GetConstVal();
            }
            else
                assert(false);
        }
        else if(kind == 2){
            std::cerr << "Can't convert function to a const value" << std::endl;
            assert(false);
        }
        else{
            std::cerr << "un-known unary_exp type" << std::endl;
            assert(false);
        }
    }
};



// PrimaryExp 也是BaseAST
class PrimaryExpAST: public BaseAST {
public:
    int kind; // kind == 0: exp; kind == 1: number; kind == 2: LVal

    std::unique_ptr<BaseAST> exp;
    int number;
    std::unique_ptr<BaseAST> lval;
    void Dump() const override {
        if(kind == 0){
            ifCnt++;
            exp->Dump(ifCnt);
        }

        else if(kind == 1){
            symbol.push(std::to_string(number));
        }

        else if(kind == 2){
            std::string ident = lval->GetIdent();
            IdentTables *curTable = curIdentTables;
            while(!(curTable->identTable).count(ident) && curTable->upper != NULL){
                curTable = curTable->upper;
            }

            if((curTable->identTable).count(ident)){
                if((curTable->identTable)[ident].type == 0) // const
                    symbol.push(std::to_string((curTable->identTable)[ident].val));
                else{ // variant
                    if((curTable->identTable)[ident].type == 1) {
                        std::cout << "  %" << cnt << " = load @" << ident << '_' << curTable->layer << curTable->cnt << std::endl;
                        symbol.push("%" + std::to_string(cnt));
                        cnt++;
                    }
                    else if((curTable->identTable)[ident].type == 3) {
                        std::cout << "  %" << cnt << " = load %" << ident << '_' << curTable->layer << curTable->cnt << std::endl;
                        symbol.push("%" + std::to_string(cnt));
                        cnt++;
                    }
                    else if((curTable->identTable)[ident].type == 4) { // 数组元素
                        std::vector<std::string> indexArray = std::vector<std::string>();
                        lval->GetVals(indexArray);

                        bool givePtr = false;
                        if(indexArray.size() < (curTable->identTable)[ident].arrayLens.size()) {
                            givePtr = true;
                            indexArray.emplace_back("0");
                        }
                        std::string prevPtr = "@" + ident + "_" + std::to_string(curTable->layer) + std::to_string(curTable->cnt);
                        for(size_t i = 0; i < indexArray.size(); i++) {
                            std::cout << "  %ptr" << ptrCnt << " = getelemptr " << prevPtr << ", " << indexArray[i] << std::endl;
                            prevPtr = "%ptr" + std::to_string(ptrCnt);
                            ptrCnt++;
                        }
                        if(!givePtr) {
                            std::cout << "  %" << cnt << " = load " << prevPtr << std::endl;
                            symbol.push("%" + std::to_string(cnt));
                            cnt++;
                        }
                        else {
                            symbol.push(prevPtr);
                        }
                    }
                    else if((curTable->identTable)[ident].type == 5) { // 函数数组参数
                        std::vector<std::string> indexArray = std::vector<std::string>();
                        lval->GetVals(indexArray);

                        bool givePtr = false;
                        if(indexArray.size() < (curTable->identTable)[ident].arrayLens.size() + 1) { // +1是因为数组参数省了一维的arrayLen
                            givePtr = true;
                            indexArray.emplace_back("0");
                        }
                        std::string prevPtr = "%" + ident + "_" + std::to_string(curTable->layer) + std::to_string(curTable->cnt);

                        std::cout << "  %ptr" << ptrCnt << " = load " << prevPtr << std::endl;
                        prevPtr = "%ptr" + std::to_string(ptrCnt);
                        ptrCnt++;

                        std::cout << "  %ptr" << ptrCnt << " = getptr " << prevPtr << ", " << indexArray[0] << std::endl;
                        prevPtr = "%ptr" + std::to_string(ptrCnt);
                        ptrCnt++;

                        for(size_t i = 1; i < indexArray.size(); i++) {
                            std::cout << "  %ptr" << ptrCnt << " = getelemptr " << prevPtr << ", " << indexArray[i] << std::endl;
                            prevPtr = "%ptr" + std::to_string(ptrCnt);
                            ptrCnt++;
                        }
                        if(!givePtr) {
                            std::cout << "  %" << cnt << " = load " << prevPtr << std::endl;
                            symbol.push("%" + std::to_string(cnt));
                            cnt++;
                        }
                        else {
                            symbol.push(prevPtr);
                        }
                    }
                    else {
                        std::cerr << "is a function type" << std::endl;
                        assert(false);
                    }

                }
            }
            else{ // 符号表中没有这个 出现语义错误
                std::cerr << "not find this ident: " << ident << std::endl;
                assert(false);
            }
        }
        else
            assert(false);
    }

    int GetConstVal() override {
        if(kind == 0){
           return exp->GetConstVal();
        }
        else if(kind == 1){
            return number;
        }
        else if(kind == 2){
            std::string ident = lval->GetIdent();
            IdentTables *curTable = curIdentTables;

            while(!(curTable->identTable).count(ident) && curTable->upper != NULL)
                curTable = curTable->upper;

            if((curTable->identTable).count(ident) && (curTable->identTable)[ident].type == 0){
                return (curTable->identTable)[ident].val;
            }
            else{ // 符号表中没有这个或者是个变量 出现语义错误
                std::cerr << "not find this ident or it is a variant: " << ident << std::endl;
                assert(false);
            }
        }
        else
            assert(false);
    }
};

// LVal 也是 AST
class LValAST: public BaseAST {
public:
    std::string ident;
    std::vector<std::unique_ptr<BaseAST>> exps;
    void Dump() const override {

    }
    std::string GetIdent() override {
        return ident;
    }
    virtual void GetVals(std::vector<std::string>& vals) override{

        for(size_t i = 0; i < exps.size(); i++) {
            exps[i]->Dump();
            std::string top = symbol.top();
            symbol.pop();
            vals.emplace_back(top);
        }
    }
};


// Number
class Number{
public:
    int int_const;
};

#endif