#include "us_eigen_nonsymmsquare.h"
#include <stdio.h>
#include <stdlib.h>
#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif
us_block_complex* us_block_complex_alloc(const size_t n) {
    us_block_complex* b;

    b = (us_block_complex*)malloc(sizeof(us_block_complex));

    if (b == 0) {
        printf("failed to allocate space for block struct");
    }

    b->data = (float*)malloc(n * sizeof(float));

    if (b->data == 0 && n > 0) /* malloc may return NULL when n == 0 */
    {
        free(b); /* exception in constructor, avoid memory leak */
        printf("failed to allocate space for block data");
    }
    b->size = n;

    return b;
}

us_block_float* us_block_float_alloc(const size_t n) {
    us_block_float* b;

    b = (us_block_float*)malloc(sizeof(us_block_float));

    if (b == 0) {
        printf("failed to allocate space for block struct");
    }

    b->data = (float*)malloc(n * sizeof(float));

    if (b->data == 0 && n > 0) /* malloc may return NULL when n == 0 */
    {
        free(b); /* exception in constructor, avoid memory leak */
        printf("failed to allocate space for block data");
    }
    b->size = n;

    return b;
}

us_matrix_float* us_matrix_float_alloc(const size_t n1, const size_t n2) {
    us_block_float* block;
    us_matrix_float* m;
    m = (us_matrix_float*)malloc(sizeof(us_matrix_float));

    if (m == 0) {
        printf("failed to allocate space for matrix struct");
    }

    /* FIXME: n1*n2 could overflow for large dimensions */
    block = us_block_float_alloc(n1 * n2);
    if (block == 0) {
        printf("failed to allocate space for block");
    }

    m->data = block->data;
    m->size1 = n1;
    m->size2 = n2;
    m->tda = n2;
    m->block = block;
    m->owner = 1;

    return m;
}

us_vector_float_view us_vector_subvector(us_vector* v, size_t offset, size_t n) {
    us_vector_float_view view = NULL_VECTOR_VIEW;

    if (offset + (n > 0 ? n - 1 : 0) >= v->size) {
        printf("view would extend past end of vector");
    }

    {
        us_vector s = NULL_VECTOR;

        s.data = v->data + MULTIPLICITY * v->stride * offset;
        s.size = n;
        s.stride = v->stride;
        s.block = v->block;
        s.owner = 0;

        view.vector = s;
        return view;
    }
}

us_matrix_complex* us_matrix_complex_alloc(const size_t n1, const size_t n2) {
    us_block_float* block;
    us_matrix_complex* m;

    m = (us_matrix_complex*)malloc(sizeof(us_matrix_complex));

    if (m == 0) {
        printf("failed to allocate space for matrix struct");
    }

    /* FIXME: n1*n2 could overflow for large dimensions */
    block = us_block_float_alloc(n1 * n2);

    if (block == 0) {
        printf("failed to allocate space for block");
    }

    m->data = block->data;
    m->size1 = n1;
    m->size2 = n2;
    m->tda = n2;
    m->block = block;
    m->owner = 1;
    return m;
}

us_vector_complex* us_vector_complex_alloc(const size_t n) {
    us_block_float* block;
    us_vector_complex* v;

    v = (us_vector_complex*)malloc(sizeof(us_vector_complex));

    if (v == 0) {
        printf("failed to allocate space for vector struct");
    }

    block = us_block_float_alloc(n);

    if (block == 0) {
        free(v);
        printf("failed to allocate space for block");
    }

    v->data = block->data;
    v->size = n;
    v->stride = 1;
    v->block = block;
    v->owner = 1;

    return v;
}

us_vector* us_vector_float_alloc(const size_t n) {
    us_block_float* block;
    us_vector* v;

    v = (us_vector*)malloc(sizeof(us_vector));

    if (v == 0) {
        printf("failed to allocate space for vector struct");
    }

    block = us_block_float_alloc(n);

    if (block == 0) {
        free(v);

        printf("failed to allocate space for block");
    }

    v->data = block->data;
    v->size = n;
    v->stride = 1;
    v->block = block;
    v->owner = 1;

    return v;
}

us_permutation* us_permutation_alloc(const size_t n) {
    us_permutation* p;

    if (n == 0) {
        printf("permutation length n must be positive integer");
    }

    p = (us_permutation*)malloc(sizeof(us_permutation));

    if (p == 0) {
        printf("failed to allocate space for permutation struct");
    }

    p->data = (size_t*)malloc(n * sizeof(size_t));

    if (p->data == 0) {
        free(p); /* exception in constructor, avoid memory leak */

        printf("failed to allocate space for permutation data");
    }

    p->size = n;

    return p;
}

void us_block_free(us_block_float* b) {
    RETURN_IF_NULL(b);
    free(b->data);
    free(b);
}

void us_matrix_free(us_matrix_float* m) {
    // printf("start matrix free\n");
    // size_t memalloced = xPortGetFreeHeapSize();
    RETURN_IF_NULL(m);
    if (m->owner) {
        us_block_free(m->block);
    }
    // printf("matrix free: after free block before free matrix %d\n",xPortGetFreeHeapSize());
    // //+6024

    free(m);
    // printf("matrix free: %d freed\n",memalloced - xPortGetFreeHeapSize()); //+32
}

void us_vector_float_free(us_vector* v) {
    RETURN_IF_NULL(v);

    if (v->owner) {
        us_block_free(v->block);
    }
    free(v);
}

void us_vector_complex_free(us_vector_complex* v) {
    RETURN_IF_NULL(v);

    if (v->owner) {
        us_block_free(v->block);
    }
    free(v);
}

void us_permutation_free(us_permutation* p) {
    RETURN_IF_NULL(p);
    free(p->data);
    free(p);
}

void us_matrix_float_set(us_matrix_float* m, const size_t i, const size_t j, const float x) {
#if GSL_RANGE_CHECK
    if (GSL_RANGE_COND(1)) {
        if (i > m->size1) {
            printf("first index out of range");
        } else if (j > m->size2) {
            printf("second index out of range");
        }
    }
#endif
    m->data[i * m->tda + j] = x;
}

float us_matrix_float_get(const us_matrix_float* m, const size_t i, const size_t j) {
#if GSL_RANGE_CHECK
    if (GSL_RANGE_COND(1)) {
        if (i > m->size1) {
            printf("first index out of range");
        } else if (j > m->size2) {
            printf("second index out of range");
        }
    }
#endif
    return m->data[i * m->tda + j];
}

void us_vector_float_set(us_vector_float* v, const size_t i, float x) {
#if GSL_RANGE_CHECK
    if (GSL_RANGE_COND(i >= v->size)) {
        printf("index out of range");
    }
#endif
    v->data[i * v->stride] = x;
}

float us_vector_float_get(const us_vector_float* v, const size_t i) {
#if GSL_RANGE_CHECK
    if (GSL_RANGE_COND(i >= v->size)) {
        printf("index out of range");
    }
#endif
    return v->data[i * v->stride];
}

float* us_vector_float_ptr(us_vector_float* v, const size_t i) {
    return (float*)(v->data + i * v->stride);
}

int us_matrix_add(us_matrix_float* a, const us_matrix_float* b) {
    const size_t M = a->size1;
    const size_t N = a->size2;
    if (b->size1 != M || b->size2 != N) {
        printf("matrices must have same dimensions");
    } else {
        const size_t tda_a = a->tda;
        const size_t tda_b = b->tda;
        size_t i, j;
        for (i = 0; i < M; i++) {
            for (j = 0; j < N; j++) {
                a->data[i * tda_a + j] += b->data[i * tda_b + j];
            }
        }
        return GSL_SUCCESS;
    }
    return -1;
}

// single precision, general matrix-matrix multiplication
void cblas_sgemm(const enum CBLAS_ORDER Order, const enum CBLAS_TRANSPOSE TransA,
                 const enum CBLAS_TRANSPOSE TransB, const int M, const int N, const int K,
                 const float alpha, const float* A, const int lda, const float* B, const int ldb,
                 const float beta, float* C, const int ldc) {
    int i, j, k;
    int n1, n2;
    int ldf, ldg;
    int TransF, TransG;
    const float *F, *G;

    // CHECK_ARGS14(GEMM,Order,TransA,TransB,M,N,K,alpha,A,lda,B,ldb,beta,C,ldc);

    if (alpha == 0.0 && beta == 1.0)
        return;

    if (Order == CblasRowMajor) {
        n1 = M;
        n2 = N;
        F = A;
        ldf = lda;
        TransF = (TransA == CblasConjTrans) ? CblasTrans : TransA;
        G = B;
        ldg = ldb;
        TransG = (TransB == CblasConjTrans) ? CblasTrans : TransB;
    } else {
        n1 = N;
        n2 = M;
        F = B;
        ldf = ldb;
        TransF = (TransB == CblasConjTrans) ? CblasTrans : TransB;
        G = A;
        ldg = lda;
        TransG = (TransA == CblasConjTrans) ? CblasTrans : TransA;
    }

    /* form  y := beta*y */
    if (beta == 0.0) {
        for (i = 0; i < n1; i++) {
            for (j = 0; j < n2; j++) {
                C[ldc * i + j] = 0.0;
            }
        }
    } else if (beta != 1.0) {
        for (i = 0; i < n1; i++) {
            for (j = 0; j < n2; j++) {
                C[ldc * i + j] *= beta;
            }
        }
    }

    if (alpha == 0.0)
        return;

    if (TransF == CblasNoTrans && TransG == CblasNoTrans) {
        /* form  C := alpha*A*B + C */

        for (k = 0; k < K; k++) {
            for (i = 0; i < n1; i++) {
                const float temp = alpha * F[ldf * i + k];
                if (temp != 0.0) {
                    for (j = 0; j < n2; j++) {
                        C[ldc * i + j] += temp * G[ldg * k + j];
                    }
                }
            }
        }
    } else if (TransF == CblasNoTrans && TransG == CblasTrans) {
        /* form  C := alpha*A*B' + C */

        for (i = 0; i < n1; i++) {
            for (j = 0; j < n2; j++) {
                float temp = 0.0;
                for (k = 0; k < K; k++) {
                    temp += F[ldf * i + k] * G[ldg * j + k];
                }
                C[ldc * i + j] += alpha * temp;
            }
        }
    } else if (TransF == CblasTrans && TransG == CblasNoTrans) {
        for (k = 0; k < K; k++) {
            for (i = 0; i < n1; i++) {
                const float temp = alpha * F[ldf * k + i];
                if (temp != 0.0) {
                    for (j = 0; j < n2; j++) {
                        C[ldc * i + j] += temp * G[ldg * k + j];
                    }
                }
            }
        }
    } else if (TransF == CblasTrans && TransG == CblasTrans) {
        for (i = 0; i < n1; i++) {
            for (j = 0; j < n2; j++) {
                float temp = 0.0;
                for (k = 0; k < K; k++) {
                    temp += F[ldf * k + i] * G[ldg * j + k];
                }
                C[ldc * i + j] += alpha * temp;
            }
        }
    } else {
        printf("unrecognized operation");
    }
}

int us_blas_sgemm(CBLAS_TRANSPOSE_t TransA, CBLAS_TRANSPOSE_t TransB, float alpha,
                  const us_matrix_float* A, const us_matrix_float* B, float beta,
                  us_matrix_float* C) {
    const size_t M = C->size1;
    const size_t N = C->size2;
    const size_t MA = (TransA == CblasNoTrans) ? A->size1 : A->size2;
    const size_t NA = (TransA == CblasNoTrans) ? A->size2 : A->size1;
    const size_t MB = (TransB == CblasNoTrans) ? B->size1 : B->size2;
    const size_t NB = (TransB == CblasNoTrans) ? B->size2 : B->size1;

    if (M == MA && N == NB && NA == MB) /* [MxN] = [MAxNA][MBxNB] */
    {
        cblas_sgemm(CblasRowMajor, TransA, TransB, M, N, NA, alpha, A->data, A->tda, B->data,
                    B->tda, beta, C->data, C->tda);
        return GSL_SUCCESS;
    } else {
        printf("invalid length");
    }
    return -1;
}

us_vector_float_view us_vector_view_array(const float* base, size_t n) {
    us_vector_float_view view = NULL_VECTOR_VIEW;

    {
        us_vector v = NULL_VECTOR;

        v.data = (float*)base;
        v.size = n;
        v.stride = 1;
        v.block = 0;
        v.owner = 0;

        view.vector = v;
        return view;
    }
}

us_matrix_float_view us_matrix_view_array(const float* array, const size_t n1, const size_t n2) {
    us_matrix_float_view view = NULL_MATRIX_VIEW;

    {
        us_matrix m = NULL_MATRIX;

        m.data = (float*)array;
        m.size1 = n1;
        m.size2 = n2;
        m.tda = n2;
        m.block = 0;
        m.owner = 0;

        view.matrix = m;
        return view;
    }
}

void us_eigen_francis_free(us_eigen_francis_workspace* w) {
    RETURN_IF_NULL(w);

    // if(w->H)
    //	us_matrix_free(w->H);

    // if(w->Zf)
    //	us_matrix_free(w->Zf);

    free(w);
} /* us_eigen_francis_free() */

void us_eigen_nonsymm_free(us_eigen_nonsymm_workspace* w) {
    RETURN_IF_NULL(w);

    if (w->tau)
        us_vector_float_free(w->tau);

    if (w->diag)
        us_vector_float_free(w->diag);

    free(w);
} /* us_eigen_nonsymm_free() */

void us_eigen_francis_T(const int compute_t, us_eigen_francis_workspace* w) {
    w->compute_t = compute_t;
}

void us_eigen_nonsymm_params(const int compute_t, const int balance, us_eigen_nonsymm_workspace* w) {
    us_eigen_francis_T(compute_t, w->francis_workspace_p);
    w->do_balance = balance;
} /* us_eigen_nonsymm_params() */

