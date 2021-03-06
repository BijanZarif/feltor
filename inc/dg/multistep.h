#pragma once

#include "cg.h"


/*! @file

  This file contains multistep explicit& implicit time-integrators
  */
namespace dg{

///@cond
template< size_t k>
struct ab_coeff
{
    static const double b[k];
};
template<>
const double ab_coeff<2>::b[2] = {1.5, -0.5};
template<>
const double ab_coeff<3>::b[3] = {23./12., -4./3., 5./12.};
template<>
const double ab_coeff<4>::b[4] = {55./24., -59./24., 37./24., -3./8.};
template<>
const double ab_coeff<5>::b[5] = {1901./720., -1387./360., 109./30., -637./360., 251/720};
///@endcond

/**
* @brief Struct for Adams-Bashforth explicit multistep time-integration
* \f[ u^{n+1} = u^n + \Delta t\sum_{j=0}^k b_j f\left(u^{n-j}\right) \f]
*
* @ingroup time
*
* Computes \f[ u_{n+1} = u_n + dt\sum_{j=0}^k b_j f(u_{n-j}) \f]
* Uses only blas1::axpby routines to integrate one step
* and only one right-hand-side evaluation per step.
* @tparam k Order of the method (Currently one of 1, 2, 3, 4 or 5)
* @tparam Vector The Argument type used in the Functor class
*/
template< size_t k, class Vector>
struct AB
{
    /**
    * @brief Reserve memory for the integration
    *
    * @param copyable Vector of size which is used in integration. 
    * A Vector object must be copy-constructible from copyable.
    */
    AB( const Vector& copyable): f_(k, Vector(copyable)), u_(copyable){ }
   
    /**
     * @brief Init with initial value
     *
     * This routine initiates the first steps in the multistep method by integrating
     * backwards with a Euler method. This routine has to be called
     * before the first timestep is made and with the same initial value as the first timestep.
     * @tparam Functor models BinaryFunction with no return type (subroutine).
        Its arguments both have to be of type Vector.
        The first argument is the actual argument, the second contains
        the return value, i.e. y' = f(y) translates to f( y, y').
     * @param f The rhs functor
     * @param u0 The initial value of the integration
     * @param dt The timestep
     * @note The class allows Functor to change its first (input) argument, i.e. the first argument need not be const
     */
    template< class Functor>
    void init( Functor& f, const Vector& u0, double dt);
    /**
    * @brief Advance u0 one timestep
    *
    * @tparam Functor models BinaryFunction with no return type (subroutine)
        Its arguments both have to be of type Vector.
        The first argument is the actual argument, The second contains
        the return value, i.e. y' = f(y) translates to f( y, y').
    * @param f right hand side function or functor
    * @param u (write-only) contains next step of the integration on output
     * @note The class allows Functor to change its first (input) argument, i.e. the first argument need not be const
    */
    template< class Functor>
    void operator()( Functor& f, Vector& u);
  private:
    double dt_;
    std::vector<Vector> f_; //TODO std::array is more natural here (but unfortunately not available)
    Vector u_;
};

template< size_t k, class Vector>
template< class Functor>
void AB<k, Vector>::init( Functor& f, const Vector& u0,  double dt)
{
    dt_ = dt;
    Vector u1(u0), u2(u0);
    blas1::axpby( 1., u0, 0, u_);
    f( u1, f_[0]);
    for( unsigned i=1; i<k; i++)
    {
        blas1::axpby( 1., u2, -dt, f_[i-1], u1);
        blas1::axpby( 1., u1, 0, u2); //f may destroy u1
        f( u1, f_[i]);
    }
}

template< size_t k, class Vector>
template< class Functor>
void AB<k, Vector>::operator()( Functor& f, Vector& u)
{
    blas1::axpby( 1., u_, 0, u);
    f( u, f_[0]);
    for( unsigned i=0; i<k; i++)
        blas1::axpby( dt_*ab_coeff<k>::b[i], f_[i], 1., u_);
    //permute f_[k-1]  to be the new f_[0]
    for( unsigned i=k-1; i>0; i--)
        f_[i-1].swap( f_[i]);
    blas1::axpby( 1., u_, 0, u);
}

///@cond
//Euler specialisation
template < class Vector>
struct AB<1, Vector>
{
    AB(){}
    AB( const Vector& copyable):temp_(2, copyable){}
    template < class Functor>
    void init( Functor& f, const Vector& u0, double dt){ dt_=dt;}
    template < class Functor>
    void operator()( Functor& f, Vector& u)
    {
        blas1::axpby( 1., u, 0, temp_[0]);
        f( u, temp_[1]);
        blas1::axpby( 1., temp_[0], dt_, temp_[1], u);
    }
    private:
    double dt_;
    std::vector<Vector> temp_;
};
///@endcond
///@cond
namespace detail{

template< class LinearOp, class container>
struct Implicit
{
    Implicit( double alpha, LinearOp& f, container& reference): f_(f), alpha_(alpha), temp_(reference){}
    void symv( const container& x, container& y) 
    {
        blas1::axpby( 1., x, 0, temp_);//f_ might destroy x
        if( alpha_ != 0);
            f_( temp_,y);
        blas1::axpby( 1., x, alpha_, y, y);
        blas2::symv( f_.weights(), y,  y);
    }
    //compute without weights
    void operator()( const container& x, container& y) 
    {
        blas1::axpby( 1., x, 0, temp_);
        if( alpha_ != 0);
            f_( temp_,y);
        blas1::axpby( 1., x, alpha_, y, y);
    }
    double& alpha( ){  return alpha_;}
    double alpha( ) const  {return alpha_;}
  private:
    LinearOp& f_;
    double alpha_;
    container& temp_;

};

}//namespace detail
template< class M, class V>
struct MatrixTraits< detail::Implicit<M, V> >
{
    typedef double value_type;
    typedef SelfMadeMatrixTag matrix_category;
};
///@endcond

/**
* @brief Struct for Karniadakis semi-implicit multistep time-integration
* \f[
* \begin{align}
    {\bar v}^n &= \frac{1}{\gamma_0}\left(\sum_{q=0}^2 \alpha_q v^{n-q} + \Delta t\sum_{q=0}^2\beta_q  N( v^{n-q})\right) \\
    \left( 1  - \frac{\Delta t}{\gamma_0}  \hat L\right)  v^{n+1} &= {\bar v}^n  
    \end{align}
    \f]
*
* Uses blas1::axpby routines to integrate one step
* and only one right-hand-side evaluation per step. 
* Uses a conjugate gradient method for the implicit operator  
* @ingroup time
* @tparam Vector The Argument type used in the Functor class
*/
template<class Vector>
struct Karniadakis
{

