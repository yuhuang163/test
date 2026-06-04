#ifndef __US_EIGEN_NONSYMMSQUARE__
#define __US_EIGEN_NONSYMMSQUARE__

#include "Abini.h"
#include <stddef.h>
#include <stdlib.h>
#define us_vector us_vector_float
#define us_matrix us_matrix_float
#ifdef INFINITY
    #define GSL_POSINF INFINITY
    #define GSL_NEGINF (-INFINITY)
#elif defined(HUGE_VAL)
    #define GSL_POSINF HUGE_VAL
    #define GSL_NEGINF (-HUGE_VAL)
#else
    #define GSL_POSINF (us_posinf())
    #define GSL_NEGINF (us_neginf())
#endif
#define GSL_FLT_EPSILON 1.1920928955078125e-07
#define RETURN_IF_NULL(x) \
    if (!x)               \
    {                     \
        return;           \
    }
#define NULL_MATRIX_VIEW     \
    {                        \
        {                    \
            0, 0, 0, 0, 0, 0 \
        }                    \
    }
#define us_matrix_get us_matrix_float_get
#define us_matrix_set us_matrix_float_set
#define us_vector_get us_vector_float_get
#define us_vector_set us_vector_float_set
#define INT(X)        ((int)(X))
#define GSL_COMPLEX_DEFINE(R, C) \
    typedef struct               \
    {                            \
        R dat[2];                \
    } C;
GSL_COMPLEX_DEFINE(float, us_complex)
#define GSL_FRANCIS_COEFF1    (0.75)
#define GSL_FRANCIS_COEFF2    (-0.4375)
#define GSL_COMPLEX_AT(zv, i) ((us_complex *)&((zv)->data[2 * (i) * (zv)->stride]))

#define GSL_SET_REAL(zp, x) \
    do                      \
    {                       \
        (zp)->dat[0] = (x); \
    } while (0)
#define GSL_SET_IMAG(zp, y) \
    do                      \
    {                       \
        (zp)->dat[1] = (y); \
    } while (0)
#define OFFSET(N, incX) ((incX) > 0 ? 0 : ((N)-1) * (-(incX)))
#define GSL_REAL(z)     ((z).dat[0])
#define GSL_IMAG(z)     ((z).dat[1])
#define REAL(a, i)      (((BASE *)a)[2 * (i)])
#define IMAG(a, i)      (((BASE *)a)[2 * (i) + 1])

#define GSL_SCHUR_SMLNUM    (2.0 * GSL_FLT_MIN)
#define GSL_SCHUR_BIGNUM    ((1.0 - GSL_FLT_EPSILON) / GSL_SCHUR_SMLNUM)
#define GSL_NONSYMMV_SMLNUM (2.0 * GSL_FLT_MIN)
#define GSL_NONSYMMV_BIGNUM ((1.0 - GSL_FLT_EPSILON) / GSL_NONSYMMV_SMLNUM)

#define FLOAT_RADIX    2.0
#define FLOAT_RADIX_SQ (FLOAT_RADIX * FLOAT_RADIX)
#define GSL_FLT_MIN    1.1754943508222875e-38
#define NULL_VECTOR_VIEW  \
    {                     \
        {                 \
            0, 0, 0, 0, 0 \
        }                 \
    }
#define NULL_VECTOR   \
    {                 \
        0, 0, 0, 0, 0 \
    }
#define MULTIPLICITY 1
#define ATOMIC       float
#define GSL_SET_COMPLEX(zp, x, y) \
    do                            \
    {                             \
        (zp)->dat[0] = (x);       \
        (zp)->dat[1] = (y);       \
    } while (0)
#define INT(X) ((int)(X))
#define NULL_MATRIX_VIEW     \
    {                        \
        {                    \
            0, 0, 0, 0, 0, 0 \
        }                    \
    }
#define NULL_MATRIX      \
    {                    \
        0, 0, 0, 0, 0, 0 \
    }