us_eigen_francis_workspace* us_eigen_francis_alloc(void) {
    us_eigen_francis_workspace* w;

    w = (us_eigen_francis_workspace*)malloc(sizeof(us_eigen_francis_workspace));

    if (w == 0) {
        printf("failed to allocate space for workspace");
    }

    /* these are filled in later */

    w->size = 0;
    w->max_iterations = 0;
    w->n_iter = 0;
    w->n_evals = 0;

    w->compute_t = 0;
    w->Zf = NULL;
    w->H = NULL;
    /*
  w->Zf = us_matrix_float_alloc(6,6);
  w->H = us_matrix_float_alloc(6,6);
    //init H，Zf
    for(int i=0; i<6;i++){
        for(int j=0; j<6; j++){
            us_matrix_float_set(w->Zf,i,j,0);
        }
    }
    for(int i=0; i<6;i++){
        for(int j=0; j<6; j++){
            us_matrix_float_set(w->H,i,j,0);
        }
    }
    */
    return (w);
} /* us_eigen_francis_alloc() */

us_eigen_nonsymm_workspace* us_eigen_nonsymm_alloc(const size_t n) {
    us_eigen_nonsymm_workspace* w;

    if (n == 0) {
        printf("matrix dimension must be positive integer");
    }

    w = (us_eigen_nonsymm_workspace*)malloc(sizeof(us_eigen_nonsymm_workspace));

    if (w == 0) {
        printf("failed to allocate space for workspace");
    }

    w->size = n;
    w->Z = us_matrix_float_alloc(n, n);
    // init Z
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            us_matrix_float_set(w->Z, i, j, 0);
        }
    }
    w->do_balance = 0;
    w->diag = us_vector_float_alloc(n);
    // init diag
    for (int i = 0; i < n; i++) {
        us_vector_float_set(w->diag, i, 0);
    }
    if (w->diag == 0) {
        us_eigen_nonsymm_free(w);
        printf("failed to allocate space for balancing vector");
    }

    w->tau = us_vector_float_alloc(n);
    // init tau
    for (int i = 0; i < n; i++) {
        us_vector_float_set(w->tau, i, 0);
    }

    if (w->tau == 0) {
        us_eigen_nonsymm_free(w);
        printf("failed to allocate space for hessenberg coefficients");
    }

    w->francis_workspace_p = us_eigen_francis_alloc();
    if (w->francis_workspace_p == 0) {
        us_eigen_nonsymm_free(w);
        printf("failed to allocate space for francis workspace");
    }

    return (w);
} /* us_eigen_nonsymm_alloc() */

void us_vector_set_all(us_vector* v, float x) {
    ATOMIC* const data = v->data;
    const size_t n = v->size;
    const size_t stride = v->stride;

    size_t i;

    for (i = 0; i < n; i++) {
        *(float*)(data + MULTIPLICITY * i * stride) = x;
    }
}

us_vector_float_view us_matrix_column(us_matrix* m, const size_t j) {
    us_vector_float_view view = NULL_VECTOR_VIEW;

    {
        us_vector v = NULL_VECTOR;

        v.data = m->data + j * MULTIPLICITY;
        v.size = m->size1;
        v.stride = m->tda;
        v.block = m->block;
        v.owner = 0;

        view.vector = v;
        return view;
    }
}

us_vector_complex_view us_matrix_complex_column(us_matrix_complex* m, const size_t j) {
    us_vector_complex_view view = NULL_VECTOR_VIEW;

    if (j >= m->size2) {
        printf("column index is out of range");
    }

    {
        us_vector_complex v = NULL_VECTOR;

        v.data = m->data + j * MULTIPLICITY;
        v.size = m->size1;
        v.stride = m->tda;
        v.block = m->block;
        v.owner = 0;

        view.vector = v;
        return view;
    }
}

int us_linalg_balance_matrix(us_matrix* A, us_vector* D) {
    const size_t N = A->size1;

    if (N != D->size) {
        printf("vector must match matrix size");
    } else {
        float row_norm, col_norm;
        int not_converged;
        us_vector_float_view v;

        /* initialize D to the identity matrix */
        us_vector_set_all(D, 1.0);

        not_converged = 1;

        while (not_converged) {
            size_t i, j;
            float g, f, s;

            not_converged = 0;

            for (i = 0; i < N; ++i) {
                row_norm = 0.0;
                col_norm = 0.0;

                for (j = 0; j < N; ++j) {
                    if (j != i) {
                        col_norm += fabs(us_matrix_float_get(A, j, i));
                        row_norm += fabs(us_matrix_float_get(A, i, j));
                    }
                }

                if ((col_norm == 0.0) || (row_norm == 0.0)) {
                    continue;
                }

                g = row_norm / FLOAT_RADIX;
                f = 1.0;
                s = col_norm + row_norm;

                /*
                 * find the integer power of the machine radix which
                 * comes closest to balancing the matrix
                 */
                while (col_norm < g) {
                    f *= FLOAT_RADIX;
                    col_norm *= FLOAT_RADIX_SQ;
                }

                g = row_norm * FLOAT_RADIX;

                while (col_norm > g) {
                    f /= FLOAT_RADIX;
                    col_norm /= FLOAT_RADIX_SQ;
                }

                if ((row_norm + col_norm) < 0.95 * s * f) {
                    not_converged = 1;

                    g = 1.0 / f;

                    /*
                     * apply similarity transformation D, where
                     * D_{ij} = f_i * delta_{ij}
                     */

                    /* multiply by D^{-1} on the left */
                    v = us_matrix_row(A, i);
                    us_blas_dscal(g, &v.vector);

                    /* multiply by D on the right */
                    v = us_matrix_column(A, i);
                    us_blas_dscal(f, &v.vector);

                    /* keep track of transformation */
                    us_vector_float_set(D, i, us_vector_float_get(D, i) * f);
                }
            }
        }

        return GSL_SUCCESS;
    }
    return -1;
} /* us_linalg_balance_matrix() */

float cblas_dnrm2(const int N, const float* X, const int incX) {
#define BASE float
    BASE scale = 0.0;
    BASE ssq = 1.0;
    int i;
    int ix = 0;

    if (N <= 0 || incX <= 0) {
        return 0;
    } else if (N == 1) {
        return fabs(X[0]);
    }

    for (i = 0; i < N; i++) {
        const BASE x = X[ix];

        if (x != 0.0) {
            const BASE ax = fabs(x);

            if (scale < ax) {
                ssq = 1.0 + ssq * (scale / ax) * (scale / ax);
                scale = ax;
            } else {
                ssq += (ax / scale) * (ax / scale);
            }
        }

        ix += incX;
    }

    return scale * sqrt(ssq);
#undef BASE
}

float us_blas_dnrm2(const us_vector* X) {
    return cblas_dnrm2(INT(X->size), X->data, INT(X->stride));
}

float us_linalg_householder_transform(us_vector* v) {
    /* replace v[0:n-1] with a householder vector (v[0:n-1]) and
       coefficient tau that annihilate v[1:n-1] */

    const size_t n = v->size;

    if (n == 1) {
        return 0.0; /* tau = 0 */
    } else {
        float alpha, beta, tau;

        us_vector_float_view x = us_vector_subvector(v, 1, n - 1);

        float xnorm = us_blas_dnrm2(&x.vector);

        if (xnorm == 0) {
            return 0.0; /* tau = 0 */
        }

        alpha = us_vector_float_get(v, 0);
        beta = -GSL_SIGN(alpha) * hypot(alpha, xnorm);
        tau = (beta - alpha) / beta;

        {
            float s = (alpha - beta);

            if (fabs(s) > GSL_FLT_MIN) {
                us_blas_dscal(1.0 / s, &x.vector);
                us_vector_float_set(v, 0, beta);
            } else {
                us_blas_dscal(GSL_FLT_EPSILON / s, &x.vector);
                us_blas_dscal(1.0 / GSL_FLT_EPSILON, &x.vector);
                us_vector_float_set(v, 0, beta);
            }
        }

        return tau;
    }
}

void cblas_daxpy(const int N, const float alpha, const float* X, const int incX, float* Y,
                 const int incY) {
#define BASE float
    int i;

    if (alpha == 0.0) {
        return;
    }

    if (incX == 1 && incY == 1) {
        const int m = N % 4;

        for (i = 0; i < m; i++) {
            Y[i] += alpha * X[i];
        }

        for (i = m; i + 3 < N; i += 4) {
            Y[i] += alpha * X[i];
            Y[i + 1] += alpha * X[i + 1];
            Y[i + 2] += alpha * X[i + 2];
            Y[i + 3] += alpha * X[i + 3];
        }
    } else {
        int ix = OFFSET(N, incX);
        int iy = OFFSET(N, incY);

        for (i = 0; i < N; i++) {
            Y[iy] += alpha * X[ix];
            ix += incX;
            iy += incY;
        }
    }
#undef BASE
}

int us_blas_daxpy(float alpha, const us_vector* X, us_vector* Y) {
    if (X->size == Y->size) {
        cblas_daxpy(INT(X->size), alpha, X->data, INT(X->stride), Y->data, INT(Y->stride));
        return GSL_SUCCESS;
    } else {
        printf("invalid length");
    }
    return -1;
}

int us_linalg_householder_hm(float tau, const us_vector* v, us_matrix* A) {
    /* applies a householder transformation v,tau to matrix m */

    if (tau == 0.0) {
        return GSL_SUCCESS;
    }

#ifdef USE_BLAS
    {
        us_vector_const_view v1 = us_vector_const_subvector(v, 1, v->size - 1);
        us_matrix_view A1 = us_matrix_submatrix(A, 1, 0, A->size1 - 1, A->size2);
        size_t j;

        for (j = 0; j < A->size2; j++) {
            float wj = 0.0;
            us_vector_view A1j = us_matrix_column(&A1.matrix, j);
            us_blas_ddot(&A1j.vector, &v1.vector, &wj);
            wj += us_matrix_get(A, 0, j);

            {
                float A0j = us_matrix_get(A, 0, j);
                us_matrix_set(A, 0, j, A0j - tau * wj);
            }

            us_blas_daxpy(-tau * wj, &v1.vector, &A1j.vector);
        }
    }
#else
    {
        size_t i, j;

        for (j = 0; j < A->size2; j++) {
            /* Compute wj = Akj vk */

            float wj = us_matrix_float_get(A, 0, j);

            for (i = 1; i < A->size1; i++) /* note, computed for v(0) = 1 above */
            {
                wj += us_matrix_float_get(A, i, j) * us_vector_float_get(v, i);
            }

            /* Aij = Aij - tau vi wj */

            /* i = 0 */
            {
                float A0j = us_matrix_float_get(A, 0, j);
                us_matrix_float_set(A, 0, j, A0j - tau * wj);
            }

            /* i = 1 .. M-1 */

            for (i = 1; i < A->size1; i++) {
                float Aij = us_matrix_float_get(A, i, j);
                float vi = us_vector_float_get(v, i);
                us_matrix_float_set(A, i, j, Aij - tau * vi * wj);
            }
        }
    }
#endif

    return GSL_SUCCESS;
}

int us_linalg_householder_mh(float tau, const us_vector* v, us_matrix* A) {
    /* applies a householder transformation v,tau to matrix m from the
       right hand side in order to zero out rows */

    if (tau == 0)
        return GSL_SUCCESS;

    /* A = A - tau w v' */

#ifdef USE_BLAS
    {
        us_vector_const_view v1 = us_vector_const_subvector(v, 1, v->size - 1);
        us_matrix_view A1 = us_matrix_submatrix(A, 0, 1, A->size1, A->size2 - 1);
        size_t i;

        for (i = 0; i < A->size1; i++) {
            float wi = 0.0;
            us_vector_view A1i = us_matrix_row(&A1.matrix, i);
            us_blas_ddot(&A1i.vector, &v1.vector, &wi);
            wi += us_matrix_get(A, i, 0);

            {
                float Ai0 = us_matrix_get(A, i, 0);
                us_matrix_set(A, i, 0, Ai0 - tau * wi);
            }

            us_blas_daxpy(-tau * wi, &v1.vector, &A1i.vector);
        }
    }
#else
    {
        size_t i, j;

        for (i = 0; i < A->size1; i++) {
            float wi = us_matrix_float_get(A, i, 0);

            for (j = 1; j < A->size2; j++) /* note, computed for v(0) = 1 above */
            {
                wi += us_matrix_float_get(A, i, j) * us_vector_float_get(v, j);
            }

            /* j = 0 */

            {
                float Ai0 = us_matrix_float_get(A, i, 0);
                us_matrix_float_set(A, i, 0, Ai0 - tau * wi);
            }

            /* j = 1 .. N-1 */

            for (j = 1; j < A->size2; j++) {
                float vj = us_vector_float_get(v, j);
                float Aij = us_matrix_float_get(A, i, j);
                us_matrix_float_set(A, i, j, Aij - tau * wi * vj);
            }
        }
    }
#endif

    return GSL_SUCCESS;
}

int us_linalg_hessenberg_decomp(us_matrix* A, us_vector* tau) {
    const size_t N = A->size1;

    if (N != A->size2) {
        printf("Hessenberg reduction requires square matrix");
    } else if (N != tau->size) {
        printf("tau vector must match matrix size");
    } else if (N < 3) {
        /* nothing to do */
        return GSL_SUCCESS;
    } else {
        size_t i;               /* looping */
        us_vector_float_view c, /* matrix column */
            hv;                 /* householder vector */
        us_matrix_float_view m;
        float tau_i; /* beta in algorithm 7.4.2 */

        for (i = 0; i < N - 2; ++i) {
            /*
             * make a copy of A(i + 1:n, i) and store it in the section
             * of 'tau' that we haven't stored coefficients in yet
             */

            c = us_matrix_subcolumn(A, i, i + 1, N - i - 1);

            hv = us_vector_subvector(tau, i + 1, N - (i + 1));
            us_vector_float_memcpy(&hv.vector, &c.vector);

            /* compute householder transformation of A(i+1:n,i) */
            tau_i = us_linalg_householder_transform(&hv.vector);

            /* apply left householder matrix (I - tau_i v v') to A */
            m = us_matrix_submatrix(A, i + 1, i, N - (i + 1), N - i);
            us_linalg_householder_hm(tau_i, &hv.vector, &m.matrix);

            /* apply right householder matrix (I - tau_i v v') to A */
            m = us_matrix_submatrix(A, 0, i + 1, N, N - (i + 1));
            us_linalg_householder_mh(tau_i, &hv.vector, &m.matrix);

            /* save Householder coefficient */
            us_vector_float_set(tau, i, tau_i);

            /*
             * store Householder vector below the subdiagonal in column
             * i of the matrix. hv(1) does not need to be stored since
             * it is always 1.
             */
            c = us_vector_subvector(&c.vector, 1, c.vector.size - 1);
            hv = us_vector_subvector(&hv.vector, 1, hv.vector.size - 1);
            us_vector_float_memcpy(&c.vector, &hv.vector);
        }

        return GSL_SUCCESS;
    }
    return -1;
} /* us_linalg_hessenberg_decomp() */

