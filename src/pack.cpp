#include "cake.h"


// pack the entire matrix A into a single cache-aligned buffer
double pack_A_single_buf(float* A, float* A_p, int M, int K, int p, cake_cntx_t* cake_cntx, blk_dims_t* blk_dims) {
	
	struct timespec start, end;
	double diff_t;
	clock_gettime(CLOCK_REALTIME, &start);

	int m_c = blk_dims->m_c;
	int k_c = blk_dims->k_c;
	int m_r = cake_cntx->mr;


   int k_pad = (K % k_c) ? 1 : 0; 
   int m_pad = (M % (p*m_c)) ? 1 : 0; 
   int Mb = (M / (p*m_c)) + m_pad;
   int Kb = (K / k_c) + k_pad;

   int mr_rem = (int) ceil( ((double) (M % (p*m_c))) / m_r) ;
   int mr_per_core = (int) ceil( ((double) mr_rem) / p );
   int p_l;

   if(mr_per_core) 
      p_l = (int) ceil( ((double) mr_rem) / mr_per_core);
   else
      p_l = 0;

   int m_c1 = mr_per_core * m_r;
   int m_c1_last_core = (mr_per_core - (p_l*mr_per_core - mr_rem)) * m_r;
   int k_c1 = K % k_c;

   int m, k, A_offset = 0, A_p_offset = 0;
   int m_cb, k_c_t, p_used, core;


   for(m = 0; m < Mb; m++) {

      if((m == Mb - 1) && m_pad) {
         p_used = p_l;
         m_cb = m_r*mr_rem ; 
      } else {
         p_used = p;
         m_cb = p_used*m_c;
      }

      for(k = 0; k < Kb; k++) {
         
         k_c_t = k_c; 
         if((k == Kb - 1) && k_pad) {
            k_c_t = k_c1;
         }

         A_offset = m*p*m_c*K + k*k_c;

         #pragma omp parallel for private(core)
         for(core = 0; core < p_used; core++) {

            int m_c_t, m_c_x;
            bool pad;

            if((m == Mb - 1) && m_pad) {
               m_c_t = (core == (p_l - 1) ? m_c1_last_core : m_c1);
               m_c_x = m_c1;
               pad = (core == (p_l - 1) ? 1 : 0);
            } else {
               m_c_t = m_c;
               m_c_x = m_c;
               pad = 0;
            }

            pack_ob_A_single_buf(&A[A_offset + core*m_c_x*K], &A_p[A_p_offset + core*m_c_x*k_c_t], 
               M, K, m*p*m_c, core*m_c_x, m_c_t, k_c_t, m_r, pad);
         }

         A_p_offset += m_cb*k_c_t;
      }
   }

     clock_gettime(CLOCK_REALTIME, &end);
     long seconds = end.tv_sec - start.tv_sec;
     long nanoseconds = end.tv_nsec - start.tv_nsec;
     diff_t = seconds + nanoseconds*1e-9;

     return diff_t;
}