    /**
    * @brief Reserve memory for the integration
    *
    * @param copyable Vector of size which is used in integration. 
    * @param max_iter parameter for cg
    * @param eps  parameter for cg
    * A Vector object must be copy-constructible from copyable.
    */
    Karniadakis( const Vector& copyable, unsigned max_iter, double eps): u_(3, Vector(copyable)), f_(3, Vector(copyable)), pcg( copyable, max_iter), eps_(eps){
        //a[0] =  1.908535476882378;  b[0] =  1.502575553858997;
        //a[1] = -1.334951446162515;  b[1] = -1.654746338401493;
        //a[2] =  0.426415969280137;  b[2] =  0.670051276940255;
        a[0] =  18./11.;    b[0] =  18./11.;
        a[1] = -9./11.;     b[1] = -18./11.;
        a[2] = 2./11.;      b[2] = 6./11.;   //Karniadakis !!!
    }
   
    /**
     * @brief Initialize with initial value
     *
     * @tparam Functor models BinaryFunction with no return type (subroutine)
        Its arguments both have to be of type Vector.
        The first argument is the actual argument, The second contains
        the return value, i.e. y' = f(y) translates to f( y, y').
     * @tparam LinearOp models BinaryFunction with no return type (subroutine)
        Its arguments both have to be of type Vector. 
        The first argument is the actual argument, The second contains
        the return value, i.e. y' = L(y) translates to diff( y, y').
        Furthermore the routines weights() and precond() must be callable
        and return diagonal weights and the preconditioner for the conjugate gradient. 
     * @param f right hand side function or functor
     * @param diff diffusion operator treated implicitely 
     * @param u0 The initial value you later use 
     * @param dt The timestep saved for later use
     * @note Both Functor and LinearOp may change their first (input) argument, i.e. the first argument need not be const
     */
    template< class Functor, class LinearOp>
    void init( Functor& f, LinearOp& diff, const Vector& u0, double dt);

