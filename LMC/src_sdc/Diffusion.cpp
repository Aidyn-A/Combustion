//
// Comment out this line to use diffusion class outside
// the context of NavierStokes and classes derived from it.
//
#include <winstd.H>

#define USE_NAVIERSTOKES 1

#include <Box.H>
#include <BoxArray.H>
#include <Geometry.H>
#include <Geometry.H>
#include <ParmParse.H>
#include <ErrorList.H>
#ifdef USE_NAVIERSTOKES
#include <NavierStokes.H>
#endif
#include <Diffusion.H>
#include <MultiGrid.H>
#include <CGSolver.H>

#include <DIFFUSION_F.H>
#include <VISCOPERATOR_F.H>

#include <algorithm>
#include <cfloat>
#include <iomanip>

#if defined(BL_OSF1)
#if defined(BL_USE_DOUBLE)
const Real BL_BOGUS      = DBL_QNAN;
#else
const Real BL_BOGUS      = FLT_QNAN;
#endif
#else
const Real BL_BOGUS      = 1.e200;
#endif

const Real BL_SAFE_BOGUS = -666.e200;
//
// Include files for tensor solve.
//
#include <DivVis.H>
#include <LO_BCTYPES.H>
#include <MCMultiGrid.H>
#include <MCCGSolver.H>
#include <ViscBndryTensor.H>

#ifdef MG_USE_HYPRE
#include <HypreABec.H>
#endif

#ifdef MG_USE_F90_SOLVERS
#include <MGT_Solver.H>
#include <stencil_types.H>
#include <mg_cpp_f.h>
#endif

#define DEF_LIMITS(fab,fabdat,fablo,fabhi)   \
const int* fablo = (fab).loVect();           \
const int* fabhi = (fab).hiVect();           \
Real* fabdat = (fab).dataPtr();

#define DEF_CLIMITS(fab,fabdat,fablo,fabhi)  \
const int* fablo = (fab).loVect();           \
const int* fabhi = (fab).hiVect();           \
const Real* fabdat = (fab).dataPtr();

#define GEOM_GROW 1

namespace
{
    bool initialized = false;
}
//
// Set default values in !initialized section of code in constructor!!!
//
int         Diffusion::verbose;
Real        Diffusion::visc_tol;
int         Diffusion::do_reflux;
int         Diffusion::max_order;
int         Diffusion::scale_abec;
int         Diffusion::use_cg_solve;
int         Diffusion::tensor_max_order;
int         Diffusion::use_tensor_cg_solve;
bool        Diffusion::use_mg_precond_flag;

Array<Real> Diffusion::visc_coef;
Array<int>  Diffusion::is_diffusive;

namespace
{
#ifdef MG_USE_HYPRE
    bool use_hypre_solve;
#endif
#ifdef MG_USE_F90_SOLVERS
    bool use_fboxlib_mg;
#endif
}

void
Diffusion::Finalize ()
{
    visc_coef.clear();
    is_diffusive.clear();

    initialized = false;
}

Diffusion::Diffusion (Amr*               Parent,
                      AmrLevel*          Caller,
                      Diffusion*         Coarser,
                      int                num_state,
                      FluxRegister*      Viscflux_reg,
                      const Array<int>&  _is_diffusive,
                      const Array<Real>& _visc_coef)
    :
    parent(Parent),
    caller(Caller),
    grids(caller->boxArray()),
    level(caller->Level()),
    coarser(Coarser),
    finer(0),
    NUM_STATE(num_state),
    viscflux_reg(Viscflux_reg)

{
    if (!initialized)
    {
        //
        // Set defaults here!!!
        //
        Diffusion::verbose             = 0;
        Diffusion::visc_tol            = 1.0e-10;
        Diffusion::do_reflux           = 1;
        Diffusion::max_order           = 2;
        Diffusion::scale_abec          = 0;
        Diffusion::use_cg_solve        = 0;
        Diffusion::tensor_max_order    = 2;
        Diffusion::use_tensor_cg_solve = 0;
        Diffusion::use_mg_precond_flag = false;

#ifdef MG_USE_HYPRE
        use_hypre_solve = false;
#endif
#ifdef MG_USE_F90_SOLVERS
        use_fboxlib_mg  = false;
#endif
        int use_mg_precond = 0;

        ParmParse ppdiff("diffuse");

        ppdiff.query("v",                   verbose);
        ppdiff.query("max_order",           max_order);
        ppdiff.query("scale_abec",          scale_abec);
        ppdiff.query("use_cg_solve",        use_cg_solve);
        ppdiff.query("use_mg_precond",      use_mg_precond);
        ppdiff.query("tensor_max_order",    tensor_max_order);
        ppdiff.query("use_tensor_cg_solve", use_tensor_cg_solve);

#ifdef MG_USE_HYPRE
        ppdiff.query("use_hypre_solve", use_hypre_solve);
        if ( use_cg_solve && use_hypre_solve )
        {
            BoxLib::Error("Diffusion::read_params: cg_solve && .not. hypre_solve");
        }
#endif

#ifdef MG_USE_F90_SOLVERS
        ppdiff.query("use_fboxlib_mg", use_fboxlib_mg);
        if ( use_cg_solve && use_fboxlib_mg )
        {
            BoxLib::Error("Diffusion::read_params: cg_solve && .not. fboxlib_solve");
        }
#endif
        use_mg_precond_flag = (use_mg_precond ? true : false);

        ParmParse pp("ns");

        pp.query("visc_tol",  visc_tol);
        pp.query("do_reflux", do_reflux);

        do_reflux = (do_reflux ? 1 : 0);

        const int n_visc = _visc_coef.size();
        const int n_diff = _is_diffusive.size();

        if (n_diff < NUM_STATE)
            BoxLib::Abort("Diffusion::Diffusion(): is_diffusive array is not long enough");

        if (n_visc < NUM_STATE)
            BoxLib::Abort("Diffusion::Diffusion(): visc_coef array is not long enough");

        if (n_visc > NUM_STATE)
            BoxLib::Abort("Diffusion::Diffusion(): TOO MANY diffusion coeffs were given!");

        visc_coef.resize(NUM_STATE);
        is_diffusive.resize(NUM_STATE);

        for (int i = 0; i < NUM_STATE; i++)
        {
            is_diffusive[i] = _is_diffusive[i];
            visc_coef[i] = _visc_coef[i];
        }

        echo_settings();

        BoxLib::ExecOnFinalize(Diffusion::Finalize);

        initialized = true;
    }

    if (level > 0)
    {
        crse_ratio = parent->refRatio(level-1);
        coarser->finer = this;
    }
}

Diffusion::~Diffusion () {}

FluxRegister*
Diffusion::viscFluxReg ()
{
    return viscflux_reg;
}

int
Diffusion::maxOrder() const
{
    return max_order;
}

int
Diffusion::tensorMaxOrder() const
{
    return tensor_max_order;
}

void
Diffusion::echo_settings () const
{
    //
    // Print out my settings.
    //
    if (verbose && ParallelDescriptor::IOProcessor())
    {
        std::cout << "Diffusion settings...\n";
        std::cout << "  From diffuse:\n";
        std::cout << "   use_cg_solve        = " << use_cg_solve        << '\n';
        std::cout << "   use_tensor_cg_solve = " << use_tensor_cg_solve << '\n';
        std::cout << "   use_mg_precond_flag = " << use_mg_precond_flag << '\n';
        std::cout << "   max_order           = " << max_order           << '\n';
        std::cout << "   tensor_max_order    = " << tensor_max_order    << '\n';
        std::cout << "   scale_abec          = " << scale_abec          << '\n';
    
        std::cout << "\n\n  From ns:\n";
        std::cout << "   do_reflux           = " << do_reflux << '\n';
        std::cout << "   visc_tol            = " << visc_tol  << '\n';
    
        std::cout << "   is_diffusive =";
        for (int i =0; i < NUM_STATE; i++)
            std::cout << "  " << is_diffusive[i];
    
        std::cout << "\n   visc_coef =";
        for (int i = 0; i < NUM_STATE; i++)
            std::cout << "  " << visc_coef[i];

        std::cout << '\n';
    }
}

Real
Diffusion::get_scaled_abs_tol (const MultiFab& rhs,
                               Real            reduction) const
{
    Real norm_est = 0;
    for (MFIter Rhsmfi(rhs); Rhsmfi.isValid(); ++Rhsmfi)
        norm_est = std::max(norm_est, rhs[Rhsmfi].norm(0));
    ParallelDescriptor::ReduceRealMax(norm_est);

    return norm_est * reduction;
}

