#include <iostream>
#include <mpi.h>

#include <netcdf_par.h>

#include "file/read_input.h"
#include "file/nc_utilities.h"

#include "dg/backend/timer.cuh"
#include "dg/backend/mpi_init.h"
#include "dg/backend/grid.h"
#include "dg/elliptic.h"
#include "dg/cg.h"

#include "solovev.h"
//#include "guenther.h"
#include "mpi_conformal.h"
#include "mpi_orthogonal.h"



int main(int argc, char**argv)
{
    MPI_Init( &argc, &argv);
    int rank;
    unsigned n, Nx, Ny, Nz; 
    MPI_Comm comm;
    mpi_init3d( dg::DIR, dg::PER, dg::PER, n, Nx, Ny, Nz, comm);
    MPI_Comm_rank( MPI_COMM_WORLD, &rank);
    Json::Reader reader;
    Json::Value js;
    if( argc==1)
    {
        std::ifstream is("geometry_params_Xpoint.js");
        reader.parse(is,js,false);
    }
    else
    {
        std::ifstream is(argv[1]);
        reader.parse(is,js,false);
    }
    solovev::GeomParameters gp(js);
    solovev::Psip psip( gp); 
    if(rank==0)std::cout << "Psi min "<<psip(gp.R_0, 0)<<"\n";
    if(rank==0)std::cout << "Type psi_0 and psi_1\n";
    double psi_0, psi_1;
    if(rank==0)std::cin >> psi_0>> psi_1;
    MPI_Bcast( &psi_0, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast( &psi_1, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    if(rank==0)gp.display( std::cout);
    if(rank==0)std::cout << "Constructing grid ... \n";
    dg::Timer t;
    t.tic();
    solovev::CollectivePsip c( gp);
    //ConformalMPIGrid3d<dg::DVec> g3d(gp, psi_0, psi_1, n, Nx, Ny,Nz, dg::DIR, comm);
    //ConformalMPIGrid2d<dg::DVec> g2d = g3d.perp_grid();
    //dg::Elliptic<ConformalMPIGrid3d<dg::DVec>, dg::MDMatrix, dg::MDVec> pol( g3d, dg::not_normed, dg::centered);
    dg::SimpleOrthogonal<solovev::Psip, solovev::PsipR, solovev::PsipZ, solovev::LaplacePsip> 
        generator( c.psip, c.psipR, c.psipZ, c.laplacePsip, psi_0, psi_1, gp.R_0, 0., 1);
    dg::OrthogonalMPIGrid3d<dg::DVec> g3d(generator, n, Nx, Ny,Nz,dg::DIR, MPI_COMM_WORLD); 
    dg::OrthogonalMPIGrid2d<dg::DVec> g2d = g3d.perp_grid();
    dg::Elliptic<dg::OrthogonalMPIGrid3d<dg::DVec>, dg::MDMatrix, dg::MDVec> pol( g3d, dg::not_normed, dg::centered);
    t.toc();
    if(rank==0)std::cout << "Construction took "<<t.diff()<<"s\n";
    ///////////////////////////////////////////////////////////////////////////
    int ncid;
    file::NC_Error_Handle ncerr;
    MPI_Info info = MPI_INFO_NULL;
    ncerr = nc_create_par( "testE_mpi.nc", NC_NETCDF4|NC_MPIIO|NC_CLOBBER, comm, info, &ncid); //MPI ON
    int dim2d[2];
    ncerr = file::define_dimensions(  ncid, dim2d, g2d.global());
    int coordsID[2], psiID, functionID, function2ID;
    ncerr = nc_def_var( ncid, "x_XYP", NC_DOUBLE, 2, dim2d, &coordsID[0]);
    ncerr = nc_def_var( ncid, "y_XYP", NC_DOUBLE, 2, dim2d, &coordsID[1]);
    ncerr = nc_def_var( ncid, "psi", NC_DOUBLE, 2, dim2d, &psiID);
    ncerr = nc_def_var( ncid, "deformation", NC_DOUBLE, 2, dim2d, &functionID);
    ncerr = nc_def_var( ncid, "divB", NC_DOUBLE, 2, dim2d, &function2ID);

    int dims[2], periods[2],  coords[2];
    MPI_Cart_get( g2d.communicator(), 2, dims, periods, coords);
    size_t count[2] = {g2d.n()*g2d.Ny(), g2d.n()*g2d.Nx()};
    size_t start[2] = {coords[1]*count[0], coords[0]*count[1]};

    ncerr = nc_var_par_access( ncid, coordsID[0], NC_COLLECTIVE);
    ncerr = nc_var_par_access( ncid, coordsID[1], NC_COLLECTIVE);
    ncerr = nc_var_par_access( ncid, psiID, NC_COLLECTIVE);
    ncerr = nc_var_par_access( ncid, functionID, NC_COLLECTIVE);
    ncerr = nc_var_par_access( ncid, function2ID, NC_COLLECTIVE);

    dg::HVec X( g2d.size()), Y(X); //P = dg::pullback( dg::coo3, g);
    for( unsigned i=0; i<g2d.size(); i++)
    {
        X[i] = g2d.r()[i];
        Y[i] = g2d.z()[i];
    }
    ncerr = nc_put_vara_double( ncid, coordsID[0], start, count, X.data());
    ncerr = nc_put_vara_double( ncid, coordsID[1], start, count, Y.data());
    ///////////////////////////////////////////////////////////////////////////
    dg::MDVec x =    dg::evaluate( dg::zero, g3d);
    const dg::MDVec b =    dg::pullback( solovev::EllipticDirPerM(gp, psi_0, psi_1, 1), g3d);
    const dg::MDVec chi =  dg::pullback( solovev::Bmodule(gp), g3d);
    const dg::MDVec solution = dg::pullback( solovev::FuncDirPer(gp, psi_0, psi_1,1 ), g3d);
    const dg::MDVec vol3d = dg::create::volume( g3d);
    pol.set_chi( chi);
    //compute error
    dg::MDVec error( solution);
    const double eps = 1e-10;
    dg::Invert<dg::MDVec > invert( x, n*n*Nx*Ny*Nz, eps);
    if(rank==0)std::cout << "eps \t # iterations \t error \t hx_max\t hy_max \t time/iteration \n";
    if(rank==0)std::cout << eps<<"\t";
    t.tic();
    unsigned number = invert(pol, x,b);// vol3d, v3d );
    if(rank==0)std::cout <<number<<"\t";
    t.toc();
    dg::blas1::axpby( 1.,x,-1., solution, error);
    double err = dg::blas2::dot( vol3d, error);
    const double norm = dg::blas2::dot( vol3d, solution);
    if(rank==0)std::cout << sqrt( err/norm) << "\t";
    dg::MDVec gyy = g2d.g_xx(), gxx=g2d.g_yy(), vol = g2d.vol();
    dg::blas1::transform( gxx, gxx, dg::SQRT<double>());
    dg::blas1::transform( gyy, gyy, dg::SQRT<double>());
    dg::blas1::pointwiseDot( gxx, vol, gxx);
    dg::blas1::pointwiseDot( gyy, vol, gyy);
    dg::blas1::scal( gxx, g2d.hx());
    dg::blas1::scal( gyy, g2d.hy());
    if(rank==0)std::cout << "Max elements on first process\n";
    if(rank==0)std::cout << *thrust::max_element( gxx.data().begin(), gxx.data().end()) << "\t";
    if(rank==0)std::cout << *thrust::max_element( gyy.data().begin(), gyy.data().end()) << "\t";
    if(rank==0)std::cout<<t.diff()/(double)number<<"s"<<std::endl;

    dg::blas1::transfer( error.data(), X );
    ncerr = nc_put_vara_double( ncid, psiID, start, count, X.data());
    dg::blas1::transfer( x.data(), X );
    ncerr = nc_put_vara_double( ncid, functionID, start, count, X.data());
    dg::blas1::transfer( solution.data(), X );
    ncerr = nc_put_vara_double( ncid, function2ID, start, count, X.data());
    ncerr = nc_close( ncid);
    MPI_Finalize();


    return 0;
}
