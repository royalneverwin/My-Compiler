#include "AST.h"
#include "koopa.h"
#include "function.h"
#include <unordered_set>
#include <cstdlib>

using namespace std;

// 符号表
IdentTables *identTalbesHead = NULL;
IdentTables *curIdentTables = NULL;

// 指令表
typedef struct {
  string ret;
} instInfo;
unordered_map<koopa_raw_value_t , instInfo> instTable = unordered_map<koopa_raw_value_t , instInfo>();

// 基本块表
typedef struct {
  string ret;
} blockInfo;
unordered_map<koopa_raw_basic_block_t, blockInfo> blockTable = unordered_map<koopa_raw_basic_block_t, blockInfo>();
std::unordered_map<int, int> layerCnt = std::unordered_map<int, int>(); 


// 栈偏移量
stack<int> SPoffset = stack<int>();
int offset = 0;

// 判断函数是否call了另一个函数
bool ifCall = false;

// 判断变量是否是全局变量
unordered_set<string> globalVar;

// 判断integer是否直接返回数字
bool retInt = false;

// 判断load的东西是指针还是指针的指针
unordered_set<string> ptr2ptr;

// 生成koopa代码时需要用到的
int cnt = 0;
int ptrCnt = 0;
stack<string> symbol = stack<string>();
int ifCnt = -1;
std::stack<bool> ifLast = std::stack<bool>();
std::stack<std::string> curEnd = std::stack<std::string>();
bool ifBlockEnd = false;
int whileCnt = 0;
std::stack<std::string> whileEntry = std::stack<std::string>();
std::stack<std::string> whileEnd = std::stack<std::string>();
std::stack<bool> curStatus = std::stack<bool>();
std::stack<int> curBlockIndex = std::stack<int>(); // 存放当前if、else或while循环中block嵌套次数
std::string curFunc = std::string();
bool ifGlobal = true;

void ShowType(std::vector<int>& arrayLens, int index, const std::string& type) { // 递归显示类型
    if(index == arrayLens.size() - 1) { // 最后一维
        if(type == "int")
            std::cout << "[i32, " << arrayLens[index] << ']';
    }
    else {
        std::cout << '[';
        ShowType(arrayLens, index+1, type);
        std::cout << ", " << arrayLens[index] << ']';
    }
}

void AllocInitVal(std::vector<int>& arrayLens, int index, std::vector<int> vals, int &valIndex, std::string prevPtr) { // 递归给数组赋值
  for(int i = 0; i <arrayLens[index]; i++) {
    std::cout << "  %ptr" << ptrCnt << " = getelemptr " << prevPtr << ", " << i << std::endl;
    ptrCnt++;
    if(index == arrayLens.size() - 1) { // 最后一位
        std::cout << "  store " << vals[valIndex] << ", %ptr" << ptrCnt-1 << std::endl;
        valIndex++;
    }
    else {
        AllocInitVal(arrayLens, index + 1, vals, valIndex, "%ptr" + std::to_string(ptrCnt-1));
    }
  }

}

void AllocValForGlobal(std::vector<int> &arrayLens, int index, std::vector<int> vals, int &valIndex) { // 递归给全局数组赋值
  std::cout << '{';
  for(int i = 0; i <arrayLens[index]; i++) {
    if(i != 0)
      std::cout << ", ";
    if(index == arrayLens.size() - 1) { // 最后一位
        std::cout << vals[valIndex];
        valIndex++;
    }
    else {
        AllocValForGlobal(arrayLens, index + 1, vals, valIndex);
    }
  }
  std::cout << '}';
}