void
Diffusion::diffuse_scalar (Real                   dt,
                           int                    sigma,
                           Real                   be_cn_theta,
                           const MultiFab*        rho_half,
                           int                    rho_flag,
                           MultiFab* const*       fluxn,
                           MultiFab* const*       fluxnp1,
                           int                    fluxComp,
                           MultiFab*              delta_rhs, 
                           int                    rhsComp,
                           const MultiFab*        alpha, 
                           int                    alphaComp,
                           const MultiFab* const* betan, 
                           const MultiFab* const* betanp1,
                           int                    betaComp,
                           const SolveMode&       solve_mode,
                           bool                   add_old_time_divFlux)
{
    //
    // This routine expects that physical BC's have been loaded into
    // the grow cells of the old and new state at this level.  If rho_flag==2,
    // the values there are rho.phi, where phi is the quantity being diffused.
    // Values in these cells will be preserved.  Also, if there are any
    // explicit update terms, these have already incremented the new state
    // on the valid region (i.e., on the valid region the new state is the old
    // state + dt*Div(explicit_fluxes), e.g.)
    //

    NavierStokes& ns = *(NavierStokes*) &(parent->getLevel(level));

    if (verbose && ParallelDescriptor::IOProcessor())
      std::cout << "... diffusing scalar: " << caller->get_desc_lst()[State_Type].name(sigma) << '\n';

    int allnull, allthere;
    checkBeta(betan, allthere, allnull);
    checkBeta(betanp1, allthere, allnull);

    BL_ASSERT(solve_mode==ONEPASS || (delta_rhs && delta_rhs->boxArray()==grids));
    //
    // At this point, S_old has bndry at time N, S_new has bndry at time N+1
    //
    MultiFab& S_old = caller->get_old_data(State_Type);
    MultiFab& S_new = caller->get_new_data(State_Type);

    //
    // Set up Rhs.
    //
    MultiFab Rhs(grids,1,0), Soln(grids,1,1);
    if (add_old_time_divFlux)
    {
        Real a = 0.0;
        Real b = -(1.0-be_cn_theta)*dt;
        if (allnull)
            b *= visc_coef[sigma];
        ViscBndry visc_bndry_0;
        const Real prev_time   = caller->get_state_data(State_Type).prevTime();
        ABecLaplacian* visc_op = getViscOp(sigma,a,b,prev_time,visc_bndry_0,
                                           rho_half,rho_flag,0,betan,betaComp);
        visc_op->maxOrder(max_order);
        //
        // Copy to single-component multifab, then apply op to rho-scaled state
        //
        MultiFab::Copy(Soln,S_old,sigma,0,1,0);
        if (rho_flag == 2)
            for (MFIter Smfi(Soln); Smfi.isValid(); ++Smfi)
                Soln[Smfi].divide(S_old[Smfi],Smfi.validbox(),Density,0,1);
        visc_op->apply(Rhs,Soln);
        visc_op->compFlux(D_DECL(*fluxn[0],*fluxn[1],*fluxn[2]),Soln,LinOp::Inhomogeneous_BC,false,0,fluxComp);
        for (int i = 0; i < BL_SPACEDIM; ++i)
            (*fluxn[i]).mult(-b/(dt*caller->Geom().CellSize()[i]),fluxComp,1,0);
        delete visc_op;
    }
    else
    {
        Rhs.setVal(0.0);
    }
    //
    // If this is a predictor step, put "explicit" updates passed via S_new
    // into delta_rhs after scaling by rho_half if reqd, so they dont get lost,
    // pull it off S_new to avoid double counting
    //   (for rho_flag == 1:
    //       S_new = S_old - dt.(U.Grad(phi)); want Rhs -= rho_half.(U.Grad(phi)),
    //    else
    //       S_new = S_old - dt.Div(U.Phi),   want Rhs -= Div(U.Phi) )
    //
    if (solve_mode == PREDICTOR)
    {
        FArrayBox tmpfab;
        for (MFIter Smfi(S_new); Smfi.isValid(); ++Smfi)
        {
            const Box& box = Smfi.validbox();
            tmpfab.resize(box,1);
            tmpfab.copy(S_new[Smfi],box,sigma,box,0,1);
            tmpfab.minus(S_old[Smfi],box,sigma,0,1);
            S_new[Smfi].minus(tmpfab,box,0,sigma,1); // Remove this term from S_new
            tmpfab.mult(1.0/dt,box,0,1);
            if (rho_flag == 1)
                tmpfab.mult((*rho_half)[Smfi],box,0,0,1);
            if (alpha!=0)
                tmpfab.mult((*alpha)[Smfi],box,alphaComp,0,1);            
            (*delta_rhs)[Smfi].plus(tmpfab,box,0,rhsComp,1);
        }
    }
    //
    // Add body sources
    //
    if (delta_rhs != 0)
    {
        FArrayBox tmpfab, volume;
        for (MFIter mfi(*delta_rhs); mfi.isValid(); ++mfi)
        {
            caller->Geom().GetVolume(volume,grids,mfi.index(),GEOM_GROW);
            const Box& box = mfi.validbox();
            tmpfab.resize(box,1);
            tmpfab.copy((*delta_rhs)[mfi],box,rhsComp,box,0,1);
            tmpfab.mult(dt,box,0,1);
            tmpfab.mult(volume,box,0,0,1);
            Rhs[mfi].plus(tmpfab,box,0,0,1);
        }
    }

    //
    // Add hoop stress for x-velocity in r-z coordinates
    // Note: we have to add hoop stress explicitly because the hoop
    // stress which is added through the operator in getViscOp
    // is eliminated by setting a = 0.
    //
#if (BL_SPACEDIM == 2) 
    if (sigma == Xvel && Geometry::IsRZ())
    {
        Array<Real> rcen;

        FArrayBox volume;

        for (MFIter Rhsmfi(Rhs); Rhsmfi.isValid(); ++Rhsmfi)
        {
            const int  i    = Rhsmfi.index();
            const Box& bx   = Rhsmfi.validbox();

            caller->Geom().GetVolume(volume,grids,i,GEOM_GROW);

            Box sbx  = BoxLib::grow(S_old.box(i),S_old.nGrow());
            Box vbox = volume.box();

            rcen.resize(bx.length(0));
            parent->Geom(level).GetCellLoc(rcen, bx, 0);

            const int*  lo      = bx.loVect();
            const int*  hi      = bx.hiVect();
            const int*  slo     = sbx.loVect();
            const int*  shi     = sbx.hiVect();
            Real*       rhs     = Rhs[Rhsmfi].dataPtr();
            const Real* sdat    = S_old[Rhsmfi].dataPtr(sigma);
            const Real* rcendat = rcen.dataPtr();
            const Real  coeff   = (1.0-be_cn_theta)*visc_coef[sigma]*dt;
            const Real* voli    = volume.dataPtr();
            const int*  vlo     = vbox.loVect();
            const int*  vhi     = vbox.hiVect();
            FORT_HOOPRHS(rhs, ARLIM(lo), ARLIM(hi), 
                         sdat, ARLIM(slo), ARLIM(shi),
                         rcendat, &coeff, voli, ARLIM(vlo),ARLIM(vhi));
        }
    }
#endif
    //
    // Increment Rhs with S_old*V (or S_old*V*rho_half if rho_flag==1
    //                             or S_old*V*rho_old  if rho_flag==3)
    //  (Note: here S_new holds S_old, but also maybe an explicit increment
    //         from advection if solve_mode != PREDICTOR)
    //
    MultiFab::Copy(Soln,S_new,sigma,0,1,0);

    {
        FArrayBox volume;

        for (MFIter mfi(Soln); mfi.isValid(); ++mfi)
        {
            caller->Geom().GetVolume(volume,grids,mfi.index(),GEOM_GROW);
            const Box& box = mfi.validbox();
            Soln[mfi].mult(volume,box,0,0,1);
            if (rho_flag == 1)
                Soln[mfi].mult((*rho_half)[mfi],box,0,0,1);
            if (rho_flag == 3)
                Soln[mfi].mult((*ns.rho_ptime)[mfi],box,0,0,1);
            if (alpha!=0)
                Soln[mfi].mult((*alpha)[mfi],box,alphaComp,0,1);
            Rhs[mfi].plus(Soln[mfi],box,0,0,1);
        }
    }

    //
    // Make a good guess for Soln
    //
    MultiFab::Copy(Soln,S_new,sigma,0,1,0);
    if (rho_flag == 2)
        for (MFIter Smfi(Soln); Smfi.isValid(); ++Smfi)
            Soln[Smfi].divide(S_new[Smfi],Smfi.validbox(),Density,0,1);

    //
    // Construct viscous operator with bndry data at time N+1.
    //
    Real a = 1.0;
    Real b = be_cn_theta*dt;
    if (allnull)
        b *= visc_coef[sigma];
    ViscBndry  visc_bndry;
    const Real cur_time = caller->get_state_data(State_Type).curTime();
    Real       rhsscale = 1.0;

    ABecLaplacian* visc_op  = getViscOp(sigma,a,b,cur_time,visc_bndry,rho_half,
                                        rho_flag,&rhsscale,betanp1,betaComp,alpha,alphaComp);
    Rhs.mult(rhsscale,0,1);
    visc_op->maxOrder(max_order);

    //
    // Construct solver and call it.
    //
    const Real S_tol     = visc_tol;
    const Real S_tol_abs = get_scaled_abs_tol(Rhs, visc_tol);
    if (use_cg_solve)
    {
        CGSolver cg(*visc_op,use_mg_precond_flag);
        cg.solve(Soln,Rhs,S_tol,S_tol_abs);
    }

#ifdef MG_USE_F90_SOLVERS
    else if ( use_fboxlib_mg )
      {
        std::vector<BoxArray> bav(1);
        bav[0] = S_new.boxArray();
        std::vector<DistributionMapping> dmv(1);
        dmv[0] = Rhs.DistributionMap();
        bool nodal = false;
	int stencil = CC_CROSS_STENCIL;
        std::vector<Geometry> geom(1);
        geom[0] = visc_bndry.getGeom();
        const BCRec& scal_bc = caller->get_desc_lst()[State_Type].getBC(sigma);
 
        int mg_bc[2*BL_SPACEDIM];
        for ( int i = 0; i < BL_SPACEDIM; ++i )
        {
          if ( geom[0].isPeriodic(i) )
          {
            mg_bc[i*2 + 0] = 0;
            mg_bc[i*2 + 1] = 0;
          }
        else
          {
            mg_bc[i*2 + 0] = scal_bc.lo(i)==EXT_DIR? MGT_BC_DIR : MGT_BC_NEU;
            mg_bc[i*2 + 1] = scal_bc.hi(i)==EXT_DIR? MGT_BC_DIR : MGT_BC_NEU;
          }
        }

        MGT_Solver mgt_solver(geom, mg_bc, bav, dmv, nodal, stencil);

        // Set xa and xb locally so we don't have to pass the mac_bndry to set_mac_coefficients
        Array< Array<Real> > xa(1);
        Array< Array<Real> > xb(1);
 
        xa[0].resize(BL_SPACEDIM);
        xb[0].resize(BL_SPACEDIM);
 
        if (level == 0) {
          for ( int i = 0; i < BL_SPACEDIM; ++i ) {
            xa[0][i] = 0.;
            xb[0][i] = 0.;
          }
        } else {
          const Real* dx_crse   = parent->Geom(level-1).CellSize();
          for ( int i = 0; i < BL_SPACEDIM; ++i ) {
            xa[0][i] = 0.5 * dx_crse[i];
            xb[0][i] = 0.5 * dx_crse[i];
          }
        }

        // Set alpha and beta as in (alpha - del dot beta grad)
        const MultiFab* aa_p[1];
        aa_p[0] = &(visc_op->aCoefficients());
        const MultiFab* bb_p[1][BL_SPACEDIM];
        for ( int i = 0; i < BL_SPACEDIM; ++i )
        {
            bb_p[0][i] = &(visc_op->bCoefficients(i));
        }
        mgt_solver.set_visc_coefficients(aa_p, bb_p, b, xa, xb);

        MultiFab* phi_p[1];
        MultiFab* Rhs_p[1];
        phi_p[0] = &Soln;
        Rhs_p[0] = &Rhs;
  
        Real final_resnorm;
        mgt_solver.solve(phi_p, Rhs_p, S_tol, S_tol_abs, visc_bndry, final_resnorm);
      }
#endif

#ifdef MG_USE_HYPRE
    else if ( use_hypre_solve )
      {
	BoxLib::Error("HypreABec not ready");
	Real* dx = 0;
	HypreABec hp(Soln.boxArray(), visc_bndry, dx, 0, false);
	hp.setup_solver(S_tol, S_tol_abs, 50);
	hp.solve(Soln, Rhs, true);
	hp.clear_solver();
      }
#endif
    else
    {
        MultiGrid mg(*visc_op);
        mg.solve(Soln,Rhs,S_tol,S_tol_abs);
    }
    Rhs.clear();
    //
    // Get extensivefluxes from new-time op
    //
    bool do_applyBC = true;
    visc_op->compFlux(D_DECL(*fluxnp1[0],*fluxnp1[1],*fluxnp1[2]),Soln,LinOp::Inhomogeneous_BC,do_applyBC,0,fluxComp);
    for (int i = 0; i < BL_SPACEDIM; ++i)
        (*fluxnp1[i]).mult(b/(dt*caller->Geom().CellSize()[i]),fluxComp,1,0);
    delete visc_op;
    //
    // Copy into state variable at new time, without bc's
    //
    MultiFab::Copy(S_new,Soln,0,sigma,1,0);
    
    if (rho_flag == 2)
        for (MFIter Smfi(S_new); Smfi.isValid(); ++Smfi)
            S_new[Smfi].mult(S_new[Smfi],Smfi.validbox(),Density,sigma,1);
}

void
Diffusion::diffuse_velocity (Real                   dt,
                             Real                   be_cn_theta,
                             const MultiFab*        rho_half,
                             int                    rho_flag,
                             MultiFab*              delta_rhs,
                             int                    rhsComp,
                             const MultiFab* const* betan, 
                             const MultiFab* const* betanp1,
                             int                    betaComp)
{
    if (verbose && ParallelDescriptor::IOProcessor())
        std::cout << "... diffuse_velocity\n";

    int allnull, allthere;
    checkBetas(betan, betanp1, allthere, allnull);
 
    BL_ASSERT( rho_flag == 1 || rho_flag == 3);

#ifndef NDEBUG
    for (int d = 0; d < BL_SPACEDIM; ++d)
        BL_ASSERT(allnull ? visc_coef[Xvel+d]>=0 : betan[d]->min(0,0) >= 0.0);
#endif

    if (allnull)
    {
        MultiFab* *fluxSCn;
        MultiFab* *fluxSCnp1;

        allocFluxBoxesLevel(fluxSCn,  0,1);
        allocFluxBoxesLevel(fluxSCnp1,0,1);

        MultiFab fluxes[BL_SPACEDIM];

        if (do_reflux && level < parent->finestLevel())
        {
            for (int i = 0; i < BL_SPACEDIM; i++)
            {
                BoxArray ba = rho_half->boxArray();
                ba.surroundingNodes(i);
                fluxes[i].define(ba, BL_SPACEDIM, 0, Fab_allocate);
            }
        }

        for (int sigma = 0; sigma < BL_SPACEDIM; ++sigma)
        {
            const int state_ind = Xvel + sigma;
        
            int fluxComp = 0;
            int RHSComp = rhsComp + sigma;
            diffuse_scalar(dt,state_ind,be_cn_theta,rho_half,rho_flag,
                           fluxSCn,fluxSCnp1,fluxComp,delta_rhs,RHSComp);

            if (do_reflux)
            {
                for (int d = 0; d < BL_SPACEDIM; ++d)
                {
                    for (MFIter fmfi(*fluxSCn[d]); fmfi.isValid(); ++fmfi)
                    {
                        (*fluxSCnp1[d])[fmfi].plus((*fluxSCn[d])[fmfi]);

                        if (level < parent->finestLevel())
                            fluxes[d][fmfi.index()].copy((*fluxSCnp1[d])[fmfi],fluxComp,sigma,1);

                        if (level > 0)
                            viscflux_reg->FineAdd((*fluxSCnp1[d])[fmfi],d,fmfi.index(),fluxComp,sigma,1,dt);
                    }
                }
            }
        }

        if (level < parent->finestLevel())
        {
            for (int d = 0; d < BL_SPACEDIM; ++d)
                finer->viscflux_reg->CrseInit(fluxes[d],d,0,0,BL_SPACEDIM,-dt);
        }

        removeFluxBoxesLevel(fluxSCn);
        removeFluxBoxesLevel(fluxSCnp1);
    }
    else
    {
        diffuse_tensor_velocity(dt,be_cn_theta,rho_half,rho_flag,
                                delta_rhs,rhsComp,betan,betanp1,betaComp);
    }
}

