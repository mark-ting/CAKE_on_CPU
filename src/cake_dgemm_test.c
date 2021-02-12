#include <stdio.h>
#include <sys/time.h> 
#include <time.h> 
#include <omp.h>
#include "blis.h"
 
void pack_B(double* B, double* B_p, int K, int N, int k_c, int n_c, int n_r);
void pack_A(double* A, double** A_p, int M, int K, int m_c, int k_c, int m_r, int p);
void pack_C(double** C_p, int M, int N, int m_c, int n_c, int p);
void unpack_C(double* C, double** C_p, int M, int N, int m_c, int n_c, int n_r, int m_r, int p);
void rand_init(double* mat, int r, int c);
void cake_dgemm_checker(double* A, double* B, double* C, int N, int M, int K);
void cake_dgemm(double* A, double* B, double* C, int M, int N, int K, int p);

int get_block_dim(int m_r, int n_r, double alpha_n);
int get_cache_size(char* level);
int lcm(int n1, int n2);



int main( int argc, char** argv ) {

    if(argc < 2) {
        printf("Enter number of threads\n");
        exit(1);
    }

	int M, K, N, p;

	// M = 96;
	// K = 183;
	// N = 96;
	M = 5760;
	K = 1234;
	N = 5760;

	// m_c = 96;
	// k_c = 96;

	// M = 23040;
	// K = 23040;
	// N = 23040;

	// M = 960;
	// K = 960;
	// N = 960;

    p = atoi(argv[1]);

	double* A = (double*) malloc(M * K * sizeof( double ));
	double* B = (double*) malloc(K * N * sizeof( double ));
	double* C = (double*) calloc(M * N , sizeof( double ));

	// initialize A and B
    srand(time(NULL));
	rand_init(A, M, K);
	rand_init(B, K, N);

	cake_dgemm(A, B, C, M, N, K, p);
	// bli_dprintm( "C: ", M, N, C, N, 1, "%4.4f", "" );
	cake_dgemm_checker(A, B, C, N, M, K);

	free(A);
	free(B);
	free(C);

	return 0;
}




