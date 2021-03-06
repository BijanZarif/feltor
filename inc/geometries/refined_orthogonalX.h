#pragma once


#include "dg/geometry/refined_gridX.h"
#include "orthogonalX.h"



namespace dg
{


template< class container>
struct OrthogonalRefinedGridX2d;

/**
 * @brief A three-dimensional grid based on "almost-conformal" coordinates by Ribeiro and Scott 2010
 */
template< class container>
struct OrthogonalRefinedGridX3d : public dg::RefinedGridX3d
{
    typedef dg::OrthogonalTag metric_category;
    typedef dg::OrthogonalRefinedGridX2d<container> perpendicular_grid;

    template <class Generator>
    OrthogonalRefinedGridX3d( unsigned add_x, unsigned add_y, unsigned howmanyX, unsigned howmanyY, const Generator& generator, double psi_0, double fx, double fy, unsigned n, unsigned n_old, unsigned Nx, unsigned Ny, unsigned Nz, dg::bc bcx, dg::bc bcy): 
        dg::RefinedGridX3d( add_x, add_y, howmanyX, howmanyY, 0,1, -2.*M_PI*fy/(1.-2.*fy), 2.*M_PI*(1.+fy/(1.-2.*fy)), 0., 2*M_PI, fx, fy, n, n_old, Nx, Ny, Nz, bcx, bcy, dg::PER),
        r_(this->size()), z_(r_), xr_(r_), xz_(r_), yr_(r_), yz_(r_),
        g_assoc_( generator, psi_0, fx, fy, n_old, Nx, Ny, Nz, bcx, bcy)
    {
        assert( generator.isOrthogonal());
        construct( generator, psi_0, fx);
    }

    perpendicular_grid perp_grid() const { return perpendicular_grid(*this);}
    const dg::OrthogonalGridX3d<container>& associated() const{ return g_assoc_;}

    const thrust::host_vector<double>& r()const{return r_;}
    const thrust::host_vector<double>& z()const{return z_;}
    const thrust::host_vector<double>& xr()const{return xr_;}
    const thrust::host_vector<double>& yr()const{return yr_;}
    const thrust::host_vector<double>& xz()const{return xz_;}
    const thrust::host_vector<double>& yz()const{return yz_;}

    const container& g_xx()const{return g_xx_;}
    const container& g_yy()const{return g_yy_;}
    const container& g_xy()const{return g_xy_;}
    const container& g_pp()const{return g_pp_;}
    const container& vol()const{return vol_;}
    const container& perpVol()const{return vol2d_;}
    private:
    template<class Generator>
    void construct( Generator generator, double psi_0, double fx)
    { 
        std::cout << "FIND X FOR PSI_0\n";
        const double x_0 = generator.f0()*psi_0;
        const double x_1 = -fx/(1.-fx)*x_0;
        std::cout << "X0 is "<<x_0<<" and X1 is "<<x_1<<"\n";
        init_X_boundaries( x_0, x_1);
        ////////////compute psi(x) for a grid on x 
        thrust::host_vector<double> x_vec(this->n()*this->Nx()); 
        for(unsigned i=0; i<x_vec.size(); i++) {
            x_vec[i] = this->abscissasX()[i];
        }
        thrust::host_vector<double> rvec, zvec, xrvec, xzvec, yrvec, yzvec;
        thrust::host_vector<double> y_vec(this->n()*this->Ny());
        for(unsigned i=0; i<y_vec.size(); i++) y_vec[i] = this->abscissasY()[i*x_vec.size()];
        std::cout << "In construct function \n";
        generator( x_vec, y_vec, 
                this->n()*this->outer_Ny(), 
                this->n()*(this->inner_Ny()+this->outer_Ny()), 
                rvec, zvec, xrvec, xzvec, yrvec, yzvec);
        std::cout << "In construct function \n";
        unsigned Mx = this->n()*this->Nx(), My = this->n()*this->Ny();
        //now lift to 3D grid and multiply with refined weights
        thrust::host_vector<double> wx = this->weightsX();
        thrust::host_vector<double> wy = this->weightsY();
        for( unsigned k=0; k<this->Nz(); k++)
            for( unsigned i=0; i<Mx*My; i++)
            {
                r_[k*Mx*My+i] = rvec[i];
                z_[k*Mx*My+i] = zvec[i];
                yr_[k*Mx*My+i] = yrvec[i]*wy[k*Mx*My+i];
                yz_[k*Mx*My+i] = yzvec[i]*wy[k*Mx*My+i];
                xr_[k*Mx*My+i] = xrvec[i]*wx[k*Mx*My+i];
                xz_[k*Mx*My+i] = xzvec[i]*wx[k*Mx*My+i];
            }
        construct_metric();
    }
    //compute metric elements from xr, xz, yr, yz, r and z
    void construct_metric()
    {
        std::cout << "CONSTRUCTING METRIC\n";
        thrust::host_vector<double> tempxx( r_), tempxy(r_), tempyy(r_), tempvol(r_);
        unsigned Nx = this->n()*this->Nx(), Ny = this->n()*this->Ny();
        for( unsigned k=0; k<this->Nz(); k++)
            for( unsigned i=0; i<Ny; i++)
                for( unsigned j=0; j<Nx; j++)
                {
                    unsigned idx = k*Ny*Nx+i*Nx+j;
                    tempxx[idx] = (xr_[idx]*xr_[idx]+xz_[idx]*xz_[idx]);
                    tempxy[idx] = (yr_[idx]*xr_[idx]+yz_[idx]*xz_[idx]);
                    tempyy[idx] = (yr_[idx]*yr_[idx]+yz_[idx]*yz_[idx]);
                    tempvol[idx] = r_[idx]/sqrt(tempxx[idx]*tempyy[idx]-tempxy[idx]*tempxy[idx]);
                }
        g_xx_=tempxx, g_xy_=tempxy, g_yy_=tempyy, vol_=tempvol;
        dg::blas1::pointwiseDivide( tempvol, r_, tempvol);
        vol2d_ = tempvol;
        thrust::host_vector<double> ones = dg::evaluate( dg::one, *this);
        dg::blas1::pointwiseDivide( ones, r_, tempxx);
        dg::blas1::pointwiseDivide( tempxx, r_, tempxx); //1/R^2
        g_pp_=tempxx;
    }
    thrust::host_vector<double> r_, z_, xr_, xz_, yr_, yz_; //3d vector
    container g_xx_, g_xy_, g_yy_, g_pp_, vol_, vol2d_;
    dg::OrthogonalGridX3d<container> g_assoc_;
};

/**
 * @brief A three-dimensional grid based on "almost-conformal" coordinates by Ribeiro and Scott 2010
 */
template< class container>
struct OrthogonalRefinedGridX2d : public dg::RefinedGridX2d
{
    typedef dg::OrthogonalTag metric_category;