void
Diffusion::diffuse_tensor_velocity (Real                   dt,
                                    Real                   be_cn_theta,
                                    const MultiFab*        rho_half,
                                    int                    rho_flag, 
                                    MultiFab*              delta_rhs,
                                    int                    rhsComp,
                                    const MultiFab* const* betan, 
                                    const MultiFab* const* betanp1,
                                    int                    betaComp)
{
    BL_ASSERT(rho_flag == 1 || rho_flag == 3);
    const int finest_level = parent->finestLevel();
    NavierStokes& ns    = *(NavierStokes*) &(parent->getLevel(level));
    //
    // At this point, S_old has bndry at time N S_new contains GRAD(SU).
    //
    MultiFab&  U_old     = caller->get_old_data(State_Type);
    MultiFab&  U_new     = caller->get_new_data(State_Type);
    const Real cur_time  = caller->get_state_data(State_Type).curTime();
    const Real prev_time = caller->get_state_data(State_Type).prevTime();

    int allnull, allthere;
    checkBetas(betan, betanp1, allthere, allnull);
    //
    // U_new now contains the inviscid update of U.
    // This is part of the RHS for the viscous solve.
    //
    MultiFab Rhs(grids,BL_SPACEDIM,0);
    Rhs.setVal(0.0);

    MultiFab** tensorflux_old;
    {
        //
        // Set up Rhs.
        //
        const int soln_old_grow = 1;
        MultiFab Soln_old(grids,BL_SPACEDIM,soln_old_grow);
        const Real a = 0.0;
        Real       b = -(1.0-be_cn_theta)*dt;
        if (allnull)
            b *= visc_coef[Xvel];
        ViscBndryTensor visc_bndry;
        const MultiFab* rho = (rho_flag == 1) ? rho_half : ns.rho_ptime;
        
        DivVis* tensor_op = getTensorOp(a,b,prev_time,visc_bndry,rho,betan,betaComp);
        tensor_op->maxOrder(tensor_max_order);
        //
        // Copy to single-component multifab.  Use Soln as a temporary here.
        //
        MultiFab::Copy(Soln_old,U_old,Xvel,0,BL_SPACEDIM,0);
        tensor_op->apply(Rhs,Soln_old);

        if (do_reflux && (level<finest_level || level>0))
        {
            allocFluxBoxesLevel(tensorflux_old,0,BL_SPACEDIM);
            tensor_op->compFlux(D_DECL(*(tensorflux_old[0]),
                                       *(tensorflux_old[1]),
                                       *(tensorflux_old[2])),Soln_old);
            for (int d = 0; d < BL_SPACEDIM; d++)
                tensorflux_old[d]->mult(-b/(dt*caller->Geom().CellSize()[d]),0);
        }
        delete tensor_op;

        MultiFab volume;

        caller->Geom().GetVolume(volume,grids,GEOM_GROW);

        for (int comp = 0; comp < BL_SPACEDIM; comp++)
        {
            int sigma = Xvel + comp;
            //
            // Complete Rhs by adding body sources.
            //
            for (MFIter Rhsmfi(Rhs); Rhsmfi.isValid(); ++Rhsmfi)
            {
                BL_ASSERT(grids[Rhsmfi.index()] == Rhsmfi.validbox());
                //
                // Scale inviscid part by volume.
                //
                U_new[Rhsmfi].mult(volume[Rhsmfi],Rhsmfi.validbox(),0,sigma,1);
                //
                // Multiply by density at time nph (if rho_flag==1)
                //                     or time n   (if rho_flag==3).
                //
                if (rho_flag == 1)
                {
                  const FArrayBox& Rh = (*rho_half)[Rhsmfi];
                  U_new[Rhsmfi].mult(Rh,Rhsmfi.validbox(),0,sigma,1);
                }
                if (rho_flag == 3)
                {
                  FArrayBox& Rh = (*ns.rho_ptime)[Rhsmfi.index()];
                  U_new[Rhsmfi].mult(Rh,Rhsmfi.validbox(),0,sigma,1);
                }
                //
                // Add to Rhs which contained operator applied to U_old.
                //
                Rhs[Rhsmfi].plus(U_new[Rhsmfi],Rhsmfi.validbox(),sigma,comp,1);
            }

            if (delta_rhs != 0)
            {
                for (MFIter Rhsmfi(Rhs); Rhsmfi.isValid(); ++Rhsmfi)
                {
                    BL_ASSERT(grids[Rhsmfi.index()] == Rhsmfi.validbox());
                    (*delta_rhs)[Rhsmfi].mult(dt,comp+rhsComp,1);
                    (*delta_rhs)[Rhsmfi].mult(volume[Rhsmfi],Rhsmfi.validbox(),0,comp+rhsComp,1);
                    Rhs[Rhsmfi].plus((*delta_rhs)[Rhsmfi],Rhsmfi.validbox(),comp+rhsComp,comp,1);
                }
            }
        }

#if (BL_SPACEDIM == 2) 
        if (Geometry::IsRZ())
        {
            int fort_xvel_comp = Xvel+1;

            for (MFIter Rhsmfi(Rhs); Rhsmfi.isValid(); ++Rhsmfi)
            {
                const int  i  = Rhsmfi.index();
                const Box& bx = Rhsmfi.validbox();
                Box sbx       = BoxLib::grow(U_old.box(i),U_old.nGrow());
                Array<Real> rcen(bx.length(0));
                parent->Geom(level).GetCellLoc(rcen, bx, 0);
                const int*       lo        = bx.loVect();
                const int*       hi        = bx.hiVect();
                const int*       slo       = sbx.loVect();
                const int*       shi       = sbx.hiVect();
                Real*            rhs       = Rhs[Rhsmfi].dataPtr();
                const Real*      sdat      = U_old[Rhsmfi].dataPtr(Xvel);
                const Real*      rcendat   = rcen.dataPtr();
                const Real       coeff     = (1.0-be_cn_theta)*dt;
                const Real*      voli      = volume[Rhsmfi].dataPtr();
                Box              vbox      = volume[Rhsmfi].box();
                const int*       vlo       = vbox.loVect();
                const int*       vhi       = vbox.hiVect();
                const FArrayBox& betax     = (*betanp1[0])[Rhsmfi];
                const int*       betax_lo  = betax.loVect();
                const int*       betax_hi  = betax.hiVect();
                const Real*      betax_dat = betax.dataPtr(betaComp);
                const FArrayBox& betay     = (*betanp1[1])[Rhsmfi];
                const int*       betay_lo  = betay.loVect();
                const int*       betay_hi  = betay.hiVect();
                const Real*      betay_dat = betay.dataPtr(betaComp);

                FORT_TENSOR_HOOPRHS(&fort_xvel_comp, rhs, ARLIM(lo), ARLIM(hi), 
                                    sdat, ARLIM(slo), ARLIM(shi),
                                    rcendat, &coeff, 
                                    voli, ARLIM(vlo), ARLIM(vhi),
                                    betax_dat,ARLIM(betax_lo),ARLIM(betax_hi),
                                    betay_dat,ARLIM(betay_lo),ARLIM(betay_hi));
            }
        }
#endif
    }
    const int soln_grow = 1;
    MultiFab Soln(grids,BL_SPACEDIM,soln_grow);
    Soln.setVal(0.0);
    //
    // Compute guess of solution.
    //
    if (level == 0)
    {
        MultiFab::Copy(Soln,U_old,Xvel,0,BL_SPACEDIM,0);
    }
    else
    {
        caller->FillCoarsePatch(Soln,0,cur_time,State_Type,Xvel,BL_SPACEDIM);
    }
    //
    // Copy guess into U_new.
    //
    // The new-time operator is initialized with a "guess" for the new-time
    // state.  We intentionally initialize the grow cells with a bogus
    // value to emphasize that the values are not to be considered "valid"
    // (we shouldn't specify any grow cell information), but rather are to
    // filled by the "physics bc's, etc" in the problem-dependent code.  In
    // the course of this filling (typically while generating/filling the
    // BndryData object for the solvers), StateData::filcc is called to get
    // physical bc's.  Here 'something computable' has to already exist in
    // the grow cells (even though filcc ultimately will fill the corner
    // correctly, if applicable).  This is apparently where the
    // `something computable' is to be set.
    //
    int n_comp  = BL_SPACEDIM;
    int n_ghost = 1;
    U_new.setVal(BL_SAFE_BOGUS,Xvel,n_comp,n_ghost);
    n_ghost = 0;
    U_new.copy(Soln,0,Xvel,n_comp);
    //
    // Construct viscous operator with bndry data at time N+1.
    //
    const Real a = 1.0;
    Real       b = be_cn_theta*dt;
    if (allnull)
        b *= visc_coef[Xvel];
       
    ViscBndryTensor visc_bndry;
    const MultiFab* rho = (rho_flag == 1) ? rho_half : ns.rho_ctime;
    DivVis* tensor_op = getTensorOp(a,b,cur_time,visc_bndry,rho,betanp1,betaComp);
    tensor_op->maxOrder(tensor_max_order);
    //
    // Construct solver and call it.
    //
    const Real S_tol     = visc_tol;
    const Real S_tol_abs = -1;
    if (use_tensor_cg_solve)
    {
        const int use_mg_pre = 0;
        MCCGSolver cg(*tensor_op,use_mg_pre);
        cg.solve(Soln,Rhs,S_tol,S_tol_abs);
    }
    else
    {
        MCMultiGrid mg(*tensor_op);
        mg.solve(Soln,Rhs,S_tol,S_tol_abs);
    }
    Rhs.clear();

    int visc_op_lev = 0;
    tensor_op->applyBC(Soln,visc_op_lev); // This may not be needed.
    //
    // Copy into state variable at new time.
    //
    n_ghost = soln_grow;
    MultiFab::Copy(U_new,Soln,0,Xvel,n_comp,n_ghost);
    //
    // Modify diffusive fluxes here.
    //
    if (do_reflux && (level < finest_level || level > 0))
    {
        MultiFab** tensorflux;
        allocFluxBoxesLevel(tensorflux,0,BL_SPACEDIM);
        tensor_op->compFlux(D_DECL(*(tensorflux[0]), *(tensorflux[1]), *(tensorflux[2])),Soln);

        for (int d = 0; d < BL_SPACEDIM; d++)
        {
            tensorflux[d]->mult(b/(dt*caller->Geom().CellSize()[d]),0);
            tensorflux[d]->plus(*(tensorflux_old[d]),0,BL_SPACEDIM,0);
        }       

        removeFluxBoxesLevel(tensorflux_old);

        if (level > 0)
        {
            for (MFIter mfi(*(tensorflux[0])); mfi.isValid(); ++mfi)
            {
                const int i = mfi.index();

                for (int k = 0; k < BL_SPACEDIM; k++)
                    viscflux_reg->FineAdd((*(tensorflux[k]))[i],k,i,Xvel,Xvel,BL_SPACEDIM,dt);
            }
        }

        if (level < finest_level)
        {
            for (int d = 0; d < BL_SPACEDIM; d++)
                (*finer->viscflux_reg).CrseInit(*tensorflux[d],d,0,Xvel,BL_SPACEDIM,-dt);
        }

        removeFluxBoxesLevel(tensorflux);
    }

    delete tensor_op;
}

void
Diffusion::diffuse_Vsync (MultiFab*              Vsync,
                          Real                   dt,
                          Real                   be_cn_theta,
                          const MultiFab*        rho_half,
                          int                    rho_flag,
                          const MultiFab* const* beta,
                          int                    betaComp)
{
    BL_ASSERT(rho_flag == 1|| rho_flag == 3);

    int allnull, allthere;
    checkBeta(beta, allthere, allnull);

#ifndef NDEBUG
    for (int d = 0; d < BL_SPACEDIM; ++d)
        BL_ASSERT(allnull ? visc_coef[Xvel+d]>=0 : beta[d]->min(0,0) >= 0.0);
#endif

    if (allnull)
        diffuse_Vsync_constant_mu(Vsync,dt,be_cn_theta,rho_half,rho_flag);
    else
        diffuse_tensor_Vsync(Vsync,dt,be_cn_theta,rho_half,rho_flag,beta,betaComp);
    //
    // applyBC has put "incorrect" values in the ghost cells
    // outside external Dirichlet boundaries. Reset these to zero
    // so that syncproject and conservative interpolation works correctly.
    //
    Box domain = BoxLib::grow(caller->Geom().Domain(),1);

    for (int n = Xvel; n < Xvel+BL_SPACEDIM; n++)
    {
        const BCRec& velbc = caller->get_desc_lst()[State_Type].getBC(n);

        for (int k = 0; k < BL_SPACEDIM; k++)
        {
            if (velbc.hi(k) == EXT_DIR)
            {
                IntVect smallend = domain.smallEnd();
                smallend.setVal(k,domain.bigEnd(k));
                Box top_strip(smallend,domain.bigEnd(),IntVect::TheCellVector());
                Vsync->setVal(0.0,top_strip,n-Xvel,1,1);
            }
            if (velbc.lo(k) == EXT_DIR)
            {
                IntVect bigend = domain.bigEnd();
                bigend.setVal(k,domain.smallEnd(k));
                Box bottom_strip(domain.smallEnd(),bigend,IntVect::TheCellVector());
                Vsync->setVal(0.0,bottom_strip,n-Xvel,1,1);
            }
        }
    }
}