void cake_dgemm(double* A, double* B, double* C, int M, int N, int K, int p) {
	// contiguous row-storage (i.e. cs_c = 1) or contiguous column-storage (i.e. rs_c = 1). 
	// This preference comes from how the microkernel is most efficiently able to load/store 
	// elements of C11 from/to memory. Most microkernels use vector instructions to access 
	// contiguous columns (or column segments) of C11
	int m_c, k_c, n_c, m_r, n_r;	
	double alpha, beta, alpha_n;
	struct timeval start, end;
	double diff_t;
	inc_t rsa, csa;
	inc_t rsb, csb;
	inc_t rsc, csc;

    // query block size for the microkernel
    cntx_t* cntx = bli_gks_query_cntx();
    m_r = (int) bli_cntx_get_blksz_def_dt(BLIS_DOUBLE, BLIS_MR, cntx);
    n_r = (int) bli_cntx_get_blksz_def_dt(BLIS_DOUBLE, BLIS_NR, cntx);
    printf("m_r = %d, n_r = %d\n\n", m_r, n_r);

    alpha_n = 1;
    m_c = get_block_dim(m_r, n_r, alpha_n);
    k_c = m_c;
    // m_c = 24;
    // k_c = 6;
    n_c = (int) (alpha_n * p * m_c);
	omp_set_num_threads(p);

    printf("mc = %d, kc = %d, nc = %d\n",m_c,k_c,n_c );

    // pack A
	int k_pad = k_c - (K % k_c); 
	double** A_p = (double**) malloc(p * ((K+k_pad)/k_c) * (M/(p*m_c)) * sizeof( double* ));
	pack_A(A, A_p, M, K, m_c, k_c, m_r, p);

	// for(int i = 0; i < (p * ((K+k_pad)/k_c) * (M/(p*m_c))); i++) {
	// 	for(int j = 0; j < m_c*k_c; j++) {
	// 		printf("%f ", A_p[i][j]);
	// 	}
	// 	printf("\n\n");
	// }

	// exit(1);

	// pack B
	double* B_p;
	int ret;
	ret = posix_memalign((void**) &B_p, 64, (K+k_pad) * N * sizeof(double));
	if(ret) {
		printf("posix memalign error\n");
		exit(1);
	}
	pack_B(B, B_p, K, N, k_c, n_c, n_r);

	// double* B_p = (double*) malloc( K * N * sizeof(double));

	// pack C
	double** C_p = (double**) malloc(p * (M/(p*m_c)) * (N/n_c) * sizeof( double* ));
	pack_C(C_p, M, N, m_c, n_c, p);

	// Set the scalars to use.
	alpha = 1.0;
	beta  = 1.0;
    rsc = 1; csc = m_r;
    rsa = 1; csa = m_r;
    rsb = n_r; csb = 1;

	// m = 6; n = 8; k = 8;
	//rsc = 1; csc = m;
	//rsa = 1; csa = m;
	//rsb = 1; csb = k;

    K += k_pad;

	gettimeofday (&start, NULL);
	int n_reg, m_reg, m, k1;
    // #pragma omp parallel for private(m)
	for(int n1 = 0; n1 < (N / n_c); n1++) {
		for(int m1 = 0; m1 < (M / (p*m_c)); m1++) {
			// pragma omp here (i_c loop)
			#pragma omp parallel for private(n_reg,m_reg,m,k1)
			for(m = 0; m < p; m++) {
				for(k1 = 0; k1 < (K / k_c); k1++) {
					// pragma omp also here possible (j_r loop)
					// #pragma omp parallel num_threads(p)
					for(n_reg = 0; n_reg < (n_c / n_r); n_reg++) {
						for(m_reg = 0; m_reg < (m_c / m_r); m_reg++) {
												
							// bli_dgemm(BLIS_NO_TRANSPOSE, BLIS_NO_TRANSPOSE,
					  //  		m_r, n_r, k_c, &alpha, 
					  //  		&A_p[m1*p*(K/k_c) + k1*p + m ][m_reg*m_r*k_c], 
					  //  		rsa, csa, 
					  //  		&B_p[n1*K*n_c + k1*k_c*n_c + n_reg*k_c*n_r], 
					  //  		rsb, csb, &beta, 
					  //  		&C_p[m + m1*p + n1*(M/m_c)][n_reg*m_c*n_r + m_reg*m_r*n_r], 
					  //  		rsc, csc);

							bli_dgemm_haswell_asm_6x8(k_c, &alpha, 
					   		&A_p[m1*p*(K/k_c) + k1*p + m ][m_reg*m_r*k_c], 
					   		&B_p[n1*K*n_c + k1*k_c*n_c + n_reg*k_c*n_r], &beta, 
					   		&C_p[m + m1*p + n1*(M/m_c)][n_reg*m_c*n_r + m_reg*m_r*n_r], 
					   		rsc, csc, NULL, NULL);
						}
					}
				}
			}
		}
	}
	
	gettimeofday (&end, NULL);
	diff_t = (((end.tv_sec - start.tv_sec)*1000000L
	+end.tv_usec) - start.tv_usec) / (1000000.0);
	printf("GEMM time: %f \n", diff_t); 

	// FILE * fp;
	// fp = fopen ("cake_time.txt", "a");
	// fprintf(fp, "%s,%s\n","num_cores", "runtime");
	// fprintf(fp, "%d,%f\n",p,diff_t);
	// fclose(fp);

	// ind1 = 0;
	// for(int n = 0; n < (N/n_c); n++) {
	// 	for(int m = 0; m < (M/(p*m_c)); m++) {
	// 		for(int p1 = 0; p1 < p; p1++) {
	// 			for(int i = 0; i < m_c*n_c; i++) {
	// 				printf("%f ",  C_p[ind1][i]);
	// 			}
	// 			ind1++; 
	// 		}
	// 		printf("\n");
	// 	}
	// }

	unpack_C(C, C_p, M, N, m_c, n_c, n_r, m_r, p); 

	for(int i = 0; i < (p * (K/k_c) * (M/(p*m_c))); i++) {
		free(A_p[i]);
	}

	for(int i = 0; i < (p * (M/(p*m_c)) * (N/n_c)); i++) {
		free(C_p[i]);
	}

	free(A_p);
	free(B_p);
	free(C_p);
}