// RISCV用到的寄存器
vector<string> regList = {"t0", "t1", "t2", "t3", "t4", "t5", "t6", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"};


// 声明 lexer 的输入, 以及 parser 函数
// 为什么不引用 sysy.tab.hpp 呢? 因为首先里面没有 yyin 的定义
// 其次, 因为这个文件不是我们自己写的, 而是被 Bison 生成出来的
// 你的代码编辑器/IDE 很可能找不到这个文件, 然后会给你报错 (虽然编译不会出错)
// 看起来会很烦人, 于是干脆采用这种看起来 dirty 但实际很有效的手段
extern FILE *yyin;
extern int yyparse(unique_ptr<BaseAST> &ast);


// DFS 得到size
int GetSize(koopa_raw_type_t ty) {
  if(ty->tag == KOOPA_RTT_UNIT) {

    return 0;
  }
  else if(ty->tag == KOOPA_RTT_INT32) {

    return 4;
  }
  else if(ty->tag == KOOPA_RTT_POINTER) {
    return 4;
  }
  else if(ty->tag == KOOPA_RTT_FUNCTION) {
    return 4;
  }
  else if(ty->tag == KOOPA_RTT_ARRAY) {
    int subSize = GetSize(ty->data.array.base);
    return subSize * ty->data.array.len;
  }
  else {
    cerr << "Wrong type of ty->tag" << endl;
    assert(false);
  }
}

// DFS
// 访问 raw program
void Visit(const koopa_raw_program_t &program) {
  // 执行一些其他的必要操作
  // 访问所有全局变量
  Visit(program.values);
  // 访问所有函数
  Visit(program.funcs);
}

// 访问 raw slice
void Visit(const koopa_raw_slice_t &slice) { // slice不是指针

  for (size_t i = 0; i < slice.len; ++i) {
    auto ptr = slice.buffer[i];
    // 根据 slice 的 kind 决定将 ptr 视作何种元素
    switch (slice.kind) {
      case KOOPA_RSIK_FUNCTION:
        // 访问函数
        Visit(reinterpret_cast<koopa_raw_function_t>(ptr));
        break;
      case KOOPA_RSIK_BASIC_BLOCK:
        // 访问基本块
        Visit(reinterpret_cast<koopa_raw_basic_block_t>(ptr));
        break;
      case KOOPA_RSIK_VALUE:
        // 访问指令
        Visit(reinterpret_cast<koopa_raw_value_t>(ptr));
        break;
      default: // KOOPA_RSIK_TYPE
        // 我们暂时不会遇到其他内容, 于是不对其做任何处理
        cerr << "NEW SLICE TYPE" << endl;
        assert(false);
    }
  }
}

// 访问函数
void Visit(const koopa_raw_function_t &func) { // func是指针
  // 判断函数是否只是函数声明
  if((func->bbs).len == 0)
    return;
  // 执行一些其他的必要操作
  cout << " .text" << endl;
  cout << " .globl " << (func->name + 1) << endl;
  cout << (func->name + 1) << ":" << endl;
  
  // 函数的 prologue
  offset = 0;
  ifCall = false;
  unsigned maxLen = 0;
  // 计算存在返回值的指令
  for(size_t i = 0; i < (func->bbs).len; i++) {
    auto ptr = (func->bbs).buffer[i]; // 基本块
    auto bb = reinterpret_cast<koopa_raw_basic_block_t>(ptr);
    for(size_t j = 0; j < (bb->insts).len; j++){
      auto ptr2 = (bb->insts).buffer[j];
      auto inst = reinterpret_cast<koopa_raw_value_t>(ptr2);
      if(inst->kind.tag == KOOPA_RVT_ALLOC) { // 返回值是个指针 但是我们要计算的是alloc的大小
        offset += GetSize(inst->ty->data.pointer.base);
      }
      else
        offset += GetSize(inst->ty);
      
      if((inst->kind).tag == KOOPA_RVT_CALL){ // 有call指令
        ifCall = true;
        maxLen = max(maxLen, (inst->kind).data.call.args.len);
      }
    }
  }
  if(ifCall)
    offset += 4;
  if(maxLen > 8)
    offset += 4 * (maxLen - 8);
  // offset 要 16字节对齐
  if(offset % 16 != 0){
    offset = (offset / 16 + 1) * 16;
  }
  if(offset > 2048){ // 就要存到寄存器里
    string ret = regList[0];
    cout << " li  " << ret << ", " << -offset << endl;
    cout << " add sp, sp, " << ret << endl; // addi是立即数
  }
  else if(offset != 0){
    cout << " addi sp, sp, " << -offset << endl;
  }
  // 看是否需要保存ra
  if(ifCall) {
    if(offset - 4 > 2047) {
      string reg = regList[0];
      cout << " li " << reg << ", " << (offset - 4) << endl;
      cout << " add " << reg << ", sp, " << reg << endl;
      cout << " sw ra, 0(" << reg << ')' << endl;
    }
    else
      cout << " sw ra, " << (offset - 4) << "(sp)" << endl;
  }
  // 初始偏移量以函数的参数为准
  if(maxLen > 8)
    SPoffset.push(4 * (maxLen - 8)); 
  else
    SPoffset.push(0);
  // 访问所有基本块
  Visit(func->bbs);
}

// 访问基本块
void Visit(const koopa_raw_basic_block_t &bb) { // bb是指针
  string bbName = string(bb->name + 1);
  if(bbName != "entry")
    cout << bbName << ":" << endl;

  // 执行一些其他的必要操作
  // ...
  // if(blockTable.count[bb])
  //   return blockTable[bb].ret;

  // 访问所有指令

  // blockInfo tmp;
  // tmp.ret = string(bb->name);
  // blockTable[bb] = tmp;
  Visit(bb->insts);
  // return blockTable[bb].ret;
}


// 访问指令
string Visit(const koopa_raw_value_t &value) { // value是指针
  if(instTable.count(value)) {
    string name = instTable[value].ret;
    if(globalVar.count(name)) { // 是全局变量 会用一个寄存器
      string reg = regList[0];
      regList.erase(regList.begin());
      cout << " la " << reg << ", " << name << endl; 
      return "0(" + reg + ")";
    }
    else {
      return instTable[value].ret;
    }
  }
  else{
    string ret = "";
    // 根据指令类型判断后续需要如何访问
    const auto &kind = value->kind;
    switch (kind.tag) {
      case KOOPA_RVT_RETURN: {
        // 访问 return 指令
        Visit(kind.data.ret); // 没有返回值
        break;
      }
      case KOOPA_RVT_INTEGER: {
        // 访问 integer 指令
        ret = Visit(kind.data.integer);
        instInfo tmp;
        tmp.ret = ret;
        instTable[value] =tmp;
        break;
      }
      case KOOPA_RVT_BINARY: {
        // 访问binary operation 指令
        ret = Visit(kind.data.binary);
        instInfo tmp;
        tmp.ret = ret;
        instTable[value] =tmp;
        break;
      }
      case KOOPA_RVT_ALLOC: {
        // 访问alloc指令
        ret = Visit(kind.data.global_alloc, false, "", GetSize(value->ty->data.pointer.base)); // name无所谓
        if(value->ty->data.pointer.base->tag == KOOPA_RTT_POINTER) { // 如果是指针的指针，存下它的名字
          ptr2ptr.insert(string(value->name));
        }
        instInfo tmp;
        tmp.ret = ret;
        instTable[value] =tmp;
        break;      
      }
      case KOOPA_RVT_STORE: {
        // 访问store指令
        Visit(kind.data.store);
        break;
      }
      case KOOPA_RVT_LOAD: {
        // 访问load指令
        ret = Visit(kind.data.load);
        instInfo tmp;
        tmp.ret = ret;
        instTable[value] =tmp;
        break;
      }
      case KOOPA_RVT_BRANCH: {
        // branch指令
        Visit(kind.data.branch);
        break;
      }
      case KOOPA_RVT_JUMP: {
        // jump 指令
        Visit(kind.data.jump);
        break;
      }
      case KOOPA_RVT_CALL: {
        // call 指令

        ret = Visit(kind.data.call);
        instInfo tmp;
        tmp.ret = ret;
        instTable[value] =tmp;
        break;
      }
      case KOOPA_RVT_FUNC_ARG_REF: {
        // 函数参数指令
        ret = Visit(kind.data.func_arg_ref);
        instInfo tmp;
        tmp.ret = ret;
        instTable[value] = tmp;
        break;
      }
      case KOOPA_RVT_GLOBAL_ALLOC: {
        // 全局变量指令
        ret = Visit(kind.data.global_alloc, true, string(value->name + 1), GetSize(value->ty->data.pointer.base));
        instInfo tmp;
        tmp.ret = ret;
        instTable[value] = tmp;
        break;
      }
      case KOOPA_RVT_GET_ELEM_PTR: {
        // getelemptr 指令
        ret = Visit(kind.data.get_elem_ptr);
        instInfo tmp;
        tmp.ret = ret;
        instTable[value] = tmp;
        break; 

      }
      case KOOPA_RVT_GET_PTR: {
        // getptr 指令
        ret = Visit(kind.data.get_ptr);
        instInfo tmp;
        tmp.ret = ret;
        instTable[value] = tmp;
        break; 
      }
      default:
        // 其他类型暂时遇不到
        cerr << kind.tag << " in big visit" << endl;
        cerr << "NEW INSTS TYPE" << endl;
        assert(false);
    }

    return ret;
  }
}

void Visit(const koopa_raw_value_t &value, vector<string>& vals) { // 访问aggregate 命令
  const auto &kind = value->kind;
  switch (kind.tag) {
    case KOOPA_RVT_AGGREGATE: {
      // 得到数组初始化的量
      Visit(kind.data.aggregate, vals);
      break; 
    }
    default:
      // 其他类型暂时遇不到
      cerr << kind.tag << " in small visit" << endl;
      cerr << "NEW INSTS TYPE" << endl;
      assert(false);
  }
} 


// 访问对应类型指令的函数定义
// 此下的函数参数都不是指针

// 访问getptr指令
string Visit(const koopa_raw_get_ptr_t& get_ptr) {
  string reg1;
  string reg2;  
  string reg3;
  string src = Visit(get_ptr.src); // 数组变量

  // 计算地址
  if((get_ptr.src->kind).tag == KOOPA_RVT_GLOBAL_ALLOC) { // 注意这里我只要地址 所以要提取出来reg
    int beginIndex = 0;
    while(beginIndex < src.length() && src[beginIndex] != '(')
      beginIndex++;
    reg1 = src.substr(beginIndex + 1, 2);
  }
  else if(((get_ptr.src->kind).tag == KOOPA_RVT_GET_PTR) || ((get_ptr.src->kind).tag == KOOPA_RVT_GET_ELEM_PTR) || ((get_ptr.src->kind).tag == KOOPA_RVT_LOAD && ptr2ptr.count(string(get_ptr.src->kind.data.load.src->name)))) { 
    reg1 = regList[0];
    regList.erase(regList.begin());

    int srcOffset = strtol(src.c_str(), NULL, 10);
    if(srcOffset > 2047){ // 要存到寄存器里
      cout << " li " << reg1 << ", " << srcOffset << endl;
      cout << " add " << reg1 << ", sp, " << reg1 << endl;
      cout << " lw " << reg1 << ", 0(" << reg1 << ')' << endl;
    }
    else{
      cout << " lw " << reg1 << ", " << src << endl;
    }
  }
  else {
    reg1 = regList[0];
    regList.erase(regList.begin());

    int srcOffset = strtol(src.c_str(), NULL, 10);
    if(srcOffset > 2047){ // 要存到寄存器里
      cout << " li " << reg1 << ", " << srcOffset << endl;
      cout << " add " << reg1 << ", sp, " << reg1 << endl;
    }
    else{
      cout << " addi " << reg1 << ", sp, " << srcOffset << endl;
    }
  }

  // 计算getptr的偏移量 存在reg2里
  reg2 = regList[0];
  retInt = true;
  string tmpIndex = Visit(get_ptr.index);
  retInt = false;
  if(tmpIndex[tmpIndex.size()-1] == ')') { // 是栈地址
    // cerr << tmpIndex << endl;
    int tmpOffset = strtol(tmpIndex.c_str(), NULL, 10);

    if(tmpOffset > 2047){ // 要存到寄存器里
      cout << " li " << reg2 << ", " << tmpOffset << endl;
      cout << " add " << reg2 << ", sp, " << reg2 << endl;
      cout << " lw " << reg2 << ", 0(" << reg2 << ')' << endl;
    }
    else
      cout << " lw " << reg2 << ", " << tmpIndex << endl;
    
    // 如果是全局变量 就把reg还回去
    if((get_ptr.index->kind).tag == KOOPA_RVT_GLOBAL_ALLOC) {
      int beginIndex = 0;
      while(beginIndex < tmpIndex.length() && tmpIndex[beginIndex] != '(')
        beginIndex++;
      
      string retReg = tmpIndex.substr(beginIndex + 1, 2);
      regList.emplace_back(retReg);
    }
    int subSize = GetSize(get_ptr.src->ty->data.pointer.base); // getPtr的话 我实际上是一个整数指针
    reg3 = regList[1];
    cout << " li " << reg3 << ", " << subSize << endl;
    cout << " mul " << reg2 << ", " << reg2 << ", " << reg3 << endl;
  }
  else {
    int curIndex = strtol(tmpIndex.c_str(), NULL, 10);
    int cntForPow = 0;
    if(curIndex >= 1) {
      while(curIndex % 2 == 0) {
        curIndex /= 2;
        cntForPow++;
      }
    }
    if(curIndex == 0) {
      reg2 = "x0";
    }
    else if(curIndex == 1) { // 是2的整数次幂
      int subSize = GetSize(get_ptr.src->ty->data.pointer.base); // getPtr的话 我实际上是一个整数指针
      if(cntForPow == 0) {
        cout << " li " << reg2 << ", " << subSize << endl;
      }
      else {
        cout << " li " << reg2 << ", " << cntForPow << endl;
        reg3 = regList[1];
        cout << " li " << reg3 << ", " << subSize << endl;
        
        cout << " sll " << reg2 << ", " << reg3 << ", " << reg2 << endl;
      }
    }
    else { // 乖乖用乘法

      cout << " li " << reg2 << ", " << tmpIndex << endl;
      int subSize = GetSize(get_ptr.src->ty->data.pointer.base); // getPtr的话 我实际上是一个整数指针
      reg3 = regList[1];
      cout << " li " << reg3 << ", " << subSize << endl;
      cout << " mul " << reg2 << ", " << reg2 << ", " << reg3 << endl;
    }
  }

  // 计算getptr的结果
  cout << " add " << reg1 << ", " << reg1 << ", " << reg2 << endl;

  // 结果保存到栈帧
  int curOffset = SPoffset.top();
  SPoffset.pop();
  SPoffset.push(curOffset + 4);
  if(curOffset > 2047) {
    string reg = regList[0];
    cout << " li " << reg << ", " << curOffset << endl;
    cout << " add " << reg << ", sp, " << reg << endl;
    cout << " sw " << reg1 << ", 0(" << reg << ')' << endl; 
  }
  else
    cout << " sw " << reg1 << ", " << curOffset << "(sp)" << endl;

  // 把reg1还回去
  regList.emplace_back(reg1);

  return to_string(curOffset) + "(sp)";
}

// 访问aggregate
void Visit(const koopa_raw_aggregate_t& aggregate, vector<string>& vals) {
  auto ptr = aggregate.elems;
  for(size_t i = 0; i < ptr.len; i++) {
    auto inst = reinterpret_cast<koopa_raw_value_t>(ptr.buffer[i]);
    if(inst->kind.tag == KOOPA_RVT_AGGREGATE) {
      Visit(inst, vals);
    }
    else
      vals.emplace_back(Visit(inst));
  }
}
 
// 访问getelemptr 指令
string Visit(const koopa_raw_get_elem_ptr_t& get_elem_ptr) {
  string reg1;
  string reg2;  
  string reg3;
  string src = Visit(get_elem_ptr.src); // 数组变量

  // 计算地址
  if((get_elem_ptr.src->kind).tag == KOOPA_RVT_GLOBAL_ALLOC) { // 注意这里我只要地址 所以要提取出来reg
    int beginIndex = 0;
    while(beginIndex < src.length() && src[beginIndex] != '(')
      beginIndex++;
    reg1 = src.substr(beginIndex + 1, 2);
  }
  else if((get_elem_ptr.src->kind).tag == KOOPA_RVT_GET_PTR || (get_elem_ptr.src->kind).tag == KOOPA_RVT_GET_ELEM_PTR) { // 是指针
    reg1 = regList[0];
    regList.erase(regList.begin());

    int srcOffset = strtol(src.c_str(), NULL, 10);
    if(srcOffset > 2047){ // 要存到寄存器里
      cout << " li " << reg1 << ", " << srcOffset << endl;
      cout << " add " << reg1 << ", sp, " << reg1 << endl;
      cout << " lw " << reg1 << ", 0(" << reg1 << ')' << endl;
    }
    else{
      cout << " lw " << reg1 << ", " << src << endl;
    }
  }
  else {
    reg1 = regList[0];
    regList.erase(regList.begin());

    int srcOffset = strtol(src.c_str(), NULL, 10);
    if(srcOffset > 2047){ // 要存到寄存器里
      cout << " li " << reg1 << ", " << srcOffset << endl;
      cout << " add " << reg1 << ", sp, " << reg1 << endl;
    }
    else{
      cout << " addi " << reg1 << ", sp, " << srcOffset << endl;
    }
  }

  // 计算getelemptr的偏移量 存在reg2里
  reg2 = regList[0];
  retInt = true;
  string tmpIndex = Visit(get_elem_ptr.index);
  retInt = false;
  if(tmpIndex[tmpIndex.size()-1] == ')') { // 是栈地址
    // cerr << tmpIndex << endl;
    int tmpOffset = strtol(tmpIndex.c_str(), NULL, 10);
    if(tmpOffset > 2047){ // 要存到寄存器里
      cout << " li " << reg2 << ", " << tmpOffset << endl;
      cout << " add " << reg2 << ", sp, " << reg2 << endl;
      cout << " lw " << reg2 << ", 0(" << reg2 << ')' << endl;
    }
    else
      cout << " lw " << reg2 << ", " << tmpIndex << endl;
    
    // 如果是全局变量 就把reg还回去
    if((get_elem_ptr.index->kind).tag == KOOPA_RVT_GLOBAL_ALLOC) {
      int beginIndex = 0;
      while(beginIndex < tmpIndex.length() && tmpIndex[beginIndex] != '(')
        beginIndex++;
      
      string retReg = tmpIndex.substr(beginIndex + 1, 2);
      regList.emplace_back(retReg);
    }
    int subSize = GetSize(get_elem_ptr.src->ty->data.pointer.base->data.array.base); // src 是指针 base是数组 再base才是数组元素大小
    reg3 = regList[1];
    cout << " li " << reg3 << ", " << subSize << endl;
    cout << " mul " << reg2 << ", " << reg2 << ", " << reg3 << endl;
  }
  else {
    int curIndex = strtol(tmpIndex.c_str(), NULL, 10);
    
    int cntForPow = 0;
    if(curIndex >= 1) {
      while(curIndex % 2 == 0) {
        curIndex /= 2;
        cntForPow++;
      }
    }
    if(curIndex == 0) {
      reg2 = "x0";
    }
    else if(curIndex == 1) { // 是2的整数次幂
      int subSize = GetSize(get_elem_ptr.src->ty->data.pointer.base->data.array.base); // src 是指针 base是数组 再base才是数组元素大小

      if(cntForPow == 0) {
        cout << " li " << reg2 << ", " << subSize << endl;
      }
      else {
        cout << " li " << reg2 << ", " << cntForPow << endl;
        reg3 = regList[1];
        cout << " li " << reg3 << ", " << subSize << endl;
        
        cout << " sll " << reg2 << ", " << reg3 << ", " << reg2 << endl;
      }
    }
    else { // 乖乖用乘法

      cout << " li " << reg2 << ", " << tmpIndex << endl;
      int subSize = GetSize(get_elem_ptr.src->ty->data.pointer.base->data.array.base);
      reg3 = regList[1];
      cout << " li " << reg3 << ", " << subSize << endl;
      cout << " mul " << reg2 << ", " << reg2 << ", " << reg3 << endl;
    }
  }

  // 计算getelemptr的结果
  cout << " add " << reg1 << ", " << reg1 << ", " << reg2 << endl;

  // 结果保存到栈帧
  int curOffset = SPoffset.top();
  SPoffset.pop();
  SPoffset.push(curOffset + 4);
  if(curOffset > 2047) {
    string reg = regList[0];
    cout << " li " << reg << ", " << curOffset << endl;
    cout << " add " << reg << ", sp, " << reg << endl;
    cout << " sw " << reg1 << ", 0(" << reg << ')' << endl; 
  }
  else
    cout << " sw " << reg1 << ", " << curOffset << "(sp)" << endl;

  // 把reg1还回去
  regList.emplace_back(reg1);

  return to_string(curOffset) + "(sp)";
}



// 访问函数参数指令
string Visit(const koopa_raw_func_arg_ref_t& func_arg_ref) {
  int index = func_arg_ref.index;
  if(index <= 7) {
    return "a" + to_string(index);
  }
  else {
    return to_string(offset + 4 * (index - 8)) + "(sp)";
  }
}



// 访问call指令
string Visit(const koopa_raw_call_t& call) {
  for (size_t i = 0; i < call.args.len; ++i) {
    auto ptr = call.args.buffer[i];
    // 访问指令
    string tmp = Visit(reinterpret_cast<koopa_raw_value_t>(ptr));
    // cerr << "tmp: " << tmp << endl;
    if(i <= 7) { // 我第一时间把reg中的值存到栈上了 所以无所谓
      if(tmp[0] >= '0' && tmp[0] <= '9') { // tmp 是栈地址
        int tmpOffset = strtol(tmp.c_str(), NULL, 10);

        //TODO
        if(tmpOffset > 2047){ // 要存到寄存器里
          string reg = regList[0];
          cout << " li " << reg << ", " << tmpOffset << endl;
          cout << " add " << reg << ", sp, " << reg << endl;
          cout << " lw a" << i << ", 0(" << reg << ')' << endl;
        }
        else
          cout << " lw a" << i << ", " << tmp << endl;
        
        // 如果是全局变量 就把reg还回去
        if((reinterpret_cast<koopa_raw_value_t>(ptr)->kind).tag == KOOPA_RVT_GLOBAL_ALLOC) {
          int beginIndex = 0;
          while(beginIndex < tmp.length() && tmp[beginIndex] != '(')
            beginIndex++;
          
          string retReg = tmp.substr(beginIndex + 1, 2);
          regList.emplace_back(retReg);
        }
      }
      else if(tmp != ("a" + to_string(i)))
        cout << " mv a" << i << ", " << tmp << endl;

      if((reinterpret_cast<koopa_raw_value_t>(ptr)->kind).tag == KOOPA_RVT_INTEGER) { // 是常数 放回寄存器
        if(tmp != "x0") {
          regList.emplace_back(tmp);
          instTable.erase(reinterpret_cast<koopa_raw_value_t>(ptr));
        }
      }
      // 不再用这个寄存器
      auto it = find(regList.begin(), regList.end(), "a" + to_string(i));
      if(it != regList.end())
        regList.erase(it);
    }
    else {
      if(tmp[0] >= '0' && tmp[0] <= '9') { // tmp 是栈地址
        string reg = regList[0];
        int tmpOffset = strtol(tmp.c_str(), NULL, 10);
        if(tmpOffset > 2047){ // 要存到寄存器里
          cout << " li " << reg << ", " << tmpOffset << endl;
          cout << " add " << reg << ", sp, " << reg << endl;
          cout << " lw " << reg << ", 0(" << reg << ')' << endl;
        }
        else 
          cout << " lw " << reg << ", " << tmp << endl;
        
        cout << " sw " << reg << ", " << 4 * (i - 8) << "(sp)" << endl; // 我们假设不会有好几百个函数参数...
        // 如果是全局变量 就把reg还回去
        if((reinterpret_cast<koopa_raw_value_t>(ptr)->kind).tag == KOOPA_RVT_GLOBAL_ALLOC){
          int beginIndex = 0;
          while(beginIndex < tmp.length() && tmp[beginIndex] != '(')
            beginIndex++;
          
          string retReg = tmp.substr(beginIndex + 1, 2);
          regList.emplace_back(retReg);
        }
      }
      else
        cout << " sw " << tmp << ", " << 4 * (i - 8) << "(sp)" << endl; // 我们假设不会有好几百个函数参数...

      if((reinterpret_cast<koopa_raw_value_t>(ptr)->kind).tag == KOOPA_RVT_INTEGER) { // 是常数 放回寄存器
        if(tmp != "x0") {
          regList.emplace_back(tmp);
          instTable.erase(reinterpret_cast<koopa_raw_value_t>(ptr));
        }
      }
    }
  }

  cout << " call " << ((call.callee)->name + 1) << endl;

  // call完就可以把reg都放回去了 除了a0 因为a0存了返回值
  if(call.args.len > 1) {
    for(int i = 1; i <= min(7, int(call.args.len) - 1); i++) {
      if(find(regList.begin(), regList.end(), "a" + to_string(i)) == regList.end())
        regList.emplace_back("a" + to_string(i));
    }
  }

  if(call.callee->ty->data.function.ret->tag == KOOPA_RTT_UNIT) { // 无返回值
    if(find(regList.begin(), regList.end(), "a0") == regList.end())
      regList.emplace_back("a0");
      
    return "";
  }
  else { // 有返回值 默认是int
    int curOffset = SPoffset.top();
    SPoffset.pop();
    SPoffset.push(curOffset + 4);
    if(curOffset > 2047) {
      string reg = regList[0];
      cout << " li " << reg << ", " << curOffset << endl;
      cout << " add " << reg << ", sp, " << reg << endl;
      cout << " sw a0, 0(" << reg << ')' << endl; 
    }
    else
      cout << " sw a0, " << curOffset << "(sp)" << endl;

    if(find(regList.begin(), regList.end(), "a0") == regList.end())
      regList.emplace_back("a0");
      
    return to_string(curOffset) + "(sp)";
  }
}



// 访问jump指令
void Visit(const koopa_raw_jump_t& jump) {
  string target((jump.target)->name + 1);
  cout << " j " << target << endl;
}



// 访问branch指令
void Visit(const koopa_raw_branch_t &branch) {
  string truebb((branch.true_bb)->name + 1);
  string falsebb((branch.false_bb)->name + 1);
  string cond = Visit(branch.cond);

  if(cond[0] >= '0' && cond[0] <= '9') { // cond 是栈地址
    int condOffset = strtol(cond.c_str(), NULL, 10);
    string reg = regList[0];

    if(condOffset > 2047){ // 要存到寄存器里
      cout << " li " << reg << ", " << condOffset << endl;
      cout << " add " << reg << ", sp, " << reg << endl;
      cout << " lw " << reg << ", 0(" << reg << ')' << endl;
    }
    else
      cout << " lw " << reg << ", " << cond << endl;
    
    // 如果是全局变量 就把reg还回去
    if((branch.cond->kind).tag == KOOPA_RVT_GLOBAL_ALLOC){
      int beginIndex = 0;
      while(beginIndex < cond.length() && cond[beginIndex] != '(')
        beginIndex++;
      
      string retReg = cond.substr(beginIndex + 1, 2);
      regList.emplace_back(retReg);
    }

    cond = reg;
  }
  else { // 虽然不需要别的操作 但是还是要还寄存器的哦
    if((branch.cond->kind).tag == KOOPA_RVT_INTEGER) { // 是常数 放回寄存器
      if(cond != "x0") {
        regList.emplace_back(cond);
        instTable.erase(branch.cond);
      }
    }
  }

  cout << " bnez " << cond << ", " << truebb << endl;
  cout << " j " << falsebb << endl;
}



// 访问alloc指令
string Visit(const koopa_raw_global_alloc_t &alloc, bool flag, string name, int size){ // false局部变量 true全局变量
  if(flag) { // 全局变量
    cout << " .data" << endl;
    cout << " .globl " << name << endl;
    cout << name << ':' << endl;
    if((alloc.init->kind).tag == KOOPA_RVT_ZERO_INIT) {
      cout << " .zero " << size << endl;
    }
    else {
      retInt = true;
      if(alloc.init->ty->tag == KOOPA_RTT_INT32) {
        string val = Visit(alloc.init);
        cout << " .word " << val << endl;
      }
      else if(alloc.init->ty->tag == KOOPA_RTT_ARRAY) {
        vector<string> vals = vector<string>();
        Visit(alloc.init, vals);
        int lastNotZero = vals.size()-1;
        while(lastNotZero >= 0 && vals[lastNotZero] == "0")
          lastNotZero--;

        lastNotZero++;
        for(size_t i = 0; i < lastNotZero; i++) {
          cout << " .word " << vals[i] << endl;
        }
        if(vals.size() > lastNotZero)
          cout << " .zero " << (vals.size() - lastNotZero) * 4 << endl;
      }
      else if(alloc.init->ty->tag == KOOPA_RTT_POINTER) { // 不可能是全局变量
        cerr << "Init val has a wrong type." << endl;
        assert(false);
      }
      else {
        cerr << "Init val has a wrong type." << endl;
        assert(false);
      }
      retInt = false;
    }
    globalVar.insert(name);
    return name;
  }
  else { // 局部变量
    int curOffset = SPoffset.top();
    SPoffset.pop();
    SPoffset.push(curOffset + size);
    return to_string(curOffset) + "(sp)";
  }
}

// 访问store指令
void Visit(const koopa_raw_store_t &store) {
  string val = Visit(store.value);
  string dst = Visit(store.dest);
  if(val[0] >= '0' && val[0] <= '9') { // val 是栈地址
    int valOffset = strtol(val.c_str(), NULL, 10);
    string reg = regList[0];
    // 如果是个指针 就要load 和 store
    if(store.value->kind.tag == KOOPA_RVT_GET_PTR || store.value->kind.tag == KOOPA_RVT_GET_ELEM_PTR) {
      if(valOffset > 2047){ // 要存到寄存器里
        cout << " li " << reg << ", " << valOffset << endl;
        cout << " add " << reg << ", sp, " << reg << endl;
        cout << " lw " << reg << ", 0(" << reg << ')' << endl;
      }
      else {
        cout << " lw " << reg << ", " << val << endl;
      }
      cout << " lw " << reg << ", 0(" << reg << ')' << endl;
    }
    else {
      if(valOffset > 2047){ // 要存到寄存器里
        cout << " li " << reg << ", " << valOffset << endl;
        cout << " add " << reg << ", sp, " << reg << endl;
        cout << " lw " << reg << ", 0(" << reg << ')' << endl;
      }
      else
        cout << " lw " << reg << ", " << val << endl;
    }

    int dstOffset = strtol(dst.c_str(), NULL, 10);
    // 如果是个指针 就要load 和 store
    if(store.dest->kind.tag == KOOPA_RVT_GET_PTR || store.dest->kind.tag == KOOPA_RVT_GET_ELEM_PTR) {
      string reg2 = regList[1];
      if(dstOffset > 2047){ // 要存到寄存器里
        cout << " li " << reg2 << ", " << dstOffset << endl;
        cout << " add " << reg2 << ", sp, " << reg2 << endl;
        cout << " lw " << reg2 << ", 0(" << reg2 << ')' << endl;
      }
      else {
        cout << " lw " << reg2 << ", " << dst << endl;
      }
      cout << " sw " << reg << ", 0(" << reg2 << ')' << endl;
    }
    else {
      if(dstOffset > 2047){ // 要存到寄存器里
        string reg2 = regList[1]; 
        cout << " li " << reg2 << ", " << dstOffset << endl;
        cout << " add " << reg2 << ", sp, " << reg2 << endl;
        cout << " sw " << reg << ", 0(" << reg2 << ')' << endl;
      }
      else {
        cout << " sw " << reg << ", " << dst << endl;
      }
    }
    // 如果是全局变量 就把reg还回去
    if((store.value->kind).tag == KOOPA_RVT_GLOBAL_ALLOC){
      int beginIndex = 0;
      while(beginIndex < val.length() && val[beginIndex] != '(')
        beginIndex++;
      
      string retReg = val.substr(beginIndex + 1, 2);
      regList.emplace_back(retReg);
    }
    // 如果是全局变量 就把reg还回去
    if((store.dest->kind).tag == KOOPA_RVT_GLOBAL_ALLOC){
      int beginIndex = 0;
      while(beginIndex < dst.length() && dst[beginIndex] != '(')
        beginIndex++;
      
      string retReg = dst.substr(beginIndex + 1, 2);
      regList.emplace_back(retReg);
    }
  }
  else{
    // 这里dst也要判断是否是指针或者offset有没有超2047
    int dstOffset = strtol(dst.c_str(), NULL, 10);
    // 如果是个指针 就要load 和 store
    if(store.dest->kind.tag == KOOPA_RVT_GET_PTR || store.dest->kind.tag == KOOPA_RVT_GET_ELEM_PTR) {
      string reg = regList[0];
      if(dstOffset > 2047){ // 要存到寄存器里
        cout << " li " << reg << ", " << dstOffset << endl;
        cout << " add " << reg << ", sp, " << reg << endl;
        cout << " lw " << reg << ", 0(" << reg << ')' << endl;
      }
      else {
        cout << " lw " << reg << ", " << dst << endl;
      }
      cout << " sw " << val << ", 0(" << reg << ')' << endl;
    }
    else {
      if(dstOffset > 2047){ // 要存到寄存器里
        string reg = regList[0]; 
        cout << " li " << reg << ", " << dstOffset << endl;
        cout << " add " << reg << ", sp, " << reg << endl;
        cout << " sw " << val << ", 0(" << reg << ')' << endl;
      }
      else {
        cout << " sw " << val << ", " << dst << endl;
      }
    }
    // 如果是全局变量 就把reg还回去
    if((store.dest->kind).tag == KOOPA_RVT_GLOBAL_ALLOC){
      int beginIndex = 0;
      while(beginIndex < dst.length() && dst[beginIndex] != '(')
        beginIndex++;
      
      string retReg = dst.substr(beginIndex + 1, 2);
      regList.emplace_back(retReg);
    }

    if(((store.value)->kind).tag == KOOPA_RVT_INTEGER){ // 是常数 放回寄存器
      if(val != "x0") {
        regList.emplace_back(val);
        instTable.erase(store.value);
      }
    }
    else {
      if(val != "x0") {
        if(find(regList.begin(), regList.end(), val) == regList.end()) {
          regList.emplace_back(val);
          instTable[store.value].ret = dst;
        }
      } 
    }
  }
}

// 访问load指令
string Visit(const koopa_raw_load_t &load){
  string src = Visit(load.src);

  int srcOffset = strtol(src.c_str(), NULL, 10);
  string reg = regList[0];
  // 如果是个数组 就要load 和 store 因为要存到指针里
  if(load.src->kind.tag == KOOPA_RVT_GET_PTR || load.src->kind.tag == KOOPA_RVT_GET_ELEM_PTR) {
    if(srcOffset > 2047){ // 要存到寄存器里
      cout << " li " << reg << ", " << srcOffset << endl;
      cout << " add " << reg << ", sp, " << reg << endl;
      cout << " lw " << reg << ", 0(" << reg << ')' << endl;
    }
    else {
      cout << " lw " << reg << ", " << src << endl;
    }
    cout << " lw " << reg << ", 0(" << reg << ')' << endl;
  }
  else {
    if(srcOffset > 2047){ // 要存到寄存器里 且这里一定是xx(sp)的形式
      cout << " li " << reg << ", " << srcOffset << endl;
      cout << " add " << reg << ", sp, " << reg << endl;
      cout << " lw " << reg << ", 0(" << reg << ')' << endl;
    }
    else
      cout << " lw " << reg << ", " << src << endl;
  }
  // 如果是全局变量 就把reg还回去
  if((load.src->kind).tag == KOOPA_RVT_GLOBAL_ALLOC){
    int beginIndex = 0;
    while(beginIndex < src.length() && src[beginIndex] != '(')
      beginIndex++;
    
    string retReg = src.substr(beginIndex + 1, 2);
    regList.emplace_back(retReg);
  }


  int curOffset = SPoffset.top();
  SPoffset.pop();
  SPoffset.push(curOffset+4);

  if(curOffset > 2047) {
    string reg2 = regList[1]; // 避免和reg重复
    cout << " li " << reg2 << ", " << curOffset << endl;
    cout << " add " << reg2 << ", sp, " << reg2 << endl;
    cout << " sw " << reg << ", 0(" << reg2 << ')' << endl; 
  }
  else
    cout << " sw " << reg << ", " << curOffset << "(sp)" << endl;

  return to_string(curOffset) + "(sp)";
}

// 访问binary指令
string Visit(const koopa_raw_binary_t &binary){


  string leftPos = Visit(binary.lhs);
  string rightPos = Visit(binary.rhs);
  
  string left, right;
  if(leftPos[0] != 't' && leftPos[0] != 'a' && leftPos != "x0"){ // 栈地址 则读到寄存器
    int leftOffset = strtol(leftPos.c_str(), NULL, 10);
    string reg = regList[0];

    if(leftOffset > 2047){ // 要存到寄存器里
      cout << " li " << reg << ", " << leftOffset << endl;
      cout << " add " << reg << ", sp, " << reg << endl;
      cout << " lw " << reg << ", 0(" << reg << ')' << endl;
    }
    else
      cout << " lw " << reg << ", " << leftPos << endl;
    
    // 如果是全局变量 就把reg还回去
    if((binary.lhs->kind).tag == KOOPA_RVT_GLOBAL_ALLOC){
      int beginIndex = 0;
      while(beginIndex < leftPos.length() && leftPos[beginIndex] != '(')
        beginIndex++;
      
      string retReg = leftPos.substr(beginIndex + 1, 2);
      regList.emplace_back(retReg);
    }
    left = reg;
  }
  else{
    left = leftPos;
  }


  if(rightPos[0] != 't' && rightPos[0] != 'a' && rightPos != "x0"){ // 栈地址 则读到寄存器
    int rightOffset = strtol(rightPos.c_str(), NULL, 10);
    string reg = regList[1];
    if(rightOffset > 2047){ // 要存到寄存器里
      cout << " li " << reg << ", " << rightOffset << endl;
      cout << " add " << reg << ", sp, " << reg << endl;
      cout << " lw " << reg << ", 0(" << reg << ')' << endl;
    }
    else
      cout << " lw " << reg << ", " << rightPos << endl;
    
    // 如果是全局变量 就把reg还回去
    if((binary.rhs->kind).tag == KOOPA_RVT_GLOBAL_ALLOC){
      int beginIndex = 0;
      while(beginIndex < rightPos.length() && rightPos[beginIndex] != '(')
        beginIndex++;
      
      string retReg = rightPos.substr(beginIndex + 1, 2);
      regList.emplace_back(retReg);
    }
    right = reg;
  }
  else{
    right = rightPos;
  }
  
  string ret;
  switch(binary.op) {

    case KOOPA_RBO_EQ: {
      // eq 指令
      if(left == "x0" && right == "x0"){ // 特殊情况 0 == 0
        ret = regList[0];
        cout << " mv  " << ret << ", x0" << endl;
      }
      else if(left == "x0"){
        ret = right;
      }
      else if(right == "x0"){
        ret = left;
      }
      else{
        ret = left;
        cout << " xor " << ret << ", " << left << ", " << right << endl;

        // reg还回去 要判断是不是reg
        if(right != "x0" && right != ret && (right[0] == 't' || right[0] == 'a')){
          if(find(regList.begin(), regList.end(), right) == regList.end()) {
            regList.emplace_back(right);
            if(right == rightPos)
              instTable.erase(binary.rhs);
          }
        }
      }
      cout << " seqz  " << ret << ", " << ret << endl;
      break;
    }

    case KOOPA_RBO_NOT_EQ: {
      // not_eq 指令
      if(left == "x0" && right == "x0"){ // 特殊情况 0 != 0
        ret = regList[0];
        cout << " mv  " << ret << ", x0" << endl;
      }
      else if(left == "x0"){
        ret = right;
      }
      else if(right == "x0"){
        ret = left;
      }
      else{
        ret = left;
        cout << " xor " << ret << ", " << left << ", " << right << endl;

        // reg还回去
        if(right != "x0" && right != ret && (right[0] == 't' || right[0] == 'a')){
          if(find(regList.begin(), regList.end(), right) == regList.end()) {
            regList.emplace_back(right);
            if(right == rightPos)
              instTable.erase(binary.rhs);
          }
        }
      }
      cout << " snez  " << ret << ", " << ret << endl;
      break;
    }
    
    case KOOPA_RBO_SUB: {
      // sub 指令
      if(left == "x0" && right == "x0"){ // 特殊情况 0 sub 0
        ret = regList[0];
        cout << " mv  " << ret << ", x0" << endl;
      }

      else{
        ret = (left == "x0") ? right : left;
        cout << " sub " << ret << ", " << left << ", " << right << endl;

        // reg还回去
        if(left != "x0" && left != ret && (left[0] == 't' || left[0] == 'a')){
          if(find(regList.begin(), regList.end(), left) == regList.end()) {
            regList.emplace_back(left);
            if(left == leftPos)
              instTable.erase(binary.lhs);
          }
        }

        if(right != "x0" && right != ret && (right[0] == 't' || right[0] == 'a')){
          if(find(regList.begin(), regList.end(), right) == regList.end()) {
            regList.emplace_back(right);
            if(right == rightPos)
              instTable.erase(binary.rhs);
          }
        }
      }      
      break;
    }

    case KOOPA_RBO_MUL: {
      // mul 指令
      if(left == "x0" && right == "x0"){ // 特殊情况 0 mul 0
        ret = regList[0];
        cout << " mv  " << ret << ", x0" << endl;
      }

      else{
        ret = (left == "x0") ? right : left;
        cout << " mul " << ret << ", " << left << ", " << right << endl;

        // reg还回去
        if(left != "x0" && left != ret && (left[0] == 't' || left[0] == 'a')){
          if(find(regList.begin(), regList.end(), left) == regList.end()) {
            regList.emplace_back(left);
            if(left == leftPos)
              instTable.erase(binary.lhs);
          }
        }

        if(right != "x0" && right != ret && (right[0] == 't' || right[0] == 'a')){
          if(find(regList.begin(), regList.end(), right) == regList.end()) {
            regList.emplace_back(right);
            if(right == rightPos)
              instTable.erase(binary.rhs);
          }
        }
      }

      break;
    }

    case KOOPA_RBO_ADD: {
      // add 指令
      if(left == "x0" && right == "x0"){ // 特殊情况 0 + 0
        ret = regList[0];
        cout << " mv  " << ret << ", x0" << endl;
      }

      else{
        ret = (left == "x0") ? right : left;
        cout << " add " << ret << ", " << left << ", " << right << endl;

        // reg还回去
        if(left != "x0" && left != ret && (left[0] == 't' || left[0] == 'a')){
          if(find(regList.begin(), regList.end(), left) == regList.end()) {
            regList.emplace_back(left);
            if(left == leftPos)
              instTable.erase(binary.lhs);
          }
        }

        if(right != "x0" && right != ret && (right[0] == 't' || right[0] == 'a')){
          if(find(regList.begin(), regList.end(), right) == regList.end()) {
            regList.emplace_back(right);
            if(right == rightPos)
              instTable.erase(binary.rhs);
          }
        }
      }

      break;
    }

    case KOOPA_RBO_DIV: {
      // div 指令
      if(right == "x0"){ // 特殊情况 0 是被除数
        cerr << "0 is the divisor" << endl;
        assert(0);
      }
      else{
        ret = right;
        cout << " div " << ret << ", " << left << ", " << right << endl;

        // reg还回去
        if(left != "x0" && left != ret && (left[0] == 't' || left[0] == 'a')){
          if(find(regList.begin(), regList.end(), left) == regList.end()) {
            regList.emplace_back(left);
            if(left == leftPos)
              instTable.erase(binary.lhs);
          }
        }
      }

      break;
    }

    case KOOPA_RBO_MOD: {
      // mod 指令
      if(right == "x0"){ // 特殊情况 0 是被除数
        cerr << "0 is the divisor" << endl;
        assert(0);
      }
      else{
        ret = right;
        cout << " rem " << ret << ", " << left << ", " << right << endl;

        // reg还回去
        if(left != "x0" && left != ret && (left[0] == 't' || left[0] == 'a')){
          if(find(regList.begin(), regList.end(), left) == regList.end()) {
            regList.emplace_back(left);
            if(left == leftPos)
              instTable.erase(binary.lhs);
          }
        }
      }

      break;
    }

    case KOOPA_RBO_OR: {
      // or 指令
      if(left == "x0" && right == "x0"){ // 特殊情况 0 | 0
        ret = regList[0];
      }

      else{
        ret = (left == "x0") ? right : left;

        // reg还回去
        if(left != "x0" && left != ret && (left[0] == 't' || left[0] == 'a')){
          if(find(regList.begin(), regList.end(), left) == regList.end()) {
            regList.emplace_back(left);
            if(left == leftPos)
              instTable.erase(binary.lhs);
          }
        }

        if(right != "x0" && right != ret && (right[0] == 't' || right[0] == 'a')){
          if(find(regList.begin(), regList.end(), right) == regList.end()) {
            regList.emplace_back(right);
            if(right == rightPos)
              instTable.erase(binary.rhs);
          }
        }
      }

      cout << " or " << ret << ", " << left << ", " << right << endl;
      break;
    }

    case KOOPA_RBO_AND: {
      // and 指令
      if(left == "x0" && right == "x0"){ // 特殊情况 0 & 0
        ret = regList[0];
      }

      else{
        ret = (left == "x0") ? right : left;

        // reg还回去
        if(left != "x0" && left != ret && (left[0] == 't' || left[0] == 'a')){
          if(find(regList.begin(), regList.end(), left) == regList.end()) {
            regList.emplace_back(left);
            if(left == leftPos)
              instTable.erase(binary.lhs);
          }
        }

        if(right != "x0" && right != ret && (right[0] == 't' || right[0] == 'a')){
          if(find(regList.begin(), regList.end(), right) == regList.end()) {
            regList.emplace_back(right);
            if(right == rightPos)
              instTable.erase(binary.rhs);
          }
        }
      }

      cout << " and " << ret << ", " << left << ", " << right << endl;
      break;
    }

    case KOOPA_RBO_LT: {
      // lt 指令
      if(left == "x0" && right == "x0"){ // 特殊情况 0 < 0
        ret = regList[0];
      }

      else{
        ret = (left == "x0") ? right : left;

        // reg还回去
        if(left != "x0" && left != ret && (left[0] == 't' || left[0] == 'a')){
          if(find(regList.begin(), regList.end(), left) == regList.end()) {
            regList.emplace_back(left);
            if(left == leftPos)
              instTable.erase(binary.lhs);
          }
        }

        if(right != "x0" && right != ret && (right[0] == 't' || right[0] == 'a')){
          if(find(regList.begin(), regList.end(), right) == regList.end()) {
            regList.emplace_back(right);
            if(right == rightPos)
              instTable.erase(binary.rhs);
          }
        }
      }

      cout << " slt " << ret << ", " << left << ", " << right << endl;
      break;
    }

    case KOOPA_RBO_GT: {
      // gt 指令
      if(left == "x0" && right == "x0"){ // 特殊情况 0 > 0
        ret = regList[0];
      }

      else{
        ret = (left == "x0") ? right : left;

        // reg还回去
        if(left != "x0" && left != ret && (left[0] == 't' || left[0] == 'a')){
          if(find(regList.begin(), regList.end(), left) == regList.end()) {
            regList.emplace_back(left);
            if(left == leftPos)
              instTable.erase(binary.lhs);
          }
        }

        if(right != "x0" && right != ret && (right[0] == 't' || right[0] == 'a')){
          if(find(regList.begin(), regList.end(), right) == regList.end()) {
            regList.emplace_back(right);
            if(right == rightPos)
              instTable.erase(binary.rhs);
          }
        }
      }

      cout << " slt " << ret << ", " << right << ", " << left << endl;
      break;
    }

    case KOOPA_RBO_LE: {
      // lt_eq 指令
      if(left == "x0" && right == "x0"){ // 特殊情况 0 <= 0
        ret = regList[0];
        cout << " li " << ret << ", " << 1 << endl;
      }

      else{
        ret = (left == "x0") ? right : left;

        // reg还回去
        if(left != "x0" && left != ret && (left[0] == 't' || left[0] == 'a')){
          if(find(regList.begin(), regList.end(), left) == regList.end()) {
            regList.emplace_back(left);
            if(left == leftPos)
              instTable.erase(binary.lhs);
          }
        }

        if(right != "x0" && right != ret && (right[0] == 't' || right[0] == 'a')){
          if(find(regList.begin(), regList.end(), right) == regList.end()) {
            regList.emplace_back(right);
            if(right == rightPos)
              instTable.erase(binary.rhs);
          }
        }

        cout << " slt " << ret << ", " << right << ", " << left << endl;
        cout << " xor " << ret << ", " << ret << ", " << 1 << endl;
      }

      break;
    }

    case KOOPA_RBO_GE: {
      // gt_eq 指令
      if(left == "x0" && right == "x0"){ // 特殊情况 0 >= 0
        ret = regList[0];
        cout << " li " << ret << ", " << 1 << endl;
      }

      else{
        ret = (left == "x0") ? right : left;

        // reg还回去
        if(left != "x0" && left != ret && (left[0] == 't' || left[0] == 'a')){
          if(find(regList.begin(), regList.end(), left) == regList.end()) {
            regList.emplace_back(left);
            if(left == leftPos)
              instTable.erase(binary.lhs);
          }
        }

        if(right != "x0" && right != ret && (right[0] == 't' || right[0] == 'a')){
          if(find(regList.begin(), regList.end(), right) == regList.end()) {
            regList.emplace_back(right);
            if(right == rightPos)
              instTable.erase(binary.rhs);
          }
        }

        cout << " slt " << ret << ", " << left << ", " << right << endl;
        cout << " xor " << ret << ", " << ret << ", " << 1 << endl;
      }
      
      break;
    }

    default:
      //  其他类型暂时遇不到
      cerr << "NEW BINARY OP" << endl;
      assert(false); 
  }

  int curOffset = SPoffset.top();
  SPoffset.pop();
  SPoffset.push(curOffset+4);

  if(curOffset > 2047) {
    string reg = regList[1]; // 避免和ret重复
    cout << " li " << reg << ", " << curOffset << endl;
    cout << " add " << reg << ", sp, " << reg << endl;
    cout << " sw " << ret << ", 0(" << reg << ')' << endl; 
  }
  else
    cout << " sw " << ret << ", " << curOffset << "(sp)" << endl;

  if(ret != "x0") {// 用不到了 把ret还回去
    if(find(regList.begin(), regList.end(), ret) == regList.end()) {
      regList.emplace_back(ret);
    }
  }

  ret = to_string(curOffset) + "(sp)";
  return ret;
}


// 访问return 指令
void Visit(const koopa_raw_return_t &ret){
  if(ret.value != NULL){ // 有返回值才Visit
    string top = Visit(ret.value);
    if(top != "a0"){
      if(top[0] >= '0' && top[0] <= '9'){ // 是栈地址

        int topOffset = strtol(top.c_str(), NULL, 10);
        // 如果是个指针 就要load 和 store
        // cerr << "top: " << top << endl;
        if(ret.value->kind.tag == KOOPA_RVT_GET_PTR || ret.value->kind.tag == KOOPA_RVT_GET_ELEM_PTR) {
          if(topOffset > 2047){ // 要存到寄存器里
            cout << " li a0, "  << topOffset << endl;
            cout << " add a0, sp, a0" << endl;
            cout << " lw a0, 0(a0)" << endl;
          }
          else {
            cout << " lw a0, " << top << endl;
          }
          cout << " lw a0, 0(a0)" << endl;
        }
        else {
          if(topOffset > 2047){ // 要存到寄存器里
            cout << " li a0, " << topOffset << endl;
            cout << " add a0, sp, a0" << endl;
            cout << " lw a0, 0(a0)"  << endl;
          }
          else
            cout << " lw a0, " << top << endl;
        }
      }
      else{
        cout << " mv a0, " << top << endl;
        if(find(regList.begin(), regList.end(), top) == regList.end()) {
          if(top != "x0")
            regList.emplace_back(top);
        }
      }
    }
    auto it = find(regList.begin(), regList.end(), "a0");
    if(it != regList.end())
      regList.erase(it);
  }
  // 函数的 epilogue
  // 是否需要读出ra
  if(ifCall) {
    if(offset - 4 > 2047) {
      string reg = regList[0];
      cout << " li " << reg << ", " << (offset - 4) << endl;
      cout << " add " << reg << ", sp, " << reg << endl;
      cout << " lw ra, 0(" << reg << ')' << endl;
    }
    else
      cout << " lw ra, " << (offset - 4) << "(sp)" << endl;
  }
  if(offset > 2047){ // 要存到寄存器里
    string ret = regList[0];
    cout << " li " << ret << ", " << offset << endl;
    cout << " add sp, sp, " << ret << endl;
  }
  else if(offset != 0){
    cout << " addi sp, sp, " << offset << endl;
  }
  cout << " ret" << endl;
}

// 访问integer 指令
string Visit(const koopa_raw_integer_t &integer){
  int32_t int_val = integer.value;
  if(retInt) // 直接返回数字
    return to_string(int_val);

  if(int_val == 0)
    return "x0";
  else{
    auto it = find(regList.begin(), regList.end(), "a0");
    string ret;
    if(it != regList.end()){
      ret = "a0";
      regList.erase(it);
    }
    else{
      ret = regList[0];
      regList.erase(regList.begin());
    }
  
    cout << " li  " << ret << ", " << int_val << endl;
    return ret;
  }
}


void HandleKoopa(const char *input, const char *output){
  // 解析字符串 str, 得到 Koopa IR 程序
  ifstream ifs(input);
  
	string str( (istreambuf_iterator<char>(ifs) ),
					 (istreambuf_iterator<char>() ) );


	ifs.close();

  koopa_program_t program;
  koopa_error_code_t ret = koopa_parse_from_string(str.c_str(), &program);
  assert(ret == KOOPA_EC_SUCCESS);  // 确保解析时没有出错
  // 创建一个 raw program builder, 用来构建 raw program
  koopa_raw_program_builder_t builder = koopa_new_raw_program_builder();
  // 将 Koopa IR 程序转换为 raw program
  koopa_raw_program_t raw = koopa_build_raw_program(builder, program);
  // 释放 Koopa IR 程序占用的内存
  koopa_delete_program(program);


  // 处理 raw program
  // 同时生成RISCV
  freopen(output, "w", stdout);
  Visit(raw);
  freopen("/dev/tty", "w", stdout); // 再次重定向回标准输出


  // 处理完成, 释放 raw program builder 占用的内存
  // 注意, raw program 中所有的指针指向的内存均为 raw program builder 的内存
  // 所以不要在 raw program 处理完毕之前释放 builder
  koopa_delete_raw_program_builder(builder);

}





int main(int argc, const char *argv[]) {

  // 初始化全局符号表
  identTalbesHead = new IdentTables;
  identTalbesHead->layer = 0;
  identTalbesHead->cnt = 0;
  identTalbesHead->upper = NULL;
  identTalbesHead->lower = NULL;
  curIdentTables = identTalbesHead;
  layerCnt[0] = 0;
  // 在全局符号表中添加SysY库函数
  IdentInfo tmp;
  // getint 函数
  tmp.type = 2;
  tmp.retType = 1;
  (identTalbesHead->identTable)["getint"] = tmp;
  // getch 函数
  (identTalbesHead->identTable)["getch"] = tmp;
  // getarray 函数
  tmp.paramsName.emplace_back("int[]");
  tmp.paramsType.emplace_back("int[]");
  (identTalbesHead->identTable)["getarray"] = tmp;
  tmp.paramsName.clear();
  tmp.paramsType.clear();
  // putint 函数
  tmp.retType = 0;
  tmp.paramsName.emplace_back("int");
  tmp.paramsType.emplace_back("int");
  (identTalbesHead->identTable)["putint"] = tmp;
  // putch 函数
  (identTalbesHead->identTable)["putch"] = tmp;
  tmp.paramsName.clear();
  tmp.paramsType.clear();
  // putarray 函数
  tmp.paramsName.emplace_back("int");
  tmp.paramsType.emplace_back("int");
  tmp.paramsName.emplace_back("int[]");
  tmp.paramsType.emplace_back("int[]");
  (identTalbesHead->identTable)["putarray"] = tmp;
  tmp.paramsName.clear();
  tmp.paramsType.clear();
  // starttime 函数
  (identTalbesHead->identTable)["starttime"] = tmp;
  // stoptime 函数
  (identTalbesHead->identTable)["stoptime"] = tmp;


  // 解析命令行参数. 测试脚本/评测平台要求你的编译器能接收如下参数:
  // compiler 模式 输入文件 -o 输出文件
  assert(argc == 5);
  auto mode = argv[1];
  auto input = argv[2];
  auto output = argv[4];

  // 打开输入文件, 并且指定 lexer 在解析的时候读取这个文件
  yyin = fopen(input, "r");
  assert(yyin);

  // 调用 parser 函数, parser 函数会进一步调用 lexer 解析输入文件的
  unique_ptr<BaseAST> ast;
  auto ret = yyparse(ast);
  assert(!ret);

  // 用strcmp而不是 == 判等
  if(strcmp(mode, "-koopa") == 0){ // 生成koopa文件 
    // 输出解析得到的 AST, 其实就是个字符串
    freopen(output, "w", stdout);
    ast->Dump();
    freopen("/dev/tty", "w", stdout); // 再次重定向回标准输出
  }

  else if(strcmp(mode, "-riscv") == 0){ // 生成riscv文件
    // 输出解析得到的 AST, 其实就是个字符串
    const char* tmpOutput = "tmp.koopa";
    freopen(tmpOutput, "w", stdout);
    ast->Dump();
    freopen("/dev/tty", "w", stdout); // 再次重定向回标准输出
    HandleKoopa(tmpOutput, output);
  }
  return 0;
}