void us_matrix_set_identity(us_matrix* m) {
    size_t i, j;
    ATOMIC* const data = m->data;
    const size_t p = m->size1;
    const size_t q = m->size2;
    const size_t tda = m->tda;

    const char zero = 0;
    const char one = 1;

    for (i = 0; i < p; i++) {
        for (j = 0; j < q; j++) {
            *(char*)(data + MULTIPLICITY * (i * tda + j)) = ((i == j) ? one : zero);
        }
    }
}

int us_linalg_hessenberg_unpack_accum(us_matrix* H, us_vector* tau, us_matrix* V) {
    const size_t N = H->size1;

    if (N != H->size2) {
        printf("Hessenberg reduction requires square matrix");
    } else if (N != tau->size) {
        printf("tau vector must match matrix size");
    } else if (N != V->size2) {
        printf("V matrix has wrong dimension");
    } else {
        size_t j;               /* looping */
        float tau_j;            /* householder coefficient */
        us_vector_float_view c, /* matrix column */
            hv;                 /* householder vector */
        us_matrix_float_view m;

        if (N < 3) {
            /* nothing to do */
            return GSL_SUCCESS;
        }

        for (j = 0; j < (N - 2); ++j) {
            c = us_matrix_column(H, j);

            tau_j = us_vector_float_get(tau, j);

            /*
             * get a view to the householder vector in column j, but
             * make sure hv(2) starts at the element below the
             * subdiagonal, since hv(1) was never stored and is always
             * 1
             */
            hv = us_vector_subvector(&c.vector, j + 1, N - (j + 1));

            /*
             * Only operate on part of the matrix since the first
             * j + 1 entries of the real householder vector are 0
             *
             * V -> V * U(j)
             *
             * Note here that V->size1 is not necessarily equal to N
             */
            m = us_matrix_submatrix(V, 0, j + 1, V->size1, N - (j + 1));

            /* apply right Householder matrix to V */
            us_linalg_householder_mh(tau_j, &hv.vector, &m.matrix);
        }

        return GSL_SUCCESS;
    }
    return -1;
} /* us_linalg_hessenberg_unpack_accum() */

int us_linalg_hessenberg_unpack(us_matrix* H, us_vector* tau, us_matrix* U) {
    int s;

    us_matrix_set_identity(U);

    s = us_linalg_hessenberg_unpack_accum(H, tau, U);

    return s;
} /* us_linalg_hessenberg_unpack() */

static inline size_t francis_search_subdiag_small_elements(us_matrix* A) {
    const size_t N = A->size1;
    size_t i;
    float dpel = us_matrix_float_get(A, N - 2, N - 2);

    for (i = N - 1; i > 0; --i) {
        float sel = us_matrix_float_get(A, i, i - 1);
        float del = us_matrix_float_get(A, i, i);

        if ((sel == 0.0) || (fabs(sel) < GSL_FLT_EPSILON * (fabs(del) + fabs(dpel)))) {
            us_matrix_float_set(A, i, i - 1, 0.0);
            return (i);
        }

        dpel = del;
    }

    return (0);
} /* francis_search_subdiag_small_elements() */

static size_t francis_get_submatrix(us_matrix* A, us_matrix* B) {
    size_t diff;
    float ratio;
    size_t top;

    diff = (size_t)(B->data - A->data);

    ratio = (float)diff / ((float)(A->tda + 1));

    top = (size_t)floor(ratio);

    return top;
} /* francis_get_submatrix() */

static inline int francis_qrstep(us_matrix* H, us_eigen_francis_workspace* w) {
    const size_t N = H->size1;
    size_t i; /* looping */
    us_matrix_float_view m;
    float tau_i;  /* householder coefficient */
    float dat[3]; /* householder vector */
    float scale;  /* scale factor to avoid overflow */
    us_vector_float_view v2, v3;
    size_t q, r;
    size_t top = 0; /* location of H in original matrix */
    float s, disc;
    float h_nn,   /* H(n,n) */
        h_nm1nm1, /* H(n-1,n-1) */
        h_cross,  /* H(n,n-1) * H(n-1,n) */
        h_tmp1, h_tmp2;

    v2 = us_vector_view_array(dat, 2);
    v3 = us_vector_view_array(dat, 3);

    if ((w->n_iter % 10) == 0) {
        /*
         * exceptional shifts: we have gone 10 iterations
         * without finding a new eigenvalue, try a new choice of shifts.
         * See LAPACK routine DLAHQR
         */
        s = fabs(us_matrix_float_get(H, N - 1, N - 2)) + fabs(us_matrix_float_get(H, N - 2, N - 3));
        h_nn = us_matrix_float_get(H, N - 1, N - 1) + GSL_FRANCIS_COEFF1 * s;
        h_nm1nm1 = h_nn;
        h_cross = GSL_FRANCIS_COEFF2 * s * s;
    } else {
        /*
         * normal shifts - compute Rayleigh quotient and use
         * Wilkinson shift if possible
         */

        h_nn = us_matrix_float_get(H, N - 1, N - 1);
        h_nm1nm1 = us_matrix_float_get(H, N - 2, N - 2);
        h_cross = us_matrix_float_get(H, N - 1, N - 2) * us_matrix_float_get(H, N - 2, N - 1);

        disc = 0.5 * (h_nm1nm1 - h_nn);
        disc = disc * disc + h_cross;
        if (disc > 0.0) {
            float ave;

            /* real roots - use Wilkinson's shift twice */
            disc = sqrt(disc);
            ave = 0.5 * (h_nm1nm1 + h_nn);
            if (fabs(h_nm1nm1) - fabs(h_nn) > 0.0) {
                h_nm1nm1 = h_nm1nm1 * h_nn - h_cross;
                h_nn = h_nm1nm1 / (disc * GSL_SIGN(ave) + ave);
            } else {
                h_nn = disc * GSL_SIGN(ave) + ave;
            }

            h_nm1nm1 = h_nn;
            h_cross = 0.0;
        }
    }

    h_tmp1 = h_nm1nm1 - us_matrix_float_get(H, 0, 0);
    h_tmp2 = h_nn - us_matrix_float_get(H, 0, 0);

    /*
     * These formulas are equivalent to those in Golub & Van Loan
     * for the normal shift case - the terms have been rearranged
     * to reduce possible roundoff error when subdiagonal elements
     * are small
     */

    dat[0] =
        (h_tmp1 * h_tmp2 - h_cross) / us_matrix_float_get(H, 1, 0) + us_matrix_float_get(H, 0, 1);
    dat[1] = us_matrix_float_get(H, 1, 1) - us_matrix_float_get(H, 0, 0) - h_tmp1 - h_tmp2;
    dat[2] = us_matrix_float_get(H, 2, 1);

    scale = fabs(dat[0]) + fabs(dat[1]) + fabs(dat[2]);
    if (scale != 0.0) {
        /* scale to prevent overflow or underflow */
        dat[0] /= scale;
        dat[1] /= scale;
        dat[2] /= scale;
    }

    if (w->Zf || w->compute_t) {
        /*
         * get absolute indices of this (sub)matrix relative to the
         * original Hessenberg matrix
         */
        top = francis_get_submatrix(w->H, H);
    }

    for (i = 0; i < N - 2; ++i) {
        tau_i = us_linalg_householder_transform(&v3.vector);

        if (tau_i != 0.0) {
            /* q = max(1, i - 1) */
            q = (1 > ((int)i - 1)) ? 0 : (i - 1);

            /* r = min(i + 3, N - 1) */
            r = ((i + 3) < (N - 1)) ? (i + 3) : (N - 1);

            if (w->compute_t) {
                /*
                 * We are computing the Schur form T, so we
                 * need to transform the whole matrix H
                 *
                 * H -> P_k^t H P_k
                 *
                 * where P_k is the current Householder matrix
                 */

                /* apply left householder matrix (I - tau_i v v') to H */
                m = us_matrix_submatrix(w->H, top + i, top + q, 3, w->size - top - q);
                us_linalg_householder_hm(tau_i, &v3.vector, &m.matrix);

                /* apply right householder matrix (I - tau_i v v') to H */
                m = us_matrix_submatrix(w->H, 0, top + i, top + r + 1, 3);
                us_linalg_householder_mh(tau_i, &v3.vector, &m.matrix);
            } else {
                /*
                 * We are not computing the Schur form T, so we
                 * only need to transform the active block
                 */

                /* apply left householder matrix (I - tau_i v v') to H */
                m = us_matrix_submatrix(H, i, q, 3, N - q);
                us_linalg_householder_hm(tau_i, &v3.vector, &m.matrix);

                /* apply right householder matrix (I - tau_i v v') to H */
                m = us_matrix_submatrix(H, 0, i, r + 1, 3);
                us_linalg_householder_mh(tau_i, &v3.vector, &m.matrix);
            }

            if (w->Zf) {
                /* accumulate the similarity transformation into Z */
                m = us_matrix_submatrix(w->Zf, 0, top + i, w->size, 3);
                us_linalg_householder_mh(tau_i, &v3.vector, &m.matrix);
            }
        } /* if (tau_i != 0.0) */

        dat[0] = us_matrix_float_get(H, i + 1, i);
        dat[1] = us_matrix_float_get(H, i + 2, i);
        if (i < (N - 3)) {
            dat[2] = us_matrix_float_get(H, i + 3, i);
        }

        scale = fabs(dat[0]) + fabs(dat[1]) + fabs(dat[2]);
        if (scale != 0.0) {
            /* scale to prevent overflow or underflow */
            dat[0] /= scale;
            dat[1] /= scale;
            dat[2] /= scale;
        }
    } /* for (i = 0; i < N - 2; ++i) */

    scale = fabs(dat[0]) + fabs(dat[1]);
    if (scale != 0.0) {
        /* scale to prevent overflow or underflow */
        dat[0] /= scale;
        dat[1] /= scale;
    }

    tau_i = us_linalg_householder_transform(&v2.vector);

    if (w->compute_t) {
        m = us_matrix_submatrix(w->H, top + N - 2, top + N - 3, 2, w->size - top - N + 3);
        us_linalg_householder_hm(tau_i, &v2.vector, &m.matrix);

        m = us_matrix_submatrix(w->H, 0, top + N - 2, top + N, 2);
        us_linalg_householder_mh(tau_i, &v2.vector, &m.matrix);
    } else {
        m = us_matrix_submatrix(H, N - 2, N - 3, 2, 3);
        us_linalg_householder_hm(tau_i, &v2.vector, &m.matrix);

        m = us_matrix_submatrix(H, 0, N - 2, N, 2);
        us_linalg_householder_mh(tau_i, &v2.vector, &m.matrix);
    }

    if (w->Zf) {
        /* accumulate transformation into Z */
        m = us_matrix_submatrix(w->Zf, 0, top + N - 2, w->size, 2);
        us_linalg_householder_mh(tau_i, &v2.vector, &m.matrix);
    }

    return GSL_SUCCESS;
} /* francis_qrstep() */

void us_vector_complex_set(us_vector_complex* v, const size_t i, us_complex z) {
#if GSL_RANGE_CHECK
    if (GSL_RANGE_COND(i >= v->size)) {
        printf("index out of range");
    }
#endif
    *GSL_COMPLEX_AT(v, i) = z;
}

void cblas_drot(const int N, float* X, const int incX, float* Y, const int incY, const float c,
                const float s) {
#define BASE float

    int i;
    int ix = OFFSET(N, incX);
    int iy = OFFSET(N, incY);
    for (i = 0; i < N; i++) {
        const BASE x = X[ix];
        const BASE y = Y[iy];
        X[ix] = c * x + s * y;
        Y[iy] = -s * x + c * y;
        ix += incX;
        iy += incY;
    }

#undef BASE
}

int us_blas_drot(us_vector* X, us_vector* Y, const float c, const float s) {
    if (X->size == Y->size) {
        cblas_drot(INT(X->size), X->data, INT(X->stride), Y->data, INT(Y->stride), c, s);
        return GSL_SUCCESS;
    } else {
        printf("invalid length");
    }
    return -1;
}

int us_isnan(const float x) {
    int status = (x != x);
    return status;
}

int us_finite(const float x) {
    const float y = x - x;
    int status = (y == y);
    return status;
}

int us_isinf(const float x) {
    if (!us_finite(x) && !us_isnan(x)) {
        return (x > 0 ? +1 : -1);
    } else {
        return 0;
    }
}

float us_hypot(const float x, const float y) {
    float xabs = fabs(x);
    float yabs = fabs(y);
    float min, max;

    /* Follow the optional behavior of the ISO C standard and return
       +Inf when any of the argument is +-Inf, even if the other is NaN.
       http://pubs.opengroup.org/onlinepubs/009695399/functions/hypot.html */
    if (us_isinf(x) || us_isinf(y)) {
        return GSL_POSINF;
    }

    if (xabs < yabs) {
        min = xabs;
        max = yabs;
    } else {
        min = yabs;
        max = xabs;
    }

    if (min == 0) {
        return max;
    }

    {
        float u = min / max;
        return max * sqrt(1 + u * u);
    }
}