void
Diffusion::diffuse_Vsync_constant_mu (MultiFab*       Vsync,
                                      Real            dt,
                                      Real            be_cn_theta,
                                      const MultiFab* rho_half,
                                      int             rho_flag)
{
    if (verbose && ParallelDescriptor::IOProcessor())
        std::cout << "Diffusion::diffuse_Vsync_constant_mu ...\n";

    NavierStokes& ns     = *(NavierStokes*) &(parent->getLevel(level));
    const Real*   dx     = caller->Geom().CellSize();
    const int     IOProc = ParallelDescriptor::IOProcessorNumber();
    //
    // At this point in time we can only do decoupled scalar
    // so we loop over components.
    //
    MultiFab volume;

    caller->Geom().GetVolume(volume,grids,GEOM_GROW);

    for (int comp = 0; comp < BL_SPACEDIM; comp++)
    {
        MultiFab Rhs(grids,1,0);

        Rhs.setVal(0);

        MultiFab::Copy(Rhs,*Vsync,comp,0,1,0);

        if (verbose > 1)
        {
            Real r_norm = 0.0;
            for (MFIter Rhsmfi(Rhs); Rhsmfi.isValid(); ++Rhsmfi)
                r_norm = std::max(r_norm,Rhs[Rhsmfi].norm(0));
            ParallelDescriptor::ReduceRealMax(r_norm,IOProc);

            if (ParallelDescriptor::IOProcessor())
                std::cout << "Original max of Vsync " << r_norm << '\n';
        }
        //
        // Multiply RHS by volume and density.
        //
        const MultiFab* rho = (rho_flag == 1) ? rho_half : ns.rho_ctime;
        for (MFIter Rhsmfi(Rhs); Rhsmfi.isValid(); ++Rhsmfi)
        {
            Rhs[Rhsmfi].mult(volume[Rhsmfi]); 
            Rhs[Rhsmfi].mult((*rho)[Rhsmfi]); 
        }
        //
        // SET UP COEFFICIENTS FOR VISCOUS SOLVER.
        //
        const Real     a        = 1.0;
        const Real     b        = be_cn_theta*dt*visc_coef[comp];
        Real           rhsscale = 1.0;
        ABecLaplacian* visc_op  = getViscOp(comp,a,b,rho,rho_flag,&rhsscale);

        visc_op->maxOrder(max_order);
        Rhs.mult(rhsscale,0,1);
        //
        // Construct solver and call it.
        //
        const Real      S_tol     = visc_tol;
        const Real      S_tol_abs = -1;

        MultiFab Soln(grids,1,1);

        Soln.setVal(0);

        if (use_cg_solve)
        {
            CGSolver cg(*visc_op,use_mg_precond_flag);
            cg.solve(Soln,Rhs,S_tol,S_tol_abs);
        }

#ifdef MG_USE_HYPRE
	else if ( use_hypre_solve )
	  {
	    BoxLib::Error("HypreABec not ready");
	    //	    Real* dx = 0;
	    //	    HypreABec hp(Soln.boxArray(), visc_bndry, dx, 0, false);
	    //	    hp.setup_solver(S_tol, S_tol_abs, 50);
	    //	    hp.solve(Soln, Rhs, true);
	    //	    hp.clear_solver();
	  }
#endif
	else
        {
            MultiGrid mg(*visc_op);
            mg.solve(Soln,Rhs,S_tol,S_tol_abs);
        }
        Rhs.clear();

        int visc_op_lev = 0;
        visc_op->applyBC(Soln,0,1,visc_op_lev);

        MultiFab::Copy(*Vsync,Soln,0,comp,1,1);

        if (verbose > 1)
        {
            Real s_norm = 0.0;
            for (MFIter Solnmfi(Soln); Solnmfi.isValid(); ++Solnmfi)
                s_norm = std::max(s_norm,Soln[Solnmfi].norm(0));
            ParallelDescriptor::ReduceRealMax(s_norm,IOProc);

            if (ParallelDescriptor::IOProcessor())
                std::cout << "Final max of Vsync " << s_norm << '\n';
        }

        delete visc_op;

        if (level > 0)
        {
            FArrayBox xflux, yflux, zflux, area[BL_SPACEDIM];

            for (MFIter Vsyncmfi(*Vsync); Vsyncmfi.isValid(); ++Vsyncmfi)
            {
                BL_ASSERT(grids[Vsyncmfi.index()] == Vsyncmfi.validbox());

                const int  i      = Vsyncmfi.index();
                const Box& grd    = Vsyncmfi.validbox();
                const int* lo     = grd.loVect();
                const int* hi     = grd.hiVect();
                FArrayBox& u_sync = (*Vsync)[i];
                const int* ulo    = u_sync.loVect();
                const int* uhi    = u_sync.hiVect();

                for (int dir = 0; dir < BL_SPACEDIM; dir++)
                {
                    caller->Geom().GetFaceArea(area[dir],grids,i,dir,GEOM_GROW);
                }

                Box xflux_bx(grd);
                Box yflux_bx(grd);

                xflux_bx.surroundingNodes(0);
                yflux_bx.surroundingNodes(1);

                xflux.resize(xflux_bx,1);
                yflux.resize(yflux_bx,1);

                DEF_LIMITS(xflux,xflux_dat,xflux_lo,xflux_hi);
                DEF_LIMITS(yflux,yflux_dat,yflux_lo,yflux_hi);

                const FArrayBox& xarea = area[0];
                const FArrayBox& yarea = area[1];

                DEF_CLIMITS(xarea,xarea_dat,xarea_lo,xarea_hi);
                DEF_CLIMITS(yarea,yarea_dat,yarea_lo,yarea_hi);
                //
                // The extra factor of dt comes from the fact that Vsync
                // looks like dV/dt, not just an increment to V.
                //
                Real mult = -be_cn_theta*dt*dt*visc_coef[comp];

#if (BL_SPACEDIM == 2)
                FORT_VISCSYNCFLUX (u_sync.dataPtr(comp), ARLIM(ulo), ARLIM(uhi),
                                   lo,hi,
                                   xflux_dat,ARLIM(xflux_lo),ARLIM(xflux_hi),
                                   yflux_dat,ARLIM(yflux_lo),ARLIM(yflux_hi),
                                   xarea_dat,ARLIM(xarea_lo),ARLIM(xarea_hi),
                                   yarea_dat,ARLIM(yarea_lo),ARLIM(yarea_hi),
                                   dx,&mult);
#endif
#if (BL_SPACEDIM == 3)
                Box zflux_bx(grd);
                zflux_bx.surroundingNodes(2);
                zflux.resize(zflux_bx,1);
                DEF_LIMITS(zflux,zflux_dat,zflux_lo,zflux_hi);

                const FArrayBox& zarea = area[2];
                DEF_CLIMITS(zarea,zarea_dat,zarea_lo,zarea_hi);

                FORT_VISCSYNCFLUX (u_sync.dataPtr(comp), ARLIM(ulo), ARLIM(uhi),
                                   lo,hi,
                                   xflux_dat,ARLIM(xflux_lo),ARLIM(xflux_hi),
                                   yflux_dat,ARLIM(yflux_lo),ARLIM(yflux_hi),
                                   zflux_dat,ARLIM(zflux_lo),ARLIM(zflux_hi),
                                   xarea_dat,ARLIM(xarea_lo),ARLIM(xarea_hi),
                                   yarea_dat,ARLIM(yarea_lo),ARLIM(yarea_hi),
                                   zarea_dat,ARLIM(zarea_lo),ARLIM(zarea_hi),
                                   dx,&mult);
#endif
                Real one = 1.0;

                D_TERM(viscflux_reg->FineAdd(xflux,0,i,0,comp,1,one);,
                       viscflux_reg->FineAdd(yflux,1,i,0,comp,1,one);,
                       viscflux_reg->FineAdd(zflux,2,i,0,comp,1,one););
            }
        }
    }
}

void
Diffusion::diffuse_tensor_Vsync (MultiFab*              Vsync,
                                 Real                   dt,
                                 Real                   be_cn_theta,
                                 const MultiFab*        rho_half,
                                 int                    rho_flag,
                                 const MultiFab* const* beta,
                                 int                    betaComp)
{
    BL_ASSERT(rho_flag == 1 || rho_flag == 3);

    if (verbose && ParallelDescriptor::IOProcessor())
        std::cout << "Diffusion::diffuse_tensor_Vsync ...\n";

    NavierStokes& ns         = *(NavierStokes*) &(parent->getLevel(level));
    const int   IOProc       = ParallelDescriptor::IOProcessorNumber();

    MultiFab Rhs(grids,BL_SPACEDIM,0);

    MultiFab::Copy(Rhs,*Vsync,0,0,BL_SPACEDIM,0);

    if (verbose > 1)
    {
        Real r_norm = 0.0;
        for (MFIter Rhsmfi(Rhs); Rhsmfi.isValid(); ++Rhsmfi)
            r_norm = std::max(r_norm,Rhs[Rhsmfi].norm(0));
        ParallelDescriptor::ReduceRealMax(r_norm,IOProc);

        if (ParallelDescriptor::IOProcessor())
            std::cout << "Original max of Vsync " << r_norm << '\n';
    }
    //
    // Multiply RHS by volume and density.
    //
    {
        MultiFab volume;

        caller->Geom().GetVolume(volume,grids,GEOM_GROW);

        for (int comp = 0; comp < BL_SPACEDIM; comp++)
        {
            for (MFIter Rhsmfi(Rhs); Rhsmfi.isValid(); ++Rhsmfi)
            {
                Rhs[Rhsmfi].mult(volume[Rhsmfi],0,comp,1); 
                if (rho_flag == 1)
                    Rhs[Rhsmfi].mult((*rho_half)[Rhsmfi],0,comp,1); 
                if (rho_flag == 3)
                    Rhs[Rhsmfi].mult((*ns.rho_ptime)[Rhsmfi],0,comp,1); 
            }
        }
    }
    //
    // SET UP COEFFICIENTS FOR VISCOUS SOLVER.
    //
    const Real      a         = 1.0;
    const Real      b         = be_cn_theta*dt;
    const MultiFab* rho       = (rho_flag == 1) ? rho_half : ns.rho_ctime;
    DivVis*         tensor_op = getTensorOp(a,b,rho,beta,betaComp);
    tensor_op->maxOrder(tensor_max_order);

    MultiFab Soln(grids,BL_SPACEDIM,1);

    Soln.setVal(0);
    //
    // Construct solver and call it.
    //
    const Real S_tol     = visc_tol;
    const Real S_tol_abs = -1;
    if (use_tensor_cg_solve)
    {
        MCCGSolver cg(*tensor_op,use_mg_precond_flag);
        cg.solve(Soln,Rhs,S_tol,S_tol_abs);
    }
    else
    {
        MCMultiGrid mg(*tensor_op);
        mg.solve(Soln,Rhs,S_tol,S_tol_abs);
    }
    Rhs.clear();

    int visc_op_lev = 0;
    tensor_op->applyBC(Soln,visc_op_lev); 

    MultiFab::Copy(*Vsync,Soln,0,0,BL_SPACEDIM,1);

    if (verbose > 1)
    {
        Real s_norm = 0.0;
        for (MFIter Solnmfi(Soln); Solnmfi.isValid(); ++Solnmfi)
            s_norm = std::max(s_norm,Soln[Solnmfi].norm(0));
        ParallelDescriptor::ReduceRealMax(s_norm,IOProc);

        if (ParallelDescriptor::IOProcessor())
            std::cout << "Final max of Vsync " << s_norm << '\n';
    }

    if (level > 0)
    {
        FArrayBox flux;
        MultiFab** tensorflux;
        allocFluxBoxesLevel(tensorflux,0,BL_SPACEDIM);
        tensor_op->compFlux(D_DECL(*(tensorflux[0]), *(tensorflux[1]), *(tensorflux[2])),Soln);
        //
        // The extra factor of dt comes from the fact that Vsync looks
        // like dV/dt, not just an increment to V.
        //
        // This is to remove the dx scaling in the coeffs
        //
        for (int d =0; d <BL_SPACEDIM; d++)
            tensorflux[d]->mult(b/(dt*caller->Geom().CellSize()[d]),0);

        for (int sigma = Xvel; sigma < BL_SPACEDIM+Xvel; sigma++)
        {
            for (MFIter mfi(*(tensorflux[0])); mfi.isValid(); ++mfi)
            {
                const int i    = mfi.index();
                const Box& grd = BoxLib::enclosedCells(mfi.validbox());

                BL_ASSERT(grd==grids[mfi.index()]);

                for (int k = 0; k < BL_SPACEDIM; k++)
                {
                    Box flux_bx(grd);
                    flux_bx.surroundingNodes(k);
                    flux.resize(flux_bx,1);
                    flux.copy((*(tensorflux[k]))[i],sigma,0,1);
                    viscflux_reg->FineAdd(flux,k,i,0,sigma,1,dt*dt);
                }
            }
        }
        removeFluxBoxesLevel(tensorflux);
    }

    delete tensor_op;
}

