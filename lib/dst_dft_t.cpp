
#include <iostream>
#include <iomanip>
#include "dst_dft.h"

using namespace std;
using namespace toefl;

int main()
{
    Matrix<double, TL_DST_DFT> m1(5, 9);
    Matrix<complex<double> >   m1_( 9, 5/2 + 1);
    DST_DFT trafo( 5,9);
    double dx = 1./(9.+1.), dy = 1./5;
    for( size_t i = 0; i < m1.rows(); i++)
        for ( size_t j=0; j < m1.cols(); j++)
            m1(i, j) = sin( M_PI*(j+1)*dx)*cos( 2.*M_PI*i*dy); //sin(kPix)*sin(qPiy)
    cout << setprecision(2) << fixed;
    cout << "One mode in every line, One mode in every column\n"<< m1<< endl;
    trafo.execute_dst_dft_r2c(m1, m1_);
    cout << "Output should show one mode\n" << m1_ << endl;
    try{
        trafo.execute_dst_dft_c2r( m1_, m1);
    }catch( Message& m){m.display();}
    cout << "The backtransformed matrix (100 times input)\n"<<m1<<endl;



    
    return 0;
}