static void francis_standard_form(us_matrix* A, float* cs, float* sn) {
    float a, b, c, d; /* input matrix values */
    float tmp;
    float p, z;
    float bcmax, bcmis, scale;
    float tau, sigma;
    float cs1, sn1;
    float aa, bb, cc, dd;
    float sab, sac;

    a = us_matrix_float_get(A, 0, 0);
    b = us_matrix_float_get(A, 0, 1);
    c = us_matrix_float_get(A, 1, 0);
    d = us_matrix_float_get(A, 1, 1);

    if (c == 0.0) {
        /*
         * matrix is already upper triangular - set rotation matrix
         * to the identity
         */
        *cs = 1.0;
        *sn = 0.0;
    } else if (b == 0.0) {
        /* swap rows and columns to make it upper triangular */

        *cs = 0.0;
        *sn = 1.0;

        tmp = d;
        d = a;
        a = tmp;
        b = -c;
        c = 0.0;
    } else if (((a - d) == 0.0) && (GSL_SIGN(b) != GSL_SIGN(c))) {
        /* the matrix has complex eigenvalues with a == d */
        *cs = 1.0;
        *sn = 0.0;
    } else {
        tmp = a - d;
        p = 0.5 * tmp;
        bcmax = GSL_MAX(fabs(b), fabs(c));
        bcmis = GSL_MIN(fabs(b), fabs(c)) * GSL_SIGN(b) * GSL_SIGN(c);
        scale = GSL_MAX(fabs(p), bcmax);
        z = (p / scale) * p + (bcmax / scale) * bcmis;

        if (z >= 4.0 * GSL_FLT_EPSILON) {
            /* real eigenvalues, compute a and d */

            z = p + GSL_SIGN(p) * fabs(sqrt(scale) * sqrt(z));
            a = d + z;
            d -= (bcmax / z) * bcmis;

            /* compute b and the rotation matrix */

            tau = us_hypot(c, z);
            *cs = z / tau;
            *sn = c / tau;
            b -= c;
            c = 0.0;
        } else {
            /*
             * complex eigenvalues, or real (almost) equal eigenvalues -
             * make diagonal elements equal
             */

            sigma = b + c;
            tau = us_hypot(sigma, tmp);
            *cs = sqrt(0.5 * (1.0 + fabs(sigma) / tau));
            *sn = -(p / (tau * (*cs))) * GSL_SIGN(sigma);

            /*
             * Compute [ AA BB ] = [ A B ] [ CS -SN ]
             *         [ CC DD ]   [ C D ] [ SN  CS ]
             */
            aa = a * (*cs) + b * (*sn);
            bb = -a * (*sn) + b * (*cs);
            cc = c * (*cs) + d * (*sn);
            dd = -c * (*sn) + d * (*cs);

            /*
             * Compute [ A B ] = [ CS SN ] [ AA BB ]
             *         [ C D ]   [-SN CS ] [ CC DD ]
             */
            a = aa * (*cs) + cc * (*sn);
            b = bb * (*cs) + dd * (*sn);
            c = -aa * (*sn) + cc * (*cs);
            d = -bb * (*sn) + dd * (*cs);

            tmp = 0.5 * (a + d);
            a = d = tmp;

            if (c != 0.0) {
                if (b != 0.0) {
                    if (GSL_SIGN(b) == GSL_SIGN(c)) {
                        /*
                         * real eigenvalues: reduce to upper triangular
                         * form
                         */
                        sab = sqrt(fabs(b));
                        sac = sqrt(fabs(c));
                        p = GSL_SIGN(c) * fabs(sab * sac);
                        tau = 1.0 / sqrt(fabs(b + c));
                        a = tmp + p;
                        d = tmp - p;
                        b -= c;
                        c = 0.0;

                        cs1 = sab * tau;
                        sn1 = sac * tau;
                        tmp = (*cs) * cs1 - (*sn) * sn1;
                        *sn = (*cs) * sn1 + (*sn) * cs1;
                        *cs = tmp;
                    }
                } else {
                    b = -c;
                    c = 0.0;
                    tmp = *cs;
                    *cs = -(*sn);
                    *sn = tmp;
                }
            }
        }
    }

    /* set new matrix elements */

    us_matrix_float_set(A, 0, 0, a);
    us_matrix_float_set(A, 0, 1, b);
    us_matrix_float_set(A, 1, 0, c);
    us_matrix_float_set(A, 1, 1, d);
} /* francis_standard_form() */

static void francis_schur_standardize(us_matrix* A, us_complex* eval1, us_complex* eval2,
                                      us_eigen_francis_workspace* w) {
    const size_t N = w->size;
    float cs, sn;
    size_t top;

    /*
     * figure out where the submatrix A resides in the
     * original matrix H
     */
    top = francis_get_submatrix(w->H, A);

    /* convert 2-by-2 block to standard form */
    francis_standard_form(A, &cs, &sn);

    /* set eigenvalues */

    GSL_SET_REAL(eval1, us_matrix_float_get(A, 0, 0));
    GSL_SET_REAL(eval2, us_matrix_float_get(A, 1, 1));
    if (us_matrix_float_get(A, 1, 0) == 0.0) {
        GSL_SET_IMAG(eval1, 0.0);
        GSL_SET_IMAG(eval2, 0.0);
    } else {
        float tmp = sqrt(fabs(us_matrix_float_get(A, 0, 1)) * fabs(us_matrix_float_get(A, 1, 0)));
        GSL_SET_IMAG(eval1, tmp);
        GSL_SET_IMAG(eval2, -tmp);
    }

    if (w->compute_t) {
        us_vector_float_view xv, yv;

        if (top < (N - 2)) {
            /* transform the 2 rows of T_{23} */

            xv = us_matrix_subrow(w->H, top, top + 2, N - top - 2);
            yv = us_matrix_subrow(w->H, top + 1, top + 2, N - top - 2);
            us_blas_drot(&xv.vector, &yv.vector, cs, sn);
        }

        if (top > 0) {
            /* transform the 2 columns of T_{12} */

            xv = us_matrix_subcolumn(w->H, top, 0, top);
            yv = us_matrix_subcolumn(w->H, top + 1, 0, top);
            us_blas_drot(&xv.vector, &yv.vector, cs, sn);
        }
    } /* if (w->compute_t) */

    if (w->Zf) {
        us_vector_float_view xv, yv;

        /*
         * Accumulate the transformation in Z. Here, Z -> Z * M
         *
         * So:
         *
         * Z -> [ Z_{11} | Z_{12} U | Z_{13} ]
         *      [ Z_{21} | Z_{22} U | Z_{23} ]
         *      [ Z_{31} | Z_{32} U | Z_{33} ]
         *
         * So we just need to apply drot() to the 2 columns
         * starting at index 'top'
         */

        xv = us_matrix_column(w->Zf, top);
        yv = us_matrix_column(w->Zf, top + 1);

        us_blas_drot(&xv.vector, &yv.vector, cs, sn);
    } /* if (w->Z) */
} /* francis_schur_standardize() */

static inline void
francis_schur_decomp(us_matrix* H, us_vector_complex* eval,
                     us_eigen_francis_workspace* w) // 优化点：找到正实特征值可以停
{
    us_matrix_float_view m; /* active matrix we are working on */
    size_t N;               /* size of matrix */
    size_t q;               /* index of small subdiagonal element */
    us_complex lambda1,     /* eigenvalues */
        lambda2;

    N = H->size1;
    m = us_matrix_submatrix(H, 0, 0, N, N);

    while ((N > 2) && ((w->n_iter)++ < w->max_iterations)) {
        /*
          Search for a small subdiagonal element starting from the bottom
          of a matrix A. A small element is one that satisfies:
          |A_{i,i-1}| <= eps * (|A_{i,i}| + |A_{i-1,i-1}|)
          meanwhile the first small element that is found (starting from bottom)
          is set to zero
        */
        q = francis_search_subdiag_small_elements(&m.matrix);

        if (q == 0) {
            /*
             * no small subdiagonal element found - perform a QR
             * sweep on the active reduced hessenberg matrix
             */
            francis_qrstep(&m.matrix, w);
            continue;
        }

        /*
         * a small subdiagonal element was found - one or two eigenvalues
         * have converged or the matrix has split into two smaller matrices
         */

        if (q == (N - 1)) {
            /*
             * the last subdiagonal element of the matrix is 0 -
             * m_{NN} is a real eigenvalue
             */
            GSL_SET_COMPLEX(&lambda1, us_matrix_float_get(&m.matrix, q, q), 0.0);
            us_vector_complex_set(eval, w->n_evals, lambda1);
            w->n_evals += 1;
            w->n_iter = 0;
            --N;
            m = us_matrix_submatrix(&m.matrix, 0, 0, N, N);
        } else if (q == (N - 2)) {
            us_matrix_float_view v;

            /*
             * The bottom right 2-by-2 block of m is an eigenvalue
             * system
             */

            v = us_matrix_submatrix(&m.matrix, q, q, 2, 2);
            francis_schur_standardize(&v.matrix, &lambda1, &lambda2, w);

            us_vector_complex_set(eval, w->n_evals, lambda1);
            us_vector_complex_set(eval, w->n_evals + 1, lambda2);
            w->n_evals += 2;
            w->n_iter = 0;

            N -= 2;
            m = us_matrix_submatrix(&m.matrix, 0, 0, N, N);
        } else if (q == 1) {
            /* the first matrix element is an eigenvalue */
            GSL_SET_COMPLEX(&lambda1, us_matrix_float_get(&m.matrix, 0, 0), 0.0);
            us_vector_complex_set(eval, w->n_evals, lambda1);
            w->n_evals += 1;
            w->n_iter = 0;

            --N;
            m = us_matrix_submatrix(&m.matrix, 1, 1, N, N);
        } else if (q == 2) {
            us_matrix_float_view v;

            /* the upper left 2-by-2 block is an eigenvalue system */

            v = us_matrix_submatrix(&m.matrix, 0, 0, 2, 2);
            francis_schur_standardize(&v.matrix, &lambda1, &lambda2, w);

            us_vector_complex_set(eval, w->n_evals, lambda1);
            us_vector_complex_set(eval, w->n_evals + 1, lambda2);
            w->n_evals += 2;
            w->n_iter = 0;

            N -= 2;
            m = us_matrix_submatrix(&m.matrix, 2, 2, N, N);
        } else {
            us_matrix_float_view v;

            /*
             * There is a zero element on the subdiagonal somewhere
             * in the middle of the matrix - we can now operate
             * separately on the two submatrices split by this
             * element. q is the row index of the zero element.
             */

            /* operate on lower right (N - q)-by-(N - q) block first */
            v = us_matrix_submatrix(&m.matrix, q, q, N - q, N - q);
            francis_schur_decomp(&v.matrix, eval, w);

            /* operate on upper left q-by-q block */
            v = us_matrix_submatrix(&m.matrix, 0, 0, q, q);
            francis_schur_decomp(&v.matrix, eval, w);

            N = 0;
        }
    }
    /* handle special cases of N = 1 or 2 */

    if (N == 1) {
        GSL_SET_COMPLEX(&lambda1, us_matrix_float_get(&m.matrix, 0, 0), 0.0);
        us_vector_complex_set(eval, w->n_evals, lambda1);
        w->n_evals += 1;
        w->n_iter = 0;
    } else if (N == 2) {
        francis_schur_standardize(&m.matrix, &lambda1, &lambda2, w);
        us_vector_complex_set(eval, w->n_evals, lambda1);
        us_vector_complex_set(eval, w->n_evals + 1, lambda2);
        w->n_evals += 2;
        w->n_iter = 0;
    }

} /* francis_schur_decomp() */

int us_eigen_francis(us_matrix* H, us_vector_complex* eval, us_eigen_francis_workspace* w) {
    /* check matrix and vector sizes */
    if (H->size1 != H->size2) {
        printf("matrix must be square to compute eigenvalues");
    } else if (eval->size != H->size1) {
        printf("eigenvalue vector must match matrix size");
    } else {
        const size_t N = H->size1;
        int j;
        /*
         * Set internal parameters which depend on matrix size.
         * The Francis solver can be called with any size matrix
         * since the workspace does not depend on N.
         * Furthermore, multishift solvers which call the Francis
         * solver may need to call it with different sized matrices
         */
        w->size = N;
        w->max_iterations = 30 * N;

        /*
         * save a pointer to original matrix since francis_schur_decomp
         * is recursive
         */
        w->H = H;

        w->n_iter = 0;
        w->n_evals = 0;

        /*
         * zero out the first two subdiagonals (below the main subdiagonal)
         * needed as scratch space by the QR sweep routine
         */
        for (j = 0; j < (int)N - 3; ++j) {
            us_matrix_float_set(H, (size_t)j + 2, (size_t)j, 0.0);
            us_matrix_float_set(H, (size_t)j + 3, (size_t)j, 0.0);
        }

        if (N > 2)
            us_matrix_float_set(H, N - 1, N - 3, 0.0);

        /*
         * compute Schur decomposition of H and store eigenvalues
         * into eval
         */
        francis_schur_decomp(H, eval, w);

        if (w->n_evals != N) {
            printf("maximum iterations reached without finding all eigenvalues\n");
            return GSL_FAILED;
        }

        return GSL_SUCCESS;
    }
    return -1;
} /* us_eigen_francis() */

us_matrix* us_eigen_francis_Z(us_matrix* H, us_vector_complex* eval, us_matrix* Z,
                              us_eigen_francis_workspace* w, int* s) {
    /* set internal Z pointer so we know to accumulate transformations */
    w->Zf = Z;
    *s = us_eigen_francis(H, eval, w);
    w->Zf = NULL; // xuyaoma?
    // us_matrix_free(w->Zf);
    return Z;
} /* us_eigen_francis_Z() */

int us_eigen_nonsymm(us_matrix* A, us_vector_complex* eval, us_eigen_nonsymm_workspace* w) {
    const size_t N = A->size1;

    /* check matrix and vector sizes */
    if (N != A->size2) {
        printf("matrix must be square to compute eigenvalues");
    } else if (eval->size != N) {
        printf("eigenvalue vector must match matrix size");
    } else {
        int s = 0;
        us_linalg_hessenberg_decomp(A, w->tau); // A已经是H和v的组合体了
        // w->Z初始化为单位阵
        for (int i = 0; i < N; i++) {
            for (int j = 0; j < N; j++) {
                if (i == j) {
                    us_matrix_float_set(w->Z, i, j, 1);
                } else {
                    us_matrix_float_set(w->Z, i, j, 0);
                }
            }
        }

        us_linalg_hessenberg_unpack(A, w->tau, w->Z); // 计算U，存在w->Z里
        w->Z = us_eigen_francis_Z(A, eval, w->Z, w->francis_workspace_p, &s);
        w->n_evals = w->francis_workspace_p->n_evals;
        return s;
    }
    return -1;
} /* us_eigen_nonsymm() */