// pack each operation block (OB) of matrix A into its own cache-aligned buffer
double pack_A_multiple_buf(float* A, float** A_p, int M, int K, int m_c, int k_c, int m_r, int p) {
	struct timespec start, end;
	double diff_t;
	clock_gettime(CLOCK_REALTIME, &start);

	int ind1 = 0;
	int m1, k1, m2, p_l;
	int k_c1 = (K % k_c);
	int mr_rem = (int) ceil( ((double) (M % (p*m_c))) / m_r) ;
	int mr_per_core = (int) ceil( ((double) mr_rem) / p );
	int m_c1 = mr_per_core * m_r;

	if(mr_per_core) 
		p_l = (int) ceil( ((double) mr_rem) / mr_per_core);
	else
		p_l = 0;

	int m_c1_last_core = (mr_per_core - (p_l*mr_per_core - mr_rem)) * m_r;

	// main portion of A that evenly fits into CBS blocks each with p m_cxk_c OBs
	for(m1 = 0; m1 < (M - (M % (p*m_c))); m1 += p*m_c) {
		for(k1 = 0; k1 < (K - (K%k_c)); k1 += k_c) {

			#pragma omp parallel for private(m2)
			for(m2 = 0; m2 < p*m_c; m2 += m_c) {
				// printf("hey %d\n", ind1 + m2/m_c);
				if(posix_memalign((void**) &A_p[ind1 + m2/m_c], 64, m_c * k_c * sizeof(float))) {
					printf("posix memalign error\n");
					exit(1);
				}

				pack_ob_A_multiple_buf(A, A_p[ind1 + m2/m_c], M, K, m1, k1, m2, m_c, k_c, m_r, 0);

				if(ARR_PRINT) print_array(A_p[ind1 + m2/m_c], k_c * m_c);
			}

			ind1 += p;
		}
		// right-most column of CBS blocks each with p m_c x k_c1 OBs
		if(k_c1) {
			k1 = K - (K%k_c);
			#pragma omp parallel for private(m2)
			for(m2 = 0; m2 < p*m_c; m2 += m_c) {

				if(posix_memalign((void**) &A_p[ind1 + m2/m_c], 64, k_c1 * m_c * sizeof(float))) {
					printf("posix memalign error\n");
					exit(1);
				}

				pack_ob_A_multiple_buf(A, A_p[ind1 + m2/m_c], M, K, m1, k1, m2, m_c, k_c1, m_r, 0);
				if(ARR_PRINT) print_array(A_p[ind1 + m2/m_c], k_c1 * m_c);
			}
			ind1 += p;
		}
	}

	// Process bottom-most rows of CBS blocks and perform M-dim padding
	if(M % (p*m_c)) {	

		m1 = (M - (M % (p*m_c)));

		for(k1 = 0; k1 < (K - (K%k_c)); k1 += k_c) {
			#pragma omp parallel for private(m2)
			for(m2 = 0; m2 < (p_l-1)*m_c1; m2 += m_c1) {
				
				if(posix_memalign((void**) &A_p[ind1 + m2/m_c1], 64, k_c * m_c1 * sizeof(float))) {
					printf("posix memalign error\n");
					exit(1);
				}

				pack_ob_A_multiple_buf(A, A_p[ind1 + m2/m_c1], M, K, m1, k1, m2, m_c1, k_c, m_r, 0);
				if(ARR_PRINT) print_array(A_p[ind1 + m2/m_c1], k_c * m_c1);
				// ind1++;
			}
			ind1 += (p_l-1);

			// final row of CBS blocks each with m_c1_last_core x k_c
			m2 = (p_l-1) * m_c1;
			if(posix_memalign((void**) &A_p[ind1], 64, k_c * m_c1_last_core * sizeof(float))) {
				printf("posix memalign error\n");
				exit(1);
			}

			pack_ob_A_multiple_buf(A, A_p[ind1], M, K, m1, k1, m2, m_c1_last_core, k_c, m_r, 1);
			if(ARR_PRINT) print_array(A_p[ind1], k_c * m_c1_last_core);
			ind1++;
		}

		// Final CBS block (with p_l-1 m_c1 x k_c1 OBs and 1 m_c1_last_core x k_c1 OB) 
		// present in the lower right hand corner of A 
		if(k_c1) {

			k1 = K - (K%k_c);
			#pragma omp parallel for private(m2)
			for(m2 = 0; m2 < (p_l-1)*m_c1; m2 += m_c1) {

				if(posix_memalign((void**) &A_p[ind1 + m2/m_c1], 64, k_c1 * m_c1 * sizeof(float))) {
					printf("posix memalign error\n");
					exit(1);
				}

				pack_ob_A_multiple_buf(A, A_p[ind1 + m2/m_c1], M, K, m1, k1, m2, m_c1, k_c1, m_r, 0);
				if(ARR_PRINT) print_array(A_p[ind1 + m2/m_c1], k_c1 * m_c1);
				// ind1++;
			}
			ind1 += (p_l-1);

			// last OB of A has shape m_c1_last_core x k_c1 
			m2 = (p_l-1) * m_c1;
			
			if(posix_memalign((void**) &A_p[ind1], 64, k_c1 * m_c1_last_core * sizeof(float))) {
				printf("posix memalign error\n");
				exit(1);
			}

			pack_ob_A_multiple_buf(A, A_p[ind1], M, K, m1, k1, m2, m_c1_last_core, k_c1, m_r, 1);
			if(ARR_PRINT) print_array(A_p[ind1], k_c1 * m_c1_last_core);
			ind1++;
		}
	}

	clock_gettime(CLOCK_REALTIME, &end);
	long seconds = end.tv_sec - start.tv_sec;
	long nanoseconds = end.tv_nsec - start.tv_nsec;
	diff_t = seconds + nanoseconds*1e-9;

     return diff_t;

}



