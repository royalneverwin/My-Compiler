#pragma once

#ifndef COMPILER_FUNCTION_H
#define COMPILER_FUNCTION_H


#include "koopa.h"
#include <string>
#include <cstdio>
#include <fstream>
#include <cstring>

using namespace std;
int GetSize(koopa_raw_type_t ty);

void Visit(const koopa_raw_program_t &program);
void Visit(const koopa_raw_slice_t &slice);
void Visit(const koopa_raw_function_t &func);
void Visit(const koopa_raw_basic_block_t &bb);


string Visit(const koopa_raw_value_t &value);
void Visit(const koopa_raw_return_t &ret);
string Visit(const koopa_raw_integer_t &integer);
string Visit(const koopa_raw_binary_t &binary);
string HandleKoopa(string input, string output);
string Visit(const koopa_raw_global_alloc_t &alloc, bool flag, string name, int size); // false局部变量 true全局变量
void Visit(const koopa_raw_store_t &store);
string Visit(const koopa_raw_load_t &load);
void Visit(const koopa_raw_branch_t &branch);
void Visit(const koopa_raw_jump_t& jump);
string Visit(const koopa_raw_call_t& call);
string Visit(const koopa_raw_func_arg_ref_t& func_arg_ref);
string Visit(const koopa_raw_get_elem_ptr_t& get_elem_ptr);
void Visit(const koopa_raw_aggregate_t& aggregate, vector<string>& vals);
string Visit(const koopa_raw_get_ptr_t& get_ptr);

#endif

