#include "common.h"


// Util functions
int run_tests();

bool cake_sgemm_checker(float* A, float* B, float* C, int N, int M, int K);

bool add_checker(float** C_arr, float* C, int M, int N, int p);

void rand_init(float* mat, int r, int c);

void print_array(float* arr, int len);

void print_mat(float* arr, int r, int c);

void print_schedule(enum sched sch);



// sparse utils
int run_tests_sparse();

void rand_sparse(float* mat, int r, int c, float sparsity);

float rand_gen();

float normalRandom();

void rand_sparse_gaussian(float* mat, int r, int c, float mu, float sigma);



int mat_to_csr_file(float* A, int M, int K, char* fname);
void mat_equals(float* C, float* C_check, int M, int N);
void test_csr_convert(int M, int K, float sparsity);
csr_t* file_to_csr(char* fname);
void csr_to_mat(float* A, int M, int K, int* rowptr, float* vals, int* colind);
int run_tests_sparse_test();
void free_csr(csr_t* x);
void free_sp_pack(sp_pack_t* x);