    /**
    * @brief Advance u for one timestep
    *
    * @tparam Functor models BinaryFunction with no return type (subroutine)
        Its arguments both have to be of type Vector.
        The first argument is the actual argument, The second contains
        the return value, i.e. y' = f(y) translates to f( y, y').
    * @tparam LinearOp models BinaryFunction with no return type (subroutine)
        Its arguments both have to be of type Vector.
        The first argument is the actual argument, The second contains
        the return value, i.e. y' = L(y) translates to diff( y, y').
        Furthermore the routines weights() and precond() must be callable
        and return diagonal weights and the preconditioner for the conjugate gradient. 
        The Operator itself need not be symmetric. Symmetrization is done by the class itself.
    * @param f right hand side function or functor (is called for u)
    * @param diff diffusion operator treated implicitely 
    * @param u (write-only), contains next step of time-integration on output
     * @note Both Functor and LinearOp may change their first (input) argument, i.e. the first argument need not be const
    */
    template< class Functor, class LinearOp>
    void operator()( Functor& f, LinearOp& diff, Vector& u);


    /**
     * @brief return the current head of the computation
     *
     * @return current head
     */
    const Vector& head()const{return u_[0];}
    /**
     * @brief return the last vector for which f was called
     *
     * @return current head^
     */
    const Vector& last()const{return u_[1];}
  private:
    std::vector<Vector> u_, f_; 
    CG< Vector> pcg;
    double eps_;
    double dt_;
    double a[3];
    double b[3];

};

///@cond
template< class Vector>
template< class Functor, class Diffusion>
void Karniadakis<Vector>::init( Functor& f, Diffusion& diff,  const Vector& u0,  double dt)
{
    dt_ = dt;
    Vector temp_(u0);
    detail::Implicit<Diffusion, Vector> implicit( -dt, diff, temp_);
    blas1::axpby( 1., u0, 0, temp_); //copy u0
    f( temp_, f_[0]);
    blas1::axpby( 1., u0, 0, u_[0]); 
    blas1::axpby( 1., u_[0], -dt, f_[0], f_[1]); //Euler step
    implicit( f_[1], u_[1]); //explicit Euler step backwards, might destroy f_[1]
    blas1::axpby( 1., u_[1], 0, temp_); 
    f( temp_, f_[1]);
    blas1::axpby( 1.,u_[1], -dt, f_[1], f_[2]);
    implicit( f_[2], u_[2]);
    blas1::axpby( 1., u_[2], 0, temp_); 
    f( temp_, f_[2]);
}

template<class Vector>
template< class Functor, class Diffusion>
void Karniadakis<Vector>::operator()( Functor& f, Diffusion& diff, Vector& u)
{

    blas1::axpby( 1., u_[0], 0, u); //save u_[0]
    f( u, f_[0]);
    blas1::axpby( dt_*b[1], f_[1], dt_*b[2], f_[2], f_[2]);
    blas1::axpby( dt_*b[0], f_[0],       1., f_[2], f_[2]);
    blas1::axpby( a[1], u_[1], a[2], u_[2], u_[2]);
    blas1::axpby( a[0], u_[0],   1., u_[2], u_[2]);
    blas1::axpby( 1., u_[2], 1., f_[2], u);
    //permute f_[2], u_[2]  to be the new f_[0], u_[0]
    for( unsigned i=2; i>0; i--)
    {
        f_[i-1].swap( f_[i]);
        u_[i-1].swap( u_[i]);
    }
    //compute implicit part
    double alpha[2] = {2., -1.};
    //double alpha[2] = {1., 0.};
    blas1::axpby( alpha[0], u_[1], alpha[1],  u_[2], u_[0]); //extrapolate previous solutions
    blas2::symv( diff.weights(), u, u);
    detail::Implicit<Diffusion, Vector> implicit( -dt_/11.*6., diff, f_[0]);
#ifdef DG_BENCHMARK
#ifdef MPI_VERSION
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif//MPI
    Timer t;
    t.tic(); 
    unsigned number = pcg( implicit, u_[0], u, diff.precond(), eps_);
    t.toc();
#ifdef MPI_VERSION
    if(rank==0)
#endif//MPI
    std::cout << "# of pcg iterations for timestep: "<<number<<"/"<<pcg.get_max()<<" took "<<t.diff()<<"s\n";
#else
    pcg( implicit, u_[0], u, diff.precond(), eps_);
#endif //BENCHMARK
    blas1::axpby( 1., u_[0], 0, u); //save u_[0]


}
///@endcond


/**
 * @brief Semi implicit Runge Kutta method after Yoh and Zhong (AIAA 42, 2004)
 *
 * @ingroup time
 * @tparam Vector Vector class to use
 */
template <class Vector>
struct SIRK
{
    /**
     * @brief Construct from copyable Vector
     *
     * @param copyable Vector of right size
     * @param max_iter maximum iterations for conjugate gradient
     * @param eps error for conjugate gradient
     */
    SIRK(const Vector& copyable, unsigned max_iter, double eps): k_(3, copyable), f_(copyable), g_(copyable), rhs( f_), pcg( copyable, max_iter), eps_(eps)
    {
        w[0] = 1./8., w[1] = 1./8., w[2] = 3./4.;
        b[1][0] = 8./7., b[2][0] = 71./252., b[2][1] = 7./36.;
        d[0] = 3./4., d[1] = 75./233., d[2] = 65./168.;
        c[1][0] = 5589./6524., c[2][0] = 7691./26096., c[2][1] = -26335/78288;
    }
    /**
     * @brief integrate one step
     *
     * @tparam Explicit Object containing explicit part 
     * @tparam Imp Object containing implicit part ( must return precond() and weights())
     * @param f explicit part of the equations
     * @param g implicit part of the equations
     * @param u0 start point
     * @param u1 end point (write only)
     * @param dt timestep
     */
    template <class Explicit, class Imp>
    void operator()( Explicit& f, Imp& g, const Vector& u0, Vector& u1, double dt)
    {
        Vector u0_ = u0;
        detail::Implicit<Imp, Vector> implicit( -dt*d[0], g, f_);
        f(u0_, f_);
        u0_ = u0;
        g(u0_, g_);
        dg::blas1::axpby( dt, f_, dt, g_, rhs);
        blas2::symv( g.weights(), rhs, rhs);
        implicit.alpha() = -dt*d[0];
        pcg( implicit, k_[0], rhs, g.precond(), eps_);

        dg::blas1::axpby( 1., u0_, b[1][0], k_[0], u1);
        f(u1, f_);
        dg::blas1::axpby( 1., u0_, c[1][0], k_[0], u1);
        g(u1, g_);
        dg::blas1::axpby( dt, f_, dt, g_, rhs);
        blas2::symv( g.weights(), rhs, rhs);
        implicit.alpha() = -dt*d[1];
        pcg( implicit, k_[1], rhs, g.precond(), eps_);

        dg::blas1::axpby( 1., u0_, b[2][0], k_[0], u1);
        dg::blas1::axpby( b[2][1], k_[1], 1., u1);
        f(u1, f_);
        dg::blas1::axpby( 1., u0_, c[2][0], k_[0], u1);
        dg::blas1::axpby( c[2][1], k_[1], 1., u1);
        g(u1, g_);
        dg::blas1::axpby( dt, f_, dt, g_, rhs);
        blas2::symv( g.weights(), rhs, rhs);
        implicit.alpha() = -dt*d[2];
        pcg( implicit, k_[2], rhs, g.precond(), eps_);
        //sum up results
        dg::blas1::axpby( 1., u0_, w[0], k_[0], u1);
        dg::blas1::axpby( w[1], k_[1], 1., u1);
        dg::blas1::axpby( w[2], k_[2], 1., u1);
    }

