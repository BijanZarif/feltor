#include <iostream>
#include <iomanip>
#include <vector>
#include <fstream>
#include <sstream>
#include <cmath>

#include "dg/backend/xspacelib.cuh"
#include "dg/functors.h"
#include "file/read_input.h"
#include "draw/host_window.h"

#include "dg/backend/timer.cuh"
#include "solovev.h"
#include "conformalX.h"
#include "dg/ds.h"
#include "init.h"

#include "file/nc_utilities.h"

//typedef dg::FieldAligned< solovev::ConformalXGrid3d<dg::DVec> , dg::IDMatrix, dg::DVec> DFA;
double sine( double x) {return sin(x);}
double cosine( double x) {return cos(x);}
typedef dg::FieldAligned< solovev::ConformalXGrid3d<dg::DVec> , dg::IDMatrix, dg::DVec> DFA;

int main( int argc, char* argv[])
{
    std::cout << "Type n, Nx, Ny, Nz (Nx must be divided by 4 and Ny by 10) \n";
    unsigned n, Nx, Ny, Nz;
    std::cin >> n>> Nx>>Ny>>Nz;   
    std::cout << "Type psi_0 \n";
    double psi_0;
    std::cin >> psi_0;
    std::cout << "Type fx \n";
    double fx_0;
    std::cin >> fx_0;
    std::vector<double> v, v2;
try{ 
        if( argc==1)
        {
            v = file::read_input( "geometry_params_Xpoint.txt"); 
        }
        else
        {
            v = file::read_input( argv[1]); 
        }
    }
    catch (toefl::Message& m) {  
        m.display(); 
        for( unsigned i = 0; i<v.size(); i++)
            std::cout << v[i] << " ";
            std::cout << std::endl;
        return -1;}
    //write parameters from file into variables
    solovev::GeomParameters gp(v);
    gp.display( std::cout);
    dg::Timer t;
    solovev::Psip psip( gp); 
    std::cout << "Psi min "<<psip(gp.R_0, 0)<<"\n";
    std::cout << "Constructing conformal grid ... \n";
    t.tic();
    solovev::ConformalXGrid3d<dg::DVec> g3d(gp, psi_0, fx_0, 0., n, Nx, Ny,Nz, dg::DIR, dg::NEU);
    solovev::ConformalXGrid2d<dg::DVec> g = g3d.perp_grid();
    t.toc();
    std::cout << "Construction took "<<t.diff()<<"s"<<std::endl;
    dg::Grid1d<double> g1d( g.x0(), g.x1(), g.n(), g.Nx());
    dg::HVec x_left = dg::evaluate( sine, g1d), x_right(x_left);
    dg::HVec y_left = dg::evaluate( cosine, g1d);
    int ncid;
    file::NC_Error_Handle err;
    err = nc_create( "testX.nc", NC_NETCDF4|NC_CLOBBER, &ncid);
    int dim3d[2], dim1d[1];
    err = file::define_dimensions(  ncid, dim3d, g.grid());
    err = file::define_dimension(  ncid, "i", dim1d, g1d);
    int coordsID[2], onesID, defID, volID, divBID;
    int coord1D[5];
    err = nc_def_var( ncid, "x_XYP", NC_DOUBLE, 2, dim3d, &coordsID[0]);
    err = nc_def_var( ncid, "y_XYP", NC_DOUBLE, 2, dim3d, &coordsID[1]);
    err = nc_def_var( ncid, "x_left", NC_DOUBLE, 1, dim1d, &coord1D[0]);
    err = nc_def_var( ncid, "y_left", NC_DOUBLE, 1, dim1d, &coord1D[1]);
    err = nc_def_var( ncid, "x_right", NC_DOUBLE, 1, dim1d, &coord1D[2]);
    err = nc_def_var( ncid, "y_right", NC_DOUBLE, 1, dim1d, &coord1D[3]);
    err = nc_def_var( ncid, "f_x", NC_DOUBLE, 1, dim1d, &coord1D[4]);
    //err = nc_def_var( ncid, "z_XYP", NC_DOUBLE, 3, dim3d, &coordsID[2]);
    err = nc_def_var( ncid, "psi", NC_DOUBLE, 2, dim3d, &onesID);
    err = nc_def_var( ncid, "deformation", NC_DOUBLE, 2, dim3d, &defID);
    err = nc_def_var( ncid, "volume", NC_DOUBLE, 2, dim3d, &volID);
    err = nc_def_var( ncid, "divB", NC_DOUBLE, 2, dim3d, &divBID);

    thrust::host_vector<double> psi_p = dg::pullback( psip, g);
    g.display();
    err = nc_put_var_double( ncid, onesID, psi_p.data());
    dg::HVec X( g.size()), Y(X); //P = dg::pullback( dg::coo3, g);
    for( unsigned i=0; i<g.size(); i++)
    {
        //X[i] = g.r()[i]*cos(P[i]);
        //Y[i] = g.r()[i]*sin(P[i]);
        X[i] = g.r()[i];
        Y[i] = g.z()[i];
    }

    dg::DVec ones = dg::evaluate( dg::one, g);
    dg::DVec temp0( g.size()), temp1(temp0);
    dg::DVec w3d = dg::create::weights( g);

    err = nc_put_var_double( ncid, coordsID[0], X.data());
    err = nc_put_var_double( ncid, coordsID[1], Y.data());
    err = nc_put_var_double( ncid, coord1D[0], g3d.rx0().data());
    err = nc_put_var_double( ncid, coord1D[1], g3d.zx0().data());
    err = nc_put_var_double( ncid, coord1D[2], g3d.rx1().data());
    err = nc_put_var_double( ncid, coord1D[3], g3d.zx1().data());
    err = nc_put_var_double( ncid, coord1D[4], g3d.f_x().data());
    //err = nc_put_var_double( ncid, coordsID[2], g.z().data());

    dg::blas1::pointwiseDivide( g.g_xy(), g.g_xx(), temp0);
    X=temp0;
    err = nc_put_var_double( ncid, defID, X.data());
    X = g.vol();
    err = nc_put_var_double( ncid, volID, X.data());

    std::cout << "Construction successful!\n";

    //compute error in volume element
    dg::blas1::pointwiseDot( g.g_xx(), g.g_yy(), temp0);
    dg::blas1::pointwiseDot( g.g_xy(), g.g_xy(), temp1);
    dg::blas1::axpby( 1., temp0, -1., temp1, temp0);
    //dg::blas1::transform( temp0, temp0, dg::SQRT<double>());
    dg::blas1::pointwiseDot( g.g_xx(), g.g_xx(), temp1);
    dg::blas1::axpby( 1., temp1, -1., temp0, temp0);
    std::cout<< "Rel Error in Determinant is "<<sqrt( dg::blas2::dot( temp0, w3d, temp0)/dg::blas2::dot( temp1, w3d, temp1))<<"\n";

    dg::blas1::pointwiseDot( g.g_xx(), g.g_yy(), temp0);
    dg::blas1::pointwiseDot( g.g_xy(), g.g_xy(), temp1);
    dg::blas1::axpby( 1., temp0, -1., temp1, temp0);
    //dg::blas1::pointwiseDot( temp0, g.g_pp(), temp0);
    dg::blas1::transform( temp0, temp0, dg::SQRT<double>());
    dg::blas1::pointwiseDivide( ones, temp0, temp0);
    dg::blas1::axpby( 1., temp0, -1., g.vol(), temp0);
    std::cout << "Rel Consistency  of volume is "<<sqrt(dg::blas2::dot( temp0, w3d, temp0)/dg::blas2::dot( g.vol(), w3d, g.vol()))<<"\n";

    //temp0=g.r();
    //dg::blas1::pointwiseDivide( temp0, g.g_xx(), temp0);
    dg::blas1::pointwiseDivide( ones, g.g_xx(), temp0);
    dg::blas1::axpby( 1., temp0, -1., g.vol(), temp0);
    std::cout << "Rel Error of volume form is "<<sqrt(dg::blas2::dot( temp0, w3d, temp0))/sqrt( dg::blas2::dot(g.vol(), w3d, g.vol()))<<"\n";

    solovev::FieldY fieldY(gp);
    //solovev::ConformalField fieldY(gp);
    dg::HVec by = dg::pullback( fieldY, g);
    //for( unsigned k=0; k<Nz; k++)
        for( unsigned i=0; i<n*Ny; i++)
            for( unsigned j=0; j<n*Nx; j++)
                //by[k*n*n*Nx*Ny + i*n*Nx + j] *= g.f_x()[j]*g.f_x()[j];
                by[i*n*Nx + j] *= g.f_x()[j]*g.f_x()[j];
    dg::DVec by_device = by;
    dg::blas1::scal( by_device, 1./gp.R_0);
    temp0=g.r();
    dg::blas1::pointwiseDivide( g.g_xx(), temp0, temp0);
    dg::blas1::axpby( 1., temp0, -1., by_device, temp1);
    double error= dg::blas2::dot( temp1, w3d, temp1);
    std::cout << "Rel Error of g.g_xx() is "<<sqrt(error/dg::blas2::dot( by_device, w3d, by_device))<<"\n";

    std::cout << "TEST VOLUME IS:\n";
    gp.psipmax = 0., gp.psipmin = psi_0;
    solovev::Iris iris( gp);
    //dg::CylindricalGrid<dg::HVec> g3d( gp.R_0 -2.*gp.a, gp.R_0 + 2*gp.a, -2*gp.a, 2*gp.a, 0, 2*M_PI, 3, 2200, 2200, 1, dg::PER, dg::PER, dg::PER);
    dg::CartesianGrid2d g2d( gp.R_0 -1.2*gp.a, gp.R_0 + 1.2*gp.a, -1.1*gp.a*gp.elongation, 1.1*gp.a*gp.elongation, 1, 5e3, 5e3, dg::PER, dg::PER);
    dg::HVec vec  = dg::evaluate( iris, g2d);
    dg::DVec cutter = dg::pullback( iris, g), vol( cutter);
    dg::blas1::pointwiseDot(cutter, w3d, vol);
    double volume = dg::blas1::dot( g.vol(), vol);
    dg::HVec g2d_weights = dg::create::volume( g2d);
    double volumeRZP = dg::blas1::dot( vec, g2d_weights);
    std::cout << "volumeXYP is "<< volume<<std::endl;
    std::cout << "volumeRZP is "<< volumeRZP<<std::endl;
    std::cout << "relative difference in volume is "<<fabs(volumeRZP - volume)/volume<<std::endl;
    std::cout << "Note that the error might also come from the volume in RZP!\n";

    ///////////////////////TEST 3d grid//////////////////////////////////////
    std::cout << "Start DS test!"<<std::endl;
    const dg::DVec vol3d = dg::create::volume( g3d);
    DFA fieldaligned( solovev::ConformalField( gp, g.x(), g.f_x()), g3d, gp.rk4eps, dg::NoLimiter(), dg::NEU); 

    dg::DS<DFA, dg::Composite<dg::DMatrix>, dg::DVec> ds( fieldaligned, solovev::ConformalField(gp, g3d.x(), g3d.f_x()), dg::normed, dg::centered, false);
    dg::DVec B = dg::pullback( solovev::InvB(gp), g3d), divB(B);
    dg::DVec lnB = dg::pullback( solovev::LnB(gp), g3d), gradB(B);
    dg::DVec gradLnB = dg::pullback( solovev::GradLnB(gp), g3d);
    dg::blas1::pointwiseDivide( ones, B, B);

    ds.centeredT( B, divB);
    std::cout << "Divergence of B is "<<sqrt( dg::blas2::dot( divB, vol3d, divB))<<"\n";
    ds.centered( lnB, gradB);
    dg::blas1::axpby( 1., gradB, -1., gradLnB, gradLnB);
    //test if topological shift was correct!!
    dg::blas1::pointwiseDot(cutter, gradLnB, gradLnB);
    std::cout << "Error of lnB is    "<<sqrt( dg::blas2::dot( gradLnB, vol3d, gradLnB))<<" (doesn't fullfill boundary conditions so it was cut before separatrix)\n";

    const dg::DVec function = dg::pullback(solovev::FuncNeu(gp), g3d);
    dg::DVec temp(function);
    const dg::DVec derivative = dg::pullback(solovev::DeriNeu(gp), g3d);
    ds( function, temp);
    dg::blas1::axpby( 1., temp, -1., derivative, temp);
    std::cout << "Error of DS  is    "<<sqrt( dg::blas2::dot( temp, vol3d, temp))<<"\n";
    X = gradB;
    err = nc_put_var_double( ncid, divBID, X.data());
    err = nc_close( ncid);


    return 0;
}
