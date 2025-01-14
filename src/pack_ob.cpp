#include "cake.h"





void csr_to_ob_A_sp(float* vals, int* colind_csr, int* rowptr_csr, int* nnz_tiles, int* num_col_tile,
   char* nnz_outer, int* k_inds, char* loc_m, float* A_p, int M, int m1, int m2, int k1,
   int m_c, int k_c, int m_r, int nz_in, int col_tile_in, int* ret) {
         

   int nnz_col, ind_tile, outer_ind = 0, a_ind = 0;
   float a_tmp = 0;

   int mr_bins = m_r + 1;
   int** cnt = (int**) malloc(mr_bins * sizeof(int*)); // 
   int* cnt_inds = (int*) malloc(mr_bins * sizeof(int)); // number of cols with 0-mr nnz vals

   for(int i = 0; i < mr_bins; i++) {
      cnt[i] = (int*) malloc(k_c * sizeof(int));
   }


   for(int m3 = 0; m3 < m_c; m3 += m_r) {

      // convert csr to temporary outer product tile
      float* A = (float*) calloc(k_c * m_r , sizeof(float));

      for(int i = 0; i < m_r; i++) {

         if(m1 + m2 + m3 + i < M) {

            int k_col = rowptr_csr[m3 + i + 1] - rowptr_csr[m3 + i];
            for(int j = 0; j < k_col; j++) {

               if((*colind_csr >= k1) && (*colind_csr < (k1 + k_c))) {
                  A[i*k_c + (*colind_csr - k1)] = *vals;
               } 

               colind_csr++;
               vals++;
            }
         }
      }

      // print_mat(A, m_r, k_c);

      // create k_inds, nnz_outer, loc_m from outer product tile
      ind_tile = 0;
      memset(cnt_inds, 0, mr_bins*sizeof(int));

      for(int i = 0; i < k_c; i++) {

         nnz_col = 0;

         for(int j = 0; j < m_r; j++) {

               if(A[i + j*k_c] != 0) {
                  nnz_col++;
               }
         }

         cnt[nnz_col][cnt_inds[nnz_col]++] = i;
      }


       for(int c = m_r; c > 0; c--) {

           if(!cnt_inds[c]) {
              continue;
           }

           for(int i = 0; i < cnt_inds[c]; i++) {

               for(int j = 0; j < m_r; j++) {
                 
                  a_tmp = A[cnt[c][i] + j*k_c];
                   if(a_tmp != 0) {
                     A_p[a_ind + ind_tile] = a_tmp;
                     loc_m[a_ind + ind_tile++] = j;
                   }
               }

              k_inds[outer_ind] = cnt[c][i];
              nnz_outer[outer_ind++] = c;
           }
       }

       // outer_ind += cnt_inds[0]; // skip ahead over cols with 0 nonzeros
       a_ind += ind_tile;
       *nnz_tiles = nz_in + a_ind;
       nnz_tiles++;

       *num_col_tile = col_tile_in + outer_ind;
       num_col_tile++;
       free(A);
   }

   for(int i = 0; i < mr_bins; i++) {
      free(cnt[i]);
   }

   free(cnt);
   free(cnt_inds);

   ret[0] = outer_ind;
   ret[1] = a_ind;
}