void
Diffusion::diffuse_Ssync (MultiFab*              Ssync,
                          int                    sigma,
                          Real                   dt,
                          Real                   be_cn_theta,
                          const MultiFab*        rho_half,
                          int                    rho_flag,
                          MultiFab* const*       flux,
                          int                    fluxComp,
                          const MultiFab* const* beta,
                          int                    betaComp,
                          const MultiFab*        alpha,
                          int                    alphaComp)
{
    const int state_ind = sigma + BL_SPACEDIM;
    const int IOProc    = ParallelDescriptor::IOProcessorNumber();

    if (verbose && ParallelDescriptor::IOProcessor())
        std::cout << "Diffusion::diffuse_Ssync: "
                  << caller->get_desc_lst()[State_Type].name(state_ind) << '\n';

    int allnull, allthere;
    checkBeta(beta, allthere, allnull);

    MultiFab  Rhs(grids,1,0);

    MultiFab::Copy(Rhs,*Ssync,sigma,0,1,0);

    if (verbose > 1)
    {
        MultiFab junk(grids,1,0);

        MultiFab::Copy(junk,Rhs,0,0,1,0);

        if (rho_flag == 2)
        {
            MultiFab& S_new = caller->get_new_data(State_Type);
            for (MFIter jmfi(junk); jmfi.isValid(); ++jmfi)
                junk[jmfi].divide(S_new[jmfi],jmfi.validbox(),Density,0,1);
        }
        Real r_norm = 0.0;
        for (MFIter jmfi(junk); jmfi.isValid(); ++jmfi)
            r_norm = std::max(r_norm,junk[jmfi].norm(0));
        ParallelDescriptor::ReduceRealMax(r_norm,IOProc);

        if (ParallelDescriptor::IOProcessor())
            std::cout << "Original max of Ssync " << r_norm << '\n';
    }
    //
    // SET UP COEFFICIENTS FOR VISCOUS SOLVER.
    //
    const Real a = 1.0;
    Real       b = be_cn_theta*dt;
    if (allnull)
        b *= visc_coef[state_ind];
    Real           rhsscale = 1.0;
    ABecLaplacian* visc_op  = getViscOp(state_ind,a,b,rho_half,rho_flag,
                                        &rhsscale,beta,betaComp,alpha,alphaComp);
    visc_op->maxOrder(max_order);
    //
    // Compute RHS.
    //
    {
        FArrayBox volume;

        for (MFIter Rhsmfi(Rhs); Rhsmfi.isValid(); ++Rhsmfi)
        {
            caller->Geom().GetVolume(volume,grids,Rhsmfi.index(),GEOM_GROW);
            Rhs[Rhsmfi].mult(volume); 
            if (rho_flag == 1)
                Rhs[Rhsmfi].mult((*rho_half)[Rhsmfi]);
        }
    }
    Rhs.mult(rhsscale,0,1);

    MultiFab Soln(grids,1,1);

    Soln.setVal(0);
    //
    // Construct solver and call it.
    //
    const Real S_tol     = visc_tol;
    const Real S_tol_abs = -1;
    if (use_cg_solve)
    {
        CGSolver cg(*visc_op,use_mg_precond_flag);
        cg.solve(Soln,Rhs,S_tol,S_tol_abs);
    }

#ifdef MG_USE_HYPRE
    else if (use_hypre_solve)
    {
        BoxLib::Error("HypreABec not ready");
        //	  HypreABec hp(Soln.boxArray(), 00, dx, 0, false);
        //	  hp.setup_solver(S_tol, S_tol_abs, 50);
        //	  hp.solve(Soln, Rhs, true);
        //	  hp.clear_solver();
    }
#endif
    else
    {
        MultiGrid mg(*visc_op);
        mg.solve(Soln,Rhs,S_tol,S_tol_abs);
    }
    Rhs.clear();

    int flux_allthere, flux_allnull;
    checkBeta(flux, flux_allthere, flux_allnull);
    if (flux_allthere)
    {
        bool do_applyBC = true;
        visc_op->compFlux(D_DECL(*flux[0],*flux[1],*flux[2]),Soln,LinOp::Inhomogeneous_BC,do_applyBC,0,fluxComp);
        for (int i = 0; i < BL_SPACEDIM; ++i)
            (*flux[i]).mult(b/(dt*caller->Geom().CellSize()[i]),fluxComp,1,0);
    }

    MultiFab::Copy(*Ssync,Soln,0,sigma,1,0);
    
    if (verbose > 1)
    {
        Real s_norm = 0.0;
        for (MFIter Solnmfi(Soln); Solnmfi.isValid(); ++Solnmfi)
            s_norm = std::max(s_norm,Soln[Solnmfi].norm(0));
        ParallelDescriptor::ReduceRealMax(s_norm,IOProc);

        if (ParallelDescriptor::IOProcessor())
            std::cout << "Final max of Ssync " << s_norm << '\n';
    }
    
    if (rho_flag == 2)
    {
        MultiFab& S_new = caller->get_new_data(State_Type);

        for (MFIter Ssyncmfi(*Ssync); Ssyncmfi.isValid(); ++Ssyncmfi)
        {
            const int i = Ssyncmfi.index();

            (*Ssync)[i].mult(S_new[i],Ssyncmfi.validbox(),Density,sigma,1);
        }
    }
    
    delete visc_op;
}

DivVis*
Diffusion::getTensorOp (Real                   a,
                        Real                   b,
                        Real                   time,
                        ViscBndryTensor&       visc_bndry,
                        const MultiFab*        rho,
                        const MultiFab* const* beta,
                        int                    betaComp)
{
    int allthere;
    checkBeta(beta, allthere);

    const Real* dx = caller->Geom().CellSize();

    getTensorBndryData(visc_bndry,time);

    DivVis* tensor_op = new DivVis(visc_bndry,dx);
    tensor_op->maxOrder(tensor_max_order);

    int isrz   = Geometry::IsRZ();
    const int nghost = 1; // Just like Bill.
    //
    // alpha should be the same size as volume.
    //
    const int nCompAlpha = BL_SPACEDIM == 2  ?  2  :  1;
    MultiFab alpha(grids,nCompAlpha,nghost);
    alpha.setVal(0.0,nghost);

    if (a != 0.0)
    {
        FArrayBox volume;

        const int N = alpha.IndexMap().size();

        for (int j = 0; j < N; j++)
        {
            const int   i         = alpha.IndexMap()[j];
            const Box&  bx        = alpha.box(i);
            Array<Real> rcen(bx.length(0));
            parent->Geom(level).GetCellLoc(rcen, bx, 0);
            caller->Geom().GetVolume(volume,grids,i,GEOM_GROW);
            const int*  lo        = bx.loVect();
            const int*  hi        = bx.hiVect();
            Real*       alpha_dat = alpha[i].dataPtr();
            Box         abx       = BoxLib::grow(bx,alpha.nGrow());
            const int*  alo       = abx.loVect();
            const int*  ahi       = abx.hiVect();
            const Real* rcendat   = rcen.dataPtr();
            const Real* voli      = volume.dataPtr();
            Box         vbox      = volume.box();
            const int*  vlo       = vbox.loVect();
            const int*  vhi       = vbox.hiVect();

            const FArrayBox& Rh = (*rho)[i];
            DEF_CLIMITS(Rh,rho_dat,rlo,rhi);

            const FArrayBox&  betax = (*beta[0])[i];
            const Real* betax_dat   = betax.dataPtr(betaComp);
            const int*  betax_lo    = betax.loVect();
            const int*  betax_hi    = betax.hiVect();

            const FArrayBox&  betay = (*beta[1])[i];
            const Real* betay_dat   = betay.dataPtr(betaComp);
            const int*  betay_lo    = betay.loVect();
            const int*  betay_hi    = betay.hiVect();

#if (BL_SPACEDIM == 3)
            const FArrayBox&  betaz     = (*beta[2])[i];
            const Real* betaz_dat = betaz.dataPtr(betaComp);
            const int*  betaz_lo  = betaz.loVect();
            const int*  betaz_hi  = betaz.hiVect();
#endif

            FORT_SET_TENSOR_ALPHA(alpha_dat, ARLIM(alo), ARLIM(ahi),
                                  lo, hi, rcendat, ARLIM(lo), ARLIM(hi), &b,
                                  voli, ARLIM(vlo), ARLIM(vhi),
                                  rho_dat,ARLIM(rlo),ARLIM(rhi),
                                  betax_dat,ARLIM(betax_lo),ARLIM(betax_hi),
                                  betay_dat,ARLIM(betay_lo),ARLIM(betay_hi),
#if (BL_SPACEDIM == 3)
                                  betaz_dat,ARLIM(betaz_lo),ARLIM(betaz_hi),
#endif
                                  &isrz);
        }
    }
    tensor_op->setScalars(a,b);
    tensor_op->aCoefficients(alpha);

    alpha.clear();

    for (int n = 0; n < BL_SPACEDIM; n++)
    {
        MultiFab bcoeffs;
        caller->Geom().GetFaceArea(bcoeffs,grids,n,nghost);
        for (MFIter bcoeffsmfi(bcoeffs); bcoeffsmfi.isValid(); ++bcoeffsmfi)
        {
            const int i = bcoeffsmfi.index();
            bcoeffs[i].mult(dx[n]);
            bcoeffs[i].mult((*beta[n])[i],betaComp,0,1);
        }
        tensor_op->bCoefficients(bcoeffs,n);
    }

    return tensor_op;
}