void pack_B(float* B, float* B_p, int K, int N, cake_cntx_t* cake_cntx, blk_dims_t* blk_dims) {

	int k1, k_c1, n1, n2, n_c1, nr_rem;
	int ind1 = 0;

	int local_ind;
	int k_c = blk_dims->k_c;
	int n_c = blk_dims->n_c;
	int n_r = cake_cntx->nr;

	// main portion of B that evenly fits into CBS blocks of size k_c x n_c 
	for(n1 = 0; n1 < (N - (N%n_c)); n1 += n_c) {
		
		#pragma omp parallel for private(k1,local_ind)
		for(k1 = 0; k1 < (K - (K%k_c)); k1 += k_c) {
			local_ind = 0;
			for(int n2 = 0; n2 < n_c; n2 += n_r) {
				for(int i = 0; i < k_c; i++) {
					for(int j = 0; j < n_r; j++) {
						B_p[ind1 + local_ind + (k1/k_c)*k_c*n_c] = B[n1 + k1*N + n2 + i*N + j];
						local_ind++;
					}
				}
			}
		}
		ind1 += ((K - (K%k_c))*n_c);

		k1 = (K - (K%k_c));
		k_c1 = (K % k_c);
		if(k_c1) {

			#pragma omp parallel for private(n2,local_ind)
			for(n2 = 0; n2 < n_c; n2 += n_r) {
				local_ind = 0;
				for(int i = 0; i < k_c1; i++) {
					for(int j = 0; j < n_r; j++) {
						B_p[ind1 + local_ind + n2*k_c1] = B[n1 + k1*N + n2 + i*N + j];
						local_ind++;
					}
				}
			}
			ind1 += k_c1*n_c;
		}
	}

	// Process the final column of CBS blocks (sized k_c x n_c1) and perform N-dim padding 
	n1 = (N - (N%n_c));
	nr_rem = (int) ceil( ((double) (N % n_c) / n_r)) ;
	n_c1 = nr_rem * n_r;

	if(n_c1) {	

		#pragma omp parallel for private(k1,local_ind)
		for(k1 = 0; k1 < (K - (K%k_c)); k1 += k_c) {
			local_ind = 0;
			for(int n2 = 0; n2 < n_c1; n2 += n_r) {
				for(int i = 0; i < k_c; i++) {
					for(int j = 0; j < n_r; j++) {

						if((n1 + n2 + j) >=  N) {
							B_p[ind1 + local_ind + (k1/k_c)*k_c*n_c1] = 0.0;
						} else {
							B_p[ind1 + local_ind + (k1/k_c)*k_c*n_c1] = B[n1 + k1*N + n2 + i*N + j];
						}

						local_ind++;
					}
				}
			}
		}
		ind1 += ((K - (K%k_c))*n_c1);

		// Final CBS block (with k_c1 x n_c1 blocks) present in the lower right hand corner of B 
		k1 = (K - (K%k_c));
		k_c1 = (K % k_c);
		if(k_c1) {

			#pragma omp parallel for private(n2,local_ind)
			for(int n2 = 0; n2 < n_c1; n2 += n_r) {
				local_ind = 0;
				for(int i = 0; i < k_c1; i++) {
					for(int j = 0; j < n_r; j++) {

						if((n1 + n2 + j) >=  N) {
							B_p[ind1 + local_ind + n2*k_c1] = 0.0;
						} else {
							B_p[ind1 + local_ind + n2*k_c1] = B[n1 + k1*N + n2 + i*N + j];
						}

						local_ind++;
					}
				}
			}
			ind1 += k_c1*n_c1;
		}
	}
}