void pack_ob_A_sp(float* A, float* A_p, char* nnz_outer, int* k_inds, char* loc_m, 
   int M, int K, int m1, int m2, int m_c, int k_c, int m_r, bool pad) {

   int nnz_col, ind_blk, outer_ind = 0, a_ind = 0;
   float a_tmp = 0;

   int mr_bins = m_r + 1;
   int** cnt = (int**) malloc(mr_bins * sizeof(int*));
   int* cnt_inds = (int*) malloc(mr_bins * sizeof(int));
   // int cnt_inds[7]; // = (int*) malloc(7 * sizeof(int));

   for(int i = 0; i < mr_bins; i++) {
      cnt[i] = (int*) malloc(k_c * sizeof(int));
   }

   if(pad) {

      for(int m3 = 0; m3 < m_c; m3 += m_r) {

         ind_blk = 0;
         memset(cnt_inds, 0, mr_bins*sizeof(int));

         for(int i = 0; i < k_c; i++) {

            nnz_col = 0;

            for(int j = 0; j < m_r; j++) {

               if((m1 + m2 + m3 + j) < M) {

                  if(A[m3*K + i + j*K] != 0) {
                     nnz_col++;
                  }
               }
            }

            cnt[nnz_col][cnt_inds[nnz_col]++] = i;
         }

         // reorder columns in A in descending order of their densities
         for(int c = m_r; c > 0; c--) {
       
            if(!cnt_inds[c]) {
               // ind_blk += 6;
               continue;
            }

            for(int i = 0; i < cnt_inds[c]; i++) {

               for(int j = 0; j < m_r; j++) {

                  if((m1 + m2 + m3 + j) >=  M) {
                     A_p[a_ind + ind_blk] = 0.0;
                  } else {

                     a_tmp = A[m3*K + cnt[c][i] + j*K];
                     if(a_tmp != 0) {
                        A_p[a_ind + ind_blk] = a_tmp;
                        loc_m[a_ind + ind_blk++] = j;
                     }
                  }
               }

               k_inds[outer_ind] = cnt[c][i];
               nnz_outer[outer_ind++] = c;
            }

         }

         outer_ind += cnt_inds[0]; // skip ahead over cols with 0 nonzeros
         a_ind += m_r*k_c;
      }
   } 

   else {

      for(int m3 = 0; m3 < m_c; m3 += m_r) {

         ind_blk = 0;
         memset(cnt_inds, 0, mr_bins*sizeof(int));

         for(int i = 0; i < k_c; i++) {

            nnz_col = 0;

            for(int j = 0; j < m_r; j++) {

               if(A[m3*K + i + j*K] != 0) {
                  nnz_col++;
               }
            }

            cnt[nnz_col][cnt_inds[nnz_col]++] = i;
         }


         for(int c = m_r; c > 0; c--) {

            if(!cnt_inds[c]) {
               // ind_blk += 6;
               continue;
            }

            for(int i = 0; i < cnt_inds[c]; i++) {

               for(int j = 0; j < m_r; j++) {

                  // A_p[a_ind + ind_blk] = A[m3*K + cnt[c][i] + j*K];
                  // loc_m[a_ind + ind_blk++] = j;
                  
                  a_tmp = A[m3*K + cnt[c][i] + j*K];
                  if(a_tmp != 0) {
                     A_p[a_ind + ind_blk] = a_tmp;
                     loc_m[a_ind + ind_blk++] = j;
                  }
               }

               k_inds[outer_ind] = cnt[c][i];
               nnz_outer[outer_ind++] = c;
            }

         }

         outer_ind += cnt_inds[0]; // skip ahead over cols with 0 nonzeros
         a_ind += m_r*k_c;
      }
   }

   for(int i = 0; i < mr_bins; i++) {
      free(cnt[i]);
   }

   free(cnt);
   free(cnt_inds);
}


// // packing without density-based reordering
// void pack_ob_A_sp(float* A, float* A_p, int* nnz_outer, int* k_inds, int* loc_m, 
//    int M, int K, int m1, int m2, int m_c, int k_c, int m_r, bool pad) {

//    int nnz_col, ind_blk, outer_ind = 0, a_ind = 0;
//    float a_tmp = 0;

//    if(pad) {
//       for(int m3 = 0; m3 < m_c; m3 += m_r) {

//          ind_blk = 0;

//          for(int i = 0; i < k_c; i++) {

//             nnz_col = 0;

//             for(int j = 0; j < m_r; j++) {

//                if((m1 + m2 + m3 + j) >=  M) {
//                   A_p[a_ind + ind_blk] = 0.0;
//                } else {

//                   a_tmp = A[m3*K + i + j*K];
//                   if(a_tmp != 0) {
//                      A_p[a_ind + ind_blk] = a_tmp;
//                      loc_m[a_ind + ind_blk++] = j;
//                      nnz_col++;
//                   }
//                }

//             }

//             nnz_outer[outer_ind++] = nnz_col;
//          }

//          a_ind += m_r*k_c;
//       }     
//    } 

//    else {
//       for(int m3 = 0; m3 < m_c; m3 += m_r) {

//          ind_blk = 0;

//          for(int i = 0; i < k_c; i++) {

//             nnz_col = 0;

//             for(int j = 0; j < m_r; j++) {

//                a_tmp = A[m3*K + i + j*K];
//                if(a_tmp != 0) {
//                   A_p[a_ind + ind_blk] = a_tmp;
//                   loc_m[a_ind + ind_blk++] = j;
//                   nnz_col++;
//                }
//             }

//             nnz_outer[outer_ind++] = nnz_col;
//          }

//          a_ind += m_r*k_c;
//       }     
//    }
// }



// void pack_ob_A_sp(float* A, float* A_p, int* nnz_outer_blk, int* nnz_outer, int* loc_m, 
//    int M, int K, int m1, int m2, int m_c, int k_c, int m_r, bool pad) {