    template<class Generator>
    OrthogonalRefinedGridX2d( unsigned add_x, unsigned add_y, unsigned howmanyX, unsigned howmanyY, const Generator& generator, double psi_0, double fx, double fy, unsigned n, unsigned n_old, unsigned Nx, unsigned Ny, dg::bc bcx, dg::bc bcy, int firstline): 
        dg::RefinedGridX2d( add_x, add_y, howmanyX, howmanyY, 0, 1,-fy*2.*M_PI/(1.-2.*fy), 2*M_PI+fy*2.*M_PI/(1.-2.*fy), fx, fy, n, n_old, Nx, Ny, bcx, bcy),
        g_assoc_( generator, fx, fy, n_old, Nx, Ny, bcx, bcy) 
    {
        dg::OrthogonalRefinedGridX3d<container> g(add_x, add_y, howmanyX, howmanyY, generator, psi_0, fx,fy, n,n_old,Nx,Ny,1,bcx,bcy);
        init_X_boundaries( g.x0(), g.x1());
        r_=g.r(), z_=g.z(), xr_=g.xr(), xz_=g.xz(), yr_=g.yr(), yz_=g.yz();
        g_xx_=g.g_xx(), g_xy_=g.g_xy(), g_yy_=g.g_yy();
        vol2d_=g.perpVol();
    }
    OrthogonalRefinedGridX2d( const OrthogonalRefinedGridX3d<container>& g):
        dg::RefinedGridX2d(g), g_assoc_(g.associated())
    {
        unsigned s = this->size();
        r_.resize( s), z_.resize(s), xr_.resize(s), xz_.resize(s), yr_.resize(s), yz_.resize(s);
        g_xx_.resize( s), g_xy_.resize(s), g_yy_.resize(s), vol2d_.resize(s);
        for( unsigned i=0; i<s; i++)
        { r_[i]=g.r()[i], z_[i]=g.z()[i], xr_[i]=g.xr()[i], xz_[i]=g.xz()[i], yr_[i]=g.yr()[i], yz_[i]=g.yz()[i]; }
        thrust::copy( g.g_xx().begin(), g.g_xx().begin()+s, g_xx_.begin());
        thrust::copy( g.g_xy().begin(), g.g_xy().begin()+s, g_xy_.begin());
        thrust::copy( g.g_yy().begin(), g.g_yy().begin()+s, g_yy_.begin());
        thrust::copy( g.perpVol().begin(), g.perpVol().begin()+s, vol2d_.begin());
    }
    const dg::OrthogonalGridX2d<container>& associated()const{return g_assoc_;}
    const thrust::host_vector<double>& r()const{return r_;}
    const thrust::host_vector<double>& z()const{return z_;}
    const thrust::host_vector<double>& xr()const{return xr_;}
    const thrust::host_vector<double>& yr()const{return yr_;}
    const thrust::host_vector<double>& xz()const{return xz_;}
    const thrust::host_vector<double>& yz()const{return yz_;}
    thrust::host_vector<double> x()const{
        dg::Grid1d gx( x0(), x1(), n(), Nx());
        return dg::create::abscissas(gx);}
    const container& g_xx()const{return g_xx_;}
    const container& g_yy()const{return g_yy_;}
    const container& g_xy()const{return g_xy_;}
    const container& vol()const{return vol2d_;}
    const container& perpVol()const{return vol2d_;}
    private:
    thrust::host_vector<double> r_, z_, xr_, xz_, yr_, yz_; //2d vector
    container g_xx_, g_xy_, g_yy_, vol2d_;
    dg::OrthogonalGridX2d<container> g_assoc_;
};

} //namespace dg

