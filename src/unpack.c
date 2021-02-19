#include "cake.h"
  

void unpack_C_rsc(double* C, double** C_p, int M, int N, int m_c, int n_c, int n_r, int m_r, int p, int alpha_n) {

	int m, m1, n1, p_l;
	int ind1 = 0;

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


	for(m1 = 0; m1 < (M/(p*m_c)); m1++) {
		for(int m = 0; m < p; m++) {
			for(int m2 = 0; m2 < m_c/m_r; m2++) {
				for(int m3 = 0; m3 < m_r; m3++) { 
					for(n1 = 0; n1 < (N/n_c); n1++) {
						for(int i = 0; i < n_c/n_r; i++) {
							for(int j = 0; j < n_r; j++) {
								C[ind1] = C_p[m1*p + m + n1*((M/(p*m_c))*p  + p_l)][m2*m_r*n_r + m3*n_r + i*m_c*n_r + j];
								ind1++;
							}
						}
					}

					n1 = N/n_c;

					for(int i = 0; i < n_c1/n_r; i++) {
						for(int j = 0; j < n_r; j++) {
							if( (i*n_r + j + n1*n_c) < N) {
								C[ind1] = C_p[m1*p + m + n1*((M/(p*m_c))*p  + p_l)][m2*m_r*n_r + m3*n_r + i*m_c*n_r + j];
								ind1++;
							}
						}
					}
				}
			}
		}	
	}

	m1 = M / (p*m_c);

	for(m = 0; m < p_l-1; m++) {

		for(int m2 = 0; m2 < m_c1/m_r; m2++) {
			for(int m3 = 0; m3 < m_r; m3++) { 
				for(n1 = 0; n1 < (N/n_c); n1++) {
					for(int i = 0; i < n_c/n_r; i++) {
						for(int j = 0; j < n_r; j++) {
							C[ind1] = C_p[m1*p + m + n1*((M/(p*m_c))*p + p_l)][m2*m_r*n_r + m3*n_r + i*m_c1*n_r + j];
							ind1++;		
						}
					}
				}


				n1 = N/n_c;

				for(int i = 0; i < n_c1/n_r; i++) {
					for(int j = 0; j < n_r; j++) {
						// ignore zero-padded rows
						if( (i*n_r + j + n1*n_c) < N ) {
							C[ind1] = C_p[m1*p + m + n1*((M/(p*m_c))*p + p_l)][m2*m_r*n_r + m3*n_r + i*m_c1*n_r + j];
							ind1++;
						}
					}
				}
			}
		}
	}

	m = p_l - 1;

	for(int m2 = 0; m2 < m_c1_last_core/m_r; m2++) {
		for(int m3 = 0; m3 < m_r; m3++) { 
			for(n1 = 0; n1 < (N/n_c); n1++) {
				for(int i = 0; i < n_c/n_r; i++) {
					for(int j = 0; j < n_r; j++) {
						if( (m3 + m2*m_r + m*m_c1 + m1*p*m_c) < M ) {
							C[ind1] = C_p[m1*p + m + n1*((M/(p*m_c))*p + p_l)][m2*m_r*n_r + m3*n_r + i*m_c1_last_core*n_r + j];
							ind1++;		
						}
					}
				}
			}


			n1 = N/n_c;

			for(int i = 0; i < n_c1/n_r; i++) {
				for(int j = 0; j < n_r; j++) {
					// ignore zero-padded rows
					if( ((i*n_r + j + n1*n_c) < N)  && ((m3 + m2*m_r + m*m_c1 + m1*p*m_c) < M)) {
						C[ind1] = C_p[m1*p + m + n1*((M/(p*m_c))*p + p_l)][m2*m_r*n_r + m3*n_r + i*m_c1_last_core*n_r + j];
						ind1++;
					}
				}
			}
		}
	}
}


void unpack_C(double* C, double** C_p, int M, int N, int m_c, int n_c, int n_r, int m_r, int p) {

	int m_pad = (M % m_c) ? 1 : 0; 
	int m1, p_l;

	int ind1 = 0;
	for(int n3 = 0; n3 < (N/n_c); n3++) {
		for(int n2 = 0; n2 < (n_c/n_r); n2++) {
			for(int n1 = 0; n1 < n_r; n1++) {
				for(m1 = 0; m1 < (M/(p*m_c)); m1++) {
					for(int m = 0; m < p; m++) {
						for(int i = 0; i < (m_c/m_r); i++) {
							for(int j = 0; j< m_r; j++) {

								if(DEBUG) printf("ind1 %d \n", n3*(M/m_c + m_pad)  + m1*p + m);
								printf("ind2 %d \n", n2*m_c*n_r + n1*m_r + i*m_r*n_r + j);
								
								printf("dude\n");
								C[ind1] = C_p[n3*(M/m_c + m_pad)  + m1*p + m][n2*m_c*n_r + n1*m_r + i*m_r*n_r + j];
								printf("done\n");
								ind1++;
							}
						}
					}
				}

				m1 = M / (p*m_c);
				p_l = (int) ceil(((double) (M % (p*m_c))) / m_c);

				for(int m = 0; m < p_l; m++) {
					for(int i = 0; i < (m_c/m_r); i++) {
						for(int j = 0; j< m_r; j++) {
							C[ind1] = C_p[n3*(M/m_c + m_pad)  + m1*p + m][n2*m_c*n_r + n1*m_r + i*m_r*n_r + j];
							ind1++;
						}
					}
				}
			}
		}
	}
}