//    int nnz_col, nnz_blk, nnz_ob = 0, ind_ob = 0, outer_ind = 0, outer_blk_ind = 0;
//    float a_tmp = 0;

//    if(pad) {
//       for(int m3 = 0; m3 < m_c; m3 += m_r) {

//          nnz_blk = 0;

//          for(int i = 0; i < k_c; i++) {

//             nnz_col = 0;

//             for(int j = 0; j < m_r; j++) {

//                if((m1 + m2 + m3 + j) >=  M) {
//                   A_p[ind_ob++] = 0.0;
//                } else {

//                   a_tmp = A[m3*K + i + j*K];
//                   if(a_tmp != 0) {
//                      A_p[ind_ob] = a_tmp;
//                      loc_m[ind_ob++] = j+1;
//                      nnz_col++;
//                      // nnz_ob++;
//                   }
//                }

//                // ind_ob++;
//             }

//             nnz_outer[outer_ind++] = nnz_col;
//             nnz_blk += nnz_col;
//          }

//          nnz_outer_blk[outer_blk_ind++] = nnz_blk;
//       }     
//    } 

//    else {
//       for(int m3 = 0; m3 < m_c; m3 += m_r) {

//          nnz_blk = 0;

//          for(int i = 0; i < k_c; i++) {

//             nnz_col = 0;

//             for(int j = 0; j < m_r; j++) {

//                a_tmp = A[m3*K + i + j*K];
//                if(a_tmp != 0) {
//                   A_p[ind_ob] = a_tmp;
//                   loc_m[ind_ob++] = j+1;
//                   nnz_col++;
//                   // nnz_ob++;
//                }

//                // ind_ob++;
//             }

//             nnz_outer[outer_ind++] = nnz_col;
//             nnz_blk += nnz_col;
//          }

//          nnz_outer_blk[outer_blk_ind++] = nnz_blk;
//       }     
//    }

//    // return nnz_ob;
// }




// void pack_ob_A_single_buf(float* A, float* A_p, int M, int K, int m1, int m2, int m_c, int k_c, int m_r, bool pad) {

//    int ind_ob = 0;

//    if(pad) {
//       for(int m3 = 0; m3 < m_c; m3 += m_r) {
//          for(int i = 0; i < k_c; i++) {
//             for(int j = 0; j < m_r; j++) {
//                if((m1 + m2 + m3 + j) <  M) {
//                   A_p[ind_ob] = A[m3*K + i + j*K];
//                }

//                ind_ob++;
//             }
//          }
//       }     
//    } 

//    else {
//       for(int m3 = 0; m3 < m_c; m3 += m_r) {
//          for(int i = 0; i < k_c; i++) {
//             for(int j = 0; j < m_r; j++) {
//                A_p[ind_ob] = A[m3*K + i + j*K];
//                ind_ob++;
//             }
//          }
//       }     
//    }
// }





void pack_ob_A_single_buf(float* A, float* A_p, int M, int K, int m1, int m2, int m_c, int k_c, int m_r, bool pad) {

   int ind_ob = 0;

   for(int m3 = 0; m3 < m_c; m3 += m_r) {
      for(int i = 0; i < k_c; i++) {
         for(int j = 0; j < m_r; j++) {

            if((m1 + m2 + m3 + j) >=  M) {
               A_p[ind_ob] = 0.0;
            } else {
               // printf("PAD IND %d\n", m3*K + i + j*K);

               A_p[ind_ob] = A[m3*K + i + j*K];
            }

            ind_ob++;
            // printf("ind_ob %d\n", ind_ob);
         }
      }
   }     

   // if(pad) {
   // printf("PAD m1 %d m2 %d mc %d kc %d pad %d\n", m1, m2, m_c, k_c, pad);

   //    for(int m3 = 0; m3 < m_c; m3 += m_r) {
   //       for(int i = 0; i < k_c; i++) {
   //          for(int j = 0; j < m_r; j++) {

   //             if((m1 + m2 + m3 + j) >=  M) {
   //                A_p[ind_ob] = 0.0;
   //             } else {
   //                printf("PAD IND %d\n", m3*K + i + j*K);

   //                A_p[ind_ob] = A[m3*K + i + j*K];
   //             }

   //             ind_ob++;
   //             // printf("ind_ob %d\n", ind_ob);
   //          }
   //       }
   //    }     
   // } 

   // else {
   //    printf("m1 %d m2 %d mc %d kc %d pad %d\n", m1, m2, m_c, k_c, pad);

   //    for(int m3 = 0; m3 < m_c; m3 += m_r) {
   //       for(int i = 0; i < k_c; i++) {
   //          for(int j = 0; j < m_r; j++) {
   //                printf("IND %d\n", m3*K + i + j*K);

   //             A_p[ind_ob] = A[m3*K + i + j*K];

   //             ind_ob++;
   //          }
   //       }
   //    }     
   // }
}