DivVis*
Diffusion::getTensorOp (Real                   a,
                        Real                   b,
                        const MultiFab*        rho,
                        const MultiFab* const* beta,
                        int                    betaComp)
{
    int allthere = beta != 0;
    if (allthere)
    {
        for (int d = 0; d < BL_SPACEDIM; d++)
        {
            allthere = allthere && beta[d] != 0;
        }
    }
    if (!allthere)
        BoxLib::Abort("Diffusion::getTensorOp(): all betas must allocated all 0 or all non-0");

    const Real* dx   = caller->Geom().CellSize();
    const int   nDer = MCLinOp::bcComponentsNeeded();

    Array<BCRec> bcarray(nDer,BCRec(D_DECL(EXT_DIR,EXT_DIR,EXT_DIR),
                                    D_DECL(EXT_DIR,EXT_DIR,EXT_DIR)));

    for (int id = 0; id < BL_SPACEDIM; id++)
    {
        bcarray[id] = caller->get_desc_lst()[State_Type].getBC(Xvel+id);
    }

    IntVect ref_ratio = level > 0 ? parent->refRatio(level-1) : IntVect::TheUnitVector();

    ViscBndryTensor bndry;

    bndry.define(grids,nDer,caller->Geom());
    bndry.setHomogValues(bcarray, ref_ratio[0]);
    DivVis* tensor_op = new DivVis(bndry,dx);
    tensor_op->maxOrder(tensor_max_order);

    int isrz   = Geometry::IsRZ();
    const int nghost = 1; // Just like Bill.
    //
    // alpha should be the same size as volume.
    //
    const int nCompAlpha = BL_SPACEDIM == 2  ?  2  :  1;
    MultiFab alpha(grids,nCompAlpha,nghost);
    alpha.setVal(0.0);

    if (a != 0.0)
    {
        FArrayBox volume;

        const int N = alpha.IndexMap().size();

        for (int j = 0; j < N; j++)
        {
            const int   i         = alpha.IndexMap()[j];
            const Box&  bx        = alpha.box(i);
            Array<Real> rcen(bx.length(0));
            parent->Geom(level).GetCellLoc(rcen, bx, 0);
            caller->Geom().GetVolume(volume,grids,i,GEOM_GROW);
            const int*  lo        = bx.loVect();
            const int*  hi        = bx.hiVect();
            Real*       alpha_dat = alpha[i].dataPtr();
            Box         abx       = BoxLib::grow(bx,alpha.nGrow());
            const int*  alo       = abx.loVect();
            const int*  ahi       = abx.hiVect();
            const Real* rcendat   = rcen.dataPtr();
            const Real* voli      = volume.dataPtr();
            Box         vbox      = volume.box();
            const int*  vlo       = vbox.loVect();
            const int*  vhi       = vbox.hiVect();

            const FArrayBox& Rh = (*rho)[i];
            DEF_CLIMITS(Rh,rho_dat,rlo,rhi);

            const FArrayBox&  betax = (*beta[0])[i];
            const Real* betax_dat   = betax.dataPtr(betaComp);
            const int*  betax_lo    = betax.loVect();
            const int*  betax_hi    = betax.hiVect();
            const FArrayBox&  betay = (*beta[1])[i];
            const Real* betay_dat   = betay.dataPtr(betaComp);
            const int*  betay_lo    = betay.loVect();
            const int*  betay_hi    = betay.hiVect();

#if (BL_SPACEDIM == 3)
            const FArrayBox&  betaz     = (*beta[2])[i];
            const Real* betaz_dat = betaz.dataPtr(betaComp);
            const int*  betaz_lo  = betaz.loVect();
            const int*  betaz_hi  = betaz.hiVect();
#endif

            FORT_SET_TENSOR_ALPHA(alpha_dat, ARLIM(alo), ARLIM(ahi),
                                  lo, hi, rcendat, ARLIM(lo), ARLIM(hi), &b,
                                  voli, ARLIM(vlo), ARLIM(vhi),
                                  rho_dat,ARLIM(rlo),ARLIM(rhi),
                                  betax_dat,ARLIM(betax_lo),ARLIM(betax_hi),
                                  betay_dat,ARLIM(betay_lo),ARLIM(betay_hi),
#if (BL_SPACEDIM == 3)
                                  betaz_dat,ARLIM(betaz_lo),ARLIM(betaz_hi),
#endif
                                  &isrz);
        }
    }
    tensor_op->setScalars(a,b);
    tensor_op->aCoefficients(alpha);

    alpha.clear();

    for (int n = 0; n < BL_SPACEDIM; n++)
    {
        MultiFab bcoeffs;
        caller->Geom().GetFaceArea(bcoeffs,grids,n,nghost);
        for (MFIter bcoeffsmfi(bcoeffs); bcoeffsmfi.isValid(); ++bcoeffsmfi)
        {
            const int i = bcoeffsmfi.index();
            bcoeffs[i].mult(dx[n]);
            bcoeffs[i].mult((*beta[n])[i],betaComp,0,1);
        }
        tensor_op->bCoefficients(bcoeffs,n);
    }

    return tensor_op;
}

ABecLaplacian*
Diffusion::getViscOp (int                    comp,
                      Real                   a,
                      Real                   b,
                      Real                   time,
                      ViscBndry&             visc_bndry,
                      const MultiFab*        rho_half,
                      int                    rho_flag, 
                      Real*                  rhsscale,
                      const MultiFab* const* beta,
                      int                    betaComp,
                      const MultiFab*        alpha_in,
                      int                    alphaComp,
                      bool		     bndry_already_filled)
{
    int allnull, allthere;
    checkBeta(beta, allthere, allnull);

    const Real* dx = caller->Geom().CellSize();

    if (!bndry_already_filled)
        getBndryData(visc_bndry,comp,1,time,rho_flag);

    ABecLaplacian* visc_op = new ABecLaplacian(visc_bndry,dx);
    visc_op->maxOrder(max_order);

    int usehoop = comp == Xvel && (Geometry::IsRZ());
    int useden  = rho_flag == 1;
    //
    // alpha should be the same size as volume.
    //
    MultiFab alpha;
    //
    // Rewrite slightly to avoid requiring rho_half when usehoop and useden are false
    // (leave function arguments for now though)
    //
    if (!usehoop)
    {
        caller->Geom().GetVolume(alpha,grids,GEOM_GROW);

        if (useden) 
        {
            MultiFab::Multiply(alpha,*rho_half,0,0,1,alpha.nGrow());
        }
    }
    else
    {
        alpha.define(grids,1,GEOM_GROW,Fab_allocate);

        FArrayBox volume;
        const int N = alpha.IndexMap().size();
        
        for (int j = 0; j < N; j++)
        {
            const int  i  = alpha.IndexMap()[j];
            const Box& bx = alpha.box(i);
            
            Array<Real> rcen(bx.length(0));
            parent->Geom(level).GetCellLoc(rcen, bx, 0);
            
            caller->Geom().GetVolume(volume,grids,i,GEOM_GROW);
            
            const int*  lo      = bx.loVect();
            const int*  hi      = bx.hiVect();
            Real*       dat     = alpha[i].dataPtr();
            Box         abx     = BoxLib::grow(bx,alpha.nGrow());
            const int*  alo     = abx.loVect();
            const int*  ahi     = abx.hiVect();
            const Real* rcendat = rcen.dataPtr();
            const Real* voli    = volume.dataPtr();
            Box         vbox    = volume.box();
            const int*  vlo     = vbox.loVect();
            const int*  vhi     = vbox.hiVect();
            
            const FArrayBox& Rh = (*rho_half)[i];
            DEF_CLIMITS(Rh,rho_dat,rlo,rhi);
            
            FORT_SETALPHA(dat, ARLIM(alo), ARLIM(ahi),
                          lo, hi, rcendat, ARLIM(lo), ARLIM(hi), &b,
                          voli, ARLIM(vlo), ARLIM(vhi),
                          rho_dat,ARLIM(rlo),ARLIM(rhi),&usehoop,&useden);
        }
    }

    if (rho_flag == 2 || rho_flag == 3)
    {
        MultiFab& S = caller->get_data(State_Type,time);

        for (MFIter alphamfi(alpha); alphamfi.isValid(); ++alphamfi)
        {
            BL_ASSERT(grids[alphamfi.index()] == alphamfi.validbox());
            alpha[alphamfi].mult(S[alphamfi],alphamfi.validbox(),Density,0,1);
        }
    }
    if (alpha_in != 0)
    {
        for (MFIter alphamfi(alpha); alphamfi.isValid(); ++alphamfi)
        {
            const int i = alphamfi.index();
            BL_ASSERT(grids[i] == alphamfi.validbox());
            alpha[i].mult((*alpha_in)[i],alphamfi.validbox(),alphaComp,0,1);
        }
    }
    if (rhsscale != 0)
    {
        *rhsscale = scale_abec ? 1.0/alpha.max(0) : 1.0;

        visc_op->setScalars(a*(*rhsscale),b*(*rhsscale));
    }
    else
    {
        visc_op->setScalars(a,b);
    }
    if (allnull)
    {
        visc_op->aCoefficients(alpha);
        alpha.clear();
        for (int n = 0; n < BL_SPACEDIM; n++)
        {
            MultiFab bcoeffs;
            caller->Geom().GetFaceArea(bcoeffs,grids,n,0);
            bcoeffs.mult(dx[n]);
            visc_op->bCoefficients(bcoeffs,n);
        }
    }
    else
    {
        visc_op->aCoefficients(alpha);
        alpha.clear();
        for (int n = 0; n < BL_SPACEDIM; n++)
        {
            MultiFab bcoeffs;
            caller->Geom().GetFaceArea(bcoeffs,grids,n,0);
            for (MFIter bcoeffsmfi(bcoeffs); bcoeffsmfi.isValid(); ++bcoeffsmfi)
            {
                const int i = bcoeffsmfi.index();
                bcoeffs[i].mult((*beta[n])[i],betaComp,0,1);
                bcoeffs[i].mult(dx[n]);
            }
            visc_op->bCoefficients(bcoeffs,n);
        }
    }

    return visc_op;
}

ABecLaplacian*
Diffusion::getViscOp (int                    comp,
                      Real                   a,
                      Real                   b,
                      const MultiFab*        rho,
                      int                    rho_flag,
                      Real*                  rhsscale,
                      const MultiFab* const* beta,
                      int                    betaComp,
                      const MultiFab*        alpha_in,
                      int                    alphaComp)
{
    //
    // Note: This assumes that the "NEW" density is to be used, if rho_flag==2
    //
    int allnull, allthere;
    checkBeta(beta, allthere, allnull);

    const Geometry& geom = caller->Geom();
    const Real*  dx      = geom.CellSize();
    const BCRec& bc      = caller->get_desc_lst()[State_Type].getBC(comp);

    IntVect ref_ratio = level > 0 ? parent->refRatio(level-1) : IntVect::TheUnitVector();

    ViscBndry bndry(grids,1,geom);
    bndry.setHomogValues(bc, ref_ratio);

    ABecLaplacian* visc_op = new ABecLaplacian(bndry,dx);
    visc_op->maxOrder(max_order);

    int usehoop = ((comp==Xvel) && (Geometry::IsRZ()));
    int useden  = (rho_flag == 1);
    //
    // alpha should be the same size as volume.
    //
    MultiFab alpha(grids,1,GEOM_GROW);

    FArrayBox volume;

    const int N = alpha.IndexMap().size();

    for (int j = 0; j < N; j++)
    {
        const int        i       = alpha.IndexMap()[j];
        const Box&       bx      = alpha.box(i);
        Array<Real> rcen(bx.length(0));
        parent->Geom(level).GetCellLoc(rcen, bx, 0);

        caller->Geom().GetVolume(volume,grids,i,GEOM_GROW);

        const int*       lo      = bx.loVect();
        const int*       hi      = bx.hiVect();
        Real*            dat     = alpha[i].dataPtr();
        Box              abx     = BoxLib::grow(bx,alpha.nGrow());
        const int*       alo     = abx.loVect();
        const int*       ahi     = abx.hiVect();
        const Real*      rcendat = rcen.dataPtr();
        const Real*      voli    = volume.dataPtr();
        Box              vbox    = volume.box();
        const int*       vlo     = vbox.loVect();
        const int*       vhi     = vbox.hiVect();
        const FArrayBox& Rh      = (*rho)[i];
        DEF_CLIMITS(Rh,rho_dat,rlo,rhi);

        FORT_SETALPHA(dat, ARLIM(alo), ARLIM(ahi),
                      lo, hi, rcendat, ARLIM(lo), ARLIM(hi), &b,
                      voli, ARLIM(vlo), ARLIM(vhi),
                      rho_dat,ARLIM(rlo),ARLIM(rhi),&usehoop,&useden);
    }

    if (rho_flag == 2 || rho_flag == 3)
    {
        MultiFab& S = caller->get_new_data(State_Type);

        for (MFIter alphamfi(alpha); alphamfi.isValid(); ++alphamfi)
        {
            BL_ASSERT(grids[alphamfi.index()] == alphamfi.validbox());
            alpha[alphamfi].mult(S[alphamfi],alphamfi.validbox(),Density,0,1);
        }
    }
    if (alpha_in != 0)
    {
        for (MFIter alphamfi(alpha); alphamfi.isValid(); ++alphamfi)
        {
            const int i = alphamfi.index();
            BL_ASSERT(grids[i] == alphamfi.validbox());
            alpha[i].mult((*alpha_in)[i],alphamfi.validbox(),alphaComp,0,1);
        }
    }
    if (rhsscale != 0)
    {
        *rhsscale = scale_abec ? 1.0/alpha.max(0) : 1.0;

        visc_op->setScalars(a*(*rhsscale),b*(*rhsscale));
    }
    else
    {
        visc_op->setScalars(a,b);
    }
    visc_op->aCoefficients(alpha);

    alpha.clear();

    if (allnull)
    {
        for (int n = 0; n < BL_SPACEDIM; n++)
        {
            MultiFab bcoeffs;
            caller->Geom().GetFaceArea(bcoeffs,grids,n,0);
            bcoeffs.mult(dx[n],0,1,0);
            visc_op->bCoefficients(bcoeffs,n);
        }
    }
    else
    {
        for (int n = 0; n < BL_SPACEDIM; n++)
        {
            MultiFab bcoeffs;
            caller->Geom().GetFaceArea(bcoeffs,grids,n,0);
            for (MFIter bcoeffsmfi(bcoeffs); bcoeffsmfi.isValid(); ++bcoeffsmfi)
            {
                const int i = bcoeffsmfi.index();
                bcoeffs[i].mult((*beta[n])[i],betaComp,0,1);
            }
            bcoeffs.mult(dx[n],0,1,0);
            visc_op->bCoefficients(bcoeffs,n);
        }
    }

    return visc_op;
}

