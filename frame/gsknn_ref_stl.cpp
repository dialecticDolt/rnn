//#include <mkl.h>
#include <omp.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include<vector>
#include<algorithm>
#include<utility>
#include<stdio.h>
#include<iostream>

extern "C" {
#include <gsknn.h>
#include <gsknn_ref_stl.hpp>
}

#ifdef USE_BLAS
// dgemm prototype
extern "C" void dgemm( char*, char*, int*, int*, int*, double*, double*, 
    int*, double*, int*, double*, double*, int* );
// sgemm prototype
extern "C" void sgemm( char*, char*, int*, int*, int*, float*, float*, 
    int*, float*, int*, float*, float*, int* );
#endif


// Heap operator
struct lessthan {
  bool operator()(const std::pair<int, double> &a, const std::pair<int, double> &b ) const {
    return a.second < b.second;
  }
};


// This reference function will call MKL
extern "C"
void dgsknn_ref_stl(
    int    m,
    int    n,
    int    k,
    int    r,
    double *XA,
    double *XA2,
    int    *alpha,
    double *XB,
    double *XB2,
    int    *beta,
    double *D,
    int    *I
    )
{
  int    i, j, l, p;
  double beg, time_collect, time_dgemm, time_square, time_heap;
  double *As, *Bs, *Cs;
  double fneg2 = -2.0, fzero = 0.0;


  // Sanity check for early return.
  if ( m == 0 || n == 0 || k == 0 || r == 0 ) {
    return;
  }

  // Allocate buffers.
  As = (double*)malloc( sizeof(double) * m * k );
  Bs = (double*)malloc( sizeof(double) * n * k );
  Cs = (double*)malloc( sizeof(double) * m * n );

  #include "gsknn_ref_impl.h"


  // STL Max heap implementation.
  beg = omp_get_wtime();
  #pragma omp parallel for private( i )
  for ( j = 0; j < n; j ++ ) {
    std::vector<std::pair<int, double> > myheap( r );
    for ( i = 0; i < r; i ++ ) {
      myheap[ i ] = std::make_pair( I[ j * r + i ], D[ j * r + i] );
    }
    for ( i = 0; i < m; i ++ ) {
      if ( myheap.front().second > Cs[ j * m + i] ) {
        myheap.front().first  = i;
        myheap.front().second = Cs[ j * m + i ];
        std::pop_heap( myheap.begin(), myheap.end(), lessthan() );
        std::push_heap( myheap.begin(), myheap.end(), lessthan() );
      }
    }
    for ( i = 0; i < r; i ++ ) {
      I[ j * r + i ] = myheap[ i ].first;
      D[ j * r + i ] = myheap[ i ].second;
    }
  }
  time_heap = omp_get_wtime() - beg;

  //printf( "Collect: %5.3lf, Gemm: %5.3lf, Sq2nrm: %5.3lf, Select: %5.3lf\n",
  //    time_collect, time_dgemm, time_square, time_heap );


  // Free the temporary buffers
  free( As );
  free( Bs );
  free( Cs );
}