void pack_A(double* A, double** A_p, int M, int K, int m_c, int k_c, int m_r, int p) {

	int ind1 = 0;
	int ind2 = 0;
	int ret;
	int k_pad = k_c - (K % k_c); 

	for(int m1 = 0; m1 < M; m1 += p*m_c) {
		for(int k1 = 0; k1 < (K + k_pad); k1 += k_c) {
			for(int m2 = 0; m2 < p*m_c; m2 += m_c) {

				ret = posix_memalign((void**) &A_p[ind1], 64, k_c * m_c * sizeof(double));
				if(ret) {
					printf("posix memalign error\n");
					exit(1);
				}

				// A_p[ind1] = (double*) malloc(k_c * m_c * sizeof(double));
				ind2 = 0;
				
				for(int m3 = 0; m3 < m_c; m3 += m_r) {
					for(int i = 0; i < k_c; i++) {
						if((i + k1) >=  K) {

							for(int j = 0; j < m_r; j++) {
								A_p[ind1][ind2] = 0;
								ind2++;
							}

						} else {

							for(int j = 0; j < m_r; j++) {
								A_p[ind1][ind2] = A[m1*K + k1 + m2*K + m3*K + i + j*K];
								ind2++;
							}
						}
					}
				}
				ind1++;
			}
		}
	}
}


void pack_B(double* B, double* B_p, int K, int N, int k_c, int n_c, int n_r) {

	int ind1 = 0;
	int k_pad = k_c - (K % k_c); 

	for(int n1 = 0; n1 < N; n1 += n_c) {
		for(int k1 = 0; k1 < (K + k_pad); k1 += k_c) {
			for(int n2 = 0; n2 < n_c; n2 += n_r) {
				for(int i = 0; i < k_c; i++) {

					if((i + k1) >=  K) {

						for(int j = 0; j < n_r; j++) {
							B_p[ind1] = 0;
							ind1++;
						}

					} else {

						for(int j = 0; j < n_r; j++) {
							B_p[ind1] = B[n1 + k1*N + n2 + i*N + j];
							ind1++;
						}
					}
				}
			}
		}
	}
}


void pack_C(double** C_p, int M, int N, int m_c, int n_c, int p) {

	int ind1 = 0;
	int ret ;
	for(int n = 0; n < (N/n_c); n++) {
		for(int m = 0; m < (M/(p*m_c)); m++) {
			for(int p1 = 0; p1 < p; p1++) {

				ret = posix_memalign((void**) &C_p[ind1], 64, m_c * n_c * sizeof(double));
				if(ret) {
					printf("posix memalign error\n");
					exit(1);
				}

				ind1++; 
			}
		}
	}
}


void unpack_C(double* C, double** C_p, int M, int N, int m_c, int n_c, int n_r, int m_r, int p) {

	int ind1 = 0;
	for(int n3 = 0; n3 < (N/n_c); n3++) {
		for(int n2 = 0; n2 < (n_c/n_r); n2++) {
			for(int n1 = 0; n1 < n_r; n1++) {
				for(int m1 = 0; m1 < (M/(p*m_c)); m1++) {
					for(int m = 0; m < p; m++) {
						for(int i = 0; i < (m_c/m_r); i++) {
							for(int j = 0; j< m_r; j++) {
								C[ind1] = C_p[n3*(M/m_c)  + m1*p + m][n2*m_c*n_r + n1*m_r + i*m_r*n_r + j];
								ind1++;
							}
						}
					}
				}
			}
		}
	}
}



void cake_dgemm_checker(double* A, double* B, double* C, int N, int M, int K) {

	double* C_check = calloc(M * N , sizeof( double ));
	#pragma omp parallel for
	for (int n = 0; n < N; n++) {
		for (int m = 0; m < M; m++) {
			C_check[m*N + n] = 0.0;
			for (int k = 0; k < K; k++) {
				C_check[m*N + n] += A[m*K + k] * B[k*N + n];
			}
		}
	}

    int CORRECT = 1;
    int cnt = 0;
    int ind1 = 0;
    double eps = 1e-11; // machine precision

    for (int n1 = 0; n1 < N; n1++) {
      for (int m1 = 0; m1 < M; m1++) {
        // if(C_check[m1*N + n1] != C[ind1]) {
        if(fabs(C_check[m1*N + n1] - C[ind1]) > eps) {
            cnt++;
            CORRECT = 0;
        }

        ind1++; 
      }
    }

    printf("\n\n");

	if(CORRECT) {
		printf("CORRECT!\n");
	} else {
		printf("WRONG!\n");
		printf("%d\n", cnt);
	}

	free(C_check);

	// for (int n1 = 0; n1 < N; n1++) {
 //      for (int m1 = 0; m1 < M; m1++) {
 //        printf("%f ", C_check[m1*N + n1]);
 //      }
 //    }
	// printf("\n\n\n\n");
}



// find cache size at levels L1d,L1i,L2,and L3 using lscpu
int get_cache_size(char* level) {

	int len, size = 0;
	FILE *fp;
	char ret[16];
	char command[128];

	sprintf(command, "lscpu --caches=NAME,ONE-SIZE \
					| grep %s \
					| grep -Eo '[0-9]*M|[0-9]*K|0-9*G' \
					| tr -d '\n'", level);
	fp = popen(command, "r");

	if (fp == NULL) {
		printf("Failed to run command\n" );
		exit(1);
	}

	if(fgets(ret, sizeof(ret), fp) == NULL) {
		printf("lscpu error\n");
	}

	pclose(fp);

	len = strlen(ret) - 1;

	// set cache size variables
	if(ret[len] == 'K') {
		ret[len] = '\0';
		size = atoi(ret) * (1 << 10);
	} else if(ret[len] == 'M') {
		ret[len] = '\0';
		size = atoi(ret) * (1 << 20);
	} else if(ret[len] == 'G') {
		ret[len] = '\0';
		size = atoi(ret) * (1 << 30);
	}

	return size;
}


int get_block_dim(int m_r, int n_r, double alpha_n) {

	int mc_L2 = 0, mc_L3 = 0;
	// find L3 and L2 cache sizes
	int max_threads = omp_get_max_threads() / 2; // 2-way hyperthreaded
	int L2_size = get_cache_size("L2");
	int L3_size = get_cache_size("L3");
	// solves for the optimal block size m_c and k_c based on the L3 size
	// L3_size >= (alpha_n*p*(1+p)) * x^2     (solve for x = m_c = k_c) 
	// We only use half of the each cache to prevent our working blocks from being evicted
	mc_L3 = (int) sqrt((((double) L3_size) / (sizeof(double) * 2))  
							/ (alpha_n*max_threads*(max_threads+1)));
	mc_L3 -= (mc_L3 % lcm(m_r, n_r));

	// solves for optimal mc,kc based on L2 size
	// L2_size >= x^2     (solve for x = m_c = k_c) 
	mc_L2 = (int) sqrt(((double) L2_size) / (sizeof(double) * 2));
	mc_L2 -= (mc_L2 % lcm(m_r, n_r));

	printf("%d %d %d\n", L2_size,L3_size,max_threads);
	// return min of possible L2 and L3 cache block sizes
	return (mc_L3 < mc_L2 ? mc_L3 : mc_L2);
}



// randomized double precision matrices in range [-1,1]
void rand_init(double* mat, int r, int c) {
	// int MAX = 65536;
	for(int i = 0; i < r*c; i++) {
		// A[i] = (double) i;
		// A[i] = 1.0;
		// A[i] =  (double) (i%MAX);
		mat[i] =  (double) rand() / RAND_MAX*2.0 - 1.0;
	}	
}


// least common multiple
int lcm(int n1, int n2) {
	int max = (n1 > n2) ? n1 : n2;
	while (1) {
		if (max % n1 == 0 && max % n2 == 0) {
			break;
		}
		++max;
	}
	return max;
}