void
Diffusion::getViscTerms (MultiFab&              visc_terms,
                         int                    src_comp,
                         int                    comp, 
                         Real                   time,
                         int                    rho_flag,
                         const MultiFab* const* beta,
                         int                    betaComp)
{
    int allnull, allthere;
    checkBeta(beta, allthere, allnull);
    //
    // Before computing the godunov predictors we may have to
    // precompute the viscous source terms.  To do this we must
    // construct a Laplacian operator, set the coeficients and apply
    // it to the time N data.  First, however, we must precompute the
    // fine N bndry values.  We will do this for each scalar that diffuses.
    //
    // Note: This routine DOES NOT fill grow cells
    //
    const Real* dx = caller->Geom().CellSize();
    MultiFab&   S  = caller->get_data(State_Type,time);

    int ngrow = visc_terms.nGrow();
    visc_terms.setVal(0.0,comp-src_comp,1,ngrow);
    //
    // FIXME
    // LinOp classes cannot handle multcomponent MultiFabs yet,
    // construct the components one at a time and copy to visc_terms.
    //
    MultiFab visc_tmp(grids,1,1);
    MultiFab s_tmp(grids,1,1);

    if (is_diffusive[comp])
    {
        ViscBndry visc_bndry;
        getBndryData(visc_bndry,comp,1,time,rho_flag);
        //
        // Set up operator and apply to compute viscous terms.
        //
        const Real a = 0.0;
        const Real b = allnull ? -visc_coef[comp] : -1.0;

        ABecLaplacian visc_op(visc_bndry,dx);

        visc_op.setScalars(a,b);
        visc_op.maxOrder(max_order);

        if (allnull)
        {
            for (int n = 0; n < BL_SPACEDIM; n++)
            {
                MultiFab bcoeffs;
                caller->Geom().GetFaceArea(bcoeffs,grids,n,0);
                bcoeffs.mult(dx[n]);
                visc_op.bCoefficients(bcoeffs,n);
            }
        }
        else
        {
            for (int n = 0; n < BL_SPACEDIM; n++)
            {
                MultiFab bcoeffs;
                caller->Geom().GetFaceArea(bcoeffs,grids,n,0);
                for (MFIter bcoeffsmfi(bcoeffs); bcoeffsmfi.isValid(); ++bcoeffsmfi)
                {
                    const int i = bcoeffsmfi.index();
                    bcoeffs[i].mult((*beta[n])[i],betaComp,0,1);
                    bcoeffs[i].mult(dx[n]);
                }
                visc_op.bCoefficients(bcoeffs,n);
            }
        }
        //
        // Copy to single component multifab for operator classes.
        //
        MultiFab::Copy(s_tmp,S,comp,0,1,0);

        if (rho_flag == 2)
        {
            //
            // We want to evaluate (div beta grad) S, not rho*S.
            //
            for (MFIter Smfi(S); Smfi.isValid(); ++Smfi)
            {
                const Box& box = Smfi.validbox();
                BL_ASSERT(S[Smfi].min(box,Density) > 0.0);
                s_tmp[Smfi].divide(S[Smfi],box,Density,0,1);
            }
        }

        visc_op.apply(visc_tmp,s_tmp);
        //
        // Must divide by volume.
        //
        {
            FArrayBox volume;
            for (MFIter visc_tmpmfi(visc_tmp); visc_tmpmfi.isValid(); ++visc_tmpmfi)
            {
                const int i = visc_tmpmfi.index();
                BL_ASSERT(grids[i] == visc_tmpmfi.validbox());
                caller->Geom().GetVolume(volume,grids,i,GEOM_GROW);
                visc_tmp[i].divide(volume,visc_tmpmfi.validbox(),0,0,1);
            }
        }

#if (BL_SPACEDIM == 2)
        if (comp == Xvel && Geometry::IsRZ())
        {
            for (MFIter visc_tmpmfi(visc_tmp); visc_tmpmfi.isValid(); ++visc_tmpmfi)
            {
                //
                // visc_tmp[k] += -mu * u / r^2
                //
                const int  i   = visc_tmpmfi.index();
                const Box& bx  = visc_tmpmfi.validbox();
                Box        vbx = BoxLib::grow(bx,visc_tmp.nGrow());
                Box        sbx = BoxLib::grow(s_tmp.box(i),s_tmp.nGrow());
                Array<Real> rcen(bx.length(0));
                parent->Geom(level).GetCellLoc(rcen, bx, 0);
                const int*  lo      = bx.loVect();
                const int*  hi      = bx.hiVect();
                const int*  vlo     = vbx.loVect();
                const int*  vhi     = vbx.hiVect();
                const int*  slo     = sbx.loVect();
                const int*  shi     = sbx.hiVect();
                Real*       vdat    = visc_tmp[visc_tmpmfi].dataPtr();
                Real*       sdat    = s_tmp[visc_tmpmfi].dataPtr();
                const Real* rcendat = rcen.dataPtr();
                const Real  mu      = visc_coef[comp];
                FORT_HOOPSRC(ARLIM(lo), ARLIM(hi),
                             vdat, ARLIM(vlo), ARLIM(vhi),
                             sdat, ARLIM(slo), ARLIM(shi),
                             rcendat, &mu);
            }
        }
#endif

        MultiFab::Copy(visc_terms,visc_tmp,0,comp-src_comp,1,0);
    }
}

void
Diffusion::getTensorViscTerms (MultiFab&              visc_terms, 
                               Real                   time,
                               const MultiFab* const* beta,
                               int                    betaComp)
{
    int allthere;
    checkBeta(beta, allthere);

    const int src_comp = Xvel;
    const int ncomp    = visc_terms.nComp();

    if (ncomp < BL_SPACEDIM)
        BoxLib::Abort("Diffusion::getTensorViscTerms(): visc_terms needs at least BL_SPACEDIM components");
    //
    // Before computing the godunov predicitors we may have to
    // precompute the viscous source terms.  To do this we must
    // construct a Laplacian operator, set the coeficients and apply
    // it to the time N data.  First, however, we must precompute the
    // fine N bndry values.  We will do this for each scalar that diffuses.
    //
    // Note: This routine DOES NOT fill grow cells
    //
    const Real* dx   = caller->Geom().CellSize();
    MultiFab&   S    = caller->get_data(State_Type,time);
    visc_terms.setVal(0.0,src_comp,BL_SPACEDIM,1);
    //
    // FIXME
    // LinOp classes cannot handle multcomponent MultiFabs yet,
    // construct the components one at a time and copy to visc_terms.
    //
    MultiFab visc_tmp(grids,BL_SPACEDIM,1);
    MultiFab s_tmp(grids,BL_SPACEDIM,1);

    if (is_diffusive[src_comp])
    {
        ViscBndryTensor visc_bndry;
        getTensorBndryData(visc_bndry,time);
        //
        // Set up operator and apply to compute viscous terms.
        //
        const Real a = 0.0;
        const Real b = -1.0;

        DivVis tensor_op(visc_bndry,dx);
        tensor_op.maxOrder(tensor_max_order);
        tensor_op.setScalars(a,b);

        const int nghost = 0;
        //
        // alpha should be the same size as volume.
        //
        const int nCompAlpha = BL_SPACEDIM == 2  ?  2 : 1;
        {
            MultiFab alpha(grids,nCompAlpha,nghost);
            alpha.setVal(0.0);
            tensor_op.aCoefficients(alpha);
        }

        for (int n = 0; n < BL_SPACEDIM; n++)
        {
            MultiFab bcoeffs;
            caller->Geom().GetFaceArea(bcoeffs,grids,n,nghost);
            for (MFIter bcoeffsmfi(bcoeffs); bcoeffsmfi.isValid(); ++bcoeffsmfi)
            {
                const int i = bcoeffsmfi.index();
                bcoeffs[i].mult(dx[n]);
                bcoeffs[i].mult((*beta[n])[i],betaComp,0,1);
            }
            tensor_op.bCoefficients(bcoeffs,n);
        }

        MultiFab::Copy(s_tmp,S,Xvel,0,BL_SPACEDIM,0);

        tensor_op.apply(visc_tmp,s_tmp);
        //
        // Must divide by volume.
        //
        {
            MultiFab volume;
            caller->Geom().GetVolume(volume,grids,GEOM_GROW);
            for (MFIter visc_tmpmfi(visc_tmp); visc_tmpmfi.isValid(); ++visc_tmpmfi)
            {
                const int i = visc_tmpmfi.index();
                for (int n = 0; n < BL_SPACEDIM; ++n)
                    visc_tmp[i].divide(volume[i],volume.box(i),0,n,1);
            }
        }

#if (BL_SPACEDIM == 2)
        if (Geometry::IsRZ())
        {
            int fort_xvel_comp = Xvel+1;

            Array<Real> rcen;

            for (MFIter vmfi(visc_tmp); vmfi.isValid(); ++vmfi)
            {
                const int  k   = vmfi.index();
                const Box& bx  = visc_tmp.box(k);
                Box        vbx = BoxLib::grow(bx,visc_tmp.nGrow());
                Box        sbx = BoxLib::grow(s_tmp.box(k),s_tmp.nGrow());

                rcen.resize(bx.length(0));

                parent->Geom(level).GetCellLoc(rcen, bx, 0);

                const int*       lo        = bx.loVect();
                const int*       hi        = bx.hiVect();
                const int*       vlo       = vbx.loVect();
                const int*       vhi       = vbx.hiVect();
                const int*       slo       = sbx.loVect();
                const int*       shi       = sbx.hiVect();
                Real*            vdat      = visc_tmp[k].dataPtr();
                Real*            sdat      = s_tmp[k].dataPtr();
                const Real*      rcendat   = rcen.dataPtr();
                const FArrayBox& betax     = (*beta[0])[k];
                const Real*      betax_dat = betax.dataPtr(betaComp);
                const int*       betax_lo  = betax.loVect();
                const int*       betax_hi  = betax.hiVect();
                const FArrayBox& betay     = (*beta[1])[k];
                const Real*      betay_dat = betay.dataPtr(betaComp);
                const int*       betay_lo  = betay.loVect();
                const int*       betay_hi  = betay.hiVect();

                FORT_TENSOR_HOOPSRC(&fort_xvel_comp,ARLIM(lo), ARLIM(hi),
                                    vdat, ARLIM(vlo), ARLIM(vhi),
                                    sdat, ARLIM(slo), ARLIM(shi),
                                    rcendat, 
                                    betax_dat,ARLIM(betax_lo),ARLIM(betax_hi),
                                    betay_dat,ARLIM(betay_lo),ARLIM(betay_hi));
            }
        }
#endif
        MultiFab::Copy(visc_terms,visc_tmp,0,0,BL_SPACEDIM,0);
    }
}

#include "Utility.H"