void pack_ob_B_single_buf(float* B, float* B_p, int K, int N, int n1,
            int k_c, int n_c, int n_r, bool pad_n) {

   int ind_ob = 0;

   if(pad_n) {
      for(int n2 = 0; n2 < n_c; n2 += n_r) {
         for(int i = 0; i < k_c; i++) {
            for(int j = 0; j < n_r; j++) {
               if((n1 + n2 + j) >=  N) {
                     B_p[ind_ob] = 0.0;
               } else {
                  // B_p[ind1 + local_ind + (k1/k_c)*k_c*n_c1] = B[n1 + k1*N + n2 + i*N + j];
                  B_p[ind_ob] = B[n2 + i*N + j];
               }
               ind_ob++;
            }
         }
      }
   }
   
   else {
      for(int n2 = 0; n2 < n_c; n2 += n_r) {
         for(int i = 0; i < k_c; i++) {
            for(int j = 0; j < n_r; j++) {
               B_p[ind_ob] = B[n2 + i*N + j];
               ind_ob++;
            }

            // _mm256_store_ps (&B_p[ind_ob], _mm256_load_ps(&B[n2 + i*N]));
            // _mm256_store_ps (&B_p[ind_ob + 8], _mm256_load_ps(&B[n2 + i*N + 8]));
            // ind_ob += n_r;
         }
      }
   }
}



// B row stored, B_p also row stored (k_c rows with n_r contiguous elements each)
void pack_B_nr_x_kc(float* B, float* B_p, int N, int k_c, int n_r) {

   int ind_ob = 0;

   for(int i = 0; i < k_c; i++) {
      for(int j = 0; j < n_r; j++) {
         B_p[ind_ob] = B[i*N + j];
         ind_ob++;
      }
   }
}


// A row stored, A_p contains k_c cols of mr contiguous elements
void pack_A_mr_x_kc(float* A, float* A_p, int K, int k_c, int m_r) {

   int ind_ob = 0;

   for(int i = 0; i < k_c; i++) {
      for(int j = 0; j < m_r; j++) {
         A_p[ind_ob] = A[i + j*K];
         ind_ob++;
      }
   }       
}



void pack_ob_B_parallel(float* B, float* B_p, int K, int N, int n1,
            int k_c, int n_c, int n_r, bool pad_n) {


   int ind_ob, n2;

   if(pad_n) {
      #pragma omp parallel for private(n2, ind_ob)
      for(n2 = 0; n2 < n_c; n2 += n_r) {
         ind_ob = 0;
         for(int i = 0; i < k_c; i++) {
            for(int j = 0; j < n_r; j++) {
               if((n1 + n2 + j) >=  N) {
                  B_p[n2*k_c + ind_ob] = 0.0;
               } else {
            // B_p[ind1 + local_ind + (k1/k_c)*k_c*n_c1] = B[n1 + k1*N + n2 + i*N + j];
                  B_p[n2*k_c + ind_ob] = B[n2 + i*N + j];
               }
               ind_ob++;
            }
         }
      }
   }

   else {
      #pragma omp parallel for private(n2, ind_ob)
      for(n2 = 0; n2 < n_c; n2 += n_r) {
         ind_ob = 0;
         for(int i = 0; i < k_c; i++) {
            for(int j = 0; j < n_r; j++) {
               B_p[n2*k_c + ind_ob] = B[n2 + i*N + j];
               ind_ob++;
            }

   // _mm256_store_ps (&B_p[ind_ob], _mm256_load_ps(&B[n2 + i*N]));
   // _mm256_store_ps (&B_p[ind_ob + 8], _mm256_load_ps(&B[n2 + i*N + 8]));
   // ind_ob += n_r;
         }
      }
   }
}

// void pack_ob_C_single_buf(float* C, float* C_p, int M, int N, int m1, int n1, int m2,
//             int m_c, int n_c, int m_r, int n_r, bool pad_m, bool pad_n) {

//    int ind_ob = 0;

//    if(pad_m || pad_n) {

//       for(int n2 = 0; n2 < n_c; n2 += n_r) {
//          for(int m3 = 0; m3 < m_c; m3 += m_r) {
//             for(int i = 0; i < m_r; i++) {
//                for(int j = 0; j < n_r; j++) {
//                   if((n1 + n2 + j) < N  ||  (m1 + m2 + m3 + i) <  M) {
//                      C_p[ind_ob] = C[n2 + m3*N + i*N + j];
//                   }
//                   ind_ob++;
//                }
//             }
//          }
//       }