void pack_C_single_buf(float* C, float* C_p, int M, int N, int p, cake_cntx_t* cake_cntx, blk_dims_t* blk_dims) {

   struct timespec start, end;
   double diff_t;
   clock_gettime(CLOCK_REALTIME, &start);

   int m_c = blk_dims->m_c;
   int n_c = blk_dims->n_c;
   int m_r = cake_cntx->mr;
   int n_r = cake_cntx->nr;

   int m_pad = (M % (p*m_c)) ? 1 : 0; 
   int n_pad = (N % n_c) ? 1 : 0;
   int Mb = (M / (p*m_c)) + m_pad;
   int Nb = (N / n_c) + n_pad;

   int mr_rem = (int) ceil( ((double) (M % (p*m_c))) / m_r) ;
   int mr_per_core = (int) ceil( ((double) mr_rem) / p );
   int p_l;

   if(mr_per_core) 
      p_l = (int) ceil( ((double) mr_rem) / mr_per_core);
   else
      p_l = 0;

   int m_c1 = mr_per_core * m_r;
   int m_c1_last_core = (mr_per_core - (p_l*mr_per_core - mr_rem)) * m_r;

   int nr_rem = (int) ceil( ((double) (N % n_c) / n_r)) ;
   int n_c1 = nr_rem * n_r;

   int m, n, C_offset = 0, C_p_offset = 0;
   int m_cb, n_c_t, p_used, core;

   int m1, n1;
   bool pad_n;

   for(n = 0; n < Nb; n++) {

      if((n == Nb - 1) && n_pad) {
         n_c_t = n_c1;
         n1 = (N - (N % n_c));
         pad_n = 1;
      } else {
         n_c_t = n_c;
         n1 = n*n_c;
         pad_n = 0;
      }

      for(m = 0; m < Mb; m++) {

         if((m == Mb - 1) && m_pad) {
            p_used = p_l;
            m_cb = m_r*mr_rem ; 
            m1 = (M - (M % (p*m_c)));
         } else {
            p_used = p;
            m_cb = p_used*m_c;
            m1 = m*p*m_c;
         }

         C_offset = m*p*m_c*N + n*n_c;

         #pragma omp parallel for private(core)
         for(core = 0; core < p_used; core++) {

            int m_c_t, m_c_x;
            bool pad_m;

            if((m == Mb - 1) && m_pad) {
               m_c_t = (core == (p_l - 1) ? m_c1_last_core : m_c1);
               m_c_x = m_c1;
               pad_m = (core == (p_l - 1) ? 1 : 0);
            } else {
               m_c_t = m_c;
               m_c_x = m_c;
               pad_m = 0;
            }

            pack_ob_C_single_buf(&C[C_offset + core*m_c_x*N], &C_p[C_p_offset + core*m_c_x*n_c_t], 
               M, N, m1, n1, core*m_c_x, m_c_t, n_c_t, m_r, n_r, pad_m, pad_n);
         }

         C_p_offset += m_cb*n_c_t;
      }
   }
}