us_complex us_vector_complex_get(const us_vector_complex* v, const size_t i) {
#if GSL_RANGE_CHECK
    if (GSL_RANGE_COND(i >= v->size)) {
        us_complex zero = {{0, 0}};
        printf("index out of range\n");
    }
#endif
    return *GSL_COMPLEX_AT(v, i);
}

int us_permute_inverse(const size_t* p, float* data, const size_t stride, const size_t n) {
    size_t i, k, pk;

    for (i = 0; i < n; i++) {
        k = p[i];

        while (k > i)
            k = p[k];

        if (k < i)
            continue;

        /* Now have k == i, i.e the least in its cycle */

        pk = p[k];

        if (pk == i)
            continue;

        /* shuffle the elements of the cycle in the inverse direction */
        {
            unsigned int a;

            float t[1];

            for (a = 0; a < 1; a++)
                t[a] = data[k * stride * 1 + a];

            while (pk != i) {
                for (a = 0; a < 1; a++) {
                    float r1 = data[pk * stride * MULTIPLICITY + a];
                    data[pk * stride * MULTIPLICITY + a] = t[a];
                    t[a] = r1;
                }

                k = pk;
                pk = p[k];
            };

            for (a = 0; a < MULTIPLICITY; a++)
                data[pk * stride * MULTIPLICITY + a] = t[a];
        }
    }

    return GSL_SUCCESS;
}

void cblas_scopy(const int N, const float* X, const int incX, float* Y, const int incY) {
#define BASE float
    int i;
    int ix = OFFSET(N, incX);
    int iy = OFFSET(N, incY);

    for (i = 0; i < N; i++) {
        Y[iy] = X[ix];
        ix += incX;
        iy += incY;
    }
#undef BASE
}

int us_blas_scopy(const us_vector_float* X, us_vector_float* Y) {
    if (X->size == Y->size) {
        cblas_scopy(INT(X->size), X->data, INT(X->stride), Y->data, INT(Y->stride));
        return GSL_SUCCESS;
    } else {
        printf("invalid length");
    }
    return -1;
}

int us_matrix_memcpy(us_matrix_float* dest, const us_matrix_float* src) {
    const size_t src_size1 = src->size1;
    const size_t src_size2 = src->size2;
    const size_t dest_size1 = dest->size1;
    const size_t dest_size2 = dest->size2;
    size_t i;

    if (src_size1 != dest_size1 || src_size2 != dest_size2) {
        printf("matrix sizes are different");
    }
    for (i = 0; i < src_size1; ++i) {
        us_vector_float_view sv = us_matrix_row((us_matrix*)src, i);
        us_vector_float_view dv = us_matrix_row(dest, i);

        us_blas_scopy(&sv.vector, &dv.vector);
    }

    return GSL_SUCCESS;
}

int us_vector_float_memcpy(us_vector* dest, const us_vector* src) {
    const size_t src_size = src->size;
    const size_t dest_size = dest->size;

    if (src_size != dest_size) {
        printf("vector lengths are not equal");
    }

#if defined(BASE_DOUBLE)

    us_blas_dcopy(src, dest);

#elif defined(BASE_FLOAT)

    us_blas_scopy(src, dest);

#elif defined(BASE_GSL_COMPLEX)

    us_blas_zcopy(src, dest);

#elif defined(BASE_GSL_COMPLEX_FLOAT)

    us_blas_ccopy(src, dest);

#else

    {
        const size_t src_stride = src->stride;
        const size_t dest_stride = dest->stride;
        size_t j;

        for (j = 0; j < src_size; j++) {
            size_t k;

            for (k = 0; k < MULTIPLICITY; k++) {
                dest->data[MULTIPLICITY * dest_stride * j + k] =
                    src->data[MULTIPLICITY * src_stride * j + k];
            }
        }
    }

#endif

    return GSL_SUCCESS;
}

us_matrix_float_view us_matrix_submatrix(us_matrix* m, const size_t i, const size_t j,
                                         const size_t n1, const size_t n2) {
    us_matrix_float_view view = NULL_MATRIX_VIEW;

    if (i >= m->size1) {
        printf("row index is out of range");
    } else if (j >= m->size2) {
        printf("column index is out of range");
    } else if (i + n1 > m->size1) {
        printf("first dimension overflows, raw:%zu, target:%zu\n", m->size1, i + n1);
        return view;
    } else if (j + n2 > m->size2) {
        printf("second dimension overflows matrix");
    }

    {
        us_matrix s = NULL_MATRIX;

        s.data = m->data + MULTIPLICITY * (i * m->tda + j);
        s.size1 = n1;
        s.size2 = n2;
        s.tda = m->tda;
        s.block = m->block;
        s.owner = 0;

        view.matrix = s;
        return view;
    }
}

us_vector_float_view us_matrix_subcolumn(us_matrix* m, const size_t j, const size_t offset,
                                         const size_t n) {
    us_vector_float_view view = NULL_VECTOR_VIEW;

    if (j >= m->size2) {
        printf("column index is out of range");
    } else if (n == 0) {
        printf("vector length n must be positive integer");
    } else if (offset + n > m->size1) {
        printf("dimension n overflows matrix");
    }

    {
        us_vector v = NULL_VECTOR;

        v.data = m->data + MULTIPLICITY * (offset * m->tda + j);
        v.size = n;
        v.stride = m->tda;
        v.block = m->block;
        v.owner = 0;

        view.vector = v;
        return view;
    }
}

us_vector_float_view us_matrix_subrow(us_matrix* m, const size_t i, const size_t offset,
                                      const size_t n) {
    us_vector_float_view view = NULL_VECTOR_VIEW;

    if (i >= m->size1) {
        printf("row index is out of range");
    } else if (n == 0) {
        printf("vector length n must be positive integer");
    } else if (offset + n > m->size2) {
        printf("dimension n overflows matrix");
    }

    {
        us_vector v = NULL_VECTOR;

        v.data = m->data + MULTIPLICITY * (i * m->tda + offset);
        v.size = n;
        v.stride = 1;
        v.block = m->block;
        v.owner = 0;

        view.vector = v;
        return view;
    }
}

int cblas_idamax(const int N, const float* X, const int incX) {
    float max = 0.0;
    int ix = 0;
    int i;
    int result = 0;

    if (incX <= 0) {
        return 0;
    }

    for (i = 0; i < N; i++) {
        if (fabs(X[ix]) > max) {
            max = fabs(X[ix]);
            result = i;
        }
        ix += incX;
    }
    return result;
}

int us_blas_idamax(const us_vector* X) {
    return cblas_idamax(INT(X->size), X->data, INT(X->stride));
}

us_vector_float_view us_matrix_row(us_matrix* m, const size_t i) {
    us_vector_float_view view = NULL_VECTOR_VIEW;

    if (i >= m->size1) {
        printf("row index is out of range");
    }

    {
        us_vector v = NULL_VECTOR;

        v.data = m->data + i * MULTIPLICITY * m->tda;
        v.size = m->size2;
        v.stride = 1;
        v.block = m->block;
        v.owner = 0;

        view.vector = v;
        return view;
    }
}

void cblas_dswap(const int N, float* X, const int incX, float* Y, const int incY) {
    int i;
    int ix = OFFSET(N, incX);
    int iy = OFFSET(N, incY);
    for (i = 0; i < N; i++) {
        const float tmp = X[ix];
        X[ix] = Y[iy];
        Y[iy] = tmp;
        ix += incX;
        iy += incY;
    }
}

int us_blas_dswap(us_vector* X, us_vector* Y) {
    if (X->size == Y->size) {
        cblas_dswap(INT(X->size), X->data, INT(X->stride), Y->data, INT(Y->stride));
        return GSL_SUCCESS;
    } else {
        printf("invalid length");
    };
    return -1;
}

void cblas_dscal(const int N, const float alpha, float* X, const int incX) {
    int i;
    int ix = 0;

    if (incX <= 0) {
        return;
    }

    for (i = 0; i < N; i++) {
        X[ix] *= alpha;
        ix += incX;
    }
}

void us_blas_dscal(float alpha, us_vector* X) {
    cblas_dscal(INT(X->size), alpha, X->data, INT(X->stride));
}

void cblas_dger(const enum CBLAS_ORDER order, const int M, const int N, const float alpha,
                const float* X, const int incX, const float* Y, const int incY, float* A,
                const int lda) {
    int i, j;
    // CHECK_ARGS10(SD_GER,order,M,N,alpha,X,incX,Y,incY,A,lda);

    if (order == CblasRowMajor) {
        int ix = OFFSET(M, incX);
        for (i = 0; i < M; i++) {
            const float tmp = alpha * X[ix];
            int jy = OFFSET(N, incY);
            for (j = 0; j < N; j++) {
                A[lda * i + j] += Y[jy] * tmp;
                jy += incY;
            }
            ix += incX;
        }
    } else if (order == CblasColMajor) {
        int jy = OFFSET(N, incY);
        for (j = 0; j < N; j++) {
            const float tmp = alpha * Y[jy];
            int ix = OFFSET(M, incX);
            for (i = 0; i < M; i++) {
                A[i + lda * j] += X[ix] * tmp;
                ix += incX;
            }
            jy += incY;
        }
    } else {
        printf("unrecognized operation");
    }
}

int us_blas_dger(float alpha, const us_vector* X, const us_vector* Y, us_matrix* A) {
    const size_t M = A->size1;
    const size_t N = A->size2;

    if (X->size == M && Y->size == N) {
        cblas_dger(CblasRowMajor, INT(M), INT(N), alpha, X->data, INT(X->stride), Y->data,
                   INT(Y->stride), A->data, INT(A->tda));
        return GSL_SUCCESS;
    } else {
        printf("invalid length");
    }
    return -1;
}

float* us_matrix_ptr(us_matrix* m, const size_t i, const size_t j) {
#if GSL_RANGE_CHECK
    if (GSL_RANGE_COND(1)) {
        if (i >= m->size1) {
            printf("first index out of range");
        } else if (j >= m->size2) {
            printf("second index out of range");
        }
    }
#endif
    return (float*)(m->data + (i * m->tda + j));
}

static int LU_decomp_L2(us_matrix* A, us_vector* ipiv) {
    const size_t M = A->size1;
    const size_t N = A->size2;
    const size_t minMN = GSL_MIN(M, N);

    if (ipiv->size != minMN) {
        printf("ipiv length must equal MIN(M,N)");
    } else {
        size_t i, j;

        for (j = 0; j < minMN; ++j) {
            /* find maximum in the j-th column */
            us_vector_float_view v = us_matrix_subcolumn(A, j, j, M - j);
            size_t j_pivot = j + us_blas_idamax(&v.vector);
            us_vector_float_view v1, v2;

            us_vector_float_set(ipiv, j, j_pivot);

            if (j_pivot != j) {
                /* swap rows j and j_pivot */
                v1 = us_matrix_row(A, j);
                v2 = us_matrix_row(A, j_pivot);
                us_blas_dswap(&v1.vector, &v2.vector);
            }

            if (j < M - 1) {
                float Ajj = us_matrix_float_get(A, j, j);

                if (fabs(Ajj) >= GSL_FLT_MIN) {
                    v1 = us_matrix_subcolumn(A, j, j + 1, M - j - 1);
                    us_blas_dscal(1.0 / Ajj, &v1.vector);
                } else {
                    for (i = 1; i < M - j; ++i) {
                        float* ptr = us_matrix_ptr(A, j + i, j);
                        *ptr /= Ajj;
                    }
                }
            }

            if (j < minMN - 1) {
                us_matrix_float_view A22 =
                    us_matrix_submatrix(A, j + 1, j + 1, M - j - 1, N - j - 1);
                v1 = us_matrix_subcolumn(A, j, j + 1, M - j - 1);
                v2 = us_matrix_subrow(A, j, j + 1, N - j - 1);

                us_blas_dger(-1.0, &v1.vector, &v2.vector, &A22.matrix);
            }
        }

        return GSL_SUCCESS;
    }
    return -1;
}

us_vector_float_view us_vector_float_subvector(us_vector* v, size_t offset, size_t n) {
    us_vector_float_view view = NULL_VECTOR_VIEW;

    if (offset + (n > 0 ? n - 1 : 0) >= v->size) {
        printf("view would extend past end of vector");
    }

    {
        us_vector s = NULL_VECTOR;

        s.data = v->data + MULTIPLICITY * v->stride * offset;
        s.size = n;
        s.stride = v->stride;
        s.block = v->block;
        s.owner = 0;

        view.vector = s;
        return view;
    }
}

static int apply_pivots(us_matrix* A, const us_vector* ipiv) {
    if (A->size1 < ipiv->size) {
        printf("matrix does not match pivot vector");
    } else {
        size_t i;

        for (i = 0; i < ipiv->size; ++i) {
            size_t pi = us_vector_float_get(ipiv, i);

            if (i != pi) {
                /* swap rows i and pi */
                us_vector_float_view v1 = us_matrix_row(A, i);
                us_vector_float_view v2 = us_matrix_row(A, pi);
                us_blas_dswap(&v1.vector, &v2.vector);
            }
        }

        return GSL_SUCCESS;
    }
    return -1;
}

