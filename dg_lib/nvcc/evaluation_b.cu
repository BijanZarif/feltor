#include <iostream>
#include <cmath>

#include <thrust/host_vector.h>
#include <thrust/device_vector.h>

#include "timer.cuh"
#include "evaluation.cuh"
#include "operators.cuh"
#include "preconditioner.cuh"


double function( double x)
{
    return exp(x);
}


const unsigned n = 3;
const unsigned N = 1e5;

typedef thrust::device_vector< double>   DVec;
typedef thrust::host_vector< double>     HVec;
typedef dg::ArrVec1d< double, n, HVec>  HArrVec;
typedef dg::ArrVec1d< double, n, DVec>  DArrVec;


using namespace std;
int main()
{
    cout << "Array size is:             "<< n-1<<endl;
    cout << "Number of intervals is:    "<< N <<endl;
    double h = 1./(double)N;
    dg::Timer t;

    t.tic();
    HArrVec h_v = dg::expand< double(&) (double), n>( function, 0, 1, N);
    t.toc(); 
    cout << "Expansion on host took         "<< t.diff()<<"s\n";

    t.tic();
    DArrVec d_v( h_v.data());
    t.toc(); 
    cout << "Copy of data host2device took  "<< t.diff()<<"s\n\n";
    t.tic();
    dg::BLAS2< dg::S<n>, DVec>::dsymv(  dg::S<n>(h), d_v.data(), d_v.data());
    t.toc(); 
    cout << "dsymv took on device           "<< t.diff()<<"s\n";
    t.tic();
    dg::BLAS2< dg::S<n>, HVec>::dsymv(  dg::S<n>(h), h_v.data(), h_v.data());
    t.toc(); 
    cout << "dsymv took on host             "<< t.diff()<<"s\n";

    double norm;
    t.tic();
    norm = dg::BLAS2< dg::T<n>, DVec>::ddot( dg::T<n>(h), d_v.data());
    t.toc(); 
    cout << "ddot took on device            "<< t.diff()<<"s\n";

    t.tic();
    norm = dg::BLAS2< dg::T<n>, HVec>::ddot( dg::T<n>(h), h_v.data());
    t.toc(); 
    cout << "ddot took on host              "<< t.diff()<<"s\n\n";
    cout<< "Square normalized norm "<< norm <<"\n";
    double solution = (exp(2.) -exp(0))/2.;
    cout << "Correct square norm of exp(x) is "<<solution<<endl;
    return 0;
}