void pack_ob_C_single_buf(float* C, float* C_p, int M, int N, int m1, int n1, int m2,
            int m_c, int n_c, int m_r, int n_r, bool pad_m, bool pad_n) {

   int ind_ob = 0;

	if(pad_m || pad_n) {

      for(int n2 = 0; n2 < n_c; n2 += n_r) {
         for(int m3 = 0; m3 < m_c; m3 += m_r) {
            for(int i = 0; i < m_r; i++) {
               for(int j = 0; j < n_r; j++) {
                  if((n1 + n2 + j) >= N  ||  (m1 + m2 + m3 + i) >=  M) {
                     C_p[ind_ob] = 0.0; // padding
                  } else {
                     C_p[ind_ob] = C[n2 + m3*N + i*N + j];
                  }
                  ind_ob++;
               }
            }
         }
      }

   } else {

      for(int n2 = 0; n2 < n_c; n2 += n_r) {
         for(int m3 = 0; m3 < m_c; m3 += m_r) {
            for(int i = 0; i < m_r; i++) {
               for(int j = 0; j < n_r; j++) {
                  C_p[ind_ob] = C[n2 + m3*N + i*N + j];
                  ind_ob++;
               }
            }
         }
      }
   }
}



void pack_C_multiple_buf(float* C, float** C_p, int M, int N, int m_c, int n_c, int m_r, int n_r, int p, int alpha_n) {

	int n1, m1, m2, p_l;

	int mr_rem = (int) ceil( ((double) (M % (p*m_c))) / m_r) ;
	int mr_per_core = (int) ceil( ((double) mr_rem) / p );
	int m_c1 = mr_per_core * m_r;

	if(mr_per_core) 
		p_l = (int) ceil( ((double) mr_rem) / mr_per_core);
	else
		p_l = 0;


	int m_c1_last_core = (mr_per_core - (p_l*mr_per_core - mr_rem)) * m_r;

	int nr_rem = (int) ceil( ((double) (N % n_c) / n_r)) ;
	int n_c1 = nr_rem * n_r;

	int ind1 = 0;

	// main portion of C that evenly fits into CBS blocks each with p m_cxn_c OBs
	for(n1 = 0; n1 < (N - (N%n_c)); n1 += n_c) {
		for(m1 = 0; m1 < (M - (M % (p*m_c))); m1 += p*m_c) {
			#pragma omp parallel for private(m2)
			for(m2 = 0; m2 < p*m_c; m2 += m_c) {

				if(posix_memalign((void**) &C_p[ind1 + m2/m_c], 64, m_c * n_c * sizeof(float))) {
					printf("posix memalign error\n");
					exit(1);
				}

				pack_ob_C_multiple_buf(C, C_p[ind1 + m2/m_c], M, N, m1, n1, m2, m_c, n_c, m_r, n_r, 0);
				if(ARR_PRINT) print_array(C_p[ind1 + m2/m_c], m_c * n_c);
				// ind1++;
			}
			ind1 += p;
		}

		// bottom row of CBS blocks with p_l-1 OBs of m_c1 x n_c and 1 OBs of shape m_c1_last_core x n_c
		if(M % (p*m_c)) {	

			m1 = (M - (M % (p*m_c)));
			#pragma omp parallel for private(m2)
			for(m2 = 0; m2 < (p_l-1)*m_c1; m2 += m_c1) {

				if(posix_memalign((void**) &C_p[ind1 + m2/m_c1], 64, m_c1 * n_c * sizeof(float))) {
					printf("posix memalign error\n");
					exit(1);
				}

				pack_ob_C_multiple_buf(C, C_p[ind1 + m2/m_c1], M, N, m1, n1, m2, m_c1, n_c, m_r, n_r, 0);
				if(ARR_PRINT) print_array(C_p[ind1 + m2/m_c1], m_c1 * n_c);
				// ind1++;
			}
			ind1 += (p_l-1);

			m2 = (p_l-1) * m_c1;
			if(posix_memalign((void**) &C_p[ind1], 64, m_c1_last_core * n_c * sizeof(float))) {
				printf("posix memalign error\n");
				exit(1);
			}

			pack_ob_C_multiple_buf(C, C_p[ind1], M, N, m1, n1, m2, m_c1_last_core, n_c, m_r, n_r, 1);
			if(ARR_PRINT) print_array(C_p[ind1], m_c1 * n_c);
			ind1++;
		}
	}

	// right-most column of CBS blocks with p OBs of shape m_c x n_c1
	n1 = (N - (N%n_c));

	if(n_c1) {	

		for(m1 = 0; m1 < (M - (M % (p*m_c))); m1 += p*m_c) {
			#pragma omp parallel for private(m2)
			for(m2 = 0; m2 < p*m_c; m2 += m_c) {

				if(posix_memalign((void**) &C_p[ind1 + m2/m_c], 64, m_c * n_c1 * sizeof(float))) {
					printf("posix memalign error\n");
					exit(1);
				}

				pack_ob_C_multiple_buf(C, C_p[ind1 + m2/m_c], M, N, m1, n1, m2, m_c, n_c1, m_r, n_r, 1);
				if(ARR_PRINT) print_array(C_p[ind1 + m2/m_c], m_c * n_c1);
				// ind1++;
			}
			ind1 += p;
		}

		if(M % (p*m_c)) {	

			m1 = (M - (M % (p*m_c)));

			// last row of CBS blocks with p_l-1 m_c1 x n_c1 OBs and 1 m_c1_last_core x n_c1 OB
			#pragma omp parallel for private(m2)
			for(m2 = 0; m2 < (p_l-1)*m_c1; m2 += m_c1) {

				if(posix_memalign((void**) &C_p[ind1 + m2/m_c1], 64, m_c1 * n_c1 * sizeof(float))) {
					printf("posix memalign error\n");
					exit(1);
				}

				pack_ob_C_multiple_buf(C, C_p[ind1 + m2/m_c1], M, N, m1, n1, m2, m_c1, n_c1, m_r, n_r, 1);
				if(ARR_PRINT) print_array(C_p[ind1 + m2/m_c1], m_c1 * n_c1);
				// ind1++;
			}
			ind1 += (p_l-1);

			// last OB in C (lower right corner) with shape m_c1_last_core * n_c1
			m2 = (p_l-1) * m_c1;
			if(posix_memalign((void**) &C_p[ind1], 64, m_c1_last_core * n_c1 * sizeof(float))) {
				printf("posix memalign error\n");
				exit(1);
			}

			pack_ob_C_multiple_buf(C, C_p[ind1], M, N, m1, n1, m2, m_c1_last_core, n_c1, m_r, n_r, 1);
			if(ARR_PRINT) print_array(C_p[ind1], m_c1_last_core * n_c1);
			ind1++;
		}
	}
}