void cblas_dtrsm(const enum CBLAS_ORDER Order, const enum CBLAS_SIDE Side,
                 const enum CBLAS_UPLO Uplo, const enum CBLAS_TRANSPOSE TransA,
                 const enum CBLAS_DIAG Diag, const int M, const int N, const float alpha,
                 const float* A, const int lda, float* B, const int ldb) {
    int i, j, k;
    int n1, n2;

    const int nonunit = (Diag == CblasNonUnit);
    int side, uplo, trans;

    // CHECK_ARGS12(TRSM,Order,Side,Uplo,TransA,Diag,M,N,alpha,A,lda,B,ldb);

    if (Order == CblasRowMajor) {
        n1 = M;
        n2 = N;
        side = Side;
        uplo = Uplo;
        trans = (TransA == CblasConjTrans) ? CblasTrans : TransA;
    } else {
        n1 = N;
        n2 = M;
        side = (Side == CblasLeft) ? CblasRight : CblasLeft;
        uplo = (Uplo == CblasUpper) ? CblasLower : CblasUpper;
        trans = (TransA == CblasConjTrans) ? CblasTrans : TransA;
    }

    if (side == CblasLeft && uplo == CblasUpper && trans == CblasNoTrans) {
        /* form  B := alpha * inv(TriU(A)) *B */

        if (alpha != 1.0) {
            for (i = 0; i < n1; i++) {
                for (j = 0; j < n2; j++) {
                    B[ldb * i + j] *= alpha;
                }
            }
        }

        for (i = n1; i > 0 && i--;) {
            if (nonunit) {
                float Aii = A[lda * i + i];
                for (j = 0; j < n2; j++) {
                    B[ldb * i + j] /= Aii;
                }
            }

            for (k = 0; k < i; k++) {
                const float Aki = A[k * lda + i];
                for (j = 0; j < n2; j++) {
                    B[ldb * k + j] -= Aki * B[ldb * i + j];
                }
            }
        }
    } else if (side == CblasLeft && uplo == CblasUpper && trans == CblasTrans) {
        /* form  B := alpha * inv(TriU(A))' *B */

        if (alpha != 1.0) {
            for (i = 0; i < n1; i++) {
                for (j = 0; j < n2; j++) {
                    B[ldb * i + j] *= alpha;
                }
            }
        }

        for (i = 0; i < n1; i++) {
            if (nonunit) {
                float Aii = A[lda * i + i];
                for (j = 0; j < n2; j++) {
                    B[ldb * i + j] /= Aii;
                }
            }

            for (k = i + 1; k < n1; k++) {
                const float Aik = A[i * lda + k];
                for (j = 0; j < n2; j++) {
                    B[ldb * k + j] -= Aik * B[ldb * i + j];
                }
            }
        }
    } else if (side == CblasLeft && uplo == CblasLower && trans == CblasNoTrans) {
        /* form  B := alpha * inv(TriL(A))*B */

        if (alpha != 1.0) {
            for (i = 0; i < n1; i++) {
                for (j = 0; j < n2; j++) {
                    B[ldb * i + j] *= alpha;
                }
            }
        }

        for (i = 0; i < n1; i++) {
            if (nonunit) {
                float Aii = A[lda * i + i];
                for (j = 0; j < n2; j++) {
                    B[ldb * i + j] /= Aii;
                }
            }

            for (k = i + 1; k < n1; k++) {
                const float Aki = A[k * lda + i];
                for (j = 0; j < n2; j++) {
                    B[ldb * k + j] -= Aki * B[ldb * i + j];
                }
            }
        }
    } else if (side == CblasLeft && uplo == CblasLower && trans == CblasTrans) {
        /* form  B := alpha * TriL(A)' *B */

        if (alpha != 1.0) {
            for (i = 0; i < n1; i++) {
                for (j = 0; j < n2; j++) {
                    B[ldb * i + j] *= alpha;
                }
            }
        }

        for (i = n1; i > 0 && i--;) {
            if (nonunit) {
                float Aii = A[lda * i + i];
                for (j = 0; j < n2; j++) {
                    B[ldb * i + j] /= Aii;
                }
            }

            for (k = 0; k < i; k++) {
                const float Aik = A[i * lda + k];
                for (j = 0; j < n2; j++) {
                    B[ldb * k + j] -= Aik * B[ldb * i + j];
                }
            }
        }
    } else if (side == CblasRight && uplo == CblasUpper && trans == CblasNoTrans) {
        /* form  B := alpha * B * inv(TriU(A)) */

        if (alpha != 1.0) {
            for (i = 0; i < n1; i++) {
                for (j = 0; j < n2; j++) {
                    B[ldb * i + j] *= alpha;
                }
            }
        }

        for (i = 0; i < n1; i++) {
            for (j = 0; j < n2; j++) {
                if (nonunit) {
                    float Ajj = A[lda * j + j];
                    B[ldb * i + j] /= Ajj;
                }

                {
                    float Bij = B[ldb * i + j];
                    for (k = j + 1; k < n2; k++) {
                        B[ldb * i + k] -= A[j * lda + k] * Bij;
                    }
                }
            }
        }
    } else if (side == CblasRight && uplo == CblasUpper && trans == CblasTrans) {
        /* form  B := alpha * B * inv(TriU(A))' */

        if (alpha != 1.0) {
            for (i = 0; i < n1; i++) {
                for (j = 0; j < n2; j++) {
                    B[ldb * i + j] *= alpha;
                }
            }
        }

        for (i = 0; i < n1; i++) {
            for (j = n2; j > 0 && j--;) {
                if (nonunit) {
                    float Ajj = A[lda * j + j];
                    B[ldb * i + j] /= Ajj;
                }

                {
                    float Bij = B[ldb * i + j];
                    for (k = 0; k < j; k++) {
                        B[ldb * i + k] -= A[k * lda + j] * Bij;
                    }
                }
            }
        }
    } else if (side == CblasRight && uplo == CblasLower && trans == CblasNoTrans) {
        /* form  B := alpha * B * inv(TriL(A)) */

        if (alpha != 1.0) {
            for (i = 0; i < n1; i++) {
                for (j = 0; j < n2; j++) {
                    B[ldb * i + j] *= alpha;
                }
            }
        }

        for (i = 0; i < n1; i++) {
            for (j = n2; j > 0 && j--;) {
                if (nonunit) {
                    float Ajj = A[lda * j + j];
                    B[ldb * i + j] /= Ajj;
                }

                {
                    float Bij = B[ldb * i + j];
                    for (k = 0; k < j; k++) {
                        B[ldb * i + k] -= A[j * lda + k] * Bij;
                    }
                }
            }
        }
    } else if (side == CblasRight && uplo == CblasLower && trans == CblasTrans) {
        /* form  B := alpha * B * inv(TriL(A))' */

        if (alpha != 1.0) {
            for (i = 0; i < n1; i++) {
                for (j = 0; j < n2; j++) {
                    B[ldb * i + j] *= alpha;
                }
            }
        }

        for (i = 0; i < n1; i++) {
            for (j = 0; j < n2; j++) {
                if (nonunit) {
                    float Ajj = A[lda * j + j];
                    B[ldb * i + j] /= Ajj;
                }

                {
                    float Bij = B[ldb * i + j];
                    for (k = j + 1; k < n2; k++) {
                        B[ldb * i + k] -= A[k * lda + j] * Bij;
                    }
                }
            }
        }
    } else {
        printf("unrecognized operation");
    }
}

int us_blas_dtrsm(CBLAS_SIDE_t Side, CBLAS_UPLO_t Uplo, CBLAS_TRANSPOSE_t TransA, CBLAS_DIAG_t Diag,
                  float alpha, const us_matrix* A, us_matrix* B) {
    const size_t M = B->size1;
    const size_t N = B->size2;
    const size_t MA = A->size1;
    const size_t NA = A->size2;

    if (MA != NA) {
        printf("matrix A must be square");
    }

    if ((Side == CblasLeft && M == MA) || (Side == CblasRight && N == MA)) {
        cblas_dtrsm(CblasRowMajor, Side, Uplo, TransA, Diag, INT(M), INT(N), alpha, A->data,
                    INT(A->tda), B->data, INT(B->tda));
        return GSL_SUCCESS;
    } else {
        printf("invalid length");
    }
    return -1;
}

void cblas_dgemm(const enum CBLAS_ORDER Order, const enum CBLAS_TRANSPOSE TransA,
                 const enum CBLAS_TRANSPOSE TransB, const int M, const int N, const int K,
                 const float alpha, const float* A, const int lda, const float* B, const int ldb,
                 const float beta, float* C, const int ldc) {
    int i, j, k;
    int n1, n2;
    int ldf, ldg;
    int TransF, TransG;
    const float *F, *G;

    // CHECK_ARGS14(GEMM,Order,TransA,TransB,M,N,K,alpha,A,lda,B,ldb,beta,C,ldc);

    if (alpha == 0.0 && beta == 1.0)
        return;

    if (Order == CblasRowMajor) {
        n1 = M;
        n2 = N;
        F = A;
        ldf = lda;
        TransF = (TransA == CblasConjTrans) ? CblasTrans : TransA;
        G = B;
        ldg = ldb;
        TransG = (TransB == CblasConjTrans) ? CblasTrans : TransB;
    } else {
        n1 = N;
        n2 = M;
        F = B;
        ldf = ldb;
        TransF = (TransB == CblasConjTrans) ? CblasTrans : TransB;
        G = A;
        ldg = lda;
        TransG = (TransA == CblasConjTrans) ? CblasTrans : TransA;
    }

    /* form  y := beta*y */
    if (beta == 0.0) {
        for (i = 0; i < n1; i++) {
            for (j = 0; j < n2; j++) {
                C[ldc * i + j] = 0.0;
            }
        }
    } else if (beta != 1.0) {
        for (i = 0; i < n1; i++) {
            for (j = 0; j < n2; j++) {
                C[ldc * i + j] *= beta;
            }
        }
    }

    if (alpha == 0.0)
        return;

    if (TransF == CblasNoTrans && TransG == CblasNoTrans) {
        /* form  C := alpha*A*B + C */

        for (k = 0; k < K; k++) {
            for (i = 0; i < n1; i++) {
                const float temp = alpha * F[ldf * i + k];
                if (temp != 0.0) {
                    for (j = 0; j < n2; j++) {
                        C[ldc * i + j] += temp * G[ldg * k + j];
                    }
                }
            }
        }
    } else if (TransF == CblasNoTrans && TransG == CblasTrans) {
        /* form  C := alpha*A*B' + C */

        for (i = 0; i < n1; i++) {
            for (j = 0; j < n2; j++) {
                float temp = 0.0;
                for (k = 0; k < K; k++) {
                    temp += F[ldf * i + k] * G[ldg * j + k];
                }
                C[ldc * i + j] += alpha * temp;
            }
        }
    } else if (TransF == CblasTrans && TransG == CblasNoTrans) {
        for (k = 0; k < K; k++) {
            for (i = 0; i < n1; i++) {
                const float temp = alpha * F[ldf * k + i];
                if (temp != 0.0) {
                    for (j = 0; j < n2; j++) {
                        C[ldc * i + j] += temp * G[ldg * k + j];
                    }
                }
            }
        }
    } else if (TransF == CblasTrans && TransG == CblasTrans) {
        for (i = 0; i < n1; i++) {
            for (j = 0; j < n2; j++) {
                float temp = 0.0;
                for (k = 0; k < K; k++) {
                    temp += F[ldf * k + i] * G[ldg * j + k];
                }
                C[ldc * i + j] += alpha * temp;
            }
        }
    } else {
        printf("unrecognized operation");
    }
}

int us_blas_dgemm(CBLAS_TRANSPOSE_t TransA, CBLAS_TRANSPOSE_t TransB, float alpha,
                  const us_matrix* A, const us_matrix* B, float beta, us_matrix* C) {
    const size_t M = C->size1;
    const size_t N = C->size2;
    const size_t MA = (TransA == CblasNoTrans) ? A->size1 : A->size2;
    const size_t NA = (TransA == CblasNoTrans) ? A->size2 : A->size1;
    const size_t MB = (TransB == CblasNoTrans) ? B->size1 : B->size2;
    const size_t NB = (TransB == CblasNoTrans) ? B->size2 : B->size1;

    if (M == MA && N == NB && NA == MB) /* [MxN] = [MAxNA][MBxNB] */
    {
        cblas_dgemm(CblasRowMajor, TransA, TransB, INT(M), INT(N), INT(NA), alpha, A->data,
                    INT(A->tda), B->data, INT(B->tda), beta, C->data, INT(C->tda));
        return GSL_SUCCESS;
    } else {
        printf("invalid length");
    }
    return -1;
}

static int LU_decomp_L3(us_matrix* A, us_vector* ipiv) {
    const size_t M = A->size1;
    const size_t N = A->size2;

    if (M < N) {
        printf("matrix must have M >= N");
    } else if (ipiv->size != GSL_MIN(M, N)) {
        printf("ipiv length must equal MIN(M,N)");
    } else if (N <= CROSSOVER_LU) {
        /* use Level 2 algorithm */
        return LU_decomp_L2(A, ipiv);
    } else {
        /*
         * partition matrix:
         *
         *       N1  N2
         * N1  [ A11 A12 ]
         * M2  [ A21 A22 ]
         *
         * and
         *      N1  N2
         * M  [ AL  AR  ]
         */
        int status;
        const size_t N1 = GSL_LINALG_SPLIT(N);
        const size_t N2 = N - N1;
        const size_t M2 = M - N1;
        us_matrix_float_view A11 = us_matrix_submatrix(A, 0, 0, N1, N1);
        us_matrix_float_view A12 = us_matrix_submatrix(A, 0, N1, N1, N2);
        us_matrix_float_view A21 = us_matrix_submatrix(A, N1, 0, M2, N1);
        us_matrix_float_view A22 = us_matrix_submatrix(A, N1, N1, M2, N2);

        us_matrix_float_view AL = us_matrix_submatrix(A, 0, 0, M, N1);
        us_matrix_float_view AR = us_matrix_submatrix(A, 0, N1, M, N2);

        /*
         * partition ipiv = [ ipiv1 ] N1
         *                  [ ipiv2 ] N2
         */
        us_vector_float_view ipiv1 = us_vector_float_subvector(ipiv, 0, N1);
        us_vector_float_view ipiv2 = us_vector_float_subvector(ipiv, N1, N2);

        size_t i;

        /* recursion on (AL, ipiv1) */
        status = LU_decomp_L3(&AL.matrix, &ipiv1.vector);
        if (status)
            return status;

        /* apply ipiv1 to AR */
        apply_pivots(&AR.matrix, &ipiv1.vector);

        /* A12 = A11^{-1} A12 */
        us_blas_dtrsm(CblasLeft, CblasLower, CblasNoTrans, CblasUnit, 1.0, &A11.matrix,
                      &A12.matrix);

        /* A22 = A22 - A21 * A12 */
        us_blas_dgemm(CblasNoTrans, CblasNoTrans, -1.0, &A21.matrix, &A12.matrix, 1.0, &A22.matrix);

        /* recursion on (A22, ipiv2) */
        status = LU_decomp_L3(&A22.matrix, &ipiv2.vector);
        if (status)
            return status;

        /* apply pivots to A21 */
        apply_pivots(&A21.matrix, &ipiv2.vector);

        /* shift pivots */
        for (i = 0; i < N2; ++i) {
            float* ptr = us_vector_float_ptr(&ipiv2.vector, i);
            *ptr += N1;
        }

        return GSL_SUCCESS;
    }
    return -1;
}