//    } else {

//       for(int n2 = 0; n2 < n_c; n2 += n_r) {
//          for(int m3 = 0; m3 < m_c; m3 += m_r) {
//             for(int i = 0; i < m_r; i++) {
//                for(int j = 0; j < n_r; j++) {
//                   C_p[ind_ob] = C[n2 + m3*N + i*N + j];
//                   ind_ob++;
//                }
//             }
//          }
//       }
//    }
// }




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




// // initialize an operation block of matrix A
// initialize an operation block of matrix A
void pack_ob_A_multiple_buf(float* A, float* A_p, int M, int K, int m1, int k1, int m2, int m_c, int k_c, int m_r, bool pad) {

   int ind2 = 0;
   
   if(pad) {
      for(int m3 = 0; m3 < m_c; m3 += m_r) {
         for(int i = 0; i < k_c; i++) {
            for(int j = 0; j < m_r; j++) {

               if((m1 + m2 + m3 + j) >=  M) {
                  A_p[ind2] = 0.0;
               } else {
                  A_p[ind2] = A[m1*K + k1 + m2*K + m3*K + i + j*K];
               }

               ind2++;
            }
         }
      }     
   } 

   else {
      for(int m3 = 0; m3 < m_c; m3 += m_r) {
         for(int i = 0; i < k_c; i++) {
            for(int j = 0; j < m_r; j++) {
               A_p[ind2] = A[m1*K + k1 + m2*K + m3*K + i + j*K];
               ind2++;
            }
         }
      }
   }
}



void pack_ob_C_multiple_buf(float* C, float* C_p, int M, int N, int m1, int n1, int m2,
            int m_c, int n_c, int m_r, int n_r, bool pad) {

   int ind2 = 0;

   if(pad) {

      for(int n2 = 0; n2 < n_c; n2 += n_r) {
         for(int m3 = 0; m3 < m_c; m3 += m_r) {
            for(int i = 0; i < m_r; i++) {
               for(int j = 0; j < n_r; j++) {
                  if((n1 + n2 + j) >= N  ||  (m1 + m2 + m3 + i) >=  M) {
                     C_p[ind2] = 0.0; // padding
                  } else {
                     C_p[ind2] = C[n1 + m1*N + m2*N + n2 + m3*N + i*N + j];
                  }
                  ind2++;
               }
            }
         }
      }

   } else {

      for(int n2 = 0; n2 < n_c; n2 += n_r) {
         for(int m3 = 0; m3 < m_c; m3 += m_r) {
            for(int i = 0; i < m_r; i++) {
               for(int j = 0; j < n_r; j++) {
                  C_p[ind2] = C[n1 + m1*N + m2*N + n2 + m3*N + i*N + j];
                  ind2++;
               }
            }
         }
      }
   }
}



void unpack_ob_C_multiple_buf(float* C, float* C_p, int M, int N, int m1, int n1, int m2,
            int m_c, int n_c, int m_r, int n_r) {

   int ind2 = 0;

   for(int n2 = 0; n2 < n_c; n2 += n_r) {
      for(int m3 = 0; m3 < m_c; m3 += m_r) {
         for(int i = 0; i < m_r; i++) {
            for(int j = 0; j < n_r; j++) {
               if((n1 + n2 + j) < N  &&  (m1 + m2 + m3 + i) <  M) {
                  C[n1 + m1*N + m2*N + n2 + m3*N + i*N + j] = C_p[ind2];
               }
               ind2++;
            }
         }
      }
   }

}


// when writing reorderd rows back to C, 
// write to correct row row_inds[m1 + m2 + m3 + i]

void unpack_ob_C_single_buf(float* C, float* C_p, int M, int N, int m1, int n1, int m2,
            int m_c, int n_c, int m_r, int n_r) {

   int ind2 = 0;

   for(int n2 = 0; n2 < n_c; n2 += n_r) {
      for(int m3 = 0; m3 < m_c; m3 += m_r) {
         for(int i = 0; i < m_r; i++) {
            for(int j = 0; j < n_r; j++) {
               if((n1 + n2 + j) < N  &&  (m1 + m2 + m3 + i) <  M) {
                  // C[n1 + m1*N + m2*N + n2 + m3*N + i*N + j] = C_p[ind2];
                  C[n2 + m3*N + i*N + j] = C_p[ind2];
               }
               ind2++;
            }
         }
      }
   }
}