// // initialize an operation block of matrix A
// initialize an operation block of matrix A
void pack_ob_A_multiple_buf(float* A, float* A_p, int M, int K, int m1, int k1, int m2, int m_c, int k_c, int m_r, bool pad) {

	int	ind_ob = 0;
	
	if(pad) {
		for(int m3 = 0; m3 < m_c; m3 += m_r) {
			for(int i = 0; i < k_c; i++) {
				for(int j = 0; j < m_r; j++) {

					if((m1 + m2 + m3 + j) >=  M) {
						A_p[ind_ob] = 0.0;
					} else {
						A_p[ind_ob] = A[m1*K + k1 + m2*K + m3*K + i + j*K];
					}

					ind_ob++;
				}
			}
		}		
	} 

	else {
		for(int m3 = 0; m3 < m_c; m3 += m_r) {
			for(int i = 0; i < k_c; i++) {
				for(int j = 0; j < m_r; j++) {
					A_p[ind_ob] = A[m1*K + k1 + m2*K + m3*K + i + j*K];
					ind_ob++;
				}
			}
		}
	}
}


void pack_ob_A_single_buf(float* A, float* A_p, int M, int K, int m1, int m2, int m_c, int k_c, int m_r, bool pad) {

   int ind_ob = 0;

   if(pad) {
      for(int m3 = 0; m3 < m_c; m3 += m_r) {
         for(int i = 0; i < k_c; i++) {
            for(int j = 0; j < m_r; j++) {

               if((m1 + m2 + m3 + j) >=  M) {
                  A_p[ind_ob] = 0.0;
               } else {
                  A_p[ind_ob] = A[m3*K + i + j*K];
               }

               ind_ob++;
            }
         }
      }     
   } 

   else {
      for(int m3 = 0; m3 < m_c; m3 += m_r) {
         for(int i = 0; i < k_c; i++) {
            for(int j = 0; j < m_r; j++) {
               A_p[ind_ob] = A[m3*K + i + j*K];
               ind_ob++;
            }
         }
      }     
   }
}