void permutation_init(us_permutation* p) {
    const size_t n = p->size;
    size_t i;

    for (i = 0; i < n; i++) {
        p->data[i] = i;
    }
}

int us_linalg_LU_decomp(us_matrix* A, us_permutation* p, int* signum) {
    const size_t M = A->size1;

    if (p->size != M) {
        printf("permutation length must match matrix size1\n");
    } else {
        int status;
        const size_t N = A->size2;
        const size_t minMN = GSL_MIN(M, N);
        us_vector_float* ipiv =
            us_vector_float_alloc(minMN); // 这里可以是int，它就存个行列号，待优化
        us_matrix_float_view AL = us_matrix_submatrix(A, 0, 0, M, minMN);
        size_t i;

        status = LU_decomp_L3(&AL.matrix, ipiv);

        /* process remaining right matrix */
        if (M < N) {
            us_matrix_float_view AR = us_matrix_submatrix(A, 0, M, M, N - M);

            /* apply pivots to AR */
            apply_pivots(&AR.matrix, ipiv);

            /* AR = AL^{-1} AR */
            us_blas_dtrsm(CblasLeft, CblasLower, CblasNoTrans, CblasUnit, 1.0, &AL.matrix,
                          &AR.matrix);
        }

        /* convert ipiv array to permutation */

        permutation_init(p);
        *signum = 1;

        for (i = 0; i < minMN; ++i) {
            unsigned int pivi = us_vector_float_get(ipiv, i);

            if (p->data[pivi] != p->data[i]) {
                size_t tmp = p->data[pivi];
                p->data[pivi] = p->data[i];
                p->data[i] = tmp;
                *signum = -(*signum);
            }
        }

        us_vector_float_free(ipiv);
        return status;
    }
    return -1;
}

static int triangular_singular(const us_matrix* T) {
    size_t i;

    for (i = 0; i < T->size1; ++i) {
        float Tii = us_matrix_float_get(T, i, i);
        if (Tii == 0.0)
            return GSL_ESING;
    }

    return GSL_SUCCESS;
}

void cblas_dtrmv(const enum CBLAS_ORDER order, const enum CBLAS_UPLO Uplo,
                 const enum CBLAS_TRANSPOSE TransA, const enum CBLAS_DIAG Diag, const int N,
                 const float* A, const int lda, float* X, const int incX) {
    int i, j;

    const int nonunit = (Diag == CblasNonUnit);
    const int Trans = (TransA != CblasConjTrans) ? TransA : CblasTrans;

    // CHECK_ARGS9(TRMV,order,Uplo,TransA,Diag,N,A,lda,X,incX);

    if ((order == CblasRowMajor && Trans == CblasNoTrans && Uplo == CblasUpper) ||
        (order == CblasColMajor && Trans == CblasTrans && Uplo == CblasLower)) {
        /* form  x := A*x */

        int ix = OFFSET(N, incX);
        for (i = 0; i < N; i++) {
            float temp = 0.0;
            const int j_min = i + 1;
            const int j_max = N;
            int jx = OFFSET(N, incX) + j_min * incX;
            for (j = j_min; j < j_max; j++) {
                temp += X[jx] * A[lda * i + j];
                jx += incX;
            }
            if (nonunit) {
                X[ix] = temp + X[ix] * A[lda * i + i];
            } else {
                X[ix] += temp;
            }
            ix += incX;
        }
    } else if ((order == CblasRowMajor && Trans == CblasNoTrans && Uplo == CblasLower) ||
               (order == CblasColMajor && Trans == CblasTrans && Uplo == CblasUpper)) {
        int ix = OFFSET(N, incX) + (N - 1) * incX;
        for (i = N; i > 0 && i--;) {
            float temp = 0.0;
            const int j_min = 0;
            const int j_max = i;
            int jx = OFFSET(N, incX) + j_min * incX;
            for (j = j_min; j < j_max; j++) {
                temp += X[jx] * A[lda * i + j];
                jx += incX;
            }
            if (nonunit) {
                X[ix] = temp + X[ix] * A[lda * i + i];
            } else {
                X[ix] += temp;
            }
            ix -= incX;
        }
    } else if ((order == CblasRowMajor && Trans == CblasTrans && Uplo == CblasUpper) ||
               (order == CblasColMajor && Trans == CblasNoTrans && Uplo == CblasLower)) {
        /* form  x := A'*x */
        int ix = OFFSET(N, incX) + (N - 1) * incX;
        for (i = N; i > 0 && i--;) {
            float temp = 0.0;
            const int j_min = 0;
            const int j_max = i;
            int jx = OFFSET(N, incX) + j_min * incX;
            for (j = j_min; j < j_max; j++) {
                temp += X[jx] * A[lda * j + i];
                jx += incX;
            }
            if (nonunit) {
                X[ix] = temp + X[ix] * A[lda * i + i];
            } else {
                X[ix] += temp;
            }
            ix -= incX;
        }
    } else if ((order == CblasRowMajor && Trans == CblasTrans && Uplo == CblasLower) ||
               (order == CblasColMajor && Trans == CblasNoTrans && Uplo == CblasUpper)) {
        int ix = OFFSET(N, incX);
        for (i = 0; i < N; i++) {
            float temp = 0.0;
            const int j_min = i + 1;
            const int j_max = N;
            int jx = OFFSET(N, incX) + (i + 1) * incX;
            for (j = j_min; j < j_max; j++) {
                temp += X[jx] * A[lda * j + i];
                jx += incX;
            }
            if (nonunit) {
                X[ix] = temp + X[ix] * A[lda * i + i];
            } else {
                X[ix] += temp;
            }
            ix += incX;
        }
    } else {
        printf("unrecognized operation");
    }
}

int us_blas_dtrmv(CBLAS_UPLO_t Uplo, CBLAS_TRANSPOSE_t TransA, CBLAS_DIAG_t Diag,
                  const us_matrix* A, us_vector* X) {
    const size_t M = A->size1;
    const size_t N = A->size2;

    if (M != N) {
        printf("matrix must be square");
    } else if (N != X->size) {
        printf("invalid length");
    }

    cblas_dtrmv(CblasRowMajor, Uplo, TransA, Diag, INT(N), A->data, INT(A->tda), X->data,
                INT(X->stride));
    return GSL_SUCCESS;
}

static int triangular_inverse_L2(CBLAS_UPLO_t Uplo, CBLAS_DIAG_t Diag, us_matrix* T) {
    const size_t N = T->size1;

    if (N != T->size2) {
        printf("matrix must be square");
    } else {
        us_matrix_float_view m;
        us_vector_float_view v;
        size_t i;

        if (Uplo == CblasUpper) {
            for (i = 0; i < N; ++i) {
                float aii;

                if (Diag == CblasNonUnit) {
                    float* Tii = us_matrix_ptr(T, i, i);
                    *Tii = 1.0 / *Tii;
                    aii = -(*Tii);
                } else {
                    aii = -1.0;
                }

                if (i > 0) {
                    m = us_matrix_submatrix(T, 0, 0, i, i);
                    v = us_matrix_subcolumn(T, i, 0, i);

                    us_blas_dtrmv(CblasUpper, CblasNoTrans, Diag, &m.matrix, &v.vector);

                    us_blas_dscal(aii, &v.vector);
                }
            } /* for (i = 0; i < N; ++i) */
        } else {
            for (i = 0; i < N; ++i) {
                float ajj;
                size_t j = N - i - 1;

                if (Diag == CblasNonUnit) {
                    float* Tjj = us_matrix_ptr(T, j, j);
                    *Tjj = 1.0 / *Tjj;
                    ajj = -(*Tjj);
                } else {
                    ajj = -1.0;
                }

                if (j < N - 1) {
                    m = us_matrix_submatrix(T, j + 1, j + 1, N - j - 1, N - j - 1);
                    v = us_matrix_subcolumn(T, j, j + 1, N - j - 1);

                    us_blas_dtrmv(CblasLower, CblasNoTrans, Diag, &m.matrix, &v.vector);

                    us_blas_dscal(ajj, &v.vector);
                }
            } /* for (i = 0; i < N; ++i) */
        }

        return GSL_SUCCESS;
    }
    return -1;
}

void cblas_dtrmm(const enum CBLAS_ORDER Order, const enum CBLAS_SIDE Side,
                 const enum CBLAS_UPLO Uplo, const enum CBLAS_TRANSPOSE TransA,
                 const enum CBLAS_DIAG Diag, const int M, const int N, const float alpha,
                 const float* A, const int lda, float* B, const int ldb) {
    int i, j, k;
    int n1, n2;

    const int nonunit = (Diag == CblasNonUnit);
    int side, uplo, trans;

    // CHECK_ARGS12(TRMM,Order,Side,Uplo,TransA,Diag,M,N,alpha,A,lda,B,ldb);

    if (Order == CblasRowMajor) {
        n1 = M;
        n2 = N;
        side = Side;
        uplo = Uplo;
        trans = (TransA == CblasConjTrans) ? CblasTrans : TransA;
    } else {
        n1 = N;
        n2 = M;
        side = (Side == CblasLeft) ? CblasRight : CblasLeft;
        uplo = (Uplo == CblasUpper) ? CblasLower : CblasUpper;
        trans = (TransA == CblasConjTrans) ? CblasTrans : TransA;
    }

    if (side == CblasLeft && uplo == CblasUpper && trans == CblasNoTrans) {
        /* form  B := alpha * TriU(A)*B */

        for (i = 0; i < n1; i++) {
            for (j = 0; j < n2; j++) {
                float temp = 0.0;

                if (nonunit) {
                    temp = A[i * lda + i] * B[i * ldb + j];
                } else {
                    temp = B[i * ldb + j];
                }

                for (k = i + 1; k < n1; k++) {
                    temp += A[lda * i + k] * B[k * ldb + j];
                }

                B[ldb * i + j] = alpha * temp;
            }
        }
    } else if (side == CblasLeft && uplo == CblasUpper && trans == CblasTrans) {
        /* form  B := alpha * (TriU(A))' *B */

        for (i = n1; i > 0 && i--;) {
            for (j = 0; j < n2; j++) {
                float temp = 0.0;

                for (k = 0; k < i; k++) {
                    temp += A[lda * k + i] * B[k * ldb + j];
                }

                if (nonunit) {
                    temp += A[i * lda + i] * B[i * ldb + j];
                } else {
                    temp += B[i * ldb + j];
                }

                B[ldb * i + j] = alpha * temp;
            }
        }
    } else if (side == CblasLeft && uplo == CblasLower && trans == CblasNoTrans) {
        /* form  B := alpha * TriL(A)*B */

        for (i = n1; i > 0 && i--;) {
            for (j = 0; j < n2; j++) {
                float temp = 0.0;

                for (k = 0; k < i; k++) {
                    temp += A[lda * i + k] * B[k * ldb + j];
                }

                if (nonunit) {
                    temp += A[i * lda + i] * B[i * ldb + j];
                } else {
                    temp += B[i * ldb + j];
                }

                B[ldb * i + j] = alpha * temp;
            }
        }
    } else if (side == CblasLeft && uplo == CblasLower && trans == CblasTrans) {
        /* form  B := alpha * TriL(A)' *B */

        for (i = 0; i < n1; i++) {
            for (j = 0; j < n2; j++) {
                float temp = 0.0;

                if (nonunit) {
                    temp = A[i * lda + i] * B[i * ldb + j];
                } else {
                    temp = B[i * ldb + j];
                }

                for (k = i + 1; k < n1; k++) {
                    temp += A[lda * k + i] * B[k * ldb + j];
                }

                B[ldb * i + j] = alpha * temp;
            }
        }
    } else if (side == CblasRight && uplo == CblasUpper && trans == CblasNoTrans) {
        /* form  B := alpha * B * TriU(A) */

        for (i = 0; i < n1; i++) {
            for (j = n2; j > 0 && j--;) {
                float temp = 0.0;

                for (k = 0; k < j; k++) {
                    temp += A[lda * k + j] * B[i * ldb + k];
                }

                if (nonunit) {
                    temp += A[j * lda + j] * B[i * ldb + j];
                } else {
                    temp += B[i * ldb + j];
                }

                B[ldb * i + j] = alpha * temp;
            }
        }
    } else if (side == CblasRight && uplo == CblasUpper && trans == CblasTrans) {
        /* form  B := alpha * B * (TriU(A))' */

        for (i = 0; i < n1; i++) {
            for (j = 0; j < n2; j++) {
                float temp = 0.0;

                if (nonunit) {
                    temp = A[j * lda + j] * B[i * ldb + j];
                } else {
                    temp = B[i * ldb + j];
                }

                for (k = j + 1; k < n2; k++) {
                    temp += A[lda * j + k] * B[i * ldb + k];
                }

                B[ldb * i + j] = alpha * temp;
            }
        }
    } else if (side == CblasRight && uplo == CblasLower && trans == CblasNoTrans) {
        /* form  B := alpha *B * TriL(A) */

        for (i = 0; i < n1; i++) {
            for (j = 0; j < n2; j++) {
                float temp = 0.0;

                if (nonunit) {
                    temp = A[j * lda + j] * B[i * ldb + j];
                } else {
                    temp = B[i * ldb + j];
                }

                for (k = j + 1; k < n2; k++) {
                    temp += A[lda * k + j] * B[i * ldb + k];
                }

                B[ldb * i + j] = alpha * temp;
            }
        }
    } else if (side == CblasRight && uplo == CblasLower && trans == CblasTrans) {
        /* form  B := alpha * B * TriL(A)' */

        for (i = 0; i < n1; i++) {
            for (j = n2; j > 0 && j--;) {
                float temp = 0.0;

                for (k = 0; k < j; k++) {
                    temp += A[lda * j + k] * B[i * ldb + k];
                }

                if (nonunit) {
                    temp += A[j * lda + j] * B[i * ldb + j];
                } else {
                    temp += B[i * ldb + j];
                }

                B[ldb * i + j] = alpha * temp;
            }
        }
    } else {
        printf("unrecognized operation");
    }
}

