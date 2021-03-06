#error Documentation only
/*! @namespace eule
 * @brief This is the namespace for all functions and 
 * classes defined and used by the ASELA solvers.
 */
/*! @mainpage
 * Welcome to the ASELA library. 
 *
 * Qadsf
 * @par Design principles
 *
 * The solver is built on top of the dg library which implements container free numerical algorithms, i.e. a variety of platforms can be used to run the code on including
  GPUs, single-core CPU, OpenMP shared memory and MPI distributed memory systems. 
 *
 * @par Project description
 *
 * The solver implements the solution of 3D electrostatic gyrofluid equations in 
 toroidal geometry:
 \f[
    \begin{align}
        \partial_t N + \bar \nabla_\parallel\left( NU\right) - NU\bar \nabla_\parallel \ln B  
        + \frac{1}{B}[\psi, N]_{RZ}
        + N \mathcal K(\psi) 
        + \frac{\mu}{2} \mathcal K(NU^2) 
        + \tau \mathcal K(N) 
      = -\nu_\perp\Delta_\perp^2 N + \nu_\parallel\Delta_\parallel N  \\
      \partial_t\left( U + \frac{\beta}{\mu}A_\parallel \right) 
      + U\bar \nabla_\parallel U + 
      \frac{1}{B}\left[\psi, U\right]_{RZ}
        + \left( 2\tau + \frac{\mu }{2}U^2 \right)\mathcal K(U) 
        + \tau U \mathcal K(\ln N) 
        + \frac{1}{2} U\mathcal K(\psi) 
        = -\frac{\tau}{\mu} \bar \nabla_\parallel \ln N 
        - \frac{1}{\mu}\bar \nabla_\parallel \psi 
        + \frac{C}{\mu} (U_e - U_i) 
        - \nu_\perp\Delta_\perp^2 U + \nu_\parallel\Delta_\parallel U  
        \end{align}
\f]
together with
\f[
    \begin{align}
        \nabla\cdot\left( \frac{N}{B^2}\nabla_\perp \phi \right) &= N_e - \Gamma_1 N_i \label{eq:Egfmaxwella} \\
        \nabla\cdot\left( \nabla_\perp A_\parallel \right) &= N_e U_e -  U_iN_i 
        \label{}
    \end{align}
\f]
 *
 */