void pack_ob_C_multiple_buf(float* C, float* C_p, int M, int N, int m1, int n1, int m2,
				int m_c, int n_c, int m_r, int n_r, bool pad) {

	int	ind_ob = 0;

	if(pad) {

		for(int n2 = 0; n2 < n_c; n2 += n_r) {
			for(int m3 = 0; m3 < m_c; m3 += m_r) {
				for(int i = 0; i < m_r; i++) {
					for(int j = 0; j < n_r; j++) {
						if((n1 + n2 + j) >= N  ||  (m1 + m2 + m3 + i) >=  M) {
							C_p[ind_ob] = 0.0; // padding
						} else {
							C_p[ind_ob] = C[n1 + m1*N + m2*N + n2 + m3*N + i*N + j];
						}
						ind_ob++;
					}
				}
			}
		}

	} else {

		for(int n2 = 0; n2 < n_c; n2 += n_r) {
			for(int m3 = 0; m3 < m_c; m3 += m_r) {
				for(int i = 0; i < m_r; i++) {
					for(int j = 0; j < n_r; j++) {
						C_p[ind_ob] = C[n1 + m1*N + m2*N + n2 + m3*N + i*N + j];
						ind_ob++;
					}
				}
			}
		}
	}
}



int cake_sgemm_packed_A_size(int M, int K, int p, cake_cntx_t* cake_cntx, blk_dims_t* blk_dims) {

	int mr_rem = (int) ceil( ((double) (M % (p*blk_dims->m_c))) / cake_cntx->mr) ;
	int M_padded = (cake_cntx->mr*mr_rem + (M /(p*blk_dims->m_c))*p*blk_dims->m_c);

	return (M_padded * K) * sizeof(float);
}



int cake_sgemm_packed_B_size(int K, int N, int p, cake_cntx_t* cake_cntx, blk_dims_t* blk_dims) {
	
	int nr_rem = (int) ceil( ((double) (N % blk_dims->n_c) / cake_cntx->nr)) ;
	int n_c1 = nr_rem * cake_cntx->nr;
	int N_padded = (N - (N%blk_dims->n_c)) + n_c1;

	return (K * N_padded) * sizeof(float);
}



int cake_sgemm_packed_C_size(int M, int N, int p, cake_cntx_t* cake_cntx, blk_dims_t* blk_dims) {

	int mr_rem = (int) ceil( ((double) (M % (p*blk_dims->m_c))) / cake_cntx->mr) ;
	int M_padded = (cake_cntx->mr*mr_rem + (M /(p*blk_dims->m_c))*p*blk_dims->m_c);

	int nr_rem = (int) ceil( ((double) (N % blk_dims->n_c) / cake_cntx->nr)) ;
	int n_c1 = nr_rem * cake_cntx->nr;
	int N_padded = (N - (N%blk_dims->n_c)) + n_c1;

	return (M_padded * N_padded) * sizeof(float);
}