    /**
     * @brief adapt timestep
     *
     * Make same timestep twice, once with half timestep. The resulting error should be smaller than some given tolerance
     *
     * @tparam Explicit Object containing explicit part 
     * @tparam Imp Object containing implicit part ( must return precond() and weights())
     * @param f explicit part of the equations
     * @param g implicit part of the equations
     * @param u0 start point
     * @param u1 end point (write only)
     * @param dt timestep ( read and write) contains new recommended timestep afterwards
     * @param tolerance tolerable error
     */
    template <class Explicit, class Imp>
    void adaptive_step( Explicit& f, Imp& g, const Vector& u0, Vector& u1, double& dt, double tolerance)
    {
        Vector temp = u0;
        this->operator()( f, g, u0, u1, dt/2.);
        this->operator()( f, g, u1, temp, dt/2.);
        this->operator()( f, g, u0, u1, dt);
        dg::blas1::axpby( 1., u1, -1., temp);
        double error = dg::blas1::dot( temp, temp);
        std::cout << "ERROR " << error<< std::endl;
        double dt_old = dt;
        dt = 0.9*dt_old*sqrt(tolerance/error);
        if( dt > 1.5*dt_old) dt = 1.5*dt_old;
        if( dt < 0.75*dt_old) dt = 0.75*dt_old;
    }
    private:
    std::vector<Vector> k_;
    Vector f_, g_, rhs;
    double w[3];
    double b[3][3];
    double d[3];
    double c[3][3];
    CG<Vector> pcg; 
    double eps_;
};

} //namespace dg