void
Diffusion::getBndryData (ViscBndry& bndry,
                         int        src_comp,
                         int        num_comp,
                         Real       time,
                         int        rho_flag)
{
    BL_ASSERT(num_comp == 1);
    //
    // Fill phys bndry vals of grow cells of (tmp) multifab passed to bndry.
    //
    // TODO -- A MultiFab is a huge amount of space in which to pass along
    // the phys bc's.  InterpBndryData needs a more efficient interface.
    //
    NavierStokes& ns    = *(NavierStokes*) &(parent->getLevel(level));
    const int     nGrow = 1;
    const BCRec&  bc    = caller->get_desc_lst()[State_Type].getBC(src_comp);

    MultiFab S(grids, num_comp, nGrow);

    S.setVal(BL_SAFE_BOGUS);

    bndry.define(grids,num_comp,caller->Geom());

    const MultiFab& rhotime = ns.get_rho(time);
    MFIter          Rho_mfi(rhotime);

    for (FillPatchIterator Phi_fpi(*caller,S,nGrow,time,State_Type,src_comp,num_comp);
         Rho_mfi.isValid() && Phi_fpi.isValid();
         ++Rho_mfi, ++Phi_fpi)
    {
        const BoxList gCells = BoxLib::boxDiff(Phi_fpi().box(),Phi_fpi.validbox());

        for (BoxList::const_iterator bli = gCells.begin(), end = gCells.end();
             bli != end;
             ++bli)
        {
            S[Phi_fpi].copy(Phi_fpi(),*bli,0,*bli,0,num_comp);

            if (rho_flag == 2)
                for (int n = 0; n < num_comp; ++n)
                    S[Phi_fpi].divide(rhotime[Rho_mfi],*bli,0,n,1);
        }
    }
    
    if (level == 0)
    {
        bndry.setBndryValues(S,0,0,num_comp,bc);
    }
    else
    {
        BoxArray cgrids = grids;
        cgrids.coarsen(crse_ratio);
        BndryRegister crse_br(cgrids,0,1,2,num_comp);
        //
        // interp for solvers over ALL c-f brs, need safe data.
        //
        crse_br.setVal(BL_BOGUS);
        coarser->FillBoundary(crse_br,src_comp,0,num_comp,time,rho_flag);
        bndry.setBndryValues(crse_br,0,S,0,0,num_comp,crse_ratio,bc);
    }
}
void
Diffusion::getBndryDataGivenS (ViscBndry& bndry,
                               MultiFab&  Rho_and_spec,
                               MultiFab&  Rho_and_spec_crse,
                               int        state_ind,
                               int        src_comp,
                               int        num_comp)
{
    BL_ASSERT(num_comp == 1);
    const int     nGrow = 1;
    //
    // Fill phys bndry vals of grow cells of (tmp) multifab passed to bndry.
    //
    // TODO -- A MultiFab is a huge amount of space in which to pass along
    // the phys bc's.  InterpBndryData needs a more efficient interface.
    //
    const BCRec& bc = caller->get_desc_lst()[State_Type].getBC(state_ind);

    bndry.define(grids,num_comp,caller->Geom());

    if (level == 0)
    {
        bndry.setBndryValues(Rho_and_spec,src_comp,0,num_comp,bc);
    }
    else
    {
        BoxArray cgrids = grids;
        cgrids.coarsen(crse_ratio);
        BndryRegister crse_br(cgrids,0,1,2,num_comp);
        //
        // interp for solvers over ALL c-f brs, need safe data.
        //
        crse_br.setVal(BL_BOGUS);
        crse_br.copyFrom(Rho_and_spec_crse,nGrow,src_comp,0,num_comp);
        bndry.setBndryValues(crse_br,0,Rho_and_spec,src_comp,0,num_comp,crse_ratio,bc);
    }
}

void
Diffusion::FillBoundary (BndryRegister& bdry,
                         int            state_ind,
                         int            dest_comp,
                         int            num_comp,
                         Real           time,
                         int            rho_flag)
{
    //
    // Need one grow cell filled for linear solvers.
    // We assume filPatch gets this right, where possible.
    //
    const int     nGrow = 1;
    NavierStokes& ns    = *(NavierStokes*) &(parent->getLevel(level));

    MultiFab S(caller->boxArray(),num_comp,nGrow);

    const MultiFab& rhotime = ns.get_rho(time);
    MFIter          Rho_mfi(rhotime);

    for (FillPatchIterator S_fpi(*caller,S,nGrow,time,State_Type,state_ind,num_comp);
         Rho_mfi.isValid() && S_fpi.isValid();
         ++Rho_mfi, ++S_fpi)
    {
        S[S_fpi.index()].copy(S_fpi(),0,0,num_comp);
   
   	if (rho_flag == 2)
        {
            for (int n = 0; n < num_comp; ++n)
                S[S_fpi.index()].divide(rhotime[Rho_mfi],0,n,1);
        }
    }
    //
    // Copy into boundary register.
    //
    bdry.copyFrom(S,nGrow,0,dest_comp,num_comp);
    
}

void
Diffusion::getTensorBndryData (ViscBndryTensor& bndry, 
                               Real             time)
{
    const int num_comp = BL_SPACEDIM;
    const int src_comp = Xvel;
    const int nDer     = MCLinOp::bcComponentsNeeded();
    //
    // Create the BCRec's interpreted by ViscBndry objects
    //
    Array<BCRec> bcarray(nDer, BCRec(D_DECL(EXT_DIR,EXT_DIR,EXT_DIR),
                                     D_DECL(EXT_DIR,EXT_DIR,EXT_DIR)));

    for (int idim = 0; idim < BL_SPACEDIM; idim++)
        bcarray[idim] = caller->get_desc_lst()[State_Type].getBC(src_comp+idim);

    bndry.define(grids,nDer,caller->Geom());

    const int nGrow = 1;

    MultiFab S(grids,num_comp,nGrow,Fab_allocate);

    for (FillPatchIterator Phi_fpi(*caller,S,nGrow,time,State_Type,src_comp,num_comp);
         Phi_fpi.isValid();
         ++Phi_fpi)
    {
        const BoxList gCells = BoxLib::boxDiff(Phi_fpi().box(), Phi_fpi.validbox());
        
        for (BoxList::const_iterator bli = gCells.begin(), end = gCells.end();
             bli != end;
             ++bli)
        {
            S[Phi_fpi].copy(Phi_fpi(),*bli,0,*bli,0,num_comp);
        }
    }
    
    if (level == 0)
    {
        bndry.setBndryValues(S,0,0,num_comp,bcarray);
    }
    else
    {
        BoxArray cgrids(grids);
        cgrids.coarsen(crse_ratio);
        BndryRegister crse_br(cgrids,0,1,1,num_comp);
        crse_br.setVal(BL_BOGUS);
        const int rho_flag = 0;
        coarser->FillBoundary(crse_br,src_comp,0,num_comp,time,rho_flag);
        bndry.setBndryValues(crse_br,0,S,0,0,num_comp,crse_ratio[0],bcarray);
    }
}

void
Diffusion::checkBetas (const MultiFab* const* beta1, 
                       const MultiFab* const* beta2,
                       int&                   allthere,
                       int&                   allnull) const
{
    int allnull1, allnull2, allthere1, allthere2;

    checkBeta(beta1,allthere1,allnull1);
    checkBeta(beta2,allthere2,allnull2);
    allnull  = allnull1 && allnull2;
    allthere = allthere1 && allthere2;

    if (!(allthere || allnull))
        BoxLib::Abort("Diffusion::checkBetas(): betas must either be all 0 or all non-0");
}

void
Diffusion::checkBeta (const MultiFab* const* beta,
                      int&                   allthere,
                      int&                   allnull) const
{
    allnull  = 1;
    allthere = beta != 0;

    if (allthere)
    {
        for (int d = 0; d < BL_SPACEDIM; d++)
        {
            allnull = allnull && beta[d] == 0;
            allthere = allthere && beta[d] != 0;
        }
    }

    if (!(allthere || allnull))
        BoxLib::Abort("Diffusion::checkBeta(): betas must be all 0 or all non-0");
}

void
Diffusion::checkBeta (const MultiFab* const* beta,
                      int&                   allthere) const
{
    allthere = beta != 0;

    if (allthere)
    {
        for (int d = 0; d < BL_SPACEDIM; d++)
            allthere = allthere && beta[d] != 0;
    }

    if (!allthere)
        BoxLib::Abort("Diffusion::checkBeta(): betas must be all non-0");
}

void
Diffusion::allocFluxBoxesLevel (MultiFab**& fluxbox, 
                                int         nghost,
                                int         nvar)
{
    fluxbox = new MultiFab*[BL_SPACEDIM];
    for (int dir = 0; dir < BL_SPACEDIM; dir++)
    {
        BoxArray edge_boxes(grids);
        edge_boxes.surroundingNodes(dir);
        fluxbox[dir] = new MultiFab(edge_boxes,nvar,nghost);
    }
}

void
Diffusion::removeFluxBoxesLevel (MultiFab**& fluxbox) 
{
    if (fluxbox != 0)
    {
        for (int i = 0; i<BL_SPACEDIM; i++)
            delete fluxbox[i];
        delete [] fluxbox;
        fluxbox = 0;
    }
}

#ifdef USE_NAVIERSTOKES
//
// This routine computes the vector div mu SI, where I is the identity 
// tensor, S = div U, and mu is constant.
//

void
Diffusion::compute_divmusi (Real      time,
                            Real      mu,
                            MultiFab& divmusi)
{
    //
    // Compute on valid region.  Calling function should fill grow cells.
    //
    if (mu > 0.0)
    {
        const int     nGrowDU  = 1;
        const Real*   dx       = caller->Geom().CellSize();
        NavierStokes& ns_level = *(NavierStokes*) &(parent->getLevel(level));
        MultiFab*     divu_fp  = ns_level.getDivCond(nGrowDU,time);

        for (MFIter divmusimfi(divmusi); divmusimfi.isValid(); ++divmusimfi)
        {
            FArrayBox& fab  = divmusi[divmusimfi];
            FArrayBox& divu = (*divu_fp)[divmusimfi];
            const Box& box  = divmusimfi.validbox();

            FORT_DIV_MU_SI(box.loVect(), box.hiVect(), dx, &mu,
                           ARLIM(divu.loVect()), ARLIM(divu.hiVect()),
                           divu.dataPtr(),
                           ARLIM(fab.loVect()),  ARLIM(fab.hiVect()),
                           fab.dataPtr());
        }

        delete divu_fp;
    }
    else
    {
        const int nGrow = 0; // Not to fill grow cells here
        divmusi.setVal(0.0,nGrow);
    }
}

//
// This routine computes the vector div beta SI, where I is the identity 
// tensor, S = div U, and beta is non-constant.
//

void
Diffusion::compute_divmusi (Real                   time,
                            const MultiFab* const* beta,
                            MultiFab&              divmusi)
{
    const int     nGrowDU  = 1;
    const Real*   dx       = caller->Geom().CellSize();
    NavierStokes& ns_level = *(NavierStokes*) &(parent->getLevel(level));
    MultiFab*     divu_fp  = ns_level.getDivCond(nGrowDU,time);

    for (MFIter divmusimfi(divmusi); divmusimfi.isValid(); ++divmusimfi)
    {
        const int  i    = divmusimfi.index();
        FArrayBox& divu = (*divu_fp)[divmusimfi];
        const Box& box  = divmusimfi.validbox();

        DEF_CLIMITS((*beta[0])[divmusimfi],betax,betaxlo,betaxhi);
        DEF_CLIMITS((*beta[1])[divmusimfi],betay,betaylo,betayhi);

#if (BL_SPACEDIM==3)
        DEF_CLIMITS((*beta[2])[divmusimfi],betaz,betazlo,betazhi);
#endif
        BL_ASSERT(grids[divmusimfi.index()] == divmusimfi.validbox());

        FORT_DIV_VARMU_SI(box.loVect(),box.hiVect(), dx,
                          ARLIM(divu.loVect()), ARLIM(divu.hiVect()),
                          divu.dataPtr(),
                          ARLIM(betaxlo), ARLIM(betaxhi), betax,
                          ARLIM(betaylo), ARLIM(betayhi), betay,
#if (BL_SPACEDIM==3)
                          ARLIM(betazlo), ARLIM(betazhi), betaz,
#endif
                          ARLIM(divmusi[i].loVect()), ARLIM(divmusi[i].hiVect()),
                          divmusi[i].dataPtr());
    }

    delete divu_fp;
}


//
// SAS: The following function is a temporary fix in the migration from
//      using is_conservative and rho_flag over to using advectionType
//      and diffusionType.
//
int
Diffusion::set_rho_flag(const DiffusionForm compDiffusionType)
{
    int rho_flag = 0;

    switch (compDiffusionType)
    {
        case Laplacian_S:
            rho_flag = 0;
            break;

        case RhoInverse_Laplacian_S:
            rho_flag = 1;
            break;

        case Laplacian_SoverRho:
            rho_flag = 2;
            break;

        default:
            std::cout << "compDiffusionType = " << compDiffusionType << '\n';
            BoxLib::Abort("An unknown NavierStokes::DiffusionForm was used in set_rho_flag");
    }

    return rho_flag;
}

bool
Diffusion::are_any(const Array<DiffusionForm>& diffusionType,
                   const DiffusionForm         testForm,
                   const int                   sComp,
                   const int                   nComp)
{
    for (int comp = sComp; comp < sComp + nComp; ++comp)
    {
        if (diffusionType[comp] == testForm)
            return true;
    }

    return false;
}

int
Diffusion::how_many(const Array<DiffusionForm>& diffusionType,
                    const DiffusionForm         testForm,
                    const int                   sComp,
                    const int                   nComp)
{
    int counter = 0;

    for (int comp = sComp; comp < sComp + nComp; ++comp)
    {
        if (diffusionType[comp] == testForm)
            ++counter;
    }

    return counter;
}
/*
bool
Diffusion::are_any_Laplacian_SoverRho(const Array<DiffusionForm>& diffusionType,
                                      const int                   sComp,
                                      const int                   nComp)
{
    for (int comp = sComp; comp < sComp + nComp; ++comp)
    {
        if (diffusionType[comp] == Laplacian_SoverRho)
            return true;
    }

    return false;
}
*/
#endif /*USE_NAVIERSTOKES*/