int us_blas_dtrmm(CBLAS_SIDE_t Side, CBLAS_UPLO_t Uplo, CBLAS_TRANSPOSE_t TransA, CBLAS_DIAG_t Diag,
                  float alpha, const us_matrix* A, us_matrix* B) {
    const size_t M = B->size1;
    const size_t N = B->size2;
    const size_t MA = A->size1;
    const size_t NA = A->size2;

    if (MA != NA) {
        printf("matrix A must be square");
    }

    if ((Side == CblasLeft && M == MA) || (Side == CblasRight && N == MA)) {
        cblas_dtrmm(CblasRowMajor, Side, Uplo, TransA, Diag, INT(M), INT(N), alpha, A->data,
                    INT(A->tda), B->data, INT(B->tda));
        return GSL_SUCCESS;
    } else {
        printf("invalid length");
    }
    return -1;
}

static int triangular_inverse_L3(CBLAS_UPLO_t Uplo, CBLAS_DIAG_t Diag, us_matrix* T) {
    const size_t N = T->size1;

    if (N != T->size2) {
        printf("matrix must be square");
    } else if (N <= CROSSOVER_INVTRI) {
        /* use Level 2 BLAS code */
        return triangular_inverse_L2(Uplo, Diag, T);
    } else {
        /*
         * partition matrix:
         *
         * T11 T12
         * T21 T22
         *
         * where T11 is N1-by-N1
         */
        int status;
        const size_t N1 = GSL_LINALG_SPLIT(N);
        const size_t N2 = N - N1;
        us_matrix_float_view T11 = us_matrix_submatrix(T, 0, 0, N1, N1);
        us_matrix_float_view T12 = us_matrix_submatrix(T, 0, N1, N1, N2);
        us_matrix_float_view T21 = us_matrix_submatrix(T, N1, 0, N2, N1);
        us_matrix_float_view T22 = us_matrix_submatrix(T, N1, N1, N2, N2);

        /* recursion on T11 */
        status = triangular_inverse_L3(Uplo, Diag, &T11.matrix);
        if (status)
            return status;

        if (Uplo == CblasLower) {
            /* T21 = - T21 * T11 */
            us_blas_dtrmm(CblasRight, Uplo, CblasNoTrans, Diag, -1.0, &T11.matrix, &T21.matrix);

            /* T21 = T22 * T21^{-1} */
            us_blas_dtrsm(CblasLeft, Uplo, CblasNoTrans, Diag, 1.0, &T22.matrix, &T21.matrix);
        } else {
            /* T12 = - T11 * T12 */
            us_blas_dtrmm(CblasLeft, Uplo, CblasNoTrans, Diag, -1.0, &T11.matrix, &T12.matrix);

            /* T12 = T12 * T22^{-1} */
            us_blas_dtrsm(CblasRight, Uplo, CblasNoTrans, Diag, 1.0, &T22.matrix, &T12.matrix);
        }

        /* recursion on T22 */
        status = triangular_inverse_L3(Uplo, Diag, &T22.matrix);
        if (status)
            return status;

        return GSL_SUCCESS;
    }
    return -1;
}

int us_linalg_tri_invert(CBLAS_UPLO_t Uplo, CBLAS_DIAG_t Diag, us_matrix* T) {
    const size_t N = T->size1;

    if (N != T->size2) {
        printf("matrix must be square");
    } else {
        int status;

        status = triangular_singular(T);
        if (status)
            return status;

        return triangular_inverse_L3(Uplo, Diag, T);
    }
    return -1;
}

void cblas_dgemv(const enum CBLAS_ORDER order, const enum CBLAS_TRANSPOSE TransA, const int M,
                 const int N, const float alpha, const float* A, const int lda, const float* X,
                 const int incX, const float beta, float* Y, const int incY) {
    int i, j;
    int lenX, lenY;

    const int Trans = (TransA != CblasConjTrans) ? TransA : CblasTrans;

    // CHECK_ARGS12(GEMV,order,TransA,M,N,alpha,A,lda,X,incX,beta,Y,incY);

    if (M == 0 || N == 0)
        return;

    if (alpha == 0.0 && beta == 1.0)
        return;

    if (Trans == CblasNoTrans) {
        lenX = N;
        lenY = M;
    } else {
        lenX = M;
        lenY = N;
    }

    /* form  y := beta*y */
    if (beta == 0.0) {
        int iy = OFFSET(lenY, incY);
        for (i = 0; i < lenY; i++) {
            Y[iy] = 0.0;
            iy += incY;
        }
    } else if (beta != 1.0) {
        int iy = OFFSET(lenY, incY);
        for (i = 0; i < lenY; i++) {
            Y[iy] *= beta;
            iy += incY;
        }
    }

    if (alpha == 0.0)
        return;

    if ((order == CblasRowMajor && Trans == CblasNoTrans) ||
        (order == CblasColMajor && Trans == CblasTrans)) {
        /* form  y := alpha*A*x + y */
        int iy = OFFSET(lenY, incY);
        for (i = 0; i < lenY; i++) {
            float temp = 0.0;
            int ix = OFFSET(lenX, incX);
            for (j = 0; j < lenX; j++) {
                temp += X[ix] * A[lda * i + j];
                ix += incX;
            }
            Y[iy] += alpha * temp;
            iy += incY;
        }
    } else if ((order == CblasRowMajor && Trans == CblasTrans) ||
               (order == CblasColMajor && Trans == CblasNoTrans)) {
        /* form  y := alpha*A'*x + y */
        int ix = OFFSET(lenX, incX);
        for (j = 0; j < lenX; j++) {
            const float temp = alpha * X[ix];
            if (temp != 0.0) {
                int iy = OFFSET(lenY, incY);
                for (i = 0; i < lenY; i++) {
                    Y[iy] += temp * A[lda * j + i];
                    iy += incY;
                }
            }
            ix += incX;
        }
    } else {
        printf("unrecognized operation");
    }
}

int us_blas_dgemv(CBLAS_TRANSPOSE_t TransA, float alpha, const us_matrix* A, const us_vector* X,
                  float beta, us_vector* Y) {
    const size_t M = A->size1;
    const size_t N = A->size2;

    if ((TransA == CblasNoTrans && N == X->size && M == Y->size) ||
        (TransA == CblasTrans && M == X->size && N == Y->size)) {
        cblas_dgemv(CblasRowMajor, TransA, INT(M), INT(N), alpha, A->data, INT(A->tda), X->data,
                    INT(X->stride), beta, Y->data, INT(Y->stride));
        return GSL_SUCCESS;
    } else {
        printf("invalid length");
    }
    return -1;
}

float cblas_ddot(const int N, const float* X, const int incX, const float* Y, const int incY) {
#define INIT_VAL 0.0
#define ACC_TYPE float
#define BASE float

    ACC_TYPE r = INIT_VAL;
    int i;
    int ix = OFFSET(N, incX);
    int iy = OFFSET(N, incY);

    for (i = 0; i < N; i++) {
        r += X[ix] * Y[iy];
        ix += incX;
        iy += incY;
    }

    return r;

#undef ACC_TYPE
#undef BASE
#undef INIT_VAL
}

int us_blas_ddot(const us_vector* X, const us_vector* Y, float* result) {
    if (X->size == Y->size) {
        *result = cblas_ddot(INT(X->size), X->data, INT(X->stride), Y->data, INT(Y->stride));
        return GSL_SUCCESS;
    } else {
        printf("invalid length");
    }
    return -1;
}

static int triangular_mult_L2(CBLAS_UPLO_t Uplo, us_matrix* A) {
    const size_t N = A->size1;

    if (N != A->size2) {
        printf("matrix must be square");
    } else {
        size_t i;

        /* quick return */
        if (N == 1)
            return GSL_SUCCESS;

        if (Uplo == CblasUpper) {
            /* compute U * L and store in A */

            for (i = 0; i < N; ++i) {
                float* Aii = us_matrix_ptr(A, i, i);
                float Uii = *Aii;

                if (i < N - 1) {
                    us_vector_float_view lb = us_matrix_subcolumn(A, i, i + 1, N - i - 1);
                    us_vector_float_view ur = us_matrix_subrow(A, i, i + 1, N - i - 1);
                    float tmp;

                    us_blas_ddot(&lb.vector, &ur.vector, &tmp);
                    *Aii += tmp;

                    if (i > 0) {
                        us_matrix_float_view U_TR = us_matrix_submatrix(A, 0, i + 1, i, N - i - 1);
                        us_matrix_float_view L_BL = us_matrix_submatrix(A, i + 1, 0, N - i - 1, i);
                        us_vector_float_view ut = us_matrix_subcolumn(A, i, 0, i);
                        us_vector_float_view ll = us_matrix_subrow(A, i, 0, i);

                        us_blas_dgemv(CblasTrans, 1.0, &L_BL.matrix, &ur.vector, Uii, &ll.vector);
                        us_blas_dgemv(CblasNoTrans, 1.0, &U_TR.matrix, &lb.vector, 1.0, &ut.vector);
                    }
                } else {
                    us_vector_float_view v = us_matrix_subrow(A, N - 1, 0, N - 1);
                    us_blas_dscal(Uii, &v.vector);
                }
            }
        } else {
        }

        return GSL_SUCCESS;
    }
    return -1;
}

static int triangular_mult_L3(CBLAS_UPLO_t Uplo, us_matrix* A) {
    const size_t N = A->size1;

    if (N != A->size2) {
        printf("matrix must be square");
    } else if (N <= CROSSOVER_TRIMULT) {
        return triangular_mult_L2(Uplo, A);
    } else {
        /* partition matrix:
         *
         * A11 A12
         * A21 A22
         *
         * where A11 is N1-by-N1
         */
        int status;
        const size_t N1 = GSL_LINALG_SPLIT(N);
        const size_t N2 = N - N1;
        us_matrix_float_view A11 = us_matrix_submatrix(A, 0, 0, N1, N1);
        us_matrix_float_view A12 = us_matrix_submatrix(A, 0, N1, N1, N2);
        us_matrix_float_view A21 = us_matrix_submatrix(A, N1, 0, N2, N1);
        us_matrix_float_view A22 = us_matrix_submatrix(A, N1, N1, N2, N2);

        /* recursion on A11 */
        status = triangular_mult_L3(Uplo, &A11.matrix);
        if (status)
            return status;

        if (Uplo == CblasLower) {
        } else {
            /* form U * L */

            /* A11 += A12 A21 */
            us_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, &A12.matrix, &A21.matrix, 1.0,
                          &A11.matrix);

            /* A12 = A12 * L22 */
            us_blas_dtrmm(CblasRight, CblasLower, CblasNoTrans, CblasUnit, 1.0, &A22.matrix,
                          &A12.matrix);

            /* A21 = U22 * A21 */
            us_blas_dtrmm(CblasLeft, CblasUpper, CblasNoTrans, CblasNonUnit, 1.0, &A22.matrix,
                          &A21.matrix);
        }

        /* recursion on A22 */
        status = triangular_mult_L3(Uplo, &A22.matrix);
        if (status)
            return status;

        return GSL_SUCCESS;
    }
    return -1;
}

int us_linalg_tri_UL(us_matrix* LU) {
    return triangular_mult_L3(CblasUpper, LU);
}

int us_permute_vector_inverse(const us_permutation* p, us_vector* v) {
    if (v->size != p->size) {
        printf("vector and permutation must be the same length");
    }

    us_permute_inverse(p->data, v->data, v->stride, v->size);

    return GSL_SUCCESS;
}

static int singular(const us_matrix* LU) {
    size_t i, n = LU->size1;

    for (i = 0; i < n; i++) {
        double u = us_matrix_float_get(LU, i, i);
        if (u == 0)
            return 1;
    }

    return 0;
}

int us_linalg_LU_invx(us_matrix* LU, const us_permutation* p) {
    if (LU->size1 != LU->size2) {
        printf("LU matrix must be square");
    } else if (LU->size1 != p->size) {
        printf("permutation length must match matrix size");
    } else if (singular(LU)) {
        printf("matrix is fking singular!!!\n");
    } else {
        int status;
        const size_t N = LU->size1;
        size_t i;

        /* compute U^{-1} */
        status = us_linalg_tri_invert(CblasUpper, CblasNonUnit, LU);
        if (status)
            return status;

        /* compute L^{-1} */
        status = us_linalg_tri_invert(CblasLower, CblasUnit, LU);
        if (status)
            return status;

        /* compute U^{-1} L^{-1} */
        status = us_linalg_tri_UL(LU);
        if (status)
            return status;

        /* apply permutation to columns of A^{-1} */
        for (i = 0; i < N; ++i) {
            us_vector_float_view v = us_matrix_row(LU, i);
            us_permute_vector_inverse(p, &v.vector);
        }

        return GSL_SUCCESS;
    }
    return -1;
}

int us_linalg_LU_invert(const us_matrix* LU, const us_permutation* p, us_matrix* inverse) {
    if (LU->size1 != LU->size2) {
        printf("LU matrix must be square");
    } else if (LU->size1 != p->size) {
        printf("permutation length must match matrix size");
    } else if (inverse->size1 != LU->size1 || inverse->size2 != LU->size2) {
        printf("inverse matrix must match LU matrix dimensions");
    } else {
        us_matrix_memcpy(inverse, LU);
        return us_linalg_LU_invx(inverse, p);
    }
    return -1;
}