#define OFFSET(N, incX)             ((incX) > 0 ? 0 : ((N)-1) * (-(incX)))
#define GSL_SIGN(x)                 ((x) >= 0.0 ? 1 : -1)
#define GSL_MAX(a, b)               ((a) > (b) ? (a) : (b))
#define GSL_MIN(a, b)               ((a) < (b) ? (a) : (b))
/* define how a problem is split recursively */
#define GSL_LINALG_SPLIT(n)         ((n >= 16) ? ((n + 8) / 16) * 8 : n / 2)
#define GSL_LINALG_SPLIT_COMPLEX(n) ((n >= 8) ? ((n + 4) / 8) * 4 : n / 2)

/* matrix size for crossover to Level 2 algorithms */
#define CROSSOVER          24
#define CROSSOVER_LU       CROSSOVER
#define CROSSOVER_CHOLESKY CROSSOVER
#define CROSSOVER_INVTRI   CROSSOVER
#define CROSSOVER_TRIMULT  CROSSOVER

struct us_block_float_struct
{
    size_t size;
    float *data;
};

typedef struct us_block_float_struct us_block_float;

typedef struct
{
    size_t size;
    size_t stride;
    float *data;
    us_block_float *block;
    int owner;
} us_vector_float;

typedef struct
{
    us_vector_float vector;
} _us_vector_float_view;

typedef _us_vector_float_view us_vector_float_view;

typedef enum CBLAS_SIDE CBLAS_SIDE_t;
enum
{
    GSL_SUCCESS = 0,
    GSL_FAILED = 1,
    GSL_ESING = 21, /* apparent singularity detected */
};
enum CBLAS_DIAG
{
    CblasNonUnit = 131,
    CblasUnit = 132
};
enum CBLAS_UPLO
{
    CblasUpper = 121,
    CblasLower = 122
};
enum CBLAS_SIDE
{
    CblasLeft = 141,
    CblasRight = 142
};
enum CBLAS_ORDER
{
    CblasRowMajor = 101,
    CblasColMajor = 102
};
enum CBLAS_TRANSPOSE
{
    CblasNoTrans = 111,
    CblasTrans = 112,
    CblasConjTrans = 113
};

typedef enum CBLAS_UPLO CBLAS_UPLO_t;
typedef enum CBLAS_DIAG CBLAS_DIAG_t;

typedef struct
{
    size_t size1;
    size_t size2;
    size_t tda;
    float *data;
    us_block_float *block;
    int owner;
} us_matrix_float;

typedef struct
{
    us_matrix_float matrix;
} _us_matrix_float_view;

typedef _us_matrix_float_view us_matrix_float_view;

struct us_permutation_struct
{
    size_t size;
    size_t *data;
};

typedef struct us_permutation_struct us_permutation;
typedef enum CBLAS_TRANSPOSE CBLAS_TRANSPOSE_t;

struct us_block_complex_struct
{
    size_t size;
    float *data;
};

typedef struct us_block_complex_struct us_block_complex;

typedef struct
{
    size_t size;
    size_t stride;
    float *data;
    us_block_float *block;
    int owner;
} us_vector_complex;

typedef struct
{
    size_t size1;
    size_t size2;
    size_t tda;
    float *data;
    us_block_float *block;
    int owner;
} us_matrix_complex;

typedef struct
{
    us_vector_complex vector;
} _us_vector_complex_view;

typedef _us_vector_complex_view us_vector_complex_view;

typedef struct
{
    us_vector_complex vector;
} _us_vector_complex_const_view;

typedef const _us_vector_complex_const_view us_vector_complex_const_view;

typedef struct
{
    us_vector vector;
} _us_vector_const_view;

typedef const _us_vector_const_view us_vector_const_view;

typedef struct
{
    size_t size;           /* matrix size */
    size_t max_iterations; /* max iterations since last eigenvalue found */
    size_t n_iter;         /* number of iterations since last eigenvalue found */
    size_t n_evals;        /* number of eigenvalues found so far */

    int compute_t; /* compute Schur form T = Z^t A Z */

    us_matrix *H;  /* pointer to Hessenberg matrix */
    us_matrix *Zf; /* pointer to Schur vector matrix */
} us_eigen_francis_workspace;

typedef struct
{
    size_t size;     /* size of matrices */
    us_vector *diag; /* diagonal matrix elements from balancing */
    us_vector *tau;  /* Householder coefficients */
    us_matrix *Z;    /* pointer to Z matrix */
    int do_balance;  /* perform balancing transformation? */
    size_t n_evals;  /* number of eigenvalues found */

    us_eigen_francis_workspace *francis_workspace_p;
} us_eigen_nonsymm_workspace;

