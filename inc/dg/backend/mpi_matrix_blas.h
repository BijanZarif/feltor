#pragma once

#include "mpi_matrix.h"

///@cond
namespace dg
{
namespace blas2
{
namespace detail
{
template< class Matrix1, class Matrix2>
inline void doTransfer( const Matrix1& m1, Matrix2& m2, MPIMatrixTag, MPIMatrixTag)
{
    Matrix2 m(m1);
    m2 = m1;
}

template< class Matrix, class Vector1, class Vector2>
inline void doSymv( Matrix& m, Vector1& x, Vector2& y, MPIMatrixTag, MPIVectorTag, MPIVectorTag )
{
    m.symv( x, y);
}

template< class Matrix, class Vector1, class Vector2>
inline void doGemv( Matrix& m, Vector1&x, Vector2& y, MPIMatrixTag, MPIVectorTag, MPIVectorTag  )
{
    doSymv( m, x, y, MPIMatrixTag(), MPIVectorTag(), MPIVectorTag());
}

} //namespace detail
} //namespace blas2
} //namespace dg
///@endcond