typedef enum
{
    GSL_EIGEN_SORT_VAL_ASC,
    GSL_EIGEN_SORT_VAL_DESC,
    GSL_EIGEN_SORT_ABS_ASC,
    GSL_EIGEN_SORT_ABS_DESC
} us_eigen_sort_t;

int us_blas_sgemm(CBLAS_TRANSPOSE_t TransA, CBLAS_TRANSPOSE_t TransB, float alpha,
                  const us_matrix_float *A, const us_matrix_float *B, float beta,
                  us_matrix_float *C);
us_vector_float_view us_vector_subvector(us_vector *v, size_t offset, size_t n);
float *us_vector_float_ptr(us_vector_float *v, const size_t i);
us_block_float *us_block_float_alloc(const size_t n);
us_vector_float *us_vector_float_alloc(const size_t n);
us_matrix_float *us_matrix_float_alloc(const size_t n1, const size_t n2);
us_vector_complex *us_vector_complex_alloc(const size_t n);
void us_vector_complex_free(us_vector_complex *v);
us_matrix_complex *us_matrix_complex_alloc(const size_t n1, const size_t n2);
us_permutation *us_permutation_alloc(const size_t n);
void us_block_free(us_block_float *b);
void us_matrix_free(us_matrix_float *m);
void us_permutation_free(us_permutation *p);
void us_matrix_float_set(us_matrix_float *m, const size_t i, const size_t j, const float x);
float us_matrix_float_get(const us_matrix_float *m, const size_t i, const size_t j);
void us_vector_float_set(us_vector_float *v, const size_t i, float x);
float us_vector_float_get(const us_vector_float *v, const size_t i);
us_vector_float_view us_vector_view_array(const float *base, size_t n);
us_matrix_float_view us_matrix_view_array(const float *array, const size_t n1, const size_t n2);
int us_matrix_add(us_matrix_float *a, const us_matrix_float *b);
int us_blas_sgemm(CBLAS_TRANSPOSE_t TransA, CBLAS_TRANSPOSE_t TransB, float alpha,
                  const us_matrix_float *A, const us_matrix_float *B, float beta,
                  us_matrix_float *C);
int us_blas_ddot(const us_vector *X, const us_vector *Y, float *result);
int us_blas_dger(float alpha, const us_vector *X, const us_vector *Y, us_matrix *A);
void us_blas_dscal(float alpha, us_vector *X);
int us_blas_dgemv(CBLAS_TRANSPOSE_t TransA, float alpha, const us_matrix *A, const us_vector *X,
                  float beta, us_vector *Y);
us_vector_float_view us_matrix_subcolumn(us_matrix *m, const size_t j, const size_t offset,
                                         const size_t n);
us_matrix_float_view us_matrix_submatrix(us_matrix *m, const size_t i, const size_t j,
                                         const size_t n1, const size_t n2);
us_vector_float_view us_matrix_subrow(us_matrix *m, const size_t i, const size_t offset,
                                      const size_t n);
us_vector_float_view us_matrix_row(us_matrix *m, const size_t i);
int us_vector_float_memcpy(us_vector *dest, const us_vector *src);
int us_matrix_memcpy(us_matrix_float *dest, const us_matrix_float *src);
int us_blas_scopy(const us_vector_float *X, us_vector_float *Y);
int us_linalg_LU_decomp(us_matrix *A, us_permutation *p, int *signum);
int us_linalg_LU_invert(const us_matrix *LU, const us_permutation *p, us_matrix *inverse);
void us_matrix_set_identity(us_matrix *m);
int us_blas_daxpy(float alpha, const us_vector *X, us_vector *Y);
float us_linalg_householder_transform(us_vector *v);
us_eigen_nonsymm_workspace *us_eigen_nonsymm_alloc(const size_t n);
us_complex us_vector_complex_get(const us_vector_complex *v, const size_t i);
int us_eigen_nonsymm(us_matrix *A, us_vector_complex *eval, us_eigen_nonsymm_workspace *w);

#endif
