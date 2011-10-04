//
// "Divu_Type" means S, where divergence U = S
// "Dsdt_Type" means pd S/pd t, where S is as above
//
#include <winstd.H>

#include <algorithm>
#include <vector>
#include <cstdio>
#include <cmath>

#include <Geometry.H>
#include <BoxDomain.H>
#include <ParmParse.H>
#include <ErrorList.H>
#include <FArrayBox.H>
#include <Godunov.H>
#include <Interpolater.H>
#include <NavierStokes.H>
#include <MultiGrid.H>
#include <ArrayLim.H>
#include <Utility.H>
#include <NAVIERSTOKES_F.H>
#include <Profiler.H>
#include <PROJECTION_F.H>
#include <PROB_NS_F.H>
#include <TagBox.H>
#include <VISCOPERATOR_F.H>
#include <BLFort.H>

#ifdef BL_USE_VELOCITY
#include <DataServices.H>
#include <AmrData.H>
#endif

#define GEOM_GROW   1
#define PRESS_GROW  1
#define DIVU_GROW   1
#define DSDT_GROW   1

#define DEF_LIMITS(fab,fabdat,fablo,fabhi)   \
const int* fablo = (fab).loVect();           \
const int* fabhi = (fab).hiVect();           \
Real* fabdat = (fab).dataPtr();

#define DEF_CLIMITS(fab,fabdat,fablo,fabhi)  \
const int* fablo = (fab).loVect();           \
const int* fabhi = (fab).hiVect();           \
const Real* fabdat = (fab).dataPtr();

namespace
{
    bool initialized = false;
}
//
// Set defaults for all variables in Initialize()!!!
//
static int hyp_grow;

ErrorList   NavierStokes::err_list;
BCRec       NavierStokes::phys_bc;
Projection* NavierStokes::projector;
MacProj*    NavierStokes::mac_projector;
Godunov*    NavierStokes::godunov;

int  NavierStokes::verbose;
Real NavierStokes::cfl;
Real NavierStokes::init_shrink;
Real NavierStokes::change_max;
Real NavierStokes::fixed_dt;
Real NavierStokes::dt_cutoff;
int  NavierStokes::init_iter;
Real NavierStokes::gravity;
int  NavierStokes::initial_step;
int  NavierStokes::initial_iter;
int  NavierStokes::radius_grow;
int  NavierStokes::sum_interval;
int  NavierStokes::turb_interval;
int  NavierStokes::jet_interval;
int  NavierStokes::jet_interval_split;
int  NavierStokes::NUM_SCALARS;
int  NavierStokes::NUM_STATE;
bool NavierStokes::def_harm_avg_cen2edge;
//
// ----------------------- viscosity parameters.
//
Real NavierStokes::be_cn_theta;
Real NavierStokes::visc_tol;           // tolerance for viscous solve
Real NavierStokes::visc_abs_tol;       // absolute tol. for visc solve
int  NavierStokes::variable_vel_visc;  // variable viscosity flag
int  NavierStokes::variable_scal_diff; // variable scalar diffusion flag
//
// Internal switches.
//
int  NavierStokes::do_temp;
int  NavierStokes::do_trac2;
int  NavierStokes::Temp;
int  NavierStokes::Tracer;
int  NavierStokes::Tracer2;
int  NavierStokes::do_sync_proj;
int  NavierStokes::do_MLsync_proj;
int  NavierStokes::do_reflux;
int  NavierStokes::modify_reflux_normal_vel;
int  NavierStokes::do_mac_proj;
int  NavierStokes::do_init_vort_proj;
int  NavierStokes::do_init_proj;
int  NavierStokes::do_refine_outflow;
int  NavierStokes::do_derefine_outflow;
int  NavierStokes::Nbuf_outflow;
int  NavierStokes::do_running_statistics;
int  NavierStokes::do_denminmax;
int  NavierStokes::do_scalminmax;
int  NavierStokes::do_density_ref;
int  NavierStokes::do_tracer_ref;
int  NavierStokes::do_tracer2_ref; 
int  NavierStokes::do_vorticity_ref;
int  NavierStokes::getForceVerbose;
int  NavierStokes::do_scalar_update_in_order;
int  NavierStokes::Dpdt_Type;
int  NavierStokes::do_mom_diff;
int  NavierStokes::predict_mom_together;
int  NavierStokes::do_cons_trac;
int  NavierStokes::do_cons_trac2;
//     
// New members for non-zero divu.
//
int  NavierStokes::additional_state_types_initialized;
int  NavierStokes::Divu_Type;
int  NavierStokes::Dsdt_Type;
int  NavierStokes::have_divu;
int  NavierStokes::have_dsdt;
int  NavierStokes::S_in_vel_diffusion;
Real NavierStokes::divu_relax_factor;
int  NavierStokes::num_state_type;     // for backward compatibility
int  NavierStokes::do_divu_sync;       // for debugging new correction to MLSP
Real NavierStokes::volWgtSum_sub_origin_x;
Real NavierStokes::volWgtSum_sub_origin_y;
Real NavierStokes::volWgtSum_sub_origin_z;
Real NavierStokes::volWgtSum_sub_Rcyl;
Real NavierStokes::volWgtSum_sub_dx;
Real NavierStokes::volWgtSum_sub_dy;
Real NavierStokes::volWgtSum_sub_dz;

Array<Real>          NavierStokes::visc_coef;
Array<int>           NavierStokes::is_diffusive;
Array<AdvectionForm> NavierStokes::advectionType;
Array<DiffusionForm> NavierStokes::diffusionType;
Array<int>           NavierStokes::scalarUpdateOrder;

int NavierStokes::DoTrac2() {return do_trac2;}

BL_FORT_PROC_DECL(BL_NS_DOTRAC2,bl_ns_dotrac2)(int* dotrac2)
{
  *dotrac2 = NavierStokes::DoTrac2();
}

void
NavierStokes::variableCleanUp ()
{
    desc_lst.clear();
    derive_lst.clear();
    err_list.clear();
    delete projector;
    projector = 0;
    delete mac_projector;
    mac_projector = 0;
    delete godunov;
    godunov = 0;

    visc_coef.clear();
    is_diffusive.clear();
    advectionType.clear();
    diffusionType.clear();
    scalarUpdateOrder.clear();

}

void
NavierStokes::read_geometry ()
{
#if (BL_SPACEDIM == 2)
    //
    // Must load coord here because Geometry hasn't read it in yet.
    //
    ParmParse pp("geometry");

    int coord;
    pp.get("coord_sys",coord);

    if ((Geometry::CoordType) coord == Geometry::RZ && phys_bc.lo(0) != Symmetry)
    {
        phys_bc.setLo(0,Symmetry);

        if (ParallelDescriptor::IOProcessor())
            std::cout << "\nWarning: Setting phys_bc at xlo to Symmetry\n\n";
    }
#endif
}

void
NavierStokes::Initialize ()
{
    if (initialized) return;

    hyp_grow                            = 3;
    NavierStokes::projector             = 0;
    NavierStokes::mac_projector         = 0;
    NavierStokes::godunov               = 0;
    NavierStokes::verbose               = 0;
    NavierStokes::cfl                   = 0.8;
    NavierStokes::init_shrink           = 1.0;
    NavierStokes::change_max            = 1.1;
    NavierStokes::fixed_dt              = -1.0;
    NavierStokes::dt_cutoff             = 0.0;
    NavierStokes::init_iter             = 2;
    NavierStokes::gravity               = 0.0;
    NavierStokes::initial_step          = false;
    NavierStokes::initial_iter          = false;
    NavierStokes::radius_grow           = 1;
    NavierStokes::sum_interval          = -1;
    NavierStokes::turb_interval         = -1;
    NavierStokes::jet_interval          = -1;
    NavierStokes::jet_interval_split    = 2;
    NavierStokes::NUM_SCALARS           = 0;
    NavierStokes::NUM_STATE             = 0;
    NavierStokes::def_harm_avg_cen2edge = false;
    //
    // ----------------------- viscosity parameters.
    //
    NavierStokes::be_cn_theta        = 0.5;
    NavierStokes::visc_tol           = 1.0e-10; // tolerance for viscous solve
    NavierStokes::visc_abs_tol       = 1.0e-10; // absolute tol. for visc solve
    NavierStokes::variable_vel_visc  = 0;       // variable viscosity flag
    NavierStokes::variable_scal_diff = 0;       // variable scalar diffusion flag
    //
    // Internal switches.
    //
    NavierStokes::do_temp                    = 0;
    NavierStokes::do_trac2                   = 0;
    NavierStokes::Temp                       = -1;
    NavierStokes::Tracer                     = -1;
    NavierStokes::Tracer2                    = -1;
    NavierStokes::do_sync_proj               = 1;
    NavierStokes::do_MLsync_proj             = 1;
    NavierStokes::do_reflux                  = 1;
    NavierStokes::modify_reflux_normal_vel   = 0;
    NavierStokes::do_mac_proj                = 1;
    NavierStokes::do_init_vort_proj          = 0;
    NavierStokes::do_init_proj               = 1;
    NavierStokes::do_refine_outflow          = 0;
    NavierStokes::do_derefine_outflow        = 1;
    NavierStokes::Nbuf_outflow               = 1;
    NavierStokes::do_running_statistics      = 0;
    NavierStokes::do_denminmax               = 0;
    NavierStokes::do_scalminmax              = 0;
    NavierStokes::do_density_ref             = 0;
    NavierStokes::do_tracer_ref              = 0;
    NavierStokes::do_tracer2_ref             = 0;
    NavierStokes::do_vorticity_ref           = 0;
    NavierStokes::getForceVerbose            = 0;
    NavierStokes::do_scalar_update_in_order  = 0;
    NavierStokes::Dpdt_Type                  = -1;
    NavierStokes::do_mom_diff                = 0;
    NavierStokes::predict_mom_together       = 0;
    NavierStokes::do_cons_trac               = 0;
    NavierStokes::do_cons_trac2              = 0;
    //     
    // New members for non-zero divu.
    //
    NavierStokes::additional_state_types_initialized = 0;
    NavierStokes::Divu_Type                          = -1;
    NavierStokes::Dsdt_Type                          = -1;
    NavierStokes::have_divu                          = 0;
    NavierStokes::have_dsdt                          = 0;
    NavierStokes::S_in_vel_diffusion                 = 1;
    NavierStokes::divu_relax_factor                  = 0.0;
    NavierStokes::num_state_type                     = 2;
    NavierStokes::do_divu_sync                       = 0;
    NavierStokes::volWgtSum_sub_origin_x             = 0;
    NavierStokes::volWgtSum_sub_origin_y             = 0;
    NavierStokes::volWgtSum_sub_origin_z             = 0;
    NavierStokes::volWgtSum_sub_Rcyl                 = -1;
    NavierStokes::volWgtSum_sub_dx                   = -1;
    NavierStokes::volWgtSum_sub_dy                   = -1;
    NavierStokes::volWgtSum_sub_dz                   = -1;

    ParmParse pp("ns");

    pp.query("v",verbose);

    Array<int> lo_bc(BL_SPACEDIM), hi_bc(BL_SPACEDIM);
    pp.getarr("lo_bc",lo_bc,0,BL_SPACEDIM);
    pp.getarr("hi_bc",hi_bc,0,BL_SPACEDIM);
    for (int i = 0; i < BL_SPACEDIM; i++)
    {
        phys_bc.setLo(i,lo_bc[i]);
        phys_bc.setHi(i,hi_bc[i]);
    }
  
    read_geometry();
    //
    // Check phys_bc against possible periodic geometry
    // if periodic, must have internal BC marked.
    //
    if (Geometry::isAnyPeriodic())
    {
        //
        // Do idiot check.  Periodic means interior in those directions.
        //
        for (int dir = 0; dir < BL_SPACEDIM; dir++)
        {
            if (Geometry::isPeriodic(dir))
            {
                if (lo_bc[dir] != Interior)
                {
                    std::cerr << "NavierStokes::variableSetUp:periodic in direction "
                              << dir
                              << " but low BC is not Interior\n";
                    BoxLib::Abort("NavierStokes::Initialize()");
                }
                if (hi_bc[dir] != Interior)
                {
                    std::cerr << "NavierStokes::variableSetUp:periodic in direction "
                              << dir
                              << " but high BC is not Interior\n";
                    BoxLib::Abort("NavierStokes::Initialize()");
                }
            } 
        }
    }

    {
        //
        // Do idiot check.  If not periodic, should be no interior.
        //
        for (int dir = 0; dir < BL_SPACEDIM; dir++)
        {
            if (!Geometry::isPeriodic(dir))
            {
              if (lo_bc[dir] == Interior)
              {
                  std::cerr << "NavierStokes::variableSetUp:Interior bc in direction "
                            << dir
                            << " but not defined as periodic\n";
                  BoxLib::Abort("NavierStokes::Initialize()");
              }
              if (hi_bc[dir] == Interior)
              {
                  std::cerr << "NavierStokes::variableSetUp:Interior bc in direction "
                            << dir
                            << " but not defined as periodic\n";
                  BoxLib::Abort("NavierStokes::Initialize()");
              }
            }
        }
    }
    //
    // Get timestepping parameters.
    //
    pp.get("cfl",cfl);
    pp.query("init_iter",init_iter);
    pp.query("init_shrink",init_shrink);
    pp.query("dt_cutoff",dt_cutoff);
    pp.query("change_max",change_max);
    pp.query("fixed_dt",fixed_dt);
    pp.query("sum_interval",sum_interval);
    pp.query("turb_interval",turb_interval);
    pp.query("jet_interval",jet_interval);
    pp.query("jet_interval_split",jet_interval_split);
    pp.query("gravity",gravity);
    //
    // Get run options.
    //
    pp.query("do_temp",                  do_temp          );
    pp.query("do_trac2",                 do_trac2         );
    pp.query("do_cons_trac",             do_cons_trac     );
    pp.query("do_cons_trac2",            do_cons_trac2    );
    int initial_do_sync_proj =           do_sync_proj;
    pp.query("do_sync_proj",             do_sync_proj     );
    pp.query("do_MLsync_proj",           do_MLsync_proj   );
    pp.query("do_reflux",                do_reflux        );
    pp.query("modify_reflux_normal_vel", modify_reflux_normal_vel);
    pp.query("do_init_vort_proj",        do_init_vort_proj);
    pp.query("do_init_proj",             do_init_proj     );
    pp.query("do_mac_proj",              do_mac_proj      );
    pp.query("do_divu_sync",             do_divu_sync     );
    pp.query("do_denminmax",             do_denminmax     );
    pp.query("do_scalminmax",            do_scalminmax    );
    pp.query("do_density_ref",           do_density_ref   );
    pp.query("do_tracer_ref",            do_tracer_ref    );
    pp.query("do_tracer2_ref",           do_tracer2_ref   );
    pp.query("do_vorticity_ref",         do_vorticity_ref );

    if (modify_reflux_normal_vel)
        BoxLib::Abort("modify_reflux_normal_vel is no longer supported");

#ifdef MOREGENGETFORCE
    pp.query("getForceVerbose",          getForceVerbose  );
    pp.query("do_scalar_update_in_order",do_scalar_update_in_order );
    if (do_scalar_update_in_order) {
	const int n_scalar_update_order_vals = pp.countval("scalar_update_order");
	scalarUpdateOrder.resize(n_scalar_update_order_vals);
	int got_scalar_update_order = pp.queryarr("scalar_update_order",scalarUpdateOrder,0,n_scalar_update_order_vals);
    }
#endif

    //
    // Make sure we don't use divu_sync.
    //
    if (do_divu_sync)
        BoxLib::Error("do_divu_sync == 1 is the wrong setting");
    //
    // This test ensures that if the user toggles do_sync_proj,
    // the user has knowledge that do_MLsync_proj is meaningless.
    //
    if (do_MLsync_proj && !do_sync_proj && initial_do_sync_proj != do_sync_proj)
    {
        std::cout << "Mismatched options for NavierStokes\n"
                  << "do_MLsync_proj and do_sync_proj are inconsistent\n";

        BoxLib::Abort("NavierStokes::Initialize()");
    }
    //
    // Read viscous/diffusive parameters and array of viscous/diffusive coeffs.
    // NOTE: at this point, we dont know number of state variables
    //       so just read all values listed.
    //
    pp.query("visc_tol",visc_tol);
    pp.query("visc_abs_tol",visc_abs_tol);
    pp.query("variable_vel_visc",variable_vel_visc);
    pp.query("variable_scal_diff",variable_scal_diff);

    const int n_vel_visc_coef   = pp.countval("vel_visc_coef");
    const int n_temp_cond_coef  = pp.countval("temp_cond_coef");
    const int n_scal_diff_coefs = pp.countval("scal_diff_coefs");

    if (n_vel_visc_coef != 1)
        BoxLib::Abort("NavierStokes::Initialize(): Only one vel_visc_coef allowed");

    if (do_temp && n_temp_cond_coef != 1)
        BoxLib::Abort("NavierStokes::Initialize(): Only one temp_cond_coef allowed");

    int n_visc = BL_SPACEDIM + 1 + n_scal_diff_coefs;
    if (do_temp)
        n_visc++;
    visc_coef.resize(n_visc);
    is_diffusive.resize(n_visc);
 
    pp.get("vel_visc_coef",visc_coef[0]);
    for (int i = 1; i < BL_SPACEDIM; i++)
      visc_coef[i] = visc_coef[0];
    //
    // Here we set the coefficient for density, which does not diffuse.
    //
    visc_coef[Density] = -1;
    //
    // Set the coefficients for the scalars, but temperature.
    //
    Array<Real> scal_diff_coefs(n_scal_diff_coefs);
    pp.getarr("scal_diff_coefs",scal_diff_coefs,0,n_scal_diff_coefs);

    int scalId = Density;

    // Will need to add more lines when more variables are added
    Tracer = Density+1;
    if (do_trac2)
	Tracer2 = Density+2;

    for (int i = 0; i < n_scal_diff_coefs; i++)
    {
        visc_coef[++scalId] = scal_diff_coefs[i];
    }
    //
    // Set the coefficient for temperature.
    //
    if (do_temp)
    {
	Temp = ++scalId;
	pp.get("temp_cond_coef",visc_coef[Temp]);
    }
    
    pp.query("divu_relax_factor",divu_relax_factor);
    pp.query("S_in_vel_diffusion",S_in_vel_diffusion);
    pp.query("be_cn_theta",be_cn_theta);
    if (be_cn_theta > 1.0 || be_cn_theta < .5)
        BoxLib::Abort("NavierStokes::Initialize(): Must have be_cn_theta <= 1.0 && >= .5");
    //
    // Set parameters dealing with how grids are treated at outflow boundaries.
    //
    pp.query("do_refine_outflow",do_refine_outflow);
    pp.query("do_derefine_outflow",do_derefine_outflow);
    if (do_derefine_outflow) do_refine_outflow = 0;

    pp.query("Nbuf_outflow",Nbuf_outflow);
    BL_ASSERT(Nbuf_outflow >= 0);
    BL_ASSERT(!(Nbuf_outflow <= 0 && do_derefine_outflow == 1));

    //
    // Check whether we are doing running statistics.
    //
    pp.query("do_running_statistics",do_running_statistics);

    // If dx,dy,dz,Rcyl<0 (default) the volWgtSum is computed over the entire domain
    pp.query("volWgtSum_sub_origin_x",volWgtSum_sub_origin_x);
    pp.query("volWgtSum_sub_origin_y",volWgtSum_sub_origin_y);
    pp.query("volWgtSum_sub_origin_z",volWgtSum_sub_origin_z);
    pp.query("volWgtSum_sub_Rcyl",volWgtSum_sub_Rcyl);
    pp.query("volWgtSum_sub_dx",volWgtSum_sub_dx);
    pp.query("volWgtSum_sub_dy",volWgtSum_sub_dy);
    pp.query("volWgtSum_sub_dz",volWgtSum_sub_dz);

    //
    // Are we going to do velocity or momentum update?
    //
    pp.query("do_mom_diff",do_mom_diff);
    pp.query("predict_mom_together",predict_mom_together);

    if (do_mom_diff == 0 && predict_mom_together == 1)
    {
      std::cout << "MAKES NO SENSE TO HAVE DO_MOM_DIFF=0 AND PREDICT_MOM_TOGETHER=1" << std::endl;
      exit(0);
    }

    pp.query("harm_avg_cen2edge", def_harm_avg_cen2edge);

    {
        ParmParse pp("godunov");
        int ppm_type = 0;
        pp.query("ppm_type",ppm_type);
        if (ppm_type == 2)
	{
            hyp_grow = 4;
	}
    }

    BoxLib::ExecOnFinalize(NavierStokes::Finalize);

    initialized = true;
}

void
NavierStokes::Finalize ()
{
    initialized = false;
}

NavierStokes::NavierStokes ()
{
    rho_avg      = 0;
    rho_half     = 0;
    rho_ptime    = 0;
    rho_ctime    = 0;
    rho_qtime    = 0;
    rho_tqtime   = 0;
    p_avg        = 0;
    Vsync        = 0;
    Ssync        = 0;
    sync_reg     = 0;
    advflux_reg  = 0;
    viscflux_reg = 0;
    u_mac        = 0;
    aofs         = 0;
    diffusion    = 0;

    if (!additional_state_types_initialized)
        init_additional_state_types();
}

NavierStokes::NavierStokes (Amr&            papa,
                            int             lev,
                            const Geometry& level_geom,
                            const BoxArray& bl,
                            Real            time)
    :
    AmrLevel(papa,lev,level_geom,bl,time)
{
    if(!additional_state_types_initialized)
        init_additional_state_types();
    
    const BoxArray& P_grids = state[Press_Type].boxArray();
    //
    // Alloc old_time pressure.
    //
    state[Press_Type].allocOldData();
    //
    // Alloc space for density and temporary pressure variables.
    //
    p_avg   = 0;
    rho_avg = 0;
    if (level > 0)
    {
        rho_avg = new MultiFab(grids,1,1);
        p_avg   = new MultiFab(P_grids,1,0);
    }

    rho_half   = new MultiFab(grids,1,1);
    rho_ptime  = new MultiFab(grids,1,1);
    rho_ctime  = new MultiFab(grids,1,1);
    rho_qtime  = 0;
    rho_tqtime = 0;
    //
    // Build metric coefficients for RZ calculations.
    //
    buildMetrics();
    //
    // Set up reflux registers.
    //
    sync_reg = 0;
    if (level > 0 && do_sync_proj)
    {
        sync_reg = new SyncRegister(grids,crse_ratio,level);
    }
    advflux_reg  = 0;
    viscflux_reg = 0;
    if (level > 0 && do_reflux)
    {
        advflux_reg  = new FluxRegister(grids,crse_ratio,level,NUM_STATE);
        viscflux_reg = new FluxRegister(grids,crse_ratio,level,NUM_STATE);
    }
    //
    // Initialize work multifabs.
    //
    Vsync   = 0;
    Ssync   = 0;
    u_mac   = 0;
    aofs    = 0;
    //
    // Set up the level projector.
    //
    if (projector == 0)
    {
        projector = new Projection(parent,&phys_bc,do_sync_proj,
                                   parent->finestLevel(),radius_grow);
    }
    projector->install_level(level,this,&radius);
    //
    // Set up the godunov box.
    //
    SetGodunov();
    //
    // Set up diffusion.
    //
    diffusion = new Diffusion(parent,this,
                              (level > 0) ? getLevel(level-1).diffusion : 0,
                              NUM_STATE,viscflux_reg,is_diffusive,visc_coef);
    //
    // Allocate the storage for variable viscosity and diffusivity
    //
    viscn_cc   = 0;
    viscnp1_cc = 0;
    if (variable_vel_visc) 
    {
        viscn_cc   = new MultiFab(grids, 1, 1);
        viscnp1_cc = new MultiFab(grids, 1, 1);
    }

    diffn_cc   = 0;
    diffnp1_cc = 0;
    if (variable_scal_diff) 
    {
        diffn_cc   = new MultiFab(grids, NUM_STATE-Density-1, 1);
        diffnp1_cc = new MultiFab(grids, NUM_STATE-Density-1, 1);
    }
    //
    // Set up the mac projector.
    //
    if (mac_projector == 0)
    {
        mac_projector = new MacProj(parent,parent->finestLevel(),
                                    &phys_bc,radius_grow);
    }
    mac_projector->install_level(level,this);
}

NavierStokes::~NavierStokes ()
{
    delete rho_avg;
    delete p_avg;
    delete rho_half;
    delete rho_ptime;
    delete rho_ctime;
    delete rho_qtime;
    delete rho_tqtime;
    delete Vsync;
    delete Ssync;
    delete sync_reg;
    delete advflux_reg;
    delete viscflux_reg;
    delete [] u_mac;
    
    if (mac_projector != 0)
        mac_projector->cleanup(level);
    //
    // Remove the arrays for variable viscosity and diffusivity
    // and delete the Diffusion object
    //
    if (variable_vel_visc)
    {
        delete viscn_cc;
        delete viscnp1_cc;
    }

    if (variable_scal_diff)
    {
        delete diffn_cc;
        delete diffnp1_cc;
    }

    delete diffusion;
}

void
NavierStokes::init_additional_state_types ()
{
    additional_state_types_initialized = 1;
    //
    // Set "Temp" from user's variable setup.
    //
    int dummy_State_Type;
    int have_temp = isStateVariable("temp", dummy_State_Type, Temp);
    have_temp &= (dummy_State_Type == State_Type);
    BL_ASSERT((do_temp && have_temp)  ||  (!do_temp && !have_temp));

    int _Divu = -1;
    int dummy_Divu_Type;
    have_divu = 0;
    have_divu = isStateVariable("divu", dummy_Divu_Type, _Divu);
    have_divu = have_divu && dummy_Divu_Type == Divu_Type;
    if (verbose && ParallelDescriptor::IOProcessor())
    {
        std::cout << "NavierStokes::init_additional_state_types()::have_divu = "
                  << have_divu << '\n';
    }
    if (have_divu && _Divu!=Divu)
    {
        std::cout << "divu must be 0-th, Divu_Type component in the state\n";

        BoxLib::Abort("NavierStokes::init_additional_state_types()");
    }

    if (have_divu && do_sync_proj && !do_MLsync_proj) 
    {
        std::cout << "Must run the ML sync project if have_divu is true " << std::endl;
        std::cout << "  because the divu sync is only implemented in the " << std::endl;
        std::cout << "  multilevel sync (MLsyncProject), not in the single level " << std::endl;
        std::cout << "  (syncProject)." << std::endl;
        BoxLib::Abort("NavierStokes::init_additional_state_types()");
    }

    int _Dsdt = -1;
    int dummy_Dsdt_Type;
    have_dsdt = 0;
    have_dsdt = isStateVariable("dsdt", dummy_Dsdt_Type, _Dsdt);
    have_dsdt = have_dsdt && dummy_Dsdt_Type==Dsdt_Type;
    if (verbose && ParallelDescriptor::IOProcessor())
    {
        std::cout << "NavierStokes::init_additional_state_types()::have_dsdt = "
                  << have_dsdt << '\n';
    }
    if (have_dsdt && _Dsdt!=Dsdt)
    {
        std::cout << "dsdt must be 0-th, Dsdt_Type component in the state\n";

        BoxLib::Abort("NavierStokes::init_additional_state_types()");
    }
    if (have_dsdt && !have_divu)
    {
        std::cout << "Must have divu in order to have dsdt\n";

        BoxLib::Abort("NavierStokes::init_additional_state_types()");
    }

    num_state_type = desc_lst.size();
    if (verbose && ParallelDescriptor::IOProcessor())
    {
        std::cout << "NavierStokes::init_additional_state_types: num_state_type = "
                  << num_state_type << '\n';
    }
}

//
// Since the pressure solver always stores its estimate of the
// pressure solver in Pnew, we need to copy it to Pold at the start.
//
void
NavierStokes::initOldPress ()
{
    MultiFab& P_new = get_new_data(Press_Type);
    MultiFab& P_old = get_old_data(Press_Type);

    for (MFIter mfi(P_new); mfi.isValid(); ++mfi)
    {
        P_old[mfi].copy(P_new[mfi]);
    }
}

void
NavierStokes::zeroNewPress ()
{
    get_new_data(Press_Type).setVal(0);
}

void
NavierStokes::zeroOldPress ()
{
    get_old_data(Press_Type).setVal(0);
}

void
NavierStokes::allocOldData ()
{
    bool init_pres = !(state[Press_Type].hasOldData());

    for (int k = 0; k < num_state_type; k++)
    {
        state[k].allocOldData();
    }
    if (init_pres)
        initOldPress();
}

void
NavierStokes::removeOldData ()
{
    AmrLevel::removeOldData();
}

void
NavierStokes::SetGodunov()
{
    if (godunov == 0)
        godunov = new Godunov();
}

void
NavierStokes::restart (Amr&          papa,
                       std::istream& is,
                       bool          bReadSpecial)
{
    AmrLevel::restart(papa,is,bReadSpecial);

    if (projector == 0)
    {
        projector = new Projection(parent,&phys_bc,do_sync_proj,
                                   parent->finestLevel(),radius_grow);
    }
    projector->install_level(level, this, &radius);
    //
    // Set the godunov box.
    //
    SetGodunov();
    
    if (mac_projector == 0)
    {
        mac_projector = new MacProj(parent,parent->finestLevel(),
                                    &phys_bc,radius_grow);
    }
    mac_projector->install_level(level,this);

    rho_avg = 0;
    p_avg   = 0;
    const BoxArray& P_grids = state[Press_Type].boxArray();
    //
    // Alloc space for density and temporary pressure variables.
    //
    if (level > 0)
    {
        rho_avg = new MultiFab(grids,1,1);
        p_avg   = new MultiFab(P_grids,1,0);
    }
    rho_half   = new MultiFab(grids,1,1);
    rho_ptime  = new MultiFab(grids,1,1);
    rho_ctime  = new MultiFab(grids,1,1);
    rho_qtime  = 0;
    rho_tqtime = 0;
    //
    // Build metric coefficients for RZ calculations.
    //
    buildMetrics();

    BL_ASSERT(sync_reg == 0);
    if (level > 0 && do_sync_proj)
    {
        sync_reg = new SyncRegister(grids,crse_ratio,level);
    }
    BL_ASSERT(advflux_reg == 0);
    if (level > 0 && do_reflux)
    {
        advflux_reg = new FluxRegister(grids,crse_ratio,level,NUM_STATE);
    }
    BL_ASSERT(viscflux_reg == 0);
    if (level > 0 && do_reflux)
    {
        viscflux_reg = new FluxRegister(grids,crse_ratio,level,NUM_STATE);
    }

    BL_ASSERT(Vsync == 0);
    BL_ASSERT(Ssync == 0);
    if (level < parent->finestLevel())
    {
        Vsync = new MultiFab(grids,BL_SPACEDIM,1);
        Ssync = new MultiFab(grids,NUM_STATE-BL_SPACEDIM,1);
    }

    diffusion = new Diffusion(parent, this,
                              (level > 0) ? getLevel(level-1).diffusion : 0,
                              NUM_STATE, viscflux_reg,is_diffusive, visc_coef);
    //
    // Allocate the storage for variable viscosity and diffusivity
    //
    viscn_cc   = 0;
    viscnp1_cc = 0;
    if (variable_vel_visc)
    {
        viscn_cc   = new MultiFab(grids, 1, 1);
        viscnp1_cc = new MultiFab(grids, 1, 1);
    }

    diffn_cc   = 0;
    diffnp1_cc = 0;
    if (variable_scal_diff)
    {
        diffn_cc   = new MultiFab(grids, NUM_STATE-Density-1, 1);
        diffnp1_cc = new MultiFab(grids, NUM_STATE-Density-1, 1);
    }

    is_first_step_after_regrid = false;
    old_intersect_new          = grids;
}

void
NavierStokes::buildMetrics ()
{
    //
    // We "should" only need radius when we're RZ, but some 2-D code is written to
    // access it first and then "use" if if RZ.  It's easier to just always build
    // it for 2D than try to fix the underlying Fortran calls that take radius.
    //
#if (BL_SPACEDIM == 2)
    radius.resize(grids.size());

    const Real dxr = geom.CellSize()[0];

    for (int i = 0; i < grids.size(); i++)
    {
        const int ilo = grids[i].smallEnd(0)-radius_grow;
        const int ihi = grids[i].bigEnd(0)+radius_grow;
        const int len = ihi - ilo + 1;

        radius[i].resize(len);

        RealBox gridloc = RealBox(grids[i],geom.CellSize(),geom.ProbLo());

        const Real xlo = gridloc.lo(0) + (0.5 - radius_grow)*dxr;
        for (int j = 0; j < len; j++)
            radius[i][j] = xlo + j*dxr;
    }
#endif
}

//
// Reset the time levels to time (time) and timestep dt.
// This is done at the start of the timestep in the pressure iteration section.
//

void
NavierStokes::resetState (Real time,
                          Real dt_old,
                          Real dt_new)
{
    //
    // Reset state types.
    //
    state[State_Type].reset();
    state[State_Type].setTimeLevel(time,dt_old,dt_new);

    initOldPress();
    if (state[Press_Type].descriptor()->timeType() == StateDescriptor::Interval) 
    {
        state[Press_Type].setTimeLevel(time-dt_old,dt_old,dt_new);
    } 
    else if (state[Press_Type].descriptor()->timeType() == StateDescriptor::Point) 
    {
        state[Press_Type].setTimeLevel(time-.5*dt_old,dt_old,dt_old);
        state[Dpdt_Type].setTimeLevel(time-dt_old,dt_old,dt_old);
    } 
    //
    // Reset state types for divu not equal to zero.
    //
    if (have_divu)
    {
        state[Divu_Type].reset();
        state[Divu_Type].setTimeLevel(time,dt_old,dt_new);
        if (have_dsdt)
        {
            //
            // Dont do this, we want to improve dsdt with press iters
            // but we do need to make sure time is set correctly..
            // state[Dsdt_Type].reset();
            state[Dsdt_Type].setTimeLevel(time,dt_old,dt_new);
        }
    }
}

//
// Set the time levels to time (time) and timestep dt.
//
void
NavierStokes::setTimeLevel (Real time,
                            Real dt_old,
                            Real dt_new)
{
    state[State_Type].setTimeLevel(time,dt_old,dt_new);

    if (have_divu)
    {
        state[Divu_Type].setTimeLevel(time,dt_old,dt_new);
        if (have_dsdt)
        {
            state[Dsdt_Type].setTimeLevel(time,dt_old,dt_new);
        }
    }

    if (state[Press_Type].descriptor()->timeType() == StateDescriptor::Interval) 
    {
        state[Press_Type].setTimeLevel(time-dt_old,dt_old,dt_old);
    } 
    else if (state[Press_Type].descriptor()->timeType() == StateDescriptor::Point) 
    {
        state[Press_Type].setTimeLevel(time-.5*dt_old,dt_old,dt_old);
        state[Dpdt_Type].setTimeLevel(time-dt_old,dt_old,dt_old);
    }
}

//
// This function initializes the State and Pressure with data.
//

void
NavierStokes::initData ()
{
    //
    // Initialize the state and the pressure.
    //
    int         ns       = NUM_STATE - BL_SPACEDIM;
    const Real* dx       = geom.CellSize();
    MultiFab&   S_new    = get_new_data(State_Type);
    MultiFab&   P_new    = get_new_data(Press_Type);
    const Real  cur_time = state[State_Type].curTime();

    for (MFIter snewmfi(S_new); snewmfi.isValid(); ++snewmfi)
    {
        BL_ASSERT(grids[snewmfi.index()] == snewmfi.validbox());

        P_new[snewmfi].setVal(0);

        const int  i       = snewmfi.index();
        RealBox    gridloc = RealBox(grids[i],geom.CellSize(),geom.ProbLo());
        const int* lo      = snewmfi.validbox().loVect();
        const int* hi      = snewmfi.validbox().hiVect();
        const int* s_lo    = S_new[snewmfi].loVect();
        const int* s_hi    = S_new[snewmfi].hiVect();
        const int* p_lo    = P_new[snewmfi].loVect();
        const int* p_hi    = P_new[snewmfi].hiVect();

        FORT_INITDATA (&level,&cur_time,lo,hi,&ns,
                       S_new[snewmfi].dataPtr(Xvel),
                       S_new[snewmfi].dataPtr(BL_SPACEDIM),
                       ARLIM(s_lo), ARLIM(s_hi),
                       P_new[snewmfi].dataPtr(),
                       ARLIM(p_lo), ARLIM(p_hi),
                       dx,gridloc.lo(),gridloc.hi() );
    }

#ifdef BL_USE_VELOCITY
    //
    // We want to add the velocity from the supplied plotfile
    // to what we already put into the velocity field via FORT_INITDATA.
    //
    // This code has a few drawbacks.  It assumes that the physical
    // domain size of the current problem is the same as that of the
    // one that generated the pltfile.  It also assumes that the pltfile
    // has at least as many levels (with the same refinement ratios) as does
    // the current problem.  If either of these are false this code is
    // likely to core dump.
    //
    ParmParse pp("ns");

    std::string velocity_plotfile;
    pp.query("velocity_plotfile", velocity_plotfile);

    std::string velocity_plotfile_xvel_name = "x_velocity";
    pp.query("velocity_plotfile_xvel_name", velocity_plotfile_xvel_name);

    Real velocity_plotfile_scale(1.0);
    pp.query("velocity_plotfile_scale",velocity_plotfile_scale);

    if (!velocity_plotfile.empty())
    {
        if (ParallelDescriptor::IOProcessor())
	  std::cout << "initData: reading data from: " << velocity_plotfile << " (" << velocity_plotfile_xvel_name << ")" << std::endl;

        DataServices::SetBatchMode();
        FileType fileType(NEWPLT);
        DataServices dataServices(velocity_plotfile, fileType);

        if (!dataServices.AmrDataOk())
            //
            // This calls ParallelDescriptor::EndParallel() and exit()
            //
            DataServices::Dispatch(DataServices::ExitRequest, NULL);
    
        AmrData&                  amrData   = dataServices.AmrDataRef();
        Array<std::string>        plotnames = amrData.PlotVarNames();

        int idX = -1;
        for (int i = 0; i < plotnames.size(); ++i)
            if (plotnames[i] == velocity_plotfile_xvel_name) idX = i;

        if (idX == -1)
            BoxLib::Abort("Could not find velocity fields in supplied velocity_plotfile");
	else
	  std::cout << "Found " << velocity_plotfile_xvel_name << ", idX = " << idX << std::endl;

        MultiFab tmp(S_new.boxArray(), 1, 0);
        for (int i = 0; i < BL_SPACEDIM; i++)
        {
            amrData.FillVar(tmp, level, plotnames[idX+i], 0);
            for (MFIter mfi(tmp); mfi.isValid(); ++mfi) {
  	        tmp[mfi].mult(velocity_plotfile_scale, 0, 1);
                S_new[mfi].plus(tmp[mfi], tmp[mfi].box(), 0, Xvel+i, 1);
	    }
            amrData.FlushGrids(idX+i);
        }

        if (ParallelDescriptor::IOProcessor())
            std::cout << "initData: finished init from velocity_plotfile" << std::endl;
    }
#endif /*BL_USE_VELOCITY*/

    make_rho_prev_time();
    make_rho_curr_time();
    //
    // Initialize other types.
    //
    initDataOtherTypes();
    //
    // Initialize divU and dSdt.
    //
    if (have_divu)
    {
        const Real dt       = 1.0;
        const Real dtin     = -1.0; // Dummy value denotes initialization.
        const Real cur_time = state[Divu_Type].curTime();
        MultiFab&  Divu_new = get_new_data(Divu_Type);

        state[State_Type].setTimeLevel(cur_time,dt,dt);

        calc_divu(cur_time,dtin,Divu_new);

        if (have_dsdt)
            get_new_data(Dsdt_Type).setVal(0);
    }

    if (state[Press_Type].descriptor()->timeType() == StateDescriptor::Point) 
    {
        get_new_data(Dpdt_Type).setVal(0);
    }

    is_first_step_after_regrid = false;
    old_intersect_new          = grids;
}

//
// Fills a new level n with best level n and coarser data available.
//

void
NavierStokes::init (AmrLevel &old)
{
    NavierStokes* oldns     = (NavierStokes*) &old;
    const Real    dt_new    = parent->dtLevel(level);
    const Real    cur_time  = oldns->state[State_Type].curTime();
    const Real    prev_time = oldns->state[State_Type].prevTime();
    const Real    dt_old    = cur_time - prev_time;
    MultiFab&     S_new     = get_new_data(State_Type);
    MultiFab&     P_new     = get_new_data(Press_Type);
    MultiFab&     P_old     = get_old_data(Press_Type);

    setTimeLevel(cur_time,dt_old,dt_new);

    const Real cur_pres_time = state[Press_Type].curTime();
    //
    // Get best state and pressure data.
    //
    for (FillPatchIterator fpi(old,S_new,0,cur_time,State_Type,0,NUM_STATE);
        fpi.isValid();
         ++fpi)
    {
        S_new[fpi.index()].copy(fpi());
    }
    //
    // Note: we don't need to worry here about using FillPatch because
    //       it will automatically use the "old dpdt" to interpolate,
    //       since we haven't yet defined a new pressure at the lower level.
    //
    for (FillPatchIterator fpi(old,P_new,0,cur_pres_time,Press_Type,0,1);
         fpi.isValid();
         ++fpi)
    {
        P_old[fpi.index()].copy(fpi());
        P_new[fpi.index()].copy(fpi());
    }

    if (state[Press_Type].descriptor()->timeType() == StateDescriptor::Point) 
    {
        MultiFab& Dpdt_new = get_new_data(Dpdt_Type);

        for (FillPatchIterator fpi(old,Dpdt_new,0,cur_pres_time,Dpdt_Type,0,1);
             fpi.isValid();
             ++fpi)
        {
            Dpdt_new[fpi.index()].copy(fpi());
        }
    }
    //
    // Get best divu and dSdt data.
    //
    if (have_divu)
    {
        MultiFab& Divu_new = get_new_data(Divu_Type);
        
        for (FillPatchIterator fpi(old,Divu_new,0,cur_time,Divu_Type,0,1);
             fpi.isValid();
             ++fpi)
        {
            Divu_new[fpi.index()].copy(fpi());
        }

        if (have_dsdt)
        {
            MultiFab& Dsdt_new = get_new_data(Dsdt_Type);

            for (FillPatchIterator fpi(old,Dsdt_new,0,cur_time,Dsdt_Type,0,1);
                 fpi.isValid();
                 ++fpi)
            {
                Dsdt_new[fpi.index()].copy(fpi());
            }
        }
    }

    old_intersect_new          = BoxLib::intersect(grids,oldns->boxArray());
    is_first_step_after_regrid = true;
}

void
NavierStokes::init ()
{
    MultiFab& S_new = get_new_data(State_Type);
    MultiFab& P_new = get_new_data(Press_Type);
    MultiFab& P_old = get_old_data(Press_Type);

    BL_ASSERT(level > 0);

    const Array<Real>& dt_amr = parent->dtLevel();
    Array<Real>        dt_new(level+1);

    for (int lev = 0; lev < level; lev++)
        dt_new[lev] = dt_amr[lev];
    //
    // Guess new dt from new data (interpolated from coarser level).
    //
    const Real dt = dt_new[level-1]/Real(parent->MaxRefRatio(level-1));
    dt_new[level] = dt;

    parent->setDtLevel(dt_new);
    //
    // Compute dt based on old data.
    //
    NavierStokes& old       = getLevel(level-1);
    const Real    cur_time  = old.state[State_Type].curTime();
    const Real    prev_time = old.state[State_Type].prevTime();
    const Real    dt_old    = (cur_time-prev_time)/Real(parent->MaxRefRatio(level-1));

    setTimeLevel(cur_time,dt_old,dt);

    Real cur_pres_time = state[Press_Type].curTime();
    //
    // Get best coarse state and pressure data.
    //
    FillCoarsePatch(S_new,0,cur_time,State_Type,0,NUM_STATE);
    FillCoarsePatch(P_new,0,cur_pres_time,Press_Type,0,1);

    if (state[Press_Type].descriptor()->timeType() == StateDescriptor::Point) 
        FillCoarsePatch(get_new_data(Dpdt_Type),0,cur_time,Dpdt_Type,0,1);

    for (MFIter mfi(P_new); mfi.isValid(); ++mfi)
    {
        P_old[mfi].copy(P_new[mfi]);
    }
    //
    // Get best coarse divU and dSdt data.
    //
    if (have_divu)
    {
        FillCoarsePatch(get_new_data(Divu_Type),0,cur_time,Divu_Type,0,1);
        if (have_dsdt)
            FillCoarsePatch(get_new_data(Dsdt_Type),0,cur_time,Dsdt_Type,0,1);
    }
    old_intersect_new = grids;
}

//
// ADVANCE FUNCTIONS
//

//
// This function ensures that the multifab registers and boundary
// flux registers needed for syncing the composite grid
//
//     u_mac, umacG, Vsync, Ssync, rhoavg, fr_adv, fr_visc
//
// are initialized to zero.  In general these quantities
// along with the pressure sync registers (sync_reg) and
// advective velocity registers (mac_reg) are compiled by first
// setting them to the coarse value acquired during a coarse timestep
// and then incrementing in the fine values acquired during the
// subcycled fine timesteps.  This compilation procedure occurs in
// different parts for different quantities
//
// * u_mac is set in predict_velocity and mac_project.
// * fr_adv, fr_visc are set in velocity_advect and scalar_advect
// * Vsync, Ssync are set in subcycled calls to post_timestep
// * mac_reg is set in mac_project
// * sync_reg is set in level_project
// * rhoavg, pavg are set in advance_setup and advance
//
// After these quantities have been compiled during a coarse
// timestep and subcycled fine timesteps.  The post_timestep function
// uses them to sync the fine and coarse levels.  If the coarse level
// is not the base level, post_timestep modifies the next coarsest levels
// registers appropriately.
//
// Note :: There is a little ambiguity as to which level owns the
// boundary flux registers.  The Multifab registers are quantities
// sized by the coarse level BoxArray and belong to the coarse level.
// The fine levels own the boundary registers, since they are sized by
// the boundaries of the fine level BoxArray.
//

void
NavierStokes::advance_setup (Real time,
                             Real dt,
                             int  iteration,
                             int  ncycle)
{
    const int finest_level = parent->finestLevel();

    mac_projector->setup(level);
    //
    // Why are they defined here versus the constructor?
    //
    if (level < finest_level)
    {
        if (Vsync == 0)
            Vsync = new MultiFab(grids,BL_SPACEDIM,1);
        if (Ssync == 0)
            Ssync = new MultiFab(grids,NUM_STATE-BL_SPACEDIM,1);
        Vsync->setVal(0);
        Ssync->setVal(0);
    }
    //
    // Set reflux registers to zero.
    //
    if (do_reflux && level < finest_level)
    {
        getAdvFluxReg(level+1).setVal(0);
        getViscFluxReg(level+1).setVal(0);
    }
    //
    // Alloc space for edge velocities (normal comp only).
    //
    if (u_mac == 0)
    {
        u_mac = new MultiFab[BL_SPACEDIM];

        for (int dir = 0; dir < BL_SPACEDIM; dir++)
        {
            BoxArray edge_grids(grids);
            edge_grids.surroundingNodes(dir);
            u_mac[dir].define(edge_grids,1,1,Fab_allocate);
            u_mac[dir].setVal(1.e40);
        }
    }
    //
    // Alloc MultiFab to hold advective update terms.
    //
    BL_ASSERT(aofs == 0);
    aofs = new MultiFab(grids,NUM_STATE,0);
    //
    // Set rho_avg.
    //
    if (!initial_step && level > 0 && iteration == 1)
        initRhoAvg(0.5/Real(ncycle));
    //
    // Set up state multifabs for the advance.
    //
    for (int k = 0; k < num_state_type; k++)
    {
        state[k].allocOldData();
        state[k].swapTimeLevels(dt);
    }

    if (state[Press_Type].descriptor()->timeType() == StateDescriptor::Point) 
    {
        const Real new_press_time = .5 * (state[State_Type].prevTime() +
                                          state[State_Type].curTime());
        state[Press_Type].setNewTimeLevel(new_press_time);
    }

    make_rho_prev_time();
    //
    // If refRatio==4 to the next level coarser, and we're going to diffuse
    // scalars as SoverRho, we're going to need rho at 1/4 and 3/4 time there.
    // Make these things if need be.
    //
    if (level > 0)
    {
        bool needs_rho4 = false;

        if (parent->nCycle(level) == 4)
            for (int i = 0; i < NUM_STATE && !needs_rho4; ++i)
                needs_rho4 = (diffusionType[i] == Laplacian_SoverRho);

        if (needs_rho4)
        {
            NavierStokes&   clevel = getLevel(level-1);
            const BoxArray& cgrids = clevel.boxArray();
            const Real      ptime  = clevel.state[State_Type].prevTime();
            const Real      ctime  = clevel.state[State_Type].curTime();

            if (clevel.rho_qtime == 0)
            {
                const Real qtime = ptime + 0.25*(ctime-ptime);
                clevel.rho_qtime = new MultiFab(cgrids,1,1);
                FillPatchIterator fpi(clevel,*(clevel.rho_qtime),
                                      1,qtime,State_Type,Density,1);
                for ( ; fpi.isValid(); ++fpi)
                    (*clevel.rho_qtime)[fpi.index()].copy(fpi());
            }
            if (clevel.rho_tqtime == 0)
            {
                const Real tqtime = ptime + 0.75*(ctime-ptime);
                clevel.rho_tqtime = new MultiFab(cgrids,1,1);
                FillPatchIterator fpi(clevel,*(clevel.rho_tqtime),
                                      1,tqtime,State_Type,Density,1);
                for ( ; fpi.isValid(); ++fpi)
                    (*clevel.rho_tqtime)[fpi.index()].copy(fpi());
            }
        }
    }
    //
    // Calculate the time N viscosity and diffusivity
    //   Note: The viscosity and diffusivity at time N+1 are 
    //         initialized here to the time N values just to
    //         have something reasonable.
    //
    const Real prev_time = state[State_Type].prevTime();

    if (variable_vel_visc)
    {
        calcViscosity(prev_time,dt,iteration,ncycle);

        for (MFIter np1Mfi(*viscnp1_cc); np1Mfi.isValid(); ++np1Mfi)
        {
            (*viscnp1_cc)[np1Mfi].copy((*viscn_cc)[np1Mfi],0,0,1);
        }
    }

    if (variable_scal_diff)
    {
        const int num_diff = NUM_STATE-Density-1;

        calcDiffusivity(prev_time,dt,iteration,ncycle,Density+1,num_diff);

        for (MFIter np1Mfi(*diffnp1_cc); np1Mfi.isValid(); ++np1Mfi)
        {
            (*diffnp1_cc)[np1Mfi].copy((*diffn_cc)[np1Mfi],0,0,num_diff);
        }
    }
}

//
// Clean up after the advance function.
//

void
NavierStokes::advance_cleanup (Real dt,
                               int  iteration,
                               int  ncycle)
{
    if (level == parent->finestLevel())
    {
        delete [] u_mac;
        u_mac = 0;
    }

    delete aofs;
    aofs = 0;
}


//
// Compute a timestep at a level. Return largest safe timestep.
//

Real
NavierStokes::advance (Real time,
                       Real dt,
                       int  iteration,
                       int  ncycle)
{
    if (verbose && ParallelDescriptor::IOProcessor())
    {
        std::cout << "Advancing grids at level " << level
                  << " : starting time = "       << time
                  << " with dt = "               << dt << '\n';
    }
    advance_setup(time,dt,iteration,ncycle);
    //
    // Compute traced states for normal comp of velocity at half time level.
    //
    Real dummy   = 0.0;
    Real dt_test = predict_velocity(dt,dummy);
    //
    // Do MAC projection and update edge velocities.
    //
    if (do_mac_proj) 
    {
        MultiFab mac_rhs(grids,1,0);
        create_mac_rhs(mac_rhs,time,dt);
        MultiFab& S_old  = get_old_data(State_Type);
        mac_project(time,dt,S_old,&mac_rhs,have_divu);
    }
    //
    // Advect velocities.
    //
    if (do_mom_diff == 0) 
        velocity_advection(dt);
    //
    // Advect scalars.
    //
    const int first_scalar = Density;
    const int last_scalar  = first_scalar + NUM_SCALARS - 1;
    scalar_advection(dt,first_scalar,last_scalar);
    //
    // Update Rho.
    //
    scalar_update(dt,first_scalar,first_scalar);

    make_rho_curr_time();
    //
    // Advect momenta after rho^(n+1) has been created.
    //
    if (do_mom_diff == 1) 
        velocity_advection(dt);
    //
    // Add the advective and other terms to get scalars at t^{n+1}.
    //
#ifdef MOREGENGETFORCE
    if (do_scalar_update_in_order)
    {
	for (int iComp=0; iComp<NUM_SCALARS-1; iComp++)
        {
	    int iScal = first_scalar+scalarUpdateOrder[iComp];
	    if (ParallelDescriptor::IOProcessor())
		std::cout << "... ... updating " << desc_lst[0].name(iScal) << std::endl;
	    scalar_update(dt,iScal,iScal);
	}
    }
    else
    {
	scalar_update(dt,first_scalar+1,last_scalar);
    }
#else
    scalar_update(dt,first_scalar+1,last_scalar);
#endif
    //
    // S appears in rhs of the velocity update, so we better do it now.
    //
    if (have_divu)
    {
        calc_divu(time+dt,dt,get_new_data(Divu_Type));
        if (have_dsdt)
        {
            calc_dsdt(time,dt,get_new_data(Dsdt_Type));
            if (initial_step)
                MultiFab::Copy(get_old_data(Dsdt_Type),
                               get_new_data(Dsdt_Type),0,0,1,0);
        }
    }
    //
    // Add the advective and other terms to get velocity at t^{n+1}.
    //
    velocity_update(dt);
    //
    // Clean up after the predicted value at t^n+1.
    // Estimate new timestep from umac cfl.
    //
    advance_cleanup(dt,iteration,ncycle);
    //
    // Increment rho average.
    //
    if (!initial_step && level > 0)
        incrRhoAvg((iteration==ncycle ? 0.5 : 1.0) / Real(ncycle));
    //
    // Do a level project to update the pressure and velocity fields.
    //
    if (!initial_step)
    {
        if (projector)
            level_projector(dt,time,iteration);
        if (level > 0 && iteration == 1)
           p_avg->setVal(0);
    }
    return dt_test;  // Return estimate of best new timestep.
}

void
NavierStokes::create_mac_rhs (MultiFab& rhs, Real time, Real dt, int nGrow)
{
    BL_ASSERT(rhs.nGrow()>=nGrow);
    BL_ASSERT(rhs.boxArray()==grids);

    int sCompDivU = 0;
    int nCompDivU = 1;
    int sCompDsdt = 0;
    int nCompDsdt = 1;

    for (FillPatchIterator Divu_fpi(*this,rhs,nGrow,time,Divu_Type,sCompDivU,nCompDivU),
             Dsdt_fpi(*this,rhs,nGrow,time,Dsdt_Type,sCompDsdt,nCompDsdt);
         Divu_fpi.isValid() && Dsdt_fpi.isValid(); 
         ++Divu_fpi, ++Dsdt_fpi)
    {
        Dsdt_fpi().mult(.5*dt);
        rhs[Divu_fpi].copy(Divu_fpi(),sCompDivU,0,nCompDivU);
        rhs[Divu_fpi].plus(Dsdt_fpi(),sCompDsdt,0,nCompDsdt);
    }
}

void
NavierStokes::mac_project (Real      time,
                           Real      dt,
                           MultiFab& Sold, 
                           MultiFab* divu,
                           int       have_divu)
{
    if (verbose && ParallelDescriptor::IOProcessor())
        std::cout << "... mac_projection\n";

    const Real strt_time = ParallelDescriptor::second();

    mac_projector->mac_project(level,u_mac,Sold,dt,time,*divu,have_divu);

    create_umac_grown();

    if (verbose)
    {
        Real run_time    = ParallelDescriptor::second() - strt_time;
        const int IOProc = ParallelDescriptor::IOProcessorNumber();

        ParallelDescriptor::ReduceRealMax(run_time,IOProc);

        if (ParallelDescriptor::IOProcessor())
        {
            std::cout << "NavierStokes:mac_project(): lev: "
                      << level
                      << ", time: " << run_time << std::endl;
        }
    }
}

void
NavierStokes::level_projector (Real dt,
                               Real time,
                               int  iteration)
{
    BL_ASSERT(iteration > 0);

    MultiFab& U_old = get_old_data(State_Type);
    MultiFab& U_new = get_new_data(State_Type);
    MultiFab& P_old = get_old_data(Press_Type);
    MultiFab& P_new = get_new_data(Press_Type);

    SyncRegister* crse_ptr = 0;

    if (level < parent->finestLevel() && do_sync_proj)
    {
        crse_ptr = &(getLevel(level+1).getSyncReg());
    }

    Array<int*>         sync_bc(grids.size());
    Array< Array<int> > sync_bc_array(grids.size());

    for (int i = 0; i < grids.size(); i++)
    {
        sync_bc_array[i] = getBCArray(State_Type,i,Xvel,BL_SPACEDIM);
        sync_bc[i]       = sync_bc_array[i].dataPtr();
    }

    int        crse_dt_ratio  = (level > 0) ? parent->nCycle(level) : -1;
    const Real cur_pres_time  = state[Press_Type].curTime();
    const Real prev_pres_time = state[Press_Type].prevTime();

    projector->level_project(level,time,dt,cur_pres_time,prev_pres_time,
                             geom,U_old,U_new,P_old,P_new,
                             get_rho_half_time(),crse_ptr,sync_reg,
                             crse_dt_ratio,iteration,have_divu);

    if (state[Press_Type].descriptor()->timeType() == StateDescriptor::Point)
        calcDpdt();
}

void
NavierStokes::make_rho_prev_time ()
{
    const Real prev_time = state[State_Type].prevTime();

    FillPatchIterator fpi(*this,*rho_ptime,1,prev_time,State_Type,Density,1);

    for ( ; fpi.isValid(); ++fpi)
    {
        (*rho_ptime)[fpi.index()].copy(fpi());
    }
}

void
NavierStokes::make_rho_curr_time ()
{
    const Real curr_time = state[State_Type].curTime();

    FillPatchIterator fpi(*this,*rho_ctime,1,curr_time,State_Type,Density,1);

    for ( ; fpi.isValid(); ++fpi)
    {
        (*rho_ctime)[fpi.index()].copy(fpi());
    }
}

MultiFab*
NavierStokes::get_rho_half_time ()
{
    //
    // Fill it in when needed ...
    //
    for (MFIter mfi(*rho_half); mfi.isValid(); ++mfi)
    {
        (*rho_half)[mfi].copy((*rho_ptime)[mfi]);
        (*rho_half)[mfi] += (*rho_ctime)[mfi];
        (*rho_half)[mfi].mult(.5);
    }

    return rho_half;
}

const MultiFab&
NavierStokes::get_rho (Real time)
{
    const TimeLevel whichTime = which_time(State_Type,time);

    if (whichTime == AmrOldTime)
    {
        return *rho_ptime;
    }
    else if (whichTime == AmrNewTime)
    {
        return *rho_ctime;
    }
    else if (whichTime == Amr1QtrTime)
    {
        BL_ASSERT(rho_qtime);
        return *rho_qtime;
    }
    else if (whichTime == Amr3QtrTime)
    {
        BL_ASSERT(rho_tqtime);
        return *rho_tqtime;
    }
    else if (whichTime == AmrHalfTime)
    {
        return *get_rho_half_time();
    }
    else
    {
        BoxLib::Error("NavierStokes::get_rho(): bad time");

        return *rho_ptime; // Got to return something to shut up compiler.
    }
}

//
// Predict the edge velocities which go into forming u_mac.  This
// function also returns an estimate of dt for use in variable timesteping.
//

Real
NavierStokes::predict_velocity (Real  dt,
                                Real& comp_cfl)
{
    if (verbose && ParallelDescriptor::IOProcessor())
        std::cout << "... predict edge velocities\n";
    //
    // Get simulation parameters.
    //
    const int   nComp          = BL_SPACEDIM;
    const Real* dx             = geom.CellSize();
    const Real  prev_time      = state[State_Type].prevTime();
    const Real  prev_pres_time = state[Press_Type].prevTime();
    //
    // Compute viscous terms at level n.
    // Ensure reasonable values in 1 grow cell.  Here, do extrap for
    // c-f/phys boundary, since we have no interpolator fn, also,
    // preserve extrap for corners at periodic/non-periodic intersections.
    //
    MultiFab visc_terms(grids,nComp,1);

    if (be_cn_theta != 1.0)
    {
	getViscTerms(visc_terms,Xvel,nComp,prev_time);
    }
    else
    {
	visc_terms.setVal(0);
    }
    //
    // Set up the timestep estimation.
    //
    Real cflgrid,u_max[3];
    Real cflmax = 1.0e-10;
    comp_cfl    = (level == 0) ? cflmax : comp_cfl;

    FArrayBox tforces;

    Array<int> bndry[BL_SPACEDIM];

    MultiFab Gp(grids,BL_SPACEDIM,1);

    getGradP(Gp, prev_pres_time);
    
    FArrayBox* null_fab = 0;

    for (FillPatchIterator U_fpi(*this,visc_terms,hyp_grow,prev_time,State_Type,Xvel,BL_SPACEDIM)
#ifdef MOREGENGETFORCE
	     , S_fpi(*this,visc_terms,1,prev_time,State_Type,Density,NUM_SCALARS);
	 S_fpi.isValid() && U_fpi.isValid();
	 ++S_fpi, ++U_fpi
#else
             ; U_fpi.isValid();
	 ++U_fpi
#endif
	)
    {
        const int i = U_fpi.index();

#ifdef GENGETFORCE
        getForce(tforces,i,1,Xvel,BL_SPACEDIM,prev_time,(*rho_ptime)[i]);
#elif MOREGENGETFORCE
	if (ParallelDescriptor::IOProcessor() && getForceVerbose)
	    std::cout << "---" << std::endl << "A - Predict velocity:" << std::endl << " Calling getForce..." << std::endl;
        getForce(tforces,i,1,Xvel,BL_SPACEDIM,prev_time,U_fpi(),S_fpi(),0);
#else
	getForce(tforces,i,1,Xvel,BL_SPACEDIM,(*rho_ptime)[i]);
#endif		 
        //
        // Test velocities, rho and cfl.
        //
        cflgrid  = godunov->test_u_rho(U_fpi(),(*rho_ptime)[i],grids[i],dx,dt,u_max);
        cflmax   = std::max(cflgrid,cflmax);
        comp_cfl = std::max(cflgrid,comp_cfl);
        //
        // Compute the total forcing.
        //
        godunov->Sum_tf_gp_visc(tforces,0,visc_terms[i],0,Gp[i],0,(*rho_ptime)[i],0);

        D_TERM(bndry[0] = getBCArray(State_Type,i,0,1);,
               bndry[1] = getBCArray(State_Type,i,1,1);,
               bndry[2] = getBCArray(State_Type,i,2,1););


        int velpred = 1;
        godunov->Setup(grids[i], dx, dt, velpred,
                       D_DECL(*null_fab,*null_fab,*null_fab),
                       D_DECL(bndry[0].dataPtr(),bndry[1].dataPtr(),bndry[2].dataPtr()),
                       D_DECL(U_fpi(),U_fpi(),U_fpi()), D_DECL(0,1,2),
                       tforces, 0);

        godunov->ComputeUmac(grids[i], dx, dt, 
                             u_mac[0][i], bndry[0].dataPtr(),
                             u_mac[1][i], bndry[1].dataPtr(),
#if (BL_SPACEDIM == 3)
                             u_mac[2][i], bndry[2].dataPtr(),
#endif
                             U_fpi(), tforces);
    }

    Real tempdt = std::min(change_max,cfl/cflmax);

    ParallelDescriptor::ReduceRealMin(tempdt);

    return dt*tempdt;
}

//
// This routine advects the velocities
//
void
NavierStokes::velocity_advection (Real dt)
{
    if (verbose && ParallelDescriptor::IOProcessor())
    {
        if (do_mom_diff == 0) 
        {
            std::cout << "... advect velocities\n";
        }
        else
        {
            if (predict_mom_together == 0)
            {
                std::cout << "Must set predict_mom_together == 1 in NavierStokes." << std::endl;
                exit(0);
            }
            std::cout << "... advect momenta\n";
        }
    }

    const int   finest_level   = parent->finestLevel();
    const Real* dx             = geom.CellSize();
    const Real  prev_time      = state[State_Type].prevTime();
    const Real  prev_pres_time = state[Press_Type].prevTime();
    //
    // Compute viscosity components.
    //
    MultiFab visc_terms(grids,BL_SPACEDIM,1);

    if (be_cn_theta != 1.0)
        getViscTerms(visc_terms,Xvel,BL_SPACEDIM,prev_time);
    else
        visc_terms.setVal(0,1);

    Array<int> bndry[BL_SPACEDIM];

    FArrayBox flux[BL_SPACEDIM], tforces, S;

    int nGrow = 1;
    MultiFab divu_fp(grids,1,nGrow);
    create_mac_rhs(divu_fp,prev_time,dt,nGrow);

    MultiFab Gp(grids,BL_SPACEDIM,1);

    getGradP(Gp, prev_pres_time);

    FArrayBox area[BL_SPACEDIM], volume;

    MultiFab fluxes[BL_SPACEDIM];

    if (do_reflux && level < parent->finestLevel())
    {
        for (int i = 0; i < BL_SPACEDIM; i++)
        {
            BoxArray ba = grids;
            ba.surroundingNodes(i);
            fluxes[i].define(ba, BL_SPACEDIM, 0, Fab_allocate);
        }
    }
    //
    // Compute the advective forcing.
    //
    for (FillPatchIterator P_fpi(*this,get_old_data(Press_Type),1,prev_pres_time,Press_Type,0,1),
#ifdef MOREGENGETFORCE
             U_fpi(*this,visc_terms,hyp_grow,prev_time,State_Type,Xvel,BL_SPACEDIM),
             S_fpi(*this,visc_terms,1,prev_time,State_Type,Density,NUM_SCALARS),
             Rho_fpi(*this,visc_terms,hyp_grow,prev_time,State_Type,Density,1);
         S_fpi.isValid() && U_fpi.isValid() && P_fpi.isValid() && Rho_fpi.isValid(); 
         ++S_fpi, ++U_fpi, ++P_fpi, ++Rho_fpi
#else
             U_fpi(*this,visc_terms,hyp_grow,prev_time,State_Type,Xvel,BL_SPACEDIM),
             Rho_fpi(*this,visc_terms,hyp_grow,prev_time,State_Type,Density,1);
         U_fpi.isValid() && P_fpi.isValid() && Rho_fpi.isValid(); 
         ++U_fpi, ++P_fpi, ++Rho_fpi
#endif
	)
    {
        const int i = U_fpi.index();

#ifdef GENGETFORCE
        getForce(tforces,i,1,Xvel,BL_SPACEDIM,prev_time,(*rho_ptime)[i]);
#elif MOREGENGETFORCE
	if (ParallelDescriptor::IOProcessor() && getForceVerbose)
	    std::cout << "---" << std::endl << "B - velocity advection:" << std::endl << "Calling getForce..." << std::endl;
        getForce(tforces,i,1,Xvel,BL_SPACEDIM,prev_time,U_fpi(),S_fpi(),0);
#else
        getForce(tforces,i,1,Xvel,BL_SPACEDIM,(*rho_ptime)[i]);
#endif		 

        godunov->Sum_tf_gp_visc(tforces,0,visc_terms[i],0,Gp[i],0,(*rho_ptime)[i],0);

        D_TERM(bndry[0] = getBCArray(State_Type,i,0,1);,
               bndry[1] = getBCArray(State_Type,i,1,1);,
               bndry[2] = getBCArray(State_Type,i,2,1););

        int velpred = 0;
        godunov->Setup (grids[i], dx, dt, velpred,
                        D_DECL(flux[0],flux[1],flux[2]), 
                        D_DECL(bndry[0].dataPtr(),bndry[1].dataPtr(),bndry[2].dataPtr()),
                        D_DECL(U_fpi(),U_fpi(),U_fpi()), 
                        D_DECL(0,1,2),
                        tforces,0);
        //
        // Loop over the velocity components.
        //
        S.resize(U_fpi().box(),BL_SPACEDIM);
        S.copy(U_fpi(),0,0,BL_SPACEDIM);

        for (int dir = 0; dir < BL_SPACEDIM; dir++)
        {
            geom.GetFaceArea(area[dir],grids,i,dir,GEOM_GROW);
        }
        geom.GetVolume(volume,grids,i,GEOM_GROW);

        for (int comp = 0 ; comp < BL_SPACEDIM ; comp++ )
        {
            int use_conserv_diff = (advectionType[comp] == Conservative) ? true : false;

            if (do_mom_diff == 1)
            {
                S.mult(Rho_fpi(),S.box(),S.box(),0,comp,1);
                tforces.mult((*rho_ptime)[i],tforces.box(),tforces.box(),0,comp,1);
            }

            godunov->AdvectState(grids[i], dx, dt, 
                                 area[0], u_mac[0][i], flux[0],
                                 area[1], u_mac[1][i], flux[1],
#if (BL_SPACEDIM == 3)                       
                                 area[2], u_mac[2][i], flux[2],
#endif
                                 U_fpi(), S, tforces, divu_fp[i], comp,
                                 (*aofs)[i],comp,use_conserv_diff,
                                 comp,bndry[comp].dataPtr(),PRE_MAC,volume);
            if (do_reflux)
            {
                if (level < parent->finestLevel())
                {
                    for (int d = 0; d < BL_SPACEDIM; d++)
                        fluxes[d][i].copy(flux[d],0,comp,1);
                }
                if (level > 0)
                {
                    for (int d = 0; d < BL_SPACEDIM; d++)
                        advflux_reg->FineAdd(flux[d],d,i,0,comp,1,dt);
                }
            }

        }
    }

    if (do_reflux && level < finest_level)
    {
        for (int i = 0; i < BL_SPACEDIM; i++)
            getAdvFluxReg(level+1).CrseInit(fluxes[i],i,0,0,BL_SPACEDIM,-dt);
    }
}

//
// This routine advects the scalars
//

void
NavierStokes::scalar_advection (Real dt,
                                int  fscalar,
                                int  lscalar)
{
    if (verbose && ParallelDescriptor::IOProcessor())
        std::cout << "... advect scalars\n";
    //
    // Get simulation parameters.
    //
    const int   num_scalars    = lscalar - fscalar + 1;
    const Real* dx             = geom.CellSize();
    const Real  prev_time      = state[State_Type].prevTime();
    const Real  prev_pres_time = state[Press_Type].prevTime();
    //
    // Get the viscous terms.
    //
    MultiFab visc_terms(grids,num_scalars,1);

    if (be_cn_theta != 1.0)
        getViscTerms(visc_terms,fscalar,num_scalars,prev_time);
    else
        visc_terms.setVal(0,1);
    //
    // Set up the grid loop.
    //
    FArrayBox flux[BL_SPACEDIM], tforces, tvelforces, area[BL_SPACEDIM], volume;

    MultiFab Gp, vel_visc_terms, fluxes[BL_SPACEDIM];

    MultiFab* divu_fp = getDivCond(1,prev_time);
    MultiFab* dsdt    = getDsdt(1,prev_time);
    for (MFIter dsdtmfi(*dsdt); dsdtmfi.isValid(); ++dsdtmfi)
    {
       (*dsdt)[dsdtmfi].mult(.5*dt);
       (*divu_fp)[dsdtmfi].plus((*dsdt)[dsdtmfi]);
    }
    delete dsdt;

    if (do_reflux && level < parent->finestLevel())
    {
        for (int i = 0; i < BL_SPACEDIM; i++)
        {
            BoxArray ba = grids;
            ba.surroundingNodes(i);
            fluxes[i].define(ba, num_scalars, 0, Fab_allocate);
        }
    }

    const int use_forces_in_trans = godunov->useForcesInTrans();

    if (use_forces_in_trans)
    {
        Gp.define(grids,BL_SPACEDIM,1,Fab_allocate);

        vel_visc_terms.define(grids,BL_SPACEDIM,1,Fab_allocate);

        getGradP(Gp, prev_pres_time);

        if (be_cn_theta != 1.0)
            getViscTerms(vel_visc_terms,Xvel,BL_SPACEDIM,prev_time);
        else
            vel_visc_terms.setVal(0,1);
    }
    Array<int> state_bc, bndry[BL_SPACEDIM];
    //
    // Compute the advective forcing.
    //
    for (FillPatchIterator P_fpi(*this,get_old_data(Press_Type),1,prev_pres_time,Press_Type,0,1),
             U_fpi(*this,visc_terms,hyp_grow,prev_time,State_Type,Xvel,BL_SPACEDIM),
             S_fpi(*this,visc_terms,hyp_grow,prev_time,State_Type,fscalar,num_scalars);
         U_fpi.isValid() && S_fpi.isValid() && P_fpi.isValid();
         ++U_fpi, ++S_fpi, ++P_fpi)
    {
        const int i = U_fpi.index();

#ifdef GENGETFORCE
        getForce(tforces,i,1,fscalar,num_scalars,prev_time,(*rho_ptime)[i]);
#elif MOREGENGETFORCE
	if (ParallelDescriptor::IOProcessor() && getForceVerbose)
	    std::cout << "---" << std::endl << "C - scalar advection:" << std::endl << " Calling getForce..." << std::endl;
        getForce(tforces,i,1,fscalar,num_scalars,prev_time,U_fpi(),S_fpi(),0);
#else
        getForce(tforces,i,1,fscalar,num_scalars,(*rho_ptime)[i]);
#endif		 
        
        if (use_forces_in_trans)
        {
#ifdef GENGETFORCE
            getForce(tvelforces,i,1,Xvel,BL_SPACEDIM,prev_time,(*rho_ptime)[i]);
#elif MOREGENGETFORCE
	    if (ParallelDescriptor::IOProcessor() && getForceVerbose)
		std::cout << "---" << std::endl << "D - scalar advection (use_forces_in_trans):" << std::endl << " Calling getForce..." << std::endl;
            getForce(tvelforces,i,1,Xvel,BL_SPACEDIM,prev_time,U_fpi(),S_fpi(),0);
#else
            getForce(tvelforces,i,1,Xvel,BL_SPACEDIM,(*rho_ptime)[i]);
#endif		 
            godunov->Sum_tf_gp_visc(tvelforces,0,vel_visc_terms[i],0,Gp[i],0,(*rho_ptime)[i],0);
        }

        D_TERM(bndry[0] = getBCArray(State_Type,i,0,1);,
               bndry[1] = getBCArray(State_Type,i,1,1);,
               bndry[2] = getBCArray(State_Type,i,2,1););

        int velpred = 0;
        godunov->Setup(grids[i], dx, dt, velpred,
                       D_DECL(flux[0],flux[1],flux[2]),
                       D_DECL(bndry[0].dataPtr(),bndry[1].dataPtr(),bndry[2].dataPtr()),
                       D_DECL(U_fpi(),U_fpi(),U_fpi()),
                       D_DECL(0,1,2),
                       tvelforces,0);

        for (int dir = 0; dir < BL_SPACEDIM; dir++)
        {
            geom.GetFaceArea(area[dir],grids,i,dir,GEOM_GROW);
        }
        geom.GetVolume(volume,grids,i,GEOM_GROW);
        //
        // Loop over the scalar components.
        //
        for (int comp = 0 ; comp < num_scalars ; comp++)
        {
            int state_ind = fscalar + comp;
            //
            // Compute total forcing.
            //
            int use_conserv_diff = (advectionType[state_ind] == Conservative) ? true : false;

            AdvectionScheme adv_scheme = PRE_MAC;

            if (adv_scheme == PRE_MAC)
            {
                godunov->Sum_tf_divu_visc(S_fpi(),state_ind,tforces,comp,1,visc_terms[i],comp,
                                          (*divu_fp)[i],0,(*rho_ptime)[i],0,use_conserv_diff);
            }
            else
            {
                FArrayBox junkDivu(tforces.box(),1);
                junkDivu.setVal(0.);
                godunov->Sum_tf_divu_visc(S_fpi(),state_ind,tforces,comp,1,visc_terms[i],comp,
                                          junkDivu,0,(*rho_ptime)[i],0,use_conserv_diff);
            }
            //
            // Advect scalar.
            //
            state_bc = getBCArray(State_Type,i,state_ind,1);

            godunov->AdvectState(grids[i], dx, dt, 
                                 area[0], u_mac[0][i], flux[0],
                                 area[1], u_mac[1][i], flux[1],
#if (BL_SPACEDIM == 3)                        
                                 area[2], u_mac[2][i], flux[2],
#endif
                                 U_fpi(),S_fpi(),tforces,(*divu_fp)[i],comp,
                                 (*aofs)[i],state_ind,use_conserv_diff,
                                 state_ind,state_bc.dataPtr(),adv_scheme,volume);
            if (do_reflux)
            {
                if (level < parent->finestLevel())
                {
                    for (int d = 0; d < BL_SPACEDIM; d++)
                        fluxes[d][i].copy(flux[d],0,comp,1);
                }
                if (level > 0)
                {
                    for (int d = 0; d < BL_SPACEDIM; d++)
                        advflux_reg->FineAdd(flux[d],d,i,0,state_ind,1,dt);
                }
            }
        }
    }

    delete divu_fp;

    if (do_reflux && level < parent->finestLevel())
    {
        for (int i = 0; i < BL_SPACEDIM; i++)
            getAdvFluxReg(level+1).CrseInit(fluxes[i],i,0,fscalar,num_scalars,-dt);
    }
}

//
// This subroutine updates the scalars, before the velocity update
// and the level projection
//
// AT this point in time, all we know is psi^n, rho^n+1/2, and the
// general forcing terms at t^n, and after solving in this routine
// viscous forcing at t^n+1/2.  Note, unless more complicated logic
// is invoked earlier, we do not have any estimate of general forcing
// terms at t^n+1/2.
//

void
NavierStokes::scalar_update (Real dt,
                             int  first_scalar,
                             int  last_scalar)
{
    if (verbose && ParallelDescriptor::IOProcessor())
        std::cout << "... update scalars\n";

    scalar_advection_update(dt, first_scalar, last_scalar);

    bool do_any_diffuse = false;
    for (int sigma = first_scalar; sigma <= last_scalar; sigma++)
        if (is_diffusive[sigma]) do_any_diffuse = true;

    if (do_any_diffuse)
      scalar_diffusion_update(dt, first_scalar, last_scalar);
}

void
NavierStokes::scalar_advection_update (Real dt,
                                       int  first_scalar,
                                       int  last_scalar)
{
    MultiFab&  S_old     = get_old_data(State_Type);
    MultiFab&  S_new     = get_new_data(State_Type);
    MultiFab&  Aofs      = *aofs;
#if defined(GENGETFORCE) || defined(MOREGENGETFORCE)
    const Real halftime  = 0.5*(state[State_Type].curTime()+state[State_Type].prevTime());
#endif
    const Real prev_time = state[State_Type].prevTime();
    Array<int> state_bc;
    FArrayBox  tforces;
    //
    // Compute inviscid estimate of scalars.
    // (do rho separate, as we do not have rho at new time yet)
    //
    int sComp = first_scalar;

    if (sComp == Density)
    {
        for (MFIter S_oldmfi(S_old); S_oldmfi.isValid(); ++S_oldmfi)
        {
            const int i = S_oldmfi.index();
            tforces.resize(grids[i],1);
            tforces.setVal(0);
            godunov->Add_aofs_tf(S_old[S_oldmfi],S_new[S_oldmfi],Density,1,
                                 Aofs[S_oldmfi],Density,tforces,0,grids[i],dt);
        }
        //
        // Call ScalMinMax to avoid overshoots in density.
        //
        if (do_denminmax)
        {
            //
            // Must do FillPatch here instead of MF iterator because we need the
            // boundary values in the old data (especially at inflow)
            //
            const int index_new_s   = Density;
            const int index_new_rho = Density;
            const int index_old_s   = index_new_s   - Density;
            const int index_old_rho = index_new_rho - Density;

            for (FillPatchIterator S_fpi(*this,S_old,1,prev_time,State_Type,Density,1);
                 S_fpi.isValid();
                 ++S_fpi)
            {
                const int i = S_fpi.index();
                state_bc = getBCArray(State_Type,i,Density,1);
                godunov->ConservativeScalMinMax(S_fpi(),S_new[S_fpi],
                                                index_old_s, index_old_rho,
                                                index_new_s, index_new_rho,
                                                state_bc.dataPtr(),grids[i]);
            }
        }
        ++sComp;
    }

    if (sComp <= last_scalar)
    {
        const MultiFab& rho_halftime = *get_rho_half_time();

        for (MFIter Rho_mfi(rho_halftime); Rho_mfi.isValid(); ++Rho_mfi)
        {
            const int i = Rho_mfi.index();
            for (int sigma = sComp; sigma <= last_scalar; sigma++)
            {
#ifdef GENGETFORCE
                getForce(tforces,i,0,sigma,1,halftime,rho_halftime[Rho_mfi]);
#elif MOREGENGETFORCE
		// Need to do some funky half-time stuff
		if (ParallelDescriptor::IOProcessor() && getForceVerbose)
		    std::cout << "---" << std::endl << "E - scalar advection update (half time):" << std::endl;

		// Average the mac face velocities to get cell centred velocities
		FArrayBox Vel(BoxLib::grow(grids[i],0),BL_SPACEDIM);
		const int* vel_lo  = Vel.loVect();
		const int* vel_hi  = Vel.hiVect();
		const int* umacx_lo = u_mac[0][i].loVect();
		const int* umacx_hi = u_mac[0][i].hiVect();
		const int* umacy_lo = u_mac[1][i].loVect();
		const int* umacy_hi = u_mac[1][i].hiVect();
#if (BL_SPACEDIM==3)
		const int* umacz_lo = u_mac[2][i].loVect();
		const int* umacz_hi = u_mac[2][i].hiVect();
#endif
		FORT_AVERAGE_EDGE_STATES(Vel.dataPtr(),
					 u_mac[0][i].dataPtr(),
					 u_mac[1][i].dataPtr(),
#if (BL_SPACEDIM==3)
					 u_mac[2][i].dataPtr(),
#endif
					 ARLIM(vel_lo),  ARLIM(vel_hi),
					 ARLIM(umacx_lo), ARLIM(umacx_hi),
					 ARLIM(umacy_lo), ARLIM(umacy_hi),
#if (BL_SPACEDIM==3)

					 ARLIM(umacz_lo), ARLIM(umacz_hi),
#endif
					 &getForceVerbose);
		//
		// Average the new and old time to get Crank-Nicholson half time approximation.
                //
		FArrayBox Scal(BoxLib::grow(grids[i],0),NUM_SCALARS);
		Scal.copy(S_old[i],Density,0,NUM_SCALARS);
		Scal.plus(S_new[i],Density,0,NUM_SCALARS);
		Scal.mult(0.5);
		
		if (ParallelDescriptor::IOProcessor() && getForceVerbose)
		    std::cout << "Calling getForce..." << std::endl;
                getForce(tforces,i,0,sigma,1,halftime,Vel,Scal,0);
#else
                getForce(tforces,i,0,sigma,1,rho_halftime[Rho_mfi]);
#endif		 
                godunov->Add_aofs_tf(S_old[Rho_mfi],S_new[Rho_mfi],sigma,1,
                                     Aofs[Rho_mfi],sigma,tforces,0,grids[i],dt);
            }
        }
    }
    //
    // Call ScalMinMax to avoid overshoots in the scalars.
    //
    if ( do_scalminmax && (sComp <= last_scalar) )
    {
        const int num_scalars = last_scalar - Density + 1;
        //
        // Must do FillPatch here instead of MF iterator because we need the
        // boundary values in the old data (especially at inflow).
        //
        for (FillPatchIterator S_fpi(*this,S_old,1,prev_time,State_Type,Density,num_scalars);
             S_fpi.isValid();
             ++S_fpi)
        {
            const int i = S_fpi.index();
            for (int sigma = sComp; sigma <= last_scalar; sigma++)
            {
                const int index_new_s   = sigma;
                const int index_new_rho = Density;
                const int index_old_s   = index_new_s   - Density;
                const int index_old_rho = index_new_rho - Density;
                state_bc = getBCArray(State_Type,i,sigma,1);
                if (advectionType[sigma] == Conservative)
                {
                    godunov->ConservativeScalMinMax(S_fpi(),S_new[S_fpi],
                                                    index_old_s, index_old_rho,
                                                    index_new_s, index_new_rho,
                                                    state_bc.dataPtr(),grids[i]);
                }
                else if (advectionType[sigma] == NonConservative)
                {
                    godunov->ConvectiveScalMinMax(S_fpi(),S_new[S_fpi],index_old_s,sigma,
                                                  state_bc.dataPtr(),grids[i]);
                }
            }
        }
    }
}

void
NavierStokes::scalar_diffusion_update (Real dt,
                                       int  first_scalar,
                                       int  last_scalar)
{
    MultiFab** fluxSCn;
    MultiFab** fluxSCnp1;

    diffusion->allocFluxBoxesLevel(fluxSCn,  0,1);
    diffusion->allocFluxBoxesLevel(fluxSCnp1,0,1);

    const MultiFab* Rh = get_rho_half_time();

    for (int sigma = first_scalar; sigma <= last_scalar; sigma++)
    {
        if (is_diffusive[sigma])
        {
            int        rho_flag    = 0;
            MultiFab*  delta_rhs   = 0;
            MultiFab*  alpha       = 0;
            MultiFab** cmp_diffn   = 0;
            MultiFab** cmp_diffnp1 = 0;

            if (variable_scal_diff)
            {
                Real diffTime = state[State_Type].prevTime();
                diffusion->allocFluxBoxesLevel(cmp_diffn, 0, 1);
                getDiffusivity(cmp_diffn, diffTime, sigma, 0, 1);

                diffTime = state[State_Type].curTime();
                diffusion->allocFluxBoxesLevel(cmp_diffnp1, 0, 1);
                getDiffusivity(cmp_diffnp1, diffTime, sigma, 0, 1);
            }

            diffuse_scalar_setup(dt, sigma, &rho_flag, 
                                 delta_rhs, alpha, cmp_diffn, cmp_diffnp1);

            diffusion->diffuse_scalar(dt,sigma,be_cn_theta,Rh,
                                      rho_flag,fluxSCn,fluxSCnp1,0,delta_rhs,
                                      alpha,cmp_diffn,cmp_diffnp1);
            if (variable_scal_diff)
            {
                diffusion->removeFluxBoxesLevel(cmp_diffn);
                diffusion->removeFluxBoxesLevel(cmp_diffnp1);
            }

            delete delta_rhs;
            delete alpha;
            //
            // Increment the viscous flux registers
            //
            if (do_reflux)
            {
                FArrayBox fluxtot;

                for (int d = 0; d < BL_SPACEDIM; d++)
                {
                    MultiFab fluxes;

                    if (level < parent->finestLevel())
                        fluxes.define(fluxSCn[d]->boxArray(), 1, 0, Fab_allocate);

                    for (MFIter fmfi(*fluxSCn[d]); fmfi.isValid(); ++fmfi)
                    {
                        const Box& ebox = (*fluxSCn[d])[fmfi].box();

                        fluxtot.resize(ebox,1);
                        fluxtot.copy((*fluxSCn[d])[fmfi],ebox,0,ebox,0,1);
                        fluxtot.plus((*fluxSCnp1[d])[fmfi],ebox,0,0,1);

                        if (level < parent->finestLevel())
                            fluxes[fmfi].copy(fluxtot);

                        if (level > 0)
                            getViscFluxReg().FineAdd(fluxtot,d,fmfi.index(),0,sigma,1,dt);
                    }

                    if (level < parent->finestLevel())
                        getLevel(level+1).getViscFluxReg().CrseInit(fluxes,d,0,sigma,1,-dt);
                }
            }
        }
    }
    diffusion->removeFluxBoxesLevel(fluxSCn);
    diffusion->removeFluxBoxesLevel(fluxSCnp1);
}

void
NavierStokes::diffuse_scalar_setup (Real        dt,
                                    int         sigma,
                                    int*        rho_flag,
                                    MultiFab*&  delta_rhs,
                                    MultiFab*&  alpha,
                                    MultiFab**& diffn,
                                    MultiFab**& diffnp1)
{
    (*rho_flag) = Diffusion::set_rho_flag(diffusionType[sigma]);
}

//
// This subroutine updates the velocity field before the level projection.
//
// At this point in time, all we know is u^n, rho^n+1/2, and the
// general forcing terms at t^n, and after solving in this routine
// viscous forcing at t^n+1/2.  Except for a simple buoyancy term,
// b = -rho^n+1/2 g, it is usually not possible to estimate more
// general forcing terms at t^n+1/2.  Since the default getForce, handles
// this case automatically, F_new and F_old have been replaced by a single
// tforces FArrayBox.
//
// We assume that if one component of velocity is viscous that all must be.
//

void
NavierStokes::velocity_update (Real dt)
{
    if (verbose && ParallelDescriptor::IOProcessor())
    {
        if (do_mom_diff == 0) 
        {
            std::cout << "... update velocities \n";
        }
        else
        {
            std::cout << "... update momenta \n";
        }
    }

    velocity_advection_update(dt);

    if (!initial_iter)
        velocity_diffusion_update(dt);
    else
        initial_velocity_diffusion_update(dt);
}

void
NavierStokes::velocity_advection_update (Real dt)
{
    FArrayBox  tforces, S;
    MultiFab&  U_old          = get_old_data(State_Type);
    MultiFab&  U_new          = get_new_data(State_Type);
    MultiFab&  P_old          = get_old_data(Press_Type);
    MultiFab&  Aofs           = *aofs;
    const Real prev_pres_time = state[Press_Type].prevTime();
#if defined(GENGETFORCE) || defined(MOREGENGETFORCE)
    const Real half_time      = 0.5*(state[State_Type].prevTime()+state[State_Type].curTime());
#endif

    MultiFab Gp(grids,BL_SPACEDIM,1);
    getGradP(Gp, prev_pres_time);

    MultiFab& halftime = *get_rho_half_time();
    MFIter    Rhohalf_mfi(halftime);

    for (FillPatchIterator P_fpi(*this,P_old,0,prev_pres_time,Press_Type,0,1);
         Rhohalf_mfi.isValid() && P_fpi.isValid();
         ++Rhohalf_mfi, ++P_fpi)
    {
        const int i = Rhohalf_mfi.index();

#ifdef GENGETFORCE
	getForce(tforces,i,0,Xvel,BL_SPACEDIM,half_time,halftime[i]);
#elif MOREGENGETFORCE
	// Need to do some funky half-time stuff
	if (ParallelDescriptor::IOProcessor() && getForceVerbose)
	    std::cout << "---" << std::endl << "F - velocity advection update (half time):" << std::endl;

	// Average the mac face velocities to get cell centred velocities
	FArrayBox Vel(BoxLib::grow(grids[i],0),BL_SPACEDIM);
	const int* vel_lo  = Vel.loVect();
	const int* vel_hi  = Vel.hiVect();
	const int* umacx_lo = u_mac[0][i].loVect();
	const int* umacx_hi = u_mac[0][i].hiVect();
	const int* umacy_lo = u_mac[1][i].loVect();
	const int* umacy_hi = u_mac[1][i].hiVect();
#if (BL_SPACEDIM==3)
	const int* umacz_lo = u_mac[2][i].loVect();
	const int* umacz_hi = u_mac[2][i].hiVect();
#endif
	FORT_AVERAGE_EDGE_STATES(Vel.dataPtr(),
				 u_mac[0][i].dataPtr(),
				 u_mac[1][i].dataPtr(),
#if (BL_SPACEDIM==3)
				 u_mac[2][i].dataPtr(),
#endif
				 ARLIM(vel_lo),  ARLIM(vel_hi),
				 ARLIM(umacx_lo), ARLIM(umacx_hi),
				 ARLIM(umacy_lo), ARLIM(umacy_hi),
#if (BL_SPACEDIM==3)
				 ARLIM(umacz_lo), ARLIM(umacz_hi),
#endif
				 &getForceVerbose);
	
	// Average the new and old time to get Crank-Nicholson half time approximation
	FArrayBox Scal(BoxLib::grow(grids[i],0),NUM_SCALARS);
	Scal.copy(U_old[i],Density,0,NUM_SCALARS);
	Scal.plus(U_new[i],Density,0,NUM_SCALARS);
	Scal.mult(0.5);
	
	if (ParallelDescriptor::IOProcessor() && getForceVerbose)
	    std::cout << "Calling getForce..." << std::endl;
	getForce(tforces,i,0,Xvel,BL_SPACEDIM,half_time,Vel,Scal,0);
#else
	getForce(tforces,i,0,Xvel,BL_SPACEDIM,halftime[i]);
#endif		 
        //
        // Do following only at initial iteration--per JBB.
        //
        if (initial_iter && is_diffusive[Xvel])
            tforces.setVal(0);

        S.resize(U_old[i].box(),BL_SPACEDIM);
        S.copy(U_old[i],0,0,BL_SPACEDIM);

        if (do_mom_diff == 1)
        {
            for (int d = 0; d < BL_SPACEDIM; d++)
            {
                Gp[i].mult(halftime[i],grids[i],grids[i],0,d,1);
                tforces.mult(halftime[i],grids[i],grids[i],0,d,1);
                S.mult((*rho_ptime)[i],grids[i],grids[i],0,d,1);
            }
        }

        godunov->Add_aofs_tf_gp(S,U_new[i],Aofs[i],tforces,
                                Gp[i],halftime[i],grids[i],dt);
        if (do_mom_diff == 1)
        {
            for (int d = 0; d < BL_SPACEDIM; d++)
                U_new[i].divide((*rho_ctime)[i],grids[i],grids[i],0,d,1);
        }
    }
}

void
NavierStokes::velocity_diffusion_update (Real dt)
{
    //
    // Compute the viscous forcing.
    // Do following except at initial iteration.
    //
    if (is_diffusive[Xvel])
    {
        int rho_flag = (do_mom_diff == 0) ? 1 : 3;

        MultiFab* delta_rhs = 0;
        if (S_in_vel_diffusion && have_divu)
        {
            delta_rhs = new MultiFab(grids,BL_SPACEDIM,0);
            delta_rhs->setVal(0);
        }

        MultiFab** loc_viscn   = 0;
        MultiFab** loc_viscnp1 = 0;

        if (variable_vel_visc)
        {
            Real viscTime = state[State_Type].prevTime();
            diffusion->allocFluxBoxesLevel(loc_viscn, 0, 1);
            getViscosity(loc_viscn, viscTime);

            viscTime = state[State_Type].curTime();
            diffusion->allocFluxBoxesLevel(loc_viscnp1, 0, 1);
            getViscosity(loc_viscnp1, viscTime);
        }

        diffuse_velocity_setup(dt, delta_rhs, loc_viscn, loc_viscnp1);

        diffusion->diffuse_velocity(dt,be_cn_theta,get_rho_half_time(),rho_flag,
                                    delta_rhs,loc_viscn,loc_viscnp1);

        if (variable_vel_visc)
        {
            diffusion->removeFluxBoxesLevel(loc_viscn);
            diffusion->removeFluxBoxesLevel(loc_viscnp1);
        }

        delete delta_rhs;
    }
}

void
NavierStokes::diffuse_velocity_setup (Real       dt,
                                      MultiFab*& delta_rhs,
                                      MultiFab**& viscn,
                                      MultiFab**& viscnp1)
{
    if (S_in_vel_diffusion && have_divu)
    {
        //
        // Include div mu S*I terms in rhs
        //  (i.e. make nonzero delta_rhs to add into RHS):
        //
        // The scalar and tensor solvers incorporate the relevant pieces of
        //  of Div(tau), provided the flow is divergence-free.  However, if
        //  Div(U) =/= 0, there is an additional piece not accounted for,
        //  which is of the form A.Div(U).  For constant viscosity, Div(tau)_i
        //  = Lapacian(U_i) + mu/3 d[Div(U)]/dx_i.  For mu not constant,
        //  Div(tau)_i = d[ mu(du_i/dx_j + du_j/dx_i) ]/dx_i - 2mu/3 d[Div(U)]/dx_i
        //
        // As a convenience, we treat this additional term as a "source" in
        // the diffusive solve, computing Div(U) in the "normal" way we
        // always do--via a call to calc_divu.  This routine computes delta_rhs
        // if necessary, and stores it as an auxilliary rhs to the viscous solves.
        // This is a little strange, but probably not bad.
        //
        const Real time = state[State_Type].prevTime();

        MultiFab divmusi(grids,BL_SPACEDIM,0);

        if (!variable_vel_visc)
        {
            diffusion->compute_divmusi(time,visc_coef[Xvel],divmusi);
            divmusi.mult((1./3.)*(1.0-be_cn_theta),0,BL_SPACEDIM,0);
            (*delta_rhs).plus(divmusi,0,BL_SPACEDIM,0);

            diffusion->compute_divmusi(time+dt,visc_coef[Xvel],divmusi);
            divmusi.mult((1./3.)*be_cn_theta,0,BL_SPACEDIM,0);
            (*delta_rhs).plus(divmusi,0,BL_SPACEDIM,0);
        }
        else
        {
            diffusion->compute_divmusi(time,viscn,divmusi);
            divmusi.mult((-2./3.)*(1.0-be_cn_theta),0,BL_SPACEDIM,0);
            (*delta_rhs).plus(divmusi,0,BL_SPACEDIM,0);

            diffusion->compute_divmusi(time+dt,viscnp1,divmusi);
            divmusi.mult((-2./3.)*be_cn_theta,0,BL_SPACEDIM,0);
            (*delta_rhs).plus(divmusi,0,BL_SPACEDIM,0);
        }
    }
}

void
NavierStokes::initial_velocity_diffusion_update (Real dt)
{
    //
    // Do following only at initial iteration.
    //
    if (is_diffusive[Xvel])
    {
        MultiFab&  U_old          = get_old_data(State_Type);
        MultiFab&  U_new          = get_new_data(State_Type);
        MultiFab&  Aofs           = *aofs;
        const int  nComp          = BL_SPACEDIM;
        const Real prev_time      = state[State_Type].prevTime();
        const Real prev_pres_time = state[Press_Type].prevTime();

        MultiFab Gp(grids,BL_SPACEDIM,1);
        getGradP(Gp, prev_pres_time);

	MultiFab visc_terms(grids,nComp,1);

	if (be_cn_theta != 1.0)
        {
	    getViscTerms(visc_terms,Xvel,nComp,prev_time);
        }
        else
	{
	    visc_terms.setVal(0);
	}

        FArrayBox tforces, S;
        //
        // Update U_new with viscosity.
        //
        MultiFab* Rh = get_rho_half_time();

        for (FillPatchIterator P_fpi(*this,get_old_data(Press_Type),0,prev_pres_time,Press_Type,0,1);
             P_fpi.isValid();
             ++P_fpi)
        {
            const int i = P_fpi.index();

#ifdef GENGETFORCE
            getForce(tforces,i,0,Xvel,BL_SPACEDIM,prev_time,(*rho_ptime)[i]);
#elif MOREGENGETFORCE
	    if (ParallelDescriptor::IOProcessor() && getForceVerbose)
		std::cout << "---" << std::endl << "G - initial velocity diffusion update:" << std::endl << "Calling getForce..." << std::endl;
            getForce(tforces,i,0,Xvel,BL_SPACEDIM,prev_time,U_old[i],U_old[i],Density);
#else
            getForce(tforces,i,0,Xvel,BL_SPACEDIM,(*rho_ptime)[i]);
#endif		 
            godunov->Sum_tf_gp_visc(tforces,0,visc_terms[i],0,Gp[i],0,(*Rh)[i],0);

            S.resize(U_old[i].box(),BL_SPACEDIM);
            S.copy(U_old[i],0,0,BL_SPACEDIM);

            if (do_mom_diff == 1)
            {
                for (int d = 0; d < BL_SPACEDIM; d++)
                {
                    tforces.mult((*Rh)[i],grids[i],grids[i],0,d,1);
                    S.mult((*rho_ptime)[i],grids[i],grids[i],0,d,1);
                }
            }

            godunov->Add_aofs_tf(S,U_new[i],0,BL_SPACEDIM,Aofs[i],
                                 0,tforces,0,grids[i],dt);

            if (do_mom_diff == 1)
            {
                for (int d = 0; d < BL_SPACEDIM; d++)
                    U_new[i].divide((*rho_ctime)[i],grids[i],grids[i],0,d,1);
            }
        }
    }
}

void
NavierStokes::errorEst (TagBoxArray& tags,
                        int          clearval,
                        int          tagval,
                        Real         time,
                        int          n_error_buf, 
                        int          ngrow)
{
    const int*  domain_lo = geom.Domain().loVect();
    const int*  domain_hi = geom.Domain().hiVect();
    const Real* dx        = geom.CellSize();
    const Real* prob_lo   = geom.ProbLo();
    Array<int>  itags;

    for (int j = 0; j < err_list.size(); j++)
    {
        MultiFab* mf = derive(err_list[j].name(), time, err_list[j].nGrow());

        for (MFIter mfi(*mf); mfi.isValid(); ++mfi)
        {
            RealBox     gridloc = RealBox(grids[mfi.index()],geom.CellSize(),geom.ProbLo());
            itags               = tags[mfi.index()].tags();
            int*        tptr    = itags.dataPtr();
            const int*  tlo     = tags[mfi.index()].box().loVect();
            const int*  thi     = tags[mfi.index()].box().hiVect();
            const int*  lo      = mfi.validbox().loVect();
            const int*  hi      = mfi.validbox().hiVect();
            const Real* xlo     = gridloc.lo();
            Real*       dat     = (*mf)[mfi].dataPtr();
            const int*  dlo     = (*mf)[mfi].box().loVect();
            const int*  dhi     = (*mf)[mfi].box().hiVect();
            const int   ncomp   = (*mf)[mfi].nComp();

            err_list[j].errFunc()(tptr, ARLIM(tlo), ARLIM(thi), &tagval,
                                  &clearval, dat, ARLIM(dlo), ARLIM(dhi),
                                  lo,hi, &ncomp, domain_lo, domain_hi,
                                  dx, xlo, prob_lo, &time, &level);
            //
            // Don't forget to set the tags in the TagBox.
            //
            tags[mfi.index()].tags(itags);
        }

        delete mf;
    }
}

Real
NavierStokes::sumDerive (const std::string& name, Real time)
{
    Real      sum = 0.0;
    MultiFab* mf  = derive(name,time,0);

    BL_ASSERT(!(mf == 0));

    BoxArray baf;

    if (level < parent->finestLevel())
    {
        baf = parent->boxArray(level+1);
        baf.coarsen(fine_ratio);
    }

    for (MFIter mfi(*mf); mfi.isValid(); ++mfi)
    {
        FArrayBox& fab = mf->get(mfi);

        if (level < parent->finestLevel())
        {
            std::vector< std::pair<int,Box> > isects = baf.intersections(grids[mfi.index()]);

            for (int ii = 0, N = isects.size(); ii < N; ii++)
            {
                fab.setVal(0,isects[ii].second,0,fab.nComp());
            }
        }

        sum += fab.sum(0);
    }

    delete mf;

    ParallelDescriptor::ReduceRealSum(sum);

    return sum;
}

Real
NavierStokes::volWgtSum (const std::string& name,
                         Real           time)
{
    Real        sum = 0.0;
    const Real* dx  = geom.CellSize();
    MultiFab*   mf  = derive(name,time,0);
    BoxArray    baf;

    if (level < parent->finestLevel())
    {
        baf = parent->boxArray(level+1);
        baf.coarsen(fine_ratio);
    }

    for (MFIter mfi(*mf); mfi.isValid(); ++mfi)
    {
        FArrayBox& fab = (*mf)[mfi];

        if (level < parent->finestLevel())
        {
            std::vector< std::pair<int,Box> > isects = baf.intersections(grids[mfi.index()]);

            for (int ii = 0, N = isects.size(); ii < N; ii++)
            {
                fab.setVal(0,isects[ii].second,0,fab.nComp());
            }
        }
        Real        s;
        const Real* dat = fab.dataPtr();
        const int*  dlo = fab.loVect();
        const int*  dhi = fab.hiVect();
        const int*  lo  = grids[mfi.index()].loVect();
        const int*  hi  = grids[mfi.index()].hiVect();

#if (BL_SPACEDIM == 2)
        int   rz_flag = Geometry::IsRZ() ? 1 : 0;
        Real* rad     = &radius[mfi.index()][0];
        int   irlo    = lo[0]-radius_grow;
        int   irhi    = hi[0]+radius_grow;
        //
        // Note that this routine will do a volume weighted sum of
        // whatever quantity is passed in, not strictly the "mass".
        //
        if (volWgtSum_sub_dz > 0 && volWgtSum_sub_Rcyl > 0)
        {
            const Real* plo = geom.ProbLo();
            FORT_SUMMASS_CYL(dat,ARLIM(dlo),ARLIM(dhi),ARLIM(lo),ARLIM(hi),
                             dx,&s,rad,&irlo,&irhi,&rz_flag,plo,
                             &volWgtSum_sub_dz,&volWgtSum_sub_Rcyl);
        }
        else
        {
            FORT_SUMMASS(dat,ARLIM(dlo),ARLIM(dhi),ARLIM(lo),ARLIM(hi),
                         dx,&s,rad,&irlo,&irhi,&rz_flag);
        }
#endif

#if (BL_SPACEDIM == 3)
        //
        // Note that this routine will do a volume weighted sum of
        // whatever quantity is passed in, not strictly the "mass".
        //
        if (volWgtSum_sub_dz > 0 && volWgtSum_sub_Rcyl > 0)
        {
            const Real* plo = geom.ProbLo();
            FORT_SUMMASS_CYL(dat,ARLIM(dlo),ARLIM(dhi),ARLIM(lo),ARLIM(hi),
                             dx,plo,&volWgtSum_sub_dz,&volWgtSum_sub_Rcyl,&s);
        }
        else
        {
            FORT_SUMMASS(dat,ARLIM(dlo),ARLIM(dhi),ARLIM(lo),ARLIM(hi),dx,&s);
        }
#endif
        sum += s;
    }

    delete mf;

    ParallelDescriptor::ReduceRealSum(sum);

    return sum;
}

Real 
NavierStokes::MaxVal (const std::string& name,
                      Real           time)
{
    Real        mxval = 0.0;
    MultiFab*   mf    = derive(name,time,0);
    BoxArray    baf;

    if (level < parent->finestLevel())
    {
        baf = parent->boxArray(level+1);
        baf.coarsen(fine_ratio);
    }

    for (MFIter mfi(*mf); mfi.isValid(); ++mfi)
    {
        FArrayBox& fab = (*mf)[mfi];

        if (level < parent->finestLevel())
        {
            std::vector< std::pair<int,Box> > isects = baf.intersections(grids[mfi.index()]);

            for (int ii = 0, N = isects.size(); ii < N; ii++)
            {
                fab.setVal(0,isects[ii].second,0,fab.nComp());
            }
        }
        Real        s;
        const Real* dat = fab.dataPtr();
        const int*  dlo = fab.loVect();
        const int*  dhi = fab.hiVect();
        const int*  lo  = grids[mfi.index()].loVect();
        const int*  hi  = grids[mfi.index()].hiVect();

        FORT_MAXVAL(dat,ARLIM(dlo),ARLIM(dhi),ARLIM(lo),ARLIM(hi),&s);

        mxval = std::max(mxval, s);
    }

    delete mf;

    ParallelDescriptor::ReduceRealMax(mxval);

    return mxval;
}

void
NavierStokes::sum_integrated_quantities ()
{
    const int finest_level = parent->finestLevel();

    Real time = state[State_Type].curTime();
//    Real mass = 0.0;
//    Real trac = 0.0;
    Real udotlapu = 0.0;
    Real energy = 0.0;
    Real forcing = 0.0;
    Real mgvort = 0.0;

    for (int lev = 0; lev <= finest_level; lev++)
    {
        NavierStokes& ns_level = getLevel(lev);
//        mass += ns_level.volWgtSum("density",time);
//        trac += ns_level.volWgtSum("tracer",time);
        udotlapu += ns_level.volWgtSum("udotlapu",time);
        energy += ns_level.volWgtSum("energy",time);
        mgvort = std::max(mgvort,ns_level.MaxVal("mag_vort",time));
#if defined(GENGETFORCE) || defined(MOREGENGETFORCE)
  	if (BL_SPACEDIM==3)
  	    forcing += ns_level.volWgtSum("forcing",time);
#endif
    }

    if (ParallelDescriptor::IOProcessor())
    {
        const int old_prec = std::cout.precision(12);
        std::cout << '\n';
//        std::cout << "TIME= " << time << " MASS= " << mass << '\n';
//        std::cout << "TIME= " << time << " TRAC= " << trac << '\n';
        std::cout << "TIME= " << time << " KENG= " << energy << '\n';
//	if (BL_SPACEDIM==3)
//	    std::cout << "TIME= " << time << " FORC= " << forcing << '\n';
	std::cout << "TIME= " << time << " MAGVORT= " << mgvort << '\n';
	std::cout << "DIAG= " << time << " " << energy << " " << udotlapu << " " << forcing << std::endl;
        std::cout.precision(old_prec);
    }
}

#if (BL_SPACEDIM==3)
void
NavierStokes::TurbSum (Real time, Real *turb, int ksize, int turbVars)
{
    const Real* dx = geom.CellSize();

    const int turbGrow(0);
    const int presGrow(0);
    MultiFab* turbMF = derive("TurbVars",time,turbGrow);
    MultiFab* presMF = derive("PresVars",time,presGrow);

    BoxArray baf;

    if (level < parent->finestLevel())
    {
        baf = parent->boxArray(level+1);
        baf.coarsen(fine_ratio);
    }

    for (MFIter turbMfi(*turbMF), presMfi(*presMF);
	 turbMfi.isValid() && presMfi.isValid();
	 ++turbMfi, ++presMfi)
    {
	FArrayBox& turbFab = (*turbMF)[turbMfi];
	FArrayBox& presFab = (*presMF)[presMfi];

        if (level < parent->finestLevel())
        {
            std::vector< std::pair<int,Box> > isects = baf.intersections(grids[turbMfi.index()]);

            for (int ii = 0, N = isects.size(); ii < N; ii++)
            {
              presFab.setVal(0,isects[ii].second,0,presMF->nComp());
              turbFab.setVal(0,isects[ii].second,0,turbMF->nComp());
            }
        }
    }

    turbMF->FillBoundary(0,turbMF->nComp());
    geom.FillPeriodicBoundary(*turbMF,0,turbMF->nComp());

    presMF->FillBoundary(0,presMF->nComp());
    geom.FillPeriodicBoundary(*presMF,0,presMF->nComp());
    
    for (MFIter turbMfi(*turbMF), presMfi(*presMF);
	 turbMfi.isValid() && presMfi.isValid();
	 ++turbMfi, ++presMfi)
    {
	FArrayBox& turbFab = (*turbMF)[turbMfi];
	FArrayBox& presFab = (*presMF)[presMfi];

        const Real* turbData = turbFab.dataPtr();
        const Real* presData = presFab.dataPtr();
        const int*  dlo = turbFab.loVect();
        const int*  dhi = turbFab.hiVect();
        const int*  plo = presFab.loVect();
        const int*  phi = presFab.hiVect();
        const int*  lo  = grids[turbMfi.index()].loVect();
        const int*  hi  = grids[turbMfi.index()].hiVect();

        FORT_SUMTURB(turbData,presData,ARLIM(dlo),ARLIM(dhi),ARLIM(plo),ARLIM(phi),ARLIM(lo),ARLIM(hi),
		     dx,turb,&ksize,&turbVars);
   } 

    delete turbMF;
    delete presMF;
}

void
NavierStokes::sum_turbulent_quantities ()
{
    Real time = state[State_Type].curTime();
    const int finestLevel = parent->finestLevel();
    const Real *dx = parent->Geom(finestLevel).CellSize();
    const int ksize(parent->Geom(finestLevel).Domain().length(2));
    const int turbVars(33);
    int refRatio(1);

    Real* turb = new Real[turbVars*ksize];

    for (int i=0; i<turbVars*ksize; i++) turb[i]=0;

    for (int lev = finestLevel; lev >= 0; lev--)
    {
	const int levKsize(parent->Geom(lev).Domain().length(2));

	Real* levTurb = new Real[turbVars*levKsize];

	for (int i=0; i<turbVars*levKsize; i++) levTurb[i]=0;
    
        NavierStokes& ns_level = getLevel(lev);
	ns_level.TurbSum(time,levTurb,levKsize,turbVars);

	if (lev<finestLevel)  refRatio *= parent->refRatio(lev)[2];
	else                  refRatio  = 1;

	for (int l=0, k=0; l<levKsize; l++)
	    for (int r=0; r<refRatio; r++, k++)
		for (int v=0; v<turbVars; v++)
		    turb[k*turbVars+v] += levTurb[l*turbVars+v];

	delete [] levTurb;
    }

    ParallelDescriptor::ReduceRealSum(&turb[0], ksize*turbVars, ParallelDescriptor::IOProcessorNumber());

    if (ParallelDescriptor::IOProcessor())
    {
        std::string DirPath = "TurbData";
        if (!BoxLib::UtilCreateDirectory(DirPath, 0755))
            BoxLib::CreateDirectoryFailed(DirPath);

        const int steps = parent->levelSteps(0);
        FILE *file;
        char filename[256];
        sprintf(filename,"TurbData/TurbData_%04d.dat",steps);
        file = fopen(filename,"w");
        for (int k=0; k<ksize; k++)
        {
            fprintf(file,"%e ",dx[2]*(0.5+(double)k));
            for (int v=0; v<turbVars; v++)
                fprintf(file,"%e ",turb[k*turbVars+v]);
            fprintf(file,"\n");
        }
        fclose(file);
    }
    
    delete [] turb;
}

#ifdef SUMJET

void
NavierStokes::JetSum (Real time, Real *jetData, int levRsize,  int levKsize,  int rsize,  int ksize, int jetVars)
{
    const Real* dx = geom.CellSize();

    const int turbGrow(0);
    const int presGrow(0);

    MultiFab* turbMF = derive("JetVars",time,turbGrow);
    MultiFab* presMF = derive("JetPresVars",time,presGrow);

    BoxArray baf;

    if (level < parent->finestLevel())
    {
        baf = parent->boxArray(level+1);
        baf.coarsen(fine_ratio);
    }

    for (MFIter turbMfi(*turbMF), presMfi(*presMF);
	 turbMfi.isValid() && presMfi.isValid();
	 ++turbMfi, ++presMfi)
    {
	FArrayBox& turbFab = (*turbMF)[turbMfi];
	FArrayBox& presFab = (*presMF)[presMfi];

        if (level < parent->finestLevel())
        {
            std::vector< std::pair<int,Box> > isects = baf.intersections(grids[turbMfi.index()]);

            for (int ii = 0, N = isects.size(); ii < N; ii++)
            {
              presFab.setVal(0,isects[ii].second,0,presMF->nComp());
              turbFab.setVal(0,isects[ii].second,0,turbMF->nComp());
            }
        }
    }

    turbMF->FillBoundary(0,turbMF->nComp());
    geom.FillPeriodicBoundary(*turbMF,0,turbMF->nComp());

    presMF->FillBoundary(0,presMF->nComp());
    geom.FillPeriodicBoundary(*presMF,0,presMF->nComp());
    
    for (MFIter turbMfi(*turbMF), presMfi(*presMF);
	 turbMfi.isValid() && presMfi.isValid();
	 ++turbMfi, ++presMfi)
    {
	FArrayBox& turbFab = (*turbMF)[turbMfi];
	FArrayBox& presFab = (*presMF)[presMfi];

        RealBox     gridloc  = RealBox(grids[turbMfi.index()],geom.CellSize(),geom.ProbLo());
        const Real* turbData = turbFab.dataPtr();
        const Real* presData = presFab.dataPtr();
        const int*  dlo = turbFab.loVect();
        const int*  dhi = turbFab.hiVect();
        const int*  plo = presFab.loVect();
        const int*  phi = presFab.hiVect();
        const int*  lo  = grids[turbMfi.index()].loVect();
        const int*  hi  = grids[turbMfi.index()].hiVect();

        FORT_SUMJET(turbData,presData,ARLIM(dlo),ARLIM(dhi),ARLIM(plo),ARLIM(phi),ARLIM(lo),ARLIM(hi),
		    dx,jetData,&levRsize,&levKsize,&rsize,&ksize,&jetVars,&jet_interval_split,
		    gridloc.lo(),gridloc.hi());
    }

    delete turbMF;
    delete presMF;
}

void
NavierStokes::sum_jet_quantities ()
{
    Real time = state[State_Type].curTime();
    const int finestLevel = parent->finestLevel();
    const Real *dx = parent->Geom(finestLevel).CellSize();
    const int isize(parent->Geom(finestLevel).Domain().length(0));
    const int ksize(parent->Geom(finestLevel).Domain().length(2));
    const int rsize=isize>>1;
    const int jetVars(104);

    if (ParallelDescriptor::IOProcessor())
	std::cout << "NavierStokes::sum_jet_quantities():" << std::endl
		  << "   jetVars: " << jetVars << std::endl
		  << "   rsize  : " << rsize << std::endl
		  << "   ksize  : " << ksize << std::endl;
    
    Real* jetData = new Real[jetVars*ksize*rsize];

    for (int i=0; i<jetVars*ksize*rsize; i++) jetData[i]=0;

    for (int lev = finestLevel; lev >= 0; lev--)
    {
	const int levIsize(parent->Geom(lev).Domain().length(0));
	const int levKsize(parent->Geom(lev).Domain().length(2));
	const int levRsize(levIsize>>1);

        NavierStokes& ns_level = getLevel(lev);
	ns_level.JetSum(time,jetData,levRsize,levKsize,rsize,ksize,jetVars);
    }

    ParallelDescriptor::ReduceRealSum(&jetData[0], ksize*rsize*jetVars, ParallelDescriptor::IOProcessorNumber());

    if (ParallelDescriptor::IOProcessor())
    {
        std::cout << "      Creating JetData..." << std::endl;
        std::string DirPath = "JetData";
        if (!BoxLib::UtilCreateDirectory(DirPath, 0755))
            BoxLib::CreateDirectoryFailed(DirPath);

        const int steps = parent->levelSteps(0);
        FILE *file;
        char filename[256];

	Array<Real> r(rsize);
	for (int i=0; i<rsize; i++)
	    r[i] = dx[0]*(0.5+(double)i);
	Array<Real> z(ksize);
	for (int k=0; k<ksize; k++)
	    z[k] = dx[2]*(0.5+(double)k);

#if 0
	sprintf(filename,"JetData/JetData_%04d_r.dat",steps);
	file = fopen(filename,"w");
	for (int i=0; i<rsize; i++)
	    fprintf(file,"%e ",r[i]);
	fclose(file);

	sprintf(filename,"JetData/JetData_%04d_z.dat",steps);
	file = fopen(filename,"w");
	for (int k=0; k<ksize; k++) 
	    fprintf(file,"%e ",dx[2]*(0.5+(double)k));
	fclose(file);

	for (int v=0; v<jetVars; v++) {
	    sprintf(filename,"JetData/JetData_%04d_v%04d.dat",steps,v);
	    file = fopen(filename,"w");
	    for (int k=0; k<ksize; k++) {
		for (int i=0; i<rsize; i++) {
		    fprintf(file,"%e ",jetData[(k*rsize+i)*jetVars+v]);
		}
		fprintf(file,"\n");
	    }
	    fclose(file);
	    std::cout << "   ...done." << std::endl;
	}
#else
	sprintf(filename,"JetData/JD%04d",steps);
	std::string FullPath = filename;
	if (!BoxLib::UtilCreateDirectory(FullPath, 0755))
	    BoxLib::CreateDirectoryFailed(FullPath);
	sprintf(filename,"%s/data.bin",FullPath.c_str());
	file=fopen(filename,"w");
	fwrite(&time,sizeof(double),1,file);
	fwrite(&rsize,sizeof(int),1,file);
	fwrite(&ksize,sizeof(int),1,file);
	fwrite(&jetVars,sizeof(int),1,file);
	fwrite(r.dataPtr(),sizeof(Real),rsize,file);
	fwrite(z.dataPtr(),sizeof(Real),ksize,file);
	fwrite(jetData,sizeof(Real),jetVars*rsize*ksize,file);
	fclose(file);
#endif
    }

    delete [] jetData;
}
#endif
#endif


void
NavierStokes::setPlotVariables()
{
    AmrLevel::setPlotVariables();
}

std::string
NavierStokes::thePlotFileType () const
{
    //
    // Increment this whenever the writePlotFile() format changes.
    //
    static const std::string the_plot_file_type("NavierStokes-V1.1");

    return the_plot_file_type;
}

void
NavierStokes::writePlotFile (const std::string& dir,
                             std::ostream&  os,
                             VisMF::How     how)
{
    if ( ! Amr::Plot_Files_Output() ) return;

    int i, n;
    //
    // The list of indices of State to write to plotfile.
    // first component of pair is state_type,
    // second component of pair is component # within the state_type
    //
    std::vector<std::pair<int,int> > plot_var_map;
    for (int typ = 0; typ < desc_lst.size(); typ++)
        for (int comp = 0; comp < desc_lst[typ].nComp();comp++)
            if (parent->isStatePlotVar(desc_lst[typ].name(comp)) &&
                desc_lst[typ].getType() == IndexType::TheCellType())
                plot_var_map.push_back(std::pair<int,int>(typ,comp));

    int num_derive = 0;
    std::list<std::string> derive_names;
    const std::list<DeriveRec>& dlist = derive_lst.dlist();

    for (std::list<DeriveRec>::const_iterator it = dlist.begin(), end = dlist.end();
         it != end;
         ++it)
    {
        if (parent->isDerivePlotVar(it->name()))
	{
            derive_names.push_back(it->name());
            num_derive += it->numDerive();
	}
    }

    int n_data_items = plot_var_map.size() + num_derive;
    Real cur_time = state[State_Type].curTime();

    if (level == 0 && ParallelDescriptor::IOProcessor())
    {
        //
        // The first thing we write out is the plotfile type.
        //
        os << thePlotFileType() << '\n';

        if (n_data_items == 0)
            BoxLib::Error("Must specify at least one valid data item to plot");

        os << n_data_items << '\n';

	//
	// Names of variables -- first state, then derived
	//
	for (i =0; i < plot_var_map.size(); i++)
        {
	    int typ  = plot_var_map[i].first;
	    int comp = plot_var_map[i].second;
	    os << desc_lst[typ].name(comp) << '\n';
        }

	for (std::list<std::string>::const_iterator it = derive_names.begin(), end = derive_names.end();
             it != end;
             ++it)
        {
	    const DeriveRec* rec = derive_lst.get(*it);
	    for (i = 0; i < rec->numDerive(); i++)
                os << rec->variableName(i) << '\n';
        }
        os << BL_SPACEDIM << '\n';
        os << parent->cumTime() << '\n';
        int f_lev = parent->finestLevel();
        os << f_lev << '\n';
        for (i = 0; i < BL_SPACEDIM; i++)
            os << Geometry::ProbLo(i) << ' ';
        os << '\n';
        for (i = 0; i < BL_SPACEDIM; i++)
            os << Geometry::ProbHi(i) << ' ';
        os << '\n';
        for (i = 0; i < f_lev; i++)
            os << parent->refRatio(i)[0] << ' ';
        os << '\n';
        for (i = 0; i <= f_lev; i++)
            os << parent->Geom(i).Domain() << ' ';
        os << '\n';
        for (i = 0; i <= f_lev; i++)
            os << parent->levelSteps(i) << ' ';
        os << '\n';
        for (i = 0; i <= f_lev; i++)
        {
            for (int k = 0; k < BL_SPACEDIM; k++)
                os << parent->Geom(i).CellSize()[k] << ' ';
            os << '\n';
        }
        os << (int) Geometry::Coord() << '\n';
        os << "0\n"; // Write bndry data.
    }
    // Build the directory to hold the MultiFab at this level.
    // The name is relative to the directory containing the Header file.
    //
    static const std::string BaseName = "/Cell";
    char buf[64];
    sprintf(buf, "Level_%d", level);
    std::string Level = buf;
    //
    // Now for the full pathname of that directory.
    //
    std::string FullPath = dir;
    if (!FullPath.empty() && FullPath[FullPath.length()-1] != '/')
        FullPath += '/';
    FullPath += Level;
    //
    // Only the I/O processor makes the directory if it doesn't already exist.
    //
    if (ParallelDescriptor::IOProcessor())
        if (!BoxLib::UtilCreateDirectory(FullPath, 0755))
            BoxLib::CreateDirectoryFailed(FullPath);
    //
    // Force other processors to wait till directory is built.
    //
    ParallelDescriptor::Barrier();

    if (ParallelDescriptor::IOProcessor())
    {
        os << level << ' ' << grids.size() << ' ' << cur_time << '\n';
        os << parent->levelSteps(level) << '\n';

        for (i = 0; i < grids.size(); ++i)
        {
            RealBox gridloc = RealBox(grids[i],geom.CellSize(),geom.ProbLo());
            for (n = 0; n < BL_SPACEDIM; n++)
                os << gridloc.lo(n) << ' ' << gridloc.hi(n) << '\n';
        }
        //
        // The full relative pathname of the MultiFabs at this level.
        // The name is relative to the Header file containing this name.
        // It's the name that gets written into the Header.
        //
        if (n_data_items > 0)
        {
            std::string PathNameInHeader = Level;
            PathNameInHeader += BaseName;
            os << PathNameInHeader << '\n';
        }
    }
    //
    // We combine all of the multifabs -- state, derived, etc -- into one
    // multifab -- plotMF.
    // NOTE: we are assuming that each state variable has one component,
    // but a derived variable is allowed to have multiple components.
    int       cnt   = 0;
    int       ncomp = 1;
    const int nGrow = 0;
    MultiFab  plotMF(grids,n_data_items,nGrow);
    MultiFab* this_dat = 0;
    //
    // Cull data from state variables -- use no ghost cells.
    //
    for (i = 0; i < plot_var_map.size(); i++)
    {
	int typ  = plot_var_map[i].first;
	int comp = plot_var_map[i].second;
	this_dat = &state[typ].newData();
	MultiFab::Copy(plotMF,*this_dat,comp,cnt,ncomp,nGrow);
	cnt+= ncomp;
    }
    //
    // Cull data from derived variables.
    // 
    Real plot_time;

    if (derive_names.size() > 0)
    {
	for (std::list<std::string>::const_iterator it = derive_names.begin(), end = derive_names.end();
             it != end;
             ++it) 
	{
            if (*it == "avg_pressure" ||
                *it == "gradpx"       ||
                *it == "gradpy"       ||
                *it == "gradpz") 
            {
                if (state[Press_Type].descriptor()->timeType() == 
                    StateDescriptor::Interval) 
                {
                    plot_time = cur_time;
                }
                else
                {
                    int f_lev = parent->finestLevel();
                    plot_time = getLevel(f_lev).state[Press_Type].curTime();
                }
            }
            else
            {
                plot_time = cur_time;
            } 
	    const DeriveRec* rec = derive_lst.get(*it);
	    ncomp = rec->numDerive();
	    MultiFab* derive_dat = derive(*it,plot_time,nGrow);
	    MultiFab::Copy(plotMF,*derive_dat,0,cnt,ncomp,nGrow);
	    delete derive_dat;
	    cnt += ncomp;
	}
    }
    //
    // Use the Full pathname when naming the MultiFab.
    //
    std::string TheFullPath = FullPath;
    TheFullPath += BaseName;
    VisMF::Write(plotMF,TheFullPath,how,true);
}

Real
NavierStokes::estTimeStep ()
{
    if (fixed_dt > 0.0)
    {
        Real factor = 1.0;

        if (!(level == 0))
        {
            int ratio = 1;
            for (int lev = 1; lev <= level; lev++)
            {
                ratio *= parent->nCycle(lev);
            }
            factor = 1.0/double(ratio);
        }

        return factor*fixed_dt;
    }

    const int   n_grow        = 0;
    Real        estdt         = 1.0e+20;
#if defined(GENGETFORCE) || defined(MOREGENGETFORCE)
    const Real  cur_time      = state[State_Type].curTime();
#endif
    const Real  cur_pres_time = state[Press_Type].curTime();
    MultiFab&   U_new         = get_new_data(State_Type);

    Real gr_max[BL_SPACEDIM], u_max[BL_SPACEDIM] = {0};

    FArrayBox tforces;
    MultiFab Gp(grids,BL_SPACEDIM,1);
    getGradP(Gp, cur_pres_time);

    for (MFIter Rho_mfi(*rho_ctime); Rho_mfi.isValid(); ++Rho_mfi)
    {
        const int i = Rho_mfi.index();
        //
        // Get the velocity forcing.  For some reason no viscous forcing.
        //
#ifdef GENGETFORCE
        getForce(tforces,i,n_grow,Xvel,BL_SPACEDIM,cur_time,(*rho_ctime)[i]);
#elif MOREGENGETFORCE
	if (ParallelDescriptor::IOProcessor() && getForceVerbose)
	    std::cout << "---" << std::endl << "H - est Time Step:" << std::endl << "Calling getForce..." << std::endl;
        getForce(tforces,i,n_grow,Xvel,BL_SPACEDIM,cur_time,U_new[i],U_new[i],Density);
#else
        getForce(tforces,i,n_grow,Xvel,BL_SPACEDIM,(*rho_ctime)[i]);
#endif		 
        tforces.minus(Gp[i],0,0,BL_SPACEDIM);
        //
        // Estimate the maximum allowable timestep from the Godunov box.
        //
        Real dt = godunov->estdt(U_new[i],tforces,(*rho_ctime)[i],grids[i],
                                 geom.CellSize(),cfl,gr_max);

        for (int k = 0; k < BL_SPACEDIM; k++)
        {
	    u_max[k] = std::max(u_max[k],gr_max[k]);
	}
	estdt = std::min(estdt,dt);
    }

    ParallelDescriptor::ReduceRealMin(estdt);

    if (verbose)
    {
        const int IOProc = ParallelDescriptor::IOProcessorNumber();

        ParallelDescriptor::ReduceRealMax(u_max, BL_SPACEDIM, IOProc);

        if (ParallelDescriptor::IOProcessor())
        {
            std::cout << "estTimeStep :: \n" << "LEV = " << level << " UMAX = ";
            for (int k = 0; k < BL_SPACEDIM; k++)
                std::cout << u_max[k] << "  ";
            std::cout << '\n';
        }
    }

    return estdt;
}

Real
NavierStokes::initialTimeStep ()
{
  Real returnDt = init_shrink*estTimeStep();

  if (ParallelDescriptor::IOProcessor())
  {
      std::cout << "Multiplying dt by init_shrink; dt = " 
		<< returnDt << std::endl;
  }

  return returnDt;
}

void
NavierStokes::computeNewDt (int                   finest_level,
                            int                   sub_cycle,
                            Array<int>&           n_cycle,
                            const Array<IntVect>& ref_ratio,
                            Array<Real>&          dt_min,
                            Array<Real>&          dt_level,
                            Real                  stop_time,
                            int                   post_regrid_flag) 
{
    //
    // We are at the end of a coarse grid timecycle.
    // Compute the timesteps for the next iteration.
    //
    if (level > 0) return;
    //
    // For Navier Stokes compute the new dt based on current velocity field.
    //
    const int max_level = parent->maxLevel();

    n_cycle[0] = 1;
    for (int i = 1; i <= max_level; i++)
    {
        n_cycle[i] = sub_cycle ? parent->MaxRefRatio(i-1) : 1;
    }

    Real dt_0     = 1.0e+100;
    int  n_factor = 1;
    for (int i = 0; i <= finest_level; i++)
    {
        dt_min[i] = std::min(dt_min[i],getLevel(i).estTimeStep());
        if (ParallelDescriptor::IOProcessor())
	  std::cout << "Actual dt after change limiting = " 
		    << dt_min[i] << std::endl;
    }

    if (fixed_dt <= 0.0) 
    {
       if (post_regrid_flag == 1)
       {
          //
          // Limit dt's by pre-regrid dt
          //
          for (int i = 0; i <= finest_level; i++)
          {
              dt_min[i] = std::min(dt_min[i],dt_level[i]);
          }
       }
       else
       {
          //
          // Limit dt's by change_max * old dt
          //
          for (int i = 0; i <= finest_level; i++)
          {
              dt_min[i] = std::min(dt_min[i],change_max*dt_level[i]);
          }
       }
    }

    //
    // Find the minimum over all levels
    //
    for (int i = 0; i <= finest_level; i++)
    {
        n_factor *= n_cycle[i];
        dt_0      = std::min(dt_0,n_factor*dt_min[i]);
    }

    //
    // Limit dt's by the value of stop_time.
    //
    const Real eps      = 0.0001*dt_0;
    const Real cur_time = state[State_Type].curTime();
    if (stop_time >= 0.0)
    {
        if ((cur_time + dt_0) > (stop_time - eps))
            dt_0 = stop_time - cur_time;
    }
    //
    // Adjust the time step to be able to output checkpoints at specific times.
    //
    const Real check_per = parent->checkPer();
    if (check_per > 0.0)
    {
        int a = int((cur_time + eps ) / check_per);
        int b = int((cur_time + dt_0) / check_per);
        if (a != b)
            dt_0 = b * check_per - cur_time;
    }
    //
    // Adjust the time step to be able to output plot files at specific times.
    //
    const Real plot_per = parent->plotPer();
    if (plot_per > 0.0)
    {
        int a = int((cur_time + eps ) / plot_per);
        int b = int((cur_time + dt_0) / plot_per);
        if (a != b)
            dt_0 = b * plot_per - cur_time;
    }

    n_factor = 1;
    for (int i = 0; i <= max_level; i++)
    {
        n_factor   *= n_cycle[i];
        dt_level[i] = dt_0/( (Real)n_factor );
    }
}

void
NavierStokes::computeInitialDt (int                   finest_level,
                                int                   sub_cycle,
                                Array<int>&           n_cycle,
                                const Array<IntVect>& ref_ratio,
                                Array<Real>&          dt_level, 
                                Real                  stop_time)
{
    //
    // Grids have been constructed, compute dt for all levels.
    //
    if (level > 0)
        return;
    //
    // For Navier Stokes compute the new dt based on current velocity field.
    //
    const int max_level = parent->maxLevel();

    n_cycle[0] = 1;
    for (int i = 1; i <= max_level; i++)
    {
        n_cycle[i] = sub_cycle ? parent->MaxRefRatio(i-1) : 1;
    }

    Real dt_0    = 1.0e+100;
    int n_factor = 1;
    for (int i = 0; i <= finest_level; i++)
    {
        dt_level[i] = getLevel(i).initialTimeStep();
        n_factor   *= n_cycle[i];
        dt_0        = std::min(dt_0,n_factor*dt_level[i]);
    }

    if (stop_time >= 0.0)
    {
        const Real eps      = 0.0001*dt_0;
        const Real cur_time = state[State_Type].curTime();
        if ((cur_time + dt_0) > (stop_time - eps))
            dt_0 = stop_time - cur_time;
    }

    n_factor = 1;
    for (int i = 0; i <= max_level; i++)
    {
        n_factor   *= n_cycle[i];
        dt_level[i] = dt_0/( (Real)n_factor );
    }
}

//
// This function estimates the initial timesteping used by the model.
//

void
NavierStokes::post_init_estDT (Real&        dt_init,
                               Array<int>&  nc_save,
                               Array<Real>& dt_save,
                               Real         stop_time)
{
    const Real strt_time    = state[State_Type].curTime();
    const int  finest_level = parent->finestLevel();

    dt_init = 1.0e+100;

    int  n_factor;
    for (int k = 0; k <= finest_level; k++)
    {
        nc_save[k] = parent->nCycle(k);
        dt_save[k] = getLevel(k).initialTimeStep();

        n_factor   = 1;
        for (int m = finest_level; m > k; m--) 
             n_factor *= parent->nCycle(m);
        dt_init    = std::min( dt_init, dt_save[k]/((Real) n_factor) );
    }
 
    Array<Real> dt_level(finest_level+1,dt_init);
    Array<int>  n_cycle(finest_level+1,1);

    Real dt0 = dt_save[0];
    n_factor = 1;
    for (int k = 0; k <= finest_level; k++)
    {
        n_factor *= nc_save[k];
        dt0       = std::min(dt0,n_factor*dt_save[k]);
    }

    if (stop_time >= 0.0)
    {
        const Real eps = 0.0001*dt0;
        if ((strt_time + dt0) > (stop_time - eps))
            dt0 = stop_time - strt_time;
    }

    n_factor = 1;
    for (int k = 0; k <= finest_level; k++)
    {
        n_factor  *= nc_save[k];
        dt_save[k] = dt0/( (Real) n_factor);
    }
    //
    // Hack.
    //
    parent->setDtLevel(dt_level);
    parent->setNCycle(n_cycle);
    for (int k = 0; k <= finest_level; k++)
    {
        getLevel(k).setTimeLevel(strt_time,dt_init,dt_init);
    }
}

//
// Fills in amrLevel okToContinue.
//

int
NavierStokes::okToContinue ()
{
    return (level > 0) ? true : (parent->dtLevel(0) > dt_cutoff);
}

//
// THE MAIN HOOKS INTO AMR AND AMRLEVEL
//

//
// Integration cycle on fine level grids is complete .
// post_timestep() is responsible for syncing levels together.
//
// The registers used for level syncing are initialized in the
// coarse level advance and incremented in the fine level advance.
// These quantities are described in comments above advance_setup.
//

void
NavierStokes::post_timestep (int crse_iteration)
{
    const int finest_level = parent->finestLevel();

    if (do_reflux && level < finest_level)
        reflux();

    if (level < finest_level)
        avgDown();

    if (do_mac_proj && level < finest_level)
        mac_sync();

    if (do_sync_proj && (level < finest_level))
        level_sync(crse_iteration);
    //
    // Test for conservation.
    //
    if (level==0 && sum_interval>0 && (parent->levelSteps(0)%sum_interval == 0))
    {
        sum_integrated_quantities();
    }
#if (BL_SPACEDIM==3)
    //
    // Derive turbulent statistics
    //
    if (level==0 && turb_interval>0 && (parent->levelSteps(0)%turb_interval == 0))
    {
        sum_turbulent_quantities();
    }
#ifdef SUMJET
    //
    // Derive turbulent statistics for the round jet
    //
    if (level==0 && jet_interval>0 && (parent->levelSteps(0)%jet_interval == 0))
    {
        sum_jet_quantities();
    }
#endif
#endif

    if (level > 0) incrPAvg();

    old_intersect_new          = grids;
    is_first_step_after_regrid = false;
}

//
// Build any additional data structures after restart.
//

void NavierStokes::post_restart()
{
    make_rho_prev_time();
    make_rho_curr_time();
}

//
// Build any additional data structures after regrid.
//

void
NavierStokes::post_regrid (int lbase,
                                int new_finest)
{
    if (projector && level == lbase)
        projector->setFinestLevel(new_finest);
}

//
// Ensure state, and pressure are consistent.
//

void
NavierStokes::post_init (Real stop_time)
{
    if (level > 0)
        //
        // Nothing to sync up at level > 0.
        //
        return;

    const int   finest_level = parent->finestLevel();
    Real        dt_init      = 0.0;
    Array<Real> dt_save(finest_level+1);
    Array<int>  nc_save(finest_level+1);
    //
    // Ensure state is consistent, i.e. velocity field is non-divergent,
    // Coarse levels are fine level averages, pressure is zero.
    //
    post_init_state();
    //
    // Estimate the initial timestepping.
    //
    post_init_estDT(dt_init, nc_save, dt_save, stop_time);
    //
    // Initialize the pressure by iterating the initial timestep.
    //
    post_init_press(dt_init, nc_save, dt_save);
    //
    // Compute the initial estimate of conservation.
    //
    if (sum_interval > 0)
        sum_integrated_quantities();
#if (BL_SPACEDIM==3)
    //
    // Derive turbulent statistics
    //
    if (turb_interval > 0)
        sum_turbulent_quantities();
#ifdef SUMJET
    //
    // Derive turbulent statistics for the round jet
    //
    if (jet_interval > 0)
        sum_jet_quantities();
#endif
#endif
}

//
// MULTILEVEL SYNC FUNCTIONS
//

void
NavierStokes::initRhoAvg (Real alpha)
{
    MultiFab& S_new = get_new_data(State_Type);

    rho_avg->setVal(0);

    for (MFIter rho_avgmfi(*rho_avg); rho_avgmfi.isValid(); ++rho_avgmfi)
    {
        const int i = rho_avgmfi.index();
        (*rho_avg)[i].copy(S_new[i],S_new.box(i),Density,S_new.box(i),0,1);
        (*rho_avg)[i].mult(alpha);
    }
}

void
NavierStokes::incrRhoAvg(const MultiFab& rho_incr,
                         int             sComp,
                         Real            alpha)
{
    for (MFIter mfi(rho_incr); mfi.isValid(); ++mfi)
    {
        const int* lo      = mfi.validbox().loVect();
        const int* hi      = mfi.validbox().hiVect();
        const int* rlo     = rho_incr[mfi].loVect();
        const int* rhi     = rho_incr[mfi].hiVect();
        const Real* rhodat = rho_incr[mfi].dataPtr(sComp);
        const int* alo     = (*rho_avg)[mfi].loVect();
        const int* ahi     = (*rho_avg)[mfi].hiVect();
        Real* avgdat       = (*rho_avg)[mfi].dataPtr();

        FORT_INCRMULT(avgdat,ARLIM(alo),ARLIM(ahi),
                      rhodat,ARLIM(rlo),ARLIM(rhi),lo,hi,&alpha);
    }
}

void
NavierStokes::incrRhoAvg (Real alpha)
{
    const MultiFab& S_new = get_new_data(State_Type);
    incrRhoAvg(S_new,Density,alpha);
}

void
NavierStokes::incrPAvg ()
{
    //
    // Increment p_avg with 1/ncycle times current pressure
    //
    MultiFab& P_new = get_new_data(Press_Type);

    Real alpha = 1.0/Real(parent->nCycle(level));

    for (MFIter P_newmfi(P_new); P_newmfi.isValid(); ++P_newmfi)
    {
        const int*  lo     = P_newmfi.validbox().loVect();
        const int*  hi     = P_newmfi.validbox().hiVect();
        const int*  p_lo   = P_new[P_newmfi].loVect();
        const int*  p_hi   = P_new[P_newmfi].hiVect();
        const Real* pdat   = P_new[P_newmfi].dataPtr();
        const int*  alo    = (*p_avg)[P_newmfi].loVect();
        const int*  ahi    = (*p_avg)[P_newmfi].hiVect();
        Real*       avgdat = (*p_avg)[P_newmfi].dataPtr();

        FORT_INCRMULT(avgdat,ARLIM(alo),ARLIM(ahi),
                      pdat,ARLIM(p_lo),ARLIM(p_hi),lo,hi,&alpha);
    }
}

//
// This function ensures that the state is initially consistent
// with respect to the divergence condition and fields are initially consistent
//

void
NavierStokes::post_init_state ()
{
    const int finest_level = parent->finestLevel();
    const Real divu_time   = have_divu ? state[Divu_Type].curTime()
                                       : state[Press_Type].curTime();
    if (do_init_vort_proj)
    {
        //
	// NOTE: this assumes have_divu == 0.
	// Only used if vorticity is used to initialize the velocity field.
        //
        BL_ASSERT(!(projector == 0));
        
	projector->initialVorticityProject(0);
    }

    if (do_init_proj && projector)
        //
        // Do sync project to define divergence free velocity field.
        //
        projector->initialVelocityProject(0,divu_time,have_divu);

    NavierStokes::initial_step = true;
    //
    // Average velocity and scalar data down from finer levels
    // so that conserved data is consistant between levels.
    //
    for (int k = finest_level-1; k>= 0; k--)
    {
        getLevel(k).avgDown();
    }
    make_rho_curr_time();

    if (do_init_proj && projector && (std::fabs(gravity)) > 0.)
        //
        // Do projection to establish initially hydrostatic pressure field.
        //
        projector->initialPressureProject(0);
}

//
// Initialize the pressure by iterating the initial timestep
//

void
NavierStokes::post_init_press (Real&        dt_init,
                               Array<int>&  nc_save,
                               Array<Real>& dt_save)
{
    const Real strt_time       = state[State_Type].curTime();
    const int  finest_level    = parent->finestLevel();
    NavierStokes::initial_iter = true;
    //
    // Iterate over the advance function.
    //
    for (int iter = 0; iter < init_iter; iter++)
    {
        for (int k = 0; k <= finest_level; k++ )
        {
            getLevel(k).advance(strt_time,dt_init,1,1);
        }
        //
        // This constructs a guess at P, also sets p_old == p_new.
        //
        MultiFab** sig = new MultiFab*[finest_level+1];

        for (int k = 0; k <= finest_level; k++)
        {
            sig[k] = getLevel(k).get_rho_half_time();
        }
        if (projector)
            projector->initialSyncProject(0,sig,parent->dtLevel(0),
                                          strt_time,have_divu);
        delete [] sig;

        for (int k = finest_level-1; k >= 0; k--)
        {
            getLevel(k).avgDown();
        }
        for (int k = 0; k <= finest_level; k++)
        {
            //
            // Reset state variables to initial time, but 
            // do not reset pressure variable, only pressure time.
            //
            getLevel(k).resetState(strt_time, dt_init, dt_init);
        }

        make_rho_curr_time();

        NavierStokes::initial_iter = false;
    }

    if (init_iter <= 0)
        NavierStokes::initial_iter = false; // Just being compulsive -- rbp.

    NavierStokes::initial_step = false;
    //
    // Re-instate timestep.
    //
    for (int k = 0; k <= finest_level; k++)
    {
        getLevel(k).setTimeLevel(strt_time,dt_save[k],dt_save[k]);
        if (state[Press_Type].descriptor()->timeType() == StateDescriptor::Point)
        {
            getLevel(k).state[Press_Type].setNewTimeLevel(.5*dt_init);
            getLevel(k).get_old_data(Dpdt_Type).setVal(0);
        }
    }

    parent->setDtLevel(dt_save);
    parent->setNCycle(nc_save);
}

//
// Helper function for NavierStokes::SyncInterp().
//

static
void
set_bc_new (int*            bc_new,
            int             n,
            int             src_comp,
            const int*      clo,
            const int*      chi,
            const int*      cdomlo,
            const int*      cdomhi,
            const BoxArray& cgrids,
            int**           bc_orig_qty)
            
{
    for (int dir = 0; dir < BL_SPACEDIM; dir++)
    {
        int bc_index = (n+src_comp)*(2*BL_SPACEDIM) + dir;
        bc_new[bc_index]             = INT_DIR;
        bc_new[bc_index+BL_SPACEDIM] = INT_DIR;
 
        if (clo[dir] < cdomlo[dir] || chi[dir] > cdomhi[dir])
        {
            for (int crse = 0; crse < cgrids.size(); crse++)
            {
                const int* c_lo = cgrids[crse].loVect();
                const int* c_hi = cgrids[crse].hiVect();

                if (clo[dir] < cdomlo[dir] && c_lo[dir] == cdomlo[dir])
                    bc_new[bc_index] = bc_orig_qty[crse][bc_index];
                if (chi[dir] > cdomhi[dir] && c_hi[dir] == cdomhi[dir])
                    bc_new[bc_index+BL_SPACEDIM] = bc_orig_qty[crse][bc_index+BL_SPACEDIM]; 
            }
        }
    }
}

//
// Interpolate A cell centered Sync correction from a
// coarse level (c_lev) to a fine level (f_lev).
//
// This routine interpolates the num_comp components of CrseSync
// (starting at src_comp) and either increments or puts the result into
// the num_comp components of FineSync (starting at dest_comp)
// The components of bc_orig_qty corespond to the quantities of CrseSync.
//

void
NavierStokes::SyncInterp (MultiFab&      CrseSync,
                          int            c_lev,
                          MultiFab&      FineSync,
                          int            f_lev,
                          IntVect&       ratio,
                          int            src_comp,
                          int            dest_comp,
                          int            num_comp,
                          int            increment,
                          Real           dt_clev, 
                          int**          bc_orig_qty,
                          SyncInterpType which_interp,
                          int            state_comp)
{
    BL_ASSERT(which_interp >= 0 && which_interp <= 5);

    Interpolater* interpolater = 0;

    switch (which_interp)
    {
    case PC_T:           interpolater = &pc_interp;           break;
    case CellCons_T:     interpolater = &cell_cons_interp;    break;
    case CellConsLin_T:  interpolater = &lincc_interp;        break;
    case CellConsProt_T: interpolater = &protected_interp;    break;
    default:
        BoxLib::Abort("NavierStokes::SyncInterp(): how did this happen");
    }

    NavierStokes&   fine_level = getLevel(f_lev);
    const BoxArray& fgrids     = fine_level.boxArray();
    const Geometry& fgeom      = parent->Geom(f_lev);
    const BoxArray& cgrids     = getLevel(c_lev).boxArray();
    const Geometry& cgeom      = parent->Geom(c_lev);
    const Real*     dx_crse    = cgeom.CellSize();
    Box             cdomain    = BoxLib::coarsen(fgeom.Domain(),ratio);
    const int*      cdomlo     = cdomain.loVect();
    const int*      cdomhi     = cdomain.hiVect();
    int*            bc_new     = new int[2*BL_SPACEDIM*(src_comp+num_comp)];

    BoxArray cdataBA(fgrids.size());

    for (int i = 0; i < fgrids.size(); i++)
        cdataBA.set(i,interpolater->CoarseBox(fgrids[i],ratio));
    //
    // Note: The boxes in cdataBA may NOT be disjoint !!!
    //
    MultiFab cdataMF(cdataBA,num_comp,0);

    cdataMF.setVal(0);

    cdataMF.copy(CrseSync, src_comp, 0, num_comp);
    //
    // Set physical boundary conditions in cdataMF.
    //
    for (MFIter mfi(cdataMF); mfi.isValid(); ++mfi)
    {
        int         i       = mfi.index();
        RealBox     gridloc = RealBox(fine_level.grids[i],fine_level.geom.CellSize(),fine_level.geom.ProbLo());
        FArrayBox&  cdata   = cdataMF[mfi];
        const int*  clo     = cdata.loVect();
        const int*  chi     = cdata.hiVect();
        const Real* xlo     = gridloc.lo();

        for (int n = 0; n < num_comp; n++)
        {
            set_bc_new(bc_new,n,src_comp,clo,chi,cdomlo,cdomhi,cgrids,bc_orig_qty);

            FORT_FILCC(cdata.dataPtr(n), ARLIM(clo), ARLIM(chi),
                       cdomlo, cdomhi, dx_crse, xlo,
                       &(bc_new[2*BL_SPACEDIM*(n+src_comp)]));
        }
    }
    cgeom.FillPeriodicBoundary(cdataMF, 0, num_comp);
    //
    // Interpolate from cdataMF to fdata and update FineSync.
    // Note that FineSync and cdataMF will have the same distribution
    // since the length of their BoxArrays are equal.
    //
    FArrayBox    fdata;
    Array<BCRec> bc_interp(num_comp);

    MultiFab* fine_stateMF = 0;
    if (interpolater == &protected_interp)
    {
        fine_stateMF = &(getLevel(f_lev).get_new_data(State_Type));
    }

    for (MFIter mfi(cdataMF); mfi.isValid(); ++mfi)
    {
        int        i     = mfi.index();
        FArrayBox& cdata = cdataMF[mfi];
        const int* clo   = cdata.loVect();
        const int* chi   = cdata.hiVect();

        fdata.resize(fgrids[i], num_comp);
        //
        // Set the boundary condition array for interpolation.
        //
        for (int n = 0; n < num_comp; n++)
        {
            set_bc_new(bc_new,n,src_comp,clo,chi,cdomlo,cdomhi,cgrids,bc_orig_qty);
        }

        for (int n = 0; n < num_comp; n++)
        {
            for (int dir = 0; dir < BL_SPACEDIM; dir++)
            {
                int bc_index = (n+src_comp)*(2*BL_SPACEDIM) + dir;
                bc_interp[n].setLo(dir,bc_new[bc_index]);
                bc_interp[n].setHi(dir,bc_new[bc_index+BL_SPACEDIM]);
            }
        }

        ScaleCrseSyncInterp(cdata, c_lev, num_comp);

        interpolater->interp(cdata,0,fdata,0,num_comp,fgrids[i],ratio,
                             cgeom,fgeom,bc_interp,src_comp,State_Type);

        reScaleFineSyncInterp(fdata, f_lev, num_comp);

        if (increment)
        {
            fdata.mult(dt_clev);

            if (interpolater == &protected_interp)
            {
              cdata.mult(dt_clev);
              FArrayBox& fine_state = (*fine_stateMF)[i];
              interpolater->protect(cdata,0,fdata,0,fine_state,state_comp,
                                    num_comp,fgrids[i],ratio,
                                    cgeom,fgeom,bc_interp);
              Real dt_clev_inv = 1./dt_clev;
              cdata.mult(dt_clev_inv);
            }
            
            FineSync[i].plus(fdata,0,dest_comp,num_comp);
        }
        else
        {
            FineSync[i].copy(fdata,0,dest_comp,num_comp);
        }
    }

    delete [] bc_new;
}

//
// Interpolate sync pressure correction to a finer level.
//

void
NavierStokes::SyncProjInterp (MultiFab& phi,
                              int       c_lev,
                              MultiFab& P_new,
                              MultiFab& P_old,
                              int       f_lev,
                              IntVect&  ratio,
                              bool      first_crse_step_after_initial_iters,
                              Real      cur_crse_pres_time,
                              Real      prev_crse_pres_time)
{
    const Geometry& fgeom   = parent->Geom(f_lev);
    const BoxArray& P_grids = P_new.boxArray();
    const Geometry& cgeom   = parent->Geom(c_lev);

    BoxArray crse_ba(P_grids.size());

    for (int i = 0; i < P_grids.size(); i++)
    {
        crse_ba.set(i,node_bilinear_interp.CoarseBox(P_grids[i],ratio));
    }

    Array<BCRec> bc(BL_SPACEDIM);
    MultiFab     crse_phi(crse_ba,1,0);

    crse_phi.setVal(1.e200);
    crse_phi.copy(phi,0,0,1);

    FArrayBox     fine_phi;
    NavierStokes& fine_lev            = getLevel(f_lev);
    const Real    cur_fine_pres_time  = fine_lev.state[Press_Type].curTime();
    const Real    prev_fine_pres_time = fine_lev.state[Press_Type].prevTime();

    if (state[Press_Type].descriptor()->timeType() == 
        StateDescriptor::Point && first_crse_step_after_initial_iters)
    {
        const Real time_since_zero  = cur_crse_pres_time - prev_crse_pres_time;
        const Real dt_to_prev_time  = prev_fine_pres_time - prev_crse_pres_time;
        const Real dt_to_cur_time   = cur_fine_pres_time - prev_crse_pres_time;
        const Real cur_mult_factor  = dt_to_cur_time / time_since_zero;
        const Real prev_mult_factor = dt_to_prev_time / dt_to_cur_time;

        for (MFIter mfi(crse_phi); mfi.isValid(); ++mfi)
        {
            fine_phi.resize(P_grids[mfi.index()],1);
            fine_phi.setVal(1.e200);
            node_bilinear_interp.interp(crse_phi[mfi],0,fine_phi,0,1,
                                        fine_phi.box(),ratio,cgeom,fgeom,bc,
                                        0,Press_Type);
            fine_phi.mult(cur_mult_factor);
            P_new[mfi.index()].plus(fine_phi);
            fine_phi.mult(prev_mult_factor);
            P_old[mfi.index()].plus(fine_phi);
        }
    }
    else 
    {
        for (MFIter mfi(crse_phi); mfi.isValid(); ++mfi)
        {
            fine_phi.resize(P_grids[mfi.index()],1);
            fine_phi.setVal(1.e200);
            node_bilinear_interp.interp(crse_phi[mfi],0,fine_phi,0,1,
                                        fine_phi.box(),ratio,cgeom,fgeom,bc,
                                        0,Press_Type);
            P_new[mfi.index()].plus(fine_phi);
            P_old[mfi.index()].plus(fine_phi);
        }
    }
}

//
// Averages a multifab of fine data down onto a multifab of coarse data.
//
// This should be an Amrlevel or Multifab function
//

void
NavierStokes::avgDown (const BoxArray& cgrids,
                       const BoxArray& fgrids,
                       MultiFab&       S_crse,
                       MultiFab&       S_fine,
                       MultiFab&       cvolume,
                       MultiFab&       fvolume,
                       int             c_level,
                       int             f_level,
                       int             scomp,
                       int             ncomp,
                       const IntVect&  fratio)
{
    BL_ASSERT(cgrids == S_crse.boxArray());
    BL_ASSERT(fgrids == S_fine.boxArray());
    BL_ASSERT(cvolume.boxArray() == cgrids);
    BL_ASSERT(fvolume.boxArray() == fgrids);
    BL_ASSERT(S_crse.nComp() == S_fine.nComp());
    BL_ASSERT(fvolume.nComp() == 1 && cvolume.nComp() == 1);
    //
    // Coarsen() the fine stuff on processors owning the fine data.
    //
    BoxArray crse_S_fine_BA(fgrids.size());

    for (int i = 0; i < fgrids.size(); ++i)
    {
        crse_S_fine_BA.set(i,BoxLib::coarsen(fgrids[i],fratio));
    }

    MultiFab crse_S_fine(crse_S_fine_BA,ncomp,0);
    MultiFab crse_fvolume(crse_S_fine_BA,1,0);

    crse_fvolume.copy(cvolume);

    for (MFIter mfi(S_fine); mfi.isValid(); ++mfi)
    {
        const int i = mfi.index();

        avgDown(S_fine[i],crse_S_fine[i],fvolume[i],crse_fvolume[i],
                f_level,c_level,crse_S_fine_BA[i],scomp,ncomp,fratio);
    }

    crse_fvolume.clear();

    S_crse.copy(crse_S_fine,0,scomp,ncomp);
}

//
// Average fine down to coarse in the ovlp intersection.
//

void
NavierStokes::avgDown (const FArrayBox& fine_fab,
                       const FArrayBox& crse_fab, 
                       const FArrayBox& fine_vol,
                       const FArrayBox& crse_vol,
                       int              f_level,
                       int              c_level,
                       const Box&       ovlp,
                       int              scomp,
                       int              ncomp,
                       const IntVect&   fratio)
{
    avgDown_doit(fine_fab,crse_fab,fine_vol,crse_vol,
                 f_level,c_level,ovlp,scomp,ncomp,fratio);
}

//
// Actually average the data down (this is static)
//

void
NavierStokes::avgDown_doit (const FArrayBox& fine_fab,
                            const FArrayBox& crse_fab, 
                            const FArrayBox& fine_vol,
                            const FArrayBox& crse_vol,
                            int              f_level,
                            int              c_level,
                            const Box&       ovlp,
                            int              scomp,
                            int              ncomp,
                            const IntVect&   fratio)
{
//
//  NOTE: We copy from component scomp of the fine fab into component 0 of the crse fab
//        because the crse fab is a temporary which was made starting at comp 0, it is
//        not the actual state data.
//
    const int*  ovlo   = ovlp.loVect();
    const int*  ovhi   = ovlp.hiVect();
    const int*  flo    = fine_fab.loVect();
    const int*  fhi    = fine_fab.hiVect();
    const Real* f_dat  = fine_fab.dataPtr(scomp);
    const int*  fvlo   = fine_vol.loVect();
    const int*  fvhi   = fine_vol.hiVect();
    const Real* fv_dat = fine_vol.dataPtr();
    const int*  clo    = crse_fab.loVect();
    const int*  chi    = crse_fab.hiVect();
    const Real* c_dat  = crse_fab.dataPtr();
    const int*  cvlo   = crse_vol.loVect();
    const int*  cvhi   = crse_vol.hiVect();
    const Real* cv_dat = crse_vol.dataPtr();

    FORT_AVGDOWN(c_dat,ARLIM(clo),ARLIM(chi),&ncomp,
                 f_dat,ARLIM(flo),ARLIM(fhi),
                 cv_dat,ARLIM(cvlo),ARLIM(cvhi),
                 fv_dat,ARLIM(fvlo),ARLIM(fvhi),
                 ovlo,ovhi,fratio.getVect());
}

void
NavierStokes::level_sync (int crse_iteration)
{
    const Real*     dx            = geom.CellSize();
    IntVect         ratio         = parent->refRatio(level);
    const int       finest_level  = parent->finestLevel();
    int             crse_dt_ratio = parent->nCycle(level);
    Real            dt            = parent->dtLevel(level);
    MultiFab&       pres          = get_new_data(Press_Type);
    MultiFab&       vel           = get_new_data(State_Type);
    SyncRegister&   rhs_sync_reg  = getLevel(level+1).getSyncReg();
    SyncRegister*   crsr_sync_ptr = 0;
    NavierStokes&   fine_level    = getLevel(level+1);
    MultiFab&       pres_fine     = fine_level.get_new_data(Press_Type);
    MultiFab&       vel_fine      = fine_level.get_new_data(State_Type);
    const BoxArray& finegrids     = vel_fine.boxArray();
    
    if (level > 0)
        crsr_sync_ptr = &(getLevel(level).getSyncReg());
    //
    // Get boundary conditions.
    //
    Array<int*>         sync_bc(grids.size());
    Array< Array<int> > sync_bc_array(grids.size());

    for (int i = 0; i < grids.size(); i++)
    {
        sync_bc_array[i] = getBCArray(State_Type,i,Xvel,BL_SPACEDIM);
        sync_bc[i] = sync_bc_array[i].dataPtr();
    }

    MultiFab cc_rhs_crse, cc_rhs_fine;

    if ((do_sync_proj && have_divu && do_divu_sync == 1) || do_MLsync_proj)
    {
        cc_rhs_crse.define(grids,1,1,Fab_allocate);
        cc_rhs_fine.define(finegrids,1,1,Fab_allocate);
        cc_rhs_crse.setVal(0);
        cc_rhs_fine.setVal(0);
    }
    //
    // At this point the Divu state data is what was used in the original
    // level project and has not been updated by avgDown or mac_sync.
    // We want to fill cc_rhs_crse and cc_rhs_fine with the difference
    // between the divu we now define using calc_divu and the divu which
    // is in the state data.
    // We are also copying the new computed value of divu into the Divu state.
    //
    if (do_sync_proj && have_divu && do_divu_sync == 1) 
    {
        const Real cur_time = state[Divu_Type].curTime();
        const Real dt_inv = 1.0 / dt;

        MultiFab& cur_divu_crse = get_new_data(Divu_Type);
        calc_divu(cur_time,dt,cc_rhs_crse);
        {
            MultiFab new_divu_crse(grids,1,0);
            MultiFab::Copy(new_divu_crse,cc_rhs_crse,0,0,1,0);
            cc_rhs_crse.minus(cur_divu_crse,0,1,0);
            MultiFab::Copy(cur_divu_crse,new_divu_crse,0,0,1,0);
        }
        cc_rhs_crse.mult(dt_inv,0,1,0);

        NavierStokes& fine_lev = getLevel(level+1);
        MultiFab& cur_divu_fine = fine_lev.get_new_data(Divu_Type);
        fine_lev.calc_divu(cur_time,dt,cc_rhs_fine);
        {
            MultiFab new_divu_fine(finegrids,1,0);
            MultiFab::Copy(new_divu_fine,cc_rhs_fine,0,0,1,0);
            cc_rhs_fine.minus(cur_divu_fine,0,1,0);
            MultiFab::Copy(cur_divu_fine,new_divu_fine,0,0,1,0);
        }
        cc_rhs_fine.mult(dt_inv,0,1,0);
        //
        // With new divu's, get new Dsdt, then average down.
        //
        calc_dsdt(cur_time, dt, get_new_data(Dsdt_Type));
        fine_lev.calc_dsdt(cur_time, dt/crse_dt_ratio,
                           fine_lev.get_new_data(Dsdt_Type));
        for (int k = level; k>= 0; k--)
        {
            MultiFab fvolume;
            MultiFab cvolume;

            NavierStokes&   flev     = getLevel(k+1);
            const BoxArray& fgrids   = flev.grids;
          
            NavierStokes&   clev     = getLevel(k);
            const BoxArray& cgrids   = clev.grids;

            flev.geom.GetVolume(fvolume,flev.grids,GEOM_GROW);
            clev.geom.GetVolume(cvolume,clev.grids,GEOM_GROW);
          
            IntVect&  fratio = clev.fine_ratio;
          
            NavierStokes::avgDown(cgrids, fgrids,
                                  clev.get_new_data(Divu_Type),
                                  flev.get_new_data(Divu_Type),
                                  cvolume, fvolume,
                                  k, k+1, 0, 1, fratio);

            NavierStokes::avgDown(cgrids, fgrids,
                                  clev.get_new_data(Dsdt_Type),
                                  flev.get_new_data(Dsdt_Type),
                                  cvolume, fvolume,
                                  k, k+1, 0, 1, fratio);
        }
    }
    //
    // Multilevel or single-level sync projection.
    //
    MultiFab* Rh = get_rho_half_time();

    if (do_MLsync_proj)
    {
        
        MultiFab&       vel_fine    = fine_level.get_new_data(State_Type);
        MultiFab&       rho_fine    = *fine_level.rho_avg;
        const Geometry& crse_geom   = parent->Geom(level);
        const BoxArray& P_finegrids = pres_fine.boxArray();

        MultiFab phi(P_finegrids,1,1);
        MultiFab V_corr(finegrids,BL_SPACEDIM,1);

        V_corr.setVal(0);
        //
        // If periodic, enforce periodicity on Vsync.
        //
        projector->EnforcePeriodicity(*Vsync, BL_SPACEDIM, grids, crse_geom);
        //
        // Interpolate Vsync to fine grid correction in Vcorr.
        //
        SyncInterp(*Vsync, level, V_corr, level+1, ratio,
                   0, 0, BL_SPACEDIM, 0 , dt, sync_bc.dataPtr());
        //
        // The multilevel projection.  This computes the projection and
        // adds in its contribution to levels (level) and (level+1).
        //
        Real  cur_crse_pres_time = state[Press_Type].curTime();
        Real prev_crse_pres_time = state[Press_Type].prevTime();

        NavierStokes& fine_lev   = getLevel(level+1);
        Real  cur_fine_pres_time = fine_lev.state[Press_Type].curTime();
        Real prev_fine_pres_time = fine_lev.state[Press_Type].prevTime();

        bool first_crse_step_after_initial_iters =
         (prev_crse_pres_time > state[State_Type].prevTime());

        bool pressure_time_is_interval = 
         (state[Press_Type].descriptor()->timeType() == StateDescriptor::Interval);
        projector->MLsyncProject(level,pres,vel,cc_rhs_crse,
                                 pres_fine,vel_fine,cc_rhs_fine,
                                 *Rh,rho_fine,Vsync,V_corr,
                                 phi,&rhs_sync_reg,crsr_sync_ptr,
                                 dt,ratio,crse_iteration,crse_dt_ratio, 
                                 geom,pressure_time_is_interval,
                                 first_crse_step_after_initial_iters,
                                 cur_crse_pres_time,prev_crse_pres_time,
                                 cur_fine_pres_time,prev_fine_pres_time);
        cc_rhs_crse.clear();
        cc_rhs_fine.clear();
        //
        // Correct pressure and velocities after the projection.
        //
        ratio = IntVect::TheUnitVector();
        Array<int*>         fine_sync_bc(finegrids.size());
        Array< Array<int> > fine_sync_bc_array(finegrids.size());

        for (int i = 0; i < finegrids.size(); i++)
        {
            fine_sync_bc_array[i] = getLevel(level+1).getBCArray(State_Type,
                                                                 i,
                                                                 Xvel,
                                                                 BL_SPACEDIM);
            fine_sync_bc[i] = fine_sync_bc_array[i].dataPtr();
        }

        for (int lev = level+2; lev <= finest_level; lev++)
        {
            ratio                 *= parent->refRatio(lev-1);
            NavierStokes& fine_lev = getLevel(lev);
            MultiFab&     P_new    = fine_lev.get_new_data(Press_Type);
            MultiFab&     P_old    = fine_lev.get_old_data(Press_Type);
            MultiFab&     U_new    = fine_lev.get_new_data(State_Type);

            SyncInterp(V_corr, level+1, U_new, lev, ratio,
                       0, 0, BL_SPACEDIM, 1 , dt, fine_sync_bc.dataPtr());
            SyncProjInterp(phi, level+1, P_new, P_old, lev, ratio,
                           first_crse_step_after_initial_iters,
                           cur_crse_pres_time, prev_crse_pres_time);
        }

        if (state[Press_Type].descriptor()->timeType() == StateDescriptor::Point)
            calcDpdt();
    }
    else if (do_sync_proj) 
    {
        MultiFab phi(pres.boxArray(),1,1);
        BoxArray sync_boxes = pres_fine.boxArray();
        sync_boxes.coarsen(ratio);
        //
        // The single level projection.  This computes the projection and
        // adds in its contribution to level (level).
        //
        projector->syncProject(level,pres,vel,Rh,Vsync,phi,
                               &rhs_sync_reg,crsr_sync_ptr,sync_boxes,
                               geom,dx,dt,crse_iteration,crse_dt_ratio);
        //
        // Correct pressure and velocities after the projection.
        //
        ratio = IntVect::TheUnitVector(); 

        const Real cur_crse_pres_time  = state[Press_Type].curTime();
        const Real prev_crse_pres_time = state[Press_Type].prevTime();

        bool first_crse_step_after_initial_iters =
         (prev_crse_pres_time > state[State_Type].prevTime());

        for (int lev = level+1; lev <= finest_level; lev++)
        {
            ratio                 *= parent->refRatio(lev-1);
            NavierStokes& fine_lev = getLevel(lev);
            MultiFab&     P_new    = fine_lev.get_new_data(Press_Type);
            MultiFab&     P_old    = fine_lev.get_old_data(Press_Type);
            MultiFab&     U_new    = fine_lev.get_new_data(State_Type);
            
            SyncInterp(*Vsync, level, U_new, lev, ratio,
                       0, 0, BL_SPACEDIM, 1 , dt, sync_bc.dataPtr());
            SyncProjInterp(phi, level, P_new, P_old, lev, ratio,
                           first_crse_step_after_initial_iters,
                           cur_crse_pres_time, prev_crse_pres_time);
        }

        if (state[Press_Type].descriptor()->timeType() == StateDescriptor::Point)
            calcDpdt();
    }
}

//
// The Mac Sync correction function
//

void
NavierStokes::mac_sync ()
{
    const int  numscal        = NUM_STATE - BL_SPACEDIM;
    const Real prev_time      = state[State_Type].prevTime();
    const Real prev_pres_time = state[Press_Type].prevTime();
    const Real dt             = parent->dtLevel(level);
    MultiFab*  DeltaSsync     = 0;// hold (Delta rho)*q for conserved quantities
    MultiFab*  Rh             = get_rho_half_time();

    sync_setup(DeltaSsync);
    //
    // Compute the u_mac for the correction.
    //
    mac_projector->mac_sync_solve(level,dt,Rh,fine_ratio);
    //
    // Update coarse grid state by adding correction from mac_sync solve
    // the correction is the advective tendency of the new velocities.
    //
    if (do_reflux)
    {
        MultiFab& S_new = get_new_data(State_Type);
        mac_projector->mac_sync_compute(level,u_mac,Vsync,Ssync,Rh,
                                        level > 0 ? &getAdvFluxReg(level) : 0,
                                        advectionType, prev_time,
                                        prev_pres_time,dt,
                                        NUM_STATE,be_cn_theta, 
                                        modify_reflux_normal_vel,
                                        do_mom_diff);
        //
        // The following used to be done in mac_sync_compute.  Ssync is
        // the source for a rate of change to S over the time step, so
        // Ssync*dt is the source to the actual sync amount.
        //
        Ssync->mult(dt,Ssync->nGrow());
        //
        // For all conservative variables Q (other than density)
        // express Q as rho*q and increment sync by -(sync_for_rho)*q
        // (See Pember, et. al., LBNL-41339, Jan. 1989)
        //
        FArrayBox delta_ssync;

        int iconserved = -1;
        for (int istate = BL_SPACEDIM; istate < NUM_STATE; istate++)
        {
            if (istate != Density && advectionType[istate] == Conservative)
            {
                iconserved++;

                for (MFIter Smfi(S_new); Smfi.isValid(); ++Smfi)
                {
                    const int  i   = Smfi.index();
                    const Box& grd = grids[i];

                    delta_ssync.resize(grd,1);
                    delta_ssync.copy(S_new[Smfi], grd, istate, grd, 0, 1);
                    delta_ssync.divide(S_new[Smfi], grd, Density, 0, 1);
                    delta_ssync.mult((*Ssync)[Smfi],grd,Density-BL_SPACEDIM,0,1);
                    (*DeltaSsync)[Smfi].copy(delta_ssync,grd,0,grd,iconserved,1);
                    (*Ssync)[Smfi].minus(delta_ssync,grd,0,istate-BL_SPACEDIM,1);
                }
            }
        }

        delta_ssync.clear();

        if (do_mom_diff == 1)
        {
            for (MFIter Vsyncmfi(*Vsync); Vsyncmfi.isValid(); ++Vsyncmfi)
            {
                const int i = Vsyncmfi.index();

                D_TERM((*Vsync)[i].divide((*rho_ctime)[i],rho_ctime->box(i),0,Xvel,1);,
                       (*Vsync)[i].divide((*rho_ctime)[i],rho_ctime->box(i),0,Yvel,1);,
                       (*Vsync)[i].divide((*rho_ctime)[i],rho_ctime->box(i),0,Zvel,1););
            }
        }
        //
        // Compute viscous sync.
        //
        if (is_diffusive[Xvel])
        {
            int rho_flag = (do_mom_diff == 0) ? 1 : 3;

            MultiFab** loc_viscn = 0;

            if (variable_vel_visc)
            {
                Real viscTime = state[State_Type].prevTime();
                diffusion->allocFluxBoxesLevel(loc_viscn, 0, 1);
                getViscosity(loc_viscn, viscTime);
            }

            diffusion->diffuse_Vsync(Vsync,dt,be_cn_theta,Rh,rho_flag,loc_viscn);

            if (variable_vel_visc)
            {
                diffusion->removeFluxBoxesLevel(loc_viscn);
            }
        }

        MultiFab** fluxSC        = 0;
        bool       any_diffusive = false;
        for (int sigma  = 0; sigma < numscal; sigma++)
            if (is_diffusive[BL_SPACEDIM+sigma])
                any_diffusive = true;

        if (any_diffusive)
            diffusion->allocFluxBoxesLevel(fluxSC,0,1);

        for (int sigma = 0; sigma<numscal; sigma++)
        {
            const int state_ind = BL_SPACEDIM + sigma;
            const int rho_flag  = Diffusion::set_rho_flag(diffusionType[state_ind]);

            if (is_diffusive[state_ind])
            {
                MultiFab** cmp_diffn=0;

                if (variable_scal_diff)
                {
                    Real diffTime = state[State_Type].prevTime();
                    diffusion->allocFluxBoxesLevel(cmp_diffn, 0, 1);
                    getDiffusivity(cmp_diffn, diffTime, BL_SPACEDIM+sigma,0,1);
                }

                diffusion->diffuse_Ssync(Ssync,sigma,dt,be_cn_theta,
                                         Rh,rho_flag,fluxSC,0,cmp_diffn);

                if (variable_scal_diff)
                {
                    diffusion->removeFluxBoxesLevel(cmp_diffn);
                }
                //
                // Increment the viscous flux registers
                //
                if (level > 0)
                {
                    for (int d = 0; d < BL_SPACEDIM; d++)
                    {
                        Real mult = dt;
                        MultiFab& fluxSCd = *fluxSC[d];
                        for (MFIter fmfi(fluxSCd); fmfi.isValid(); ++fmfi)
                            getViscFluxReg().FineAdd(fluxSCd[fmfi],d,
                                                     fmfi.index(),
                                                     0,state_ind,1,mult);
                    }
                }
            }
        }

        if (any_diffusive)
            diffusion->removeFluxBoxesLevel(fluxSC);
        //
        // For all conservative variables Q (other than density)
        // increment sync by (sync_for_rho)*q_presync.
        // (See Pember, et. al., LBNL-41339, Jan. 1989)
        //
        iconserved = -1;
        for (int istate = BL_SPACEDIM; istate < NUM_STATE; istate++)
        {
            if (istate != Density && advectionType[istate] == Conservative)
            {
                iconserved++;

                for (MFIter SsyncMfi(*Ssync); SsyncMfi.isValid(); ++SsyncMfi)
                {
                    const int i = SsyncMfi.index();
                    (*Ssync)[i].plus((*DeltaSsync)[i], grids[i],
                                     iconserved, istate-BL_SPACEDIM, 1);
                }
            }
        }
        //
        // Add the sync correction to the state.
        //
        for (int sigma  = 0; sigma < numscal; sigma++)
        {
            for (MFIter S_newmfi(S_new); S_newmfi.isValid(); ++S_newmfi)
            {
                S_new[S_newmfi].plus((*Ssync)[S_newmfi],S_newmfi.validbox(),
                                     sigma,BL_SPACEDIM+sigma,1);
            }
        }
        //
        // Update rho_ctime after rho is updated with Ssync.
        //
        make_rho_curr_time();
        if (level > 0) incrRhoAvg((*Ssync),Density-BL_SPACEDIM,1.0);
        //
        // Get boundary conditions.
        //
        Array<int*>         sync_bc(grids.size());
        Array< Array<int> > sync_bc_array(grids.size());

        for (int i = 0; i < grids.size(); i++)
        {
            sync_bc_array[i] = getBCArray(State_Type,i,Density,numscal);
            sync_bc[i]       = sync_bc_array[i].dataPtr();
        }
        //
        // Interpolate the sync correction to the finer levels,
        //  and update rho_ctime, rhoAvg at those levels.
        //
        IntVect    ratio = IntVect::TheUnitVector();
        const Real mult  = 1.0;
        for (int lev = level+1; lev <= parent->finestLevel(); lev++)
        {
            ratio                     *= parent->refRatio(lev-1);
            NavierStokes&     fine_lev = getLevel(lev);
            const BoxArray& fine_grids = fine_lev.boxArray();
            MultiFab sync_incr(fine_grids,numscal,0);
            sync_incr.setVal(0.0);

            SyncInterp(*Ssync,level,sync_incr,lev,ratio,0,0,
                       numscal,1,mult,sync_bc.dataPtr());

            MultiFab& S_new = fine_lev.get_new_data(State_Type);
            for (MFIter mfi(S_new); mfi.isValid(); ++mfi)
                S_new[mfi].plus(sync_incr[mfi],fine_grids[mfi.index()],0,Density,numscal);

            fine_lev.make_rho_curr_time();
            fine_lev.incrRhoAvg(sync_incr,Density-BL_SPACEDIM,1.0);
        }
    }

    sync_cleanup(DeltaSsync);
}

void
NavierStokes::sync_setup (MultiFab*& DeltaSsync)
{
    BL_ASSERT(DeltaSsync == 0);

    int nconserved = Godunov::how_many(advectionType, Conservative,
                                       BL_SPACEDIM, NUM_STATE-BL_SPACEDIM);

    if (nconserved > 0 && level < parent->finestLevel())
    {
        DeltaSsync = new MultiFab(grids, nconserved, 1, Fab_allocate);
        DeltaSsync->setVal(0,1);
    }
}

void
NavierStokes::sync_cleanup (MultiFab*& DeltaSsync)
{
    delete DeltaSsync;

    DeltaSsync = 0;
}


//
// The reflux function
//

void
NavierStokes::reflux ()
{
    if (level == parent->finestLevel())
        return;

    BL_ASSERT(do_reflux);
    //
    // First do refluxing step.
    //
    FluxRegister& fr_adv  = getAdvFluxReg(level+1);
    FluxRegister& fr_visc = getViscFluxReg(level+1);
    Real          dt_crse = parent->dtLevel(level);
    Real          scale   = 1.0/dt_crse;
    //
    // It is important, for do_mom_diff == 0, to do the viscous
    //   refluxing first, since this will be divided by rho_half
    //   before the advective refluxing is added.  In the case of
    //   do_mom_diff == 1, both components of the refluxing will
    //   be divided by rho^(n+1) in level_sync.
    //
    MultiFab volume;
    geom.GetVolume(volume,grids,GEOM_GROW);

    fr_visc.Reflux(*Vsync,volume,scale,0,0,BL_SPACEDIM,geom);
    fr_visc.Reflux(*Ssync,volume,scale,BL_SPACEDIM,0,NUM_STATE-BL_SPACEDIM,geom);

    const MultiFab* Rh = get_rho_half_time();

    if (do_mom_diff == 0)
    {
        for (MFIter Vsyncmfi(*Vsync); Vsyncmfi.isValid(); ++Vsyncmfi)
        {
            const int i = Vsyncmfi.index();

            D_TERM((*Vsync)[i].divide((*Rh)[i],Rh->box(i),0,Xvel,1);,
                   (*Vsync)[i].divide((*Rh)[i],Rh->box(i),0,Yvel,1);,
                   (*Vsync)[i].divide((*Rh)[i],Rh->box(i),0,Zvel,1););
        }
    }

    for (int istate = BL_SPACEDIM; istate < NUM_STATE; istate++)
    {
        if (advectionType[istate] == NonConservative)
        {
            const int sigma = istate -  BL_SPACEDIM;

            for (MFIter Ssyncmfi(*Ssync); Ssyncmfi.isValid(); ++Ssyncmfi)
            {
                const int i = Ssyncmfi.index();

                (*Ssync)[i].divide((*Rh)[i],Rh->box(i),0,sigma,1);
            }
        }
    }

    fr_adv.Reflux(*Vsync,volume,scale,0,0,BL_SPACEDIM,geom);
    fr_adv.Reflux(*Ssync,volume,scale,BL_SPACEDIM,0,NUM_STATE-BL_SPACEDIM,geom);

    const BoxArray& fine_boxes = getLevel(level+1).boxArray();
    //
    // Zero out coarse grid cells which underlie fine grid cells.
    //
    BoxArray baf = fine_boxes;

    baf.coarsen(fine_ratio);

    for (MFIter Vsyncmfi(*Vsync); Vsyncmfi.isValid(); ++Vsyncmfi)
    {
        const int i = Vsyncmfi.index();

        BL_ASSERT(grids[i] == Vsyncmfi.validbox());

        std::vector< std::pair<int,Box> > isects = baf.intersections(Vsyncmfi.validbox());

        for (int ii = 0, N = isects.size(); ii < N; ii++)
        {
            (*Vsync)[i].setVal(0,isects[ii].second,0,BL_SPACEDIM);
            (*Ssync)[i].setVal(0,isects[ii].second,0,NUM_STATE-BL_SPACEDIM);
        }
    }
}

//
// Average down a single state component.
//

void
NavierStokes::avgDown (int comp)
{
    if (level == parent->finestLevel())
        return;

    NavierStokes&   fine_lev = getLevel(level+1);
    const BoxArray& fgrids   = fine_lev.grids;
    MultiFab&       S_crse   = get_new_data(State_Type);
    MultiFab&       S_fine   = fine_lev.get_new_data(State_Type);

    MultiFab fvolume, volume;
    geom.GetVolume(volume, grids,GEOM_GROW);
    fine_lev.geom.GetVolume(fvolume,fine_lev.grids,GEOM_GROW);

    avgDown(grids,fgrids,S_crse,S_fine,volume,fvolume,level,level+1,comp,1,fine_ratio);

    if (comp == Density) 
    {
        //
        // Fill rho_ctime at current and finer levels with the correct data.
        //
        for (int lev = level; lev <= parent->finestLevel(); lev++)
        {
            getLevel(lev).make_rho_curr_time();
        }
    }
}

//
// Inject fine pressure nodes down onto coarse nodes.
//

void
NavierStokes::injectDown (const Box&       ovlp,
                          FArrayBox&       Pcrse,
                          const FArrayBox& Pfine,
                          IntVect&         fine_ratio )
{
    const int*  ovlo  = ovlp.loVect();
    const int*  ovhi  = ovlp.hiVect();
    Real*       cpres = Pcrse.dataPtr();
    const int*  clo   = Pcrse.loVect();
    const int*  chi   = Pcrse.hiVect();
    const Real* fpres = Pfine.dataPtr();
    const int*  flo   = Pfine.loVect();
    const int*  fhi   = Pfine.hiVect();

    FORT_PUTDOWN(cpres,ARLIM(clo),ARLIM(chi),
                 fpres,ARLIM(flo),ARLIM(fhi),
                 ovlo,ovhi,fine_ratio.getVect());
}

//
// Average fine information from the complete set of state types to coarse.
//

void
NavierStokes::avgDown ()
{
    if (level == parent->finestLevel())
        return;

    NavierStokes&   fine_lev = getLevel(level+1);
    const BoxArray& fgrids   = fine_lev.grids;

    MultiFab fvolume, volume;
    geom.GetVolume(volume, grids,GEOM_GROW);
    fine_lev.geom.GetVolume(fvolume,fine_lev.grids,GEOM_GROW);
    //
    // Average down the states at the new time.
    //
    MultiFab& S_crse = get_new_data(State_Type);
    MultiFab& S_fine = fine_lev.get_new_data(State_Type);

    avgDown(grids,fgrids,S_crse,S_fine,volume,fvolume,level,level+1,0,S_crse.nComp(), fine_ratio);
    //
    // Now average down pressure over time n-(n+1) interval.
    //
    MultiFab&       P_crse      = get_new_data(Press_Type);
    MultiFab&       P_fine_init = fine_lev.get_new_data(Press_Type);
    MultiFab&       P_fine_avg  = *fine_lev.p_avg;
    MultiFab&       P_fine      = initial_step ? P_fine_init : P_fine_avg;
    const BoxArray& P_fgrids    = fine_lev.state[Press_Type].boxArray();

    BoxArray crse_P_fine_BA(P_fgrids.size());

    for (int i = 0; i < P_fgrids.size(); ++i)
    {
        crse_P_fine_BA.set(i,BoxLib::coarsen(P_fgrids[i],fine_ratio));
    }
    MultiFab crse_P_fine(crse_P_fine_BA,1,0);

    for (MFIter mfi(P_fine); mfi.isValid(); ++mfi)
    {
        const int i = mfi.index();

        injectDown(crse_P_fine_BA[i],crse_P_fine[i],P_fine[i],fine_ratio);
    }
    P_crse.copy(crse_P_fine);

    crse_P_fine.clear();
    //
    // Next average down divu and dSdT at new time.
    //
    if (have_divu)
    {
        MultiFab& Divu_crse = get_new_data(Divu_Type);
        MultiFab& Divu_fine = fine_lev.get_new_data(Divu_Type);
        
        avgDown(grids,fgrids,Divu_crse,Divu_fine,volume,fvolume,level,level+1,0,1,fine_ratio);
    }
    if (have_dsdt)
    {
        MultiFab& Dsdt_crse = get_new_data(Dsdt_Type);
        MultiFab& Dsdt_fine = fine_lev.get_new_data(Dsdt_Type);
        
        avgDown(grids,fgrids,Dsdt_crse,Dsdt_fine,volume,fvolume,level,level+1,0,1,fine_ratio);
    }
    //
    // Fill rho_ctime at the current and finer levels with the correct data.
    //
    for (int lev = level; lev <= parent->finestLevel(); lev++)
    {
        getLevel(lev).make_rho_curr_time();
    }
}

//
// ACCESS FUNCTIONS FOLLOW
//

//
// Virtual access function for getting the advective flux out of the
// advection routines for diagnostics and refluxing.
//

void
NavierStokes::pullFluxes (int        i,
                          int        start_ind,
                          int        ncomp,
                          FArrayBox& xflux,
                          FArrayBox& yflux,
                          FArrayBox& zflux,
                          Real       dt)
{
    //
    // Add fluxes into the refluxing counters.
    //
    if (do_reflux)
    {
        if (level < parent->finestLevel())
        {
            FluxRegister& fr = getAdvFluxReg(level+1);
            fr.CrseInit(xflux,xflux.box(),0,0,start_ind,ncomp,-dt);
            fr.CrseInit(yflux,yflux.box(),1,0,start_ind,ncomp,-dt);
#if (BL_SPACEDIM == 3)                              
            fr.CrseInit(zflux,zflux.box(),2,0,start_ind,ncomp,-dt);
#endif
        }
        if (level > 0)
        {
            advflux_reg->FineAdd(xflux,0,i,0,start_ind,ncomp,dt);
            advflux_reg->FineAdd(yflux,1,i,0,start_ind,ncomp,dt);
#if (BL_SPACEDIM == 3)                                
            advflux_reg->FineAdd(zflux,2,i,0,start_ind,ncomp,dt);
#endif
        }
    }
}

//
// Virtual access function for getting the forcing terms for the
// velocities and scalars.  The base version computes a buoyancy.
//
// As NavierStokes is currently implemented.  Velocities are integrated
// according to the equation
//
//     ui_t + uj ui_j = S_ui        ===> tforces = rho S_ui
//
// and scalars psi where (psi = rho q) as
//
//     psi_t + (uj psi)_j = S_psi   ===> tforces = S_psi = rho S_q
//
// q is a concentration.  This function returns a rho weighted
// source term, which requires a division by rho in the predict_velocity
// and velocity_advection routines.
//
#ifdef GENGETFORCE
void
NavierStokes::getForce (FArrayBox&       force,
                        int              gridno,
                        int              ngrow,
                        int              scomp,
                        int              ncomp,
                        const Real       time,
                        const FArrayBox& Rho)
{
    BL_ASSERT(Rho.nComp() == 1);

    force.resize(BoxLib::grow(grids[gridno],ngrow),ncomp);

    BL_ASSERT(Rho.box().contains(force.box()));

    RealBox     gridloc = RealBox(grids[gridno],geom.CellSize(),geom.ProbLo());
    const Real* dx      = geom.CellSize();
    const Real  grav    = gravity;
    const int*  s_lo    = Rho.loVect();
    const int*  s_hi    = Rho.hiVect();
    const int*  f_lo    = force.loVect();
    const int*  f_hi    = force.hiVect();

    FORT_MAKEFORCE (&time,
		    force.dataPtr(),
		    Rho.dataPtr(),
		    ARLIM(f_lo), ARLIM(f_hi),
		    ARLIM(s_lo), ARLIM(s_hi),
		    dx,
		    gridloc.lo(),
		    gridloc.hi(),
		    &grav,&scomp,&ncomp);
}
#else
#ifdef MOREGENGETFORCE
void
NavierStokes::getForce (FArrayBox&       force,
                        int              gridno,
                        int              ngrow,
                        int              scomp,
                        int              ncomp,
                        const Real       time,
			const FArrayBox& Vel,
                        const FArrayBox& Scal,
			int              scalScomp)
{
    if (ParallelDescriptor::IOProcessor() && getForceVerbose) {
	std::cout << "NavierStokes::getForce(): Entered..." << std::endl 
		  << "time      = " << time << std::endl
		  << "scomp     = " << scomp << std::endl
		  << "ncomp     = " << ncomp << std::endl
		  << "ngrow     = " << ngrow << std::endl
		  << "scalScomp = " << scalScomp << std::endl;

	if (scomp==0)
	    if  (ncomp==3) std::cout << "Doing velocities only" << std::endl;
	    else           std::cout << "Doing all components" << std::endl;
	else if (scomp==3)
	    if  (ncomp==1) std::cout << "Doing density only" << std::endl;
	    else           std::cout << "Doing all scalars" << std::endl;
	else if (scomp==4) std::cout << "Doing tracer only" << std::endl;
	else               std::cout << "Doing individual scalar" << std::endl;

    }

    force.resize(BoxLib::grow(grids[gridno],ngrow),ncomp);

    const Real* VelDataPtr  = Vel.dataPtr();
    const Real* ScalDataPtr = Scal.dataPtr(scalScomp);

    const Real* dx       = geom.CellSize();
    const Real  grav     = gravity;
    const int*  f_lo     = force.loVect();
    const int*  f_hi     = force.hiVect();
    const int*  v_lo     = Vel.loVect();
    const int*  v_hi     = Vel.hiVect();
    const int*  s_lo     = Scal.loVect();
    const int*  s_hi     = Scal.hiVect();
    const int   nscal    = NUM_SCALARS;

    if (ParallelDescriptor::IOProcessor() && getForceVerbose) {
#if (BL_SPACEDIM == 3)
	std::cout << "NavierStokes::getForce(): Force Domain:" << std::endl;
	std::cout << "(" << f_lo[0] << "," << f_lo[1] << "," << f_lo[2] << ") - "
		  << "(" << f_hi[0] << "," << f_hi[1] << "," << f_hi[2] << ")" << std::endl;
	std::cout << "NavierStokes::getForce(): Vel Domain:" << std::endl;
	std::cout << "(" << v_lo[0] << "," << v_lo[1] << "," << v_lo[2] << ") - "
		  << "(" << v_hi[0] << "," << v_hi[1] << "," << v_hi[2] << ")" << std::endl;
	std::cout << "NavierStokes::getForce(): Scal Domain:" << std::endl;
	std::cout << "(" << s_lo[0] << "," << s_lo[1] << "," << s_lo[2] << ") - "
		  << "(" << s_hi[0] << "," << s_hi[1] << "," << s_hi[2] << ")" << std::endl;
#else
	std::cout << "NavierStokes::getForce(): Force Domain:" << std::endl;
	std::cout << "(" << f_lo[0] << "," << f_lo[1] << ") - "
		  << "(" << f_hi[0] << "," << f_hi[1] << ")" << std::endl;
	std::cout << "NavierStokes::getForce(): Vel Domain:" << std::endl;
	std::cout << "(" << v_lo[0] << "," << v_lo[1] << ") - "
		  << "(" << v_hi[0] << "," << v_hi[1] << ")" << std::endl;
	std::cout << "NavierStokes::getForce(): Scal Domain:" << std::endl;
	std::cout << "(" << s_lo[0] << "," << s_lo[1] << ") - "
		  << "(" << s_hi[0] << "," << s_hi[1] << ")" << std::endl;
#endif

	Array<Real> velmin(BL_SPACEDIM), velmax(BL_SPACEDIM);
	Array<Real> scalmin(NUM_SCALARS), scalmax(NUM_SCALARS);
	for (int n=0; n<BL_SPACEDIM; n++) {
	    velmin[n]= 1.e234;
	    velmax[n]=-1.e234;
	}
	int ix = v_hi[0]-v_lo[0]+1;
	int jx = v_hi[1]-v_lo[1]+1;
#if (BL_SPACEDIM == 3)
	int kx = v_hi[2]-v_lo[2]+1;
	for (int k=0; k<kx; k++) {
#endif
	    for (int j=0; j<jx; j++) {
		for (int i=0; i<ix; i++) {
		    for (int n=0; n<BL_SPACEDIM; n++) {
#if (BL_SPACEDIM == 3)
			int cell = ((n*kx+k)*jx+j)*ix+i;
#else
			int cell = (n*jx+j)*ix+i;
#endif
			Real v = VelDataPtr[cell];
			if (v<velmin[n]) velmin[n] = v;
			if (v>velmax[n]) velmax[n] = v;
		    }
		}
	    }
#if (BL_SPACEDIM == 3)
	}
#endif
	for (int n=0; n<BL_SPACEDIM; n++) 
	    std::cout << "Vel  " << n << " min/max " << velmin[n] << " / " << velmax[n] << std::endl;
	
	for (int n=0; n<NUM_SCALARS; n++) {
	    scalmin[n]= 1.e234;
	    scalmax[n]=-1.e234;
	}
	ix = s_hi[0]-s_lo[0]+1;
	jx = s_hi[1]-s_lo[1]+1;
#if (BL_SPACEDIM == 3)
	kx = s_hi[2]-s_lo[2]+1;
	for (int k=0; k<kx; k++) {
#endif
	    for (int j=0; j<jx; j++) {
		for (int i=0; i<ix; i++) {
		    for (int n=0; n<NUM_SCALARS; n++) {
#if (BL_SPACEDIM == 3)
			int cell = ((n*kx+k)*jx+j)*ix+i;
#else
			int cell = (n*jx+j)*ix+i;
#endif
			Real s = ScalDataPtr[cell];
			if (s<scalmin[n]) scalmin[n] = s;
			if (s>scalmax[n]) scalmax[n] = s;
		    }
		}
	    }
#if (BL_SPACEDIM == 3)
	}
#endif
	for (int n=0; n<NUM_SCALARS; n++) 
	    std::cout << "Scal " << n << " min/max " << scalmin[n] << " / " << scalmax[n] << std::endl;
    }

    RealBox gridloc = RealBox(grids[gridno],geom.CellSize(),geom.ProbLo());
    
    // Here's the meat
    FORT_MAKEFORCE (&time,
		    force.dataPtr(),
		    VelDataPtr,
		    ScalDataPtr,
		    ARLIM(f_lo), ARLIM(f_hi),
		    ARLIM(v_lo), ARLIM(v_hi),
		    ARLIM(s_lo), ARLIM(s_hi),
		    dx,
		    gridloc.lo(),
		    gridloc.hi(),
		    &grav,&scomp,&ncomp,&nscal,&getForceVerbose);

    if (ParallelDescriptor::IOProcessor() && getForceVerbose) {
	Array<Real> forcemin(ncomp);
	Array<Real> forcemax(ncomp);
	for (int n=0; n<ncomp; n++) {
	    forcemin[n]= 1.e234;
	    forcemax[n]=-1.e234;
	}
	int ix = f_hi[0]-f_lo[0]+1;
	int jx = f_hi[1]-f_lo[1]+1;
#if (BL_SPACEDIM == 3)
	int kx = f_hi[2]-f_lo[2]+1;
	for (int k=0; k<kx; k++) {
#endif
	    for (int j=0; j<jx; j++) {
		for (int i=0; i<ix; i++) {
		    for (int n=0; n<ncomp; n++) {
#if (BL_SPACEDIM == 3)
			int cell = ((n*kx+k)*jx+j)*ix+i;
#else
			int cell = (n*jx+j)*ix+i;
#endif
			Real f = force.dataPtr()[cell];
			if (f<forcemin[n]) forcemin[n] = f;
			if (f>forcemax[n]) forcemax[n] = f;
		    }
		}
	    }
#if (BL_SPACEDIM == 3)
	}
#endif
	for (int n=0; n<ncomp; n++) 
	    std::cout << "Force " << n+scomp << " min/max " << forcemin[n] << " / " << forcemax[n] << std::endl;
	
	std::cout << "NavierStokes::getForce(): Leaving..." << std::endl << "---" << std::endl;
    }
}
#else
void
NavierStokes::getForce (FArrayBox&       force,
                        int              gridno,
                        int              ngrow,
                        int              scomp,
                        int              ncomp,
                        const FArrayBox& Rho)
{
    BL_ASSERT(Rho.nComp() == 1);

    force.resize(BoxLib::grow(grids[gridno],ngrow),ncomp);

    BL_ASSERT(Rho.box().contains(force.box()));

    const Real grav = gravity;

    for (int dc = 0; dc < ncomp; dc++)
    {
        const int sc = scomp + dc;
#if (BL_SPACEDIM == 2)
        if (sc == Yvel && std::fabs(grav) > 0.001) 
#endif
#if (BL_SPACEDIM == 3)
        if (sc == Zvel && std::fabs(grav) > 0.001) 
#endif
        {
            //
            // Set force to -rho*g.
            //
            force.copy(Rho,0,dc,1);
            force.mult(grav,dc,1);
        }
        else
        {
            force.setVal(0,dc);
        }
    }    
}
// Generalised getForce
#endif
#endif

void
NavierStokes::getGradP (MultiFab& gp,
                        Real      time)
{
    const int NGrow = gp.nGrow();
    MultiFab& P_old = get_old_data(Press_Type);

    const Real* dx             = geom.CellSize();

    if (level > 0 && state[Press_Type].descriptor()->timeType() ==
                     StateDescriptor::Point)
    {
        //
        // We want to be sure the intersection of old and new grids is
        // entirely contained within gp.boxArray()
        //
        BL_ASSERT(gp.boxArray() == grids);

        {
            //
            // Build MultiFab whose valid region encompasses NGrow grow cells.
            // The valid region of the MultiFab will contain overlaps!
            //
            const BoxArray& pBA = state[Press_Type].boxArray();

            BoxArray ovlpBA(pBA.size());

            for (int j = 0; j < ovlpBA.size(); j++)
                ovlpBA.set(j,BoxLib::grow(pBA[j],NGrow));

            MultiFab pMF(ovlpBA,1,0);

            if (time == getLevel(level-1).state[Press_Type].prevTime() || 
                time == getLevel(level-1).state[Press_Type].curTime())
            {
                FillCoarsePatch(pMF,0,time,Press_Type,0,1);
            } 
            else
            {
                Real crse_time;

                if (time > getLevel(level-1).state[State_Type].prevTime())
                {
                    crse_time = getLevel(level-1).state[Press_Type].curTime();
                }
                else
                {
                    crse_time = getLevel(level-1).state[Press_Type].prevTime();
                }
  
                FillCoarsePatch(pMF,0,crse_time,Press_Type,0,1);
  
                MultiFab dpdtMF(ovlpBA,1,0);

                FillCoarsePatch(dpdtMF,0,time,Dpdt_Type,0,1);

                Real dt_temp = time - crse_time;

                dpdtMF.mult(dt_temp);

                pMF.plus(dpdtMF,0,1,0);
            }

            for (MFIter mfi(pMF); mfi.isValid(); ++mfi) 
            {
                Projection::getGradP(pMF[mfi],gp[mfi],gp[mfi].box(),dx);
            }
        }
        //
        // We've now got good coarse data everywhere in gp.
        // FillPatch temp version of gp having overlapping valid regions.
        //
        BoxArray ovlpBA(gp.boxArray().size());

        for (int j = 0; j < gp.boxArray().size(); j++)
            ovlpBA.set(j,BoxLib::grow(gp.boxArray()[j],NGrow));

        MultiFab gpTmp(ovlpBA,gp.nComp(),0);

        for (FillPatchIterator P_fpi(*this,P_old,NGrow,time,Press_Type,0,1);
             P_fpi.isValid();
             ++P_fpi) 
        {
            Projection::getGradP(P_fpi(),gpTmp[P_fpi],gpTmp[P_fpi].box(),dx);
        }
        //
        // Now must decide which parts of gpTmp to copy to gp.
        //
        BoxArray fineBA(old_intersect_new.size());

        for (int j = 0; j < old_intersect_new.size(); j++)
        {
            Box bx = old_intersect_new[j];

            for (int i = 0; i < BL_SPACEDIM; i++)
            {
                if (!geom.isPeriodic(i))
                {
                    if (bx.smallEnd(i) == geom.Domain().smallEnd(i))
                        bx.growLo(i,NGrow);
                    if (bx.bigEnd(i) == geom.Domain().bigEnd(i))
                        bx.growHi(i,NGrow);
                }
            }

            fineBA.set(j,bx);
        }

        for (MFIter mfi(gpTmp); mfi.isValid(); ++mfi) 
        {
            std::vector< std::pair<int,Box> > isects = fineBA.intersections(gpTmp[mfi].box());

            for (int ii = 0, N = isects.size(); ii < N; ii++)
            {
                gp[mfi].copy(gpTmp[mfi],isects[ii].second);
            }
        }

        geom.FillPeriodicBoundary(gp,true);
    }
    else
    {
        FillPatchIterator P_fpi(*this,P_old,NGrow,time,Press_Type,0,1);

        for ( ; P_fpi.isValid(); ++P_fpi) 
        {
            BL_ASSERT(BoxLib::grow(grids[P_fpi.index()],NGrow) == gp[P_fpi].box());

            Projection::getGradP(P_fpi(),gp[P_fpi],gp[P_fpi].box(),dx);
        }
    }
}

//
// Fill patch divU.
//

MultiFab*
NavierStokes::getDivCond (int  ngrow, 
                          Real time)
{
    MultiFab* divu = 0;

    if (!have_divu)
    {
        divu = new MultiFab(grids,1,ngrow);

        divu->setVal(0);
    }
    else
    {
        divu = getState(ngrow,Divu_Type,0,1,time);
    }

    return divu;
}

//
// Fill patch dSdt.
//

MultiFab*
NavierStokes::getDsdt (int  ngrow, 
                       Real time)
{
    MultiFab* dsdt = 0;

    if (!(have_dsdt && have_divu))
    {
        dsdt = new MultiFab(grids,1,ngrow);

        dsdt->setVal(0);
    }
    else
    {
        dsdt = getState(ngrow,Dsdt_Type,0,1,time);
    }

    return dsdt;
}


//
// Fill patch a state component.
//

MultiFab*
NavierStokes::getState (int  ngrow,
                        int  state_idx,
                        int  scomp,
                        int  ncomp, 
                        Real time)
{
    MultiFab* mf = new MultiFab(state[state_idx].boxArray(),ncomp,ngrow);

    FillPatchIterator fpi(*this,*mf,ngrow,time,state_idx,scomp,ncomp);

    for ( ; fpi.isValid(); ++fpi)
    {
        (*mf)[fpi.index()].copy(fpi());
    }

    return mf;
}

//
// Fills ghost cells of state:
//

void
NavierStokes::FillStateBndry (Real time,
                              int  state_idx,
                              int  src_comp, 
                              int  ncomp) 
{
    MultiFab& S = get_data(state_idx,time);

    if (S.nGrow() == 0)
        return;

    FillPatchIterator fpi(*this,S,S.nGrow(),time,state_idx,src_comp,ncomp);

    for ( ; fpi.isValid(); ++fpi)
    {
        //
        // Fill all ghost cells interior & exterior to valid region.
        //
        BoxList boxes = BoxLib::boxDiff(fpi().box(),grids[fpi.index()]);

        for (BoxList::iterator bli = boxes.begin(), end = boxes.end(); bli != end; ++bli)
        {
            S[fpi.index()].copy(fpi(),*bli,0,*bli,src_comp,ncomp);
        }
    }
}

//
// Default divU is set to zero.
//

void
NavierStokes::calc_divu (Real      time,
                         Real      dt,
                         MultiFab& divu)
{
    if (have_divu)
    {
        divu.setVal(0);

        if (do_temp && visc_coef[Temp] > 0.0)
        {
            //
            // Compute Div(U) = Div(visc_cond_coef * Grad(T))/(c_p*rho*T)
            //
            getViscTerms(divu,Temp,1,time);

            const MultiFab&   rhotime = get_rho(time);
            FillPatchIterator temp_fpi(*this,divu,0,time,State_Type,Temp,1);
            MFIter            rho_mfi(rhotime);

            for ( ;
                  rho_mfi.isValid() && temp_fpi.isValid();
                  ++rho_mfi, ++temp_fpi)
            {
                const int i = rho_mfi.index();

                divu[i].divide(rhotime[i],divu.box(i),0,0,1);
                divu[i].divide(temp_fpi(),divu.box(i),0,0,1);
            }
            Real THERMO_cp_inv = 1.0 / 1004.6;
            divu.mult(THERMO_cp_inv);
        }
    }
}

//
// Default dSdt is set to zero.
//

void
NavierStokes::calc_dsdt (Real      time,
                         Real      dt,
                         MultiFab& dsdt)
{
    if (have_divu && have_dsdt)
    {
        dsdt.setVal(0);

        if (do_temp)
        {
            MultiFab& Divu_new = get_new_data(Divu_Type);
            MultiFab& Divu_old = get_old_data(Divu_Type);

            for (MFIter mfi(dsdt); mfi.isValid(); ++mfi)
            {
                dsdt[mfi].copy(Divu_new[mfi],mfi.validbox(),0,mfi.validbox(),0,1);
                dsdt[mfi].minus(Divu_old[mfi],mfi.validbox(),0,0,1);
                dsdt[mfi].divide(dt,mfi.validbox(),0,1);
            }
        }
    }
}

void
NavierStokes::getViscTerms (MultiFab& visc_terms,
                            int       src_comp, 
                            int       ncomp,
                            Real      time)
{
    //
    // The logic below for selecting between scalar or tensor solves does 
    // not allow for calling NavierStokes::getViscTerms with src_comp=Yvel
    // or Zvel
    //
#ifndef NDEBUG
    if (src_comp<BL_SPACEDIM && (src_comp!=Xvel || ncomp<BL_SPACEDIM))
    {
        std::cout << "src_comp=" << src_comp << "   ncomp=" << ncomp << std::endl;
        BoxLib::Error("must call NavierStokes::getViscTerms with all three velocity components");
    }
#endif
    // 
    // Initialize all viscous terms to zero
    //
    const int nGrow = visc_terms.nGrow();
    visc_terms.setVal(0,0,ncomp,nGrow);
    //
    // 
    // Get Velocity Viscous Terms
    //
    if (src_comp == Xvel && is_diffusive[Xvel])
    {
        MultiFab** viscosity;

        if (variable_vel_visc)
        {
            diffusion->allocFluxBoxesLevel(viscosity, 0, 1);
            getViscosity(viscosity, time);

            diffusion->getTensorViscTerms(visc_terms,time,0,viscosity);
        }
        else
        {
            for (int icomp = Xvel; icomp < BL_SPACEDIM; icomp++)
            {
                int rho_flag = Diffusion::set_rho_flag(diffusionType[icomp]);

                diffusion->getViscTerms(visc_terms,src_comp,icomp,time,rho_flag);
            }
        }
        //
        // Add Div(u) term if desired, if this is velocity, and if Div(u) 
        // is nonzero.  If const-visc, term is mu.Div(u)/3, else 
        // it's -Div(mu.Div(u).I)*2/3
        //
        if (have_divu && S_in_vel_diffusion)
        {
            MultiFab divmusi(grids,BL_SPACEDIM,1);

            if (variable_vel_visc)
            {
                diffusion->compute_divmusi(time,viscosity,divmusi);
                divmusi.mult((-2./3.),0,BL_SPACEDIM,0);
            }
            else
            {
                diffusion->compute_divmusi(time,visc_coef[Xvel],divmusi);
                divmusi.mult((1./3.),0,BL_SPACEDIM,0);
            }

            visc_terms.plus(divmusi,Xvel,BL_SPACEDIM,0);
        }
        //
        // Clean up variable viscosity arrays
        //
        if (variable_vel_visc)
            diffusion->removeFluxBoxesLevel(viscosity);
    }
    //
    // Get Scalar Diffusive Terms
    //
    const int first_scal = (src_comp==Xvel) ? BL_SPACEDIM : src_comp;
    const int num_scal   = (src_comp==Xvel) ? ncomp-BL_SPACEDIM : ncomp;

    if (num_scal > 0)
    {
        for (int icomp = first_scal; icomp < first_scal+num_scal; icomp++)
        {
            if (is_diffusive[icomp])
            {
                int rho_flag = Diffusion::set_rho_flag(diffusionType[icomp]);

                MultiFab** cmp_diffn = 0;

                if (variable_scal_diff)
                {
                    diffusion->allocFluxBoxesLevel(cmp_diffn, 0, 1);
                    getDiffusivity(cmp_diffn, time, icomp, 0, 1);
                }

                diffusion->getViscTerms(visc_terms,src_comp,icomp,
                                        time,rho_flag,0,cmp_diffn);

                if (variable_scal_diff)
                {
                    diffusion->removeFluxBoxesLevel(cmp_diffn);
                }
            }
        }
    }
    //
    // Ensure consistent grow cells
    //    
    if (nGrow > 0)
    {
        const int N = visc_terms.IndexMap().size();

#ifdef BL_USE_OMP
#pragma omp parallel for
#endif
        for (int i = 0; i < N; i++)
        {
            const int  k   = visc_terms.IndexMap()[i];
            FArrayBox& vt  = visc_terms[k];
            const Box& box = visc_terms.box(k);
            FORT_VISCEXTRAP(vt.dataPtr(),ARLIM(vt.loVect()),ARLIM(vt.hiVect()),
                            box.loVect(),box.hiVect(),&ncomp);
        }
        visc_terms.FillBoundary(0,ncomp);
        //
        // Note: this is a special periodic fill in that we want to
        // preserve the extrapolated grow values when periodic --
        // usually we preserve only valid data.  The scheme relies on
        // the fact that there is good data in the "non-periodic" grow cells.
        // ("good" data produced via VISCEXTRAP above)
        //
        geom.FillPeriodicBoundary(visc_terms,0,ncomp,true);
    }
}

//
// Functions for calculating the variable viscosity and diffusivity.
// These default to setting the variable viscosity and diffusivity arrays
// to the values in visc_coef and diff_coef.  These functions would
// need to be replaced in any class derived from NavierStokes that
// wants variable coefficients.
//
void 
NavierStokes::calcViscosity (const Real time, 
                             const Real dt,
                             const int  iteration,
                             const int  ncycle)
{
    //
    // Select time level to work with (N or N+1)
    //
    MultiFab* visc_cc = 0;

    const TimeLevel whichTime = which_time(State_Type,time);

    BL_ASSERT(whichTime == AmrOldTime || whichTime == AmrNewTime);

    if (whichTime == AmrOldTime)               // time N
    {
        visc_cc = viscn_cc;
    }
    else if (whichTime == AmrNewTime)          // time N+1
    {
        visc_cc = viscnp1_cc;
    }
    //
    // Calculate viscosity
    //
    const int nGrow = visc_cc->nGrow();

    if (is_diffusive[Xvel]) 
    {
        if (visc_coef[Xvel] >= 0.0)
        {
            visc_cc->setVal(visc_coef[Xvel], 0, 1, nGrow);
        }
        else
        {
            BoxLib::Abort("NavierStokes::calcViscosity() : must have velocity visc_coef >= 0.0");
        }
    }
}

void 
NavierStokes::calcDiffusivity (const Real time, 
                               const Real dt,
                               const int  iteration,
                               const int  ncycle,
                               const int  src_comp, 
                               const int  ncomp)
{
    //
    // NOTE:  The component numbers passed into NavierStokes::calcDiffusivity
    //        correspond to the components in the state.  In the diffusivity 
    //        arrays, there is an offset since no diffusivity array
    //        is kept for the velocities or the density.  So, the scalar
    //        component Density+1 in the state corresponds to component
    //        0 in the arrays diffn and diffnp1.
    //
    BL_ASSERT(src_comp > Density);
    //
    // Select time level to work with (N or N+1)
    //
    MultiFab* diff_cc = 0;

    const TimeLevel whichTime = which_time(State_Type,time);

    BL_ASSERT(whichTime == AmrOldTime || whichTime == AmrNewTime);

    if (whichTime == AmrOldTime)               // time N
    {
        diff_cc = diffn_cc;
    }
    else if (whichTime == AmrNewTime)          // time N+1
    {
        diff_cc = diffnp1_cc;
    }
    //
    // Calculate diffusivity
    //
    const int nGrow = diff_cc->nGrow();

    for (int comp=src_comp; comp<src_comp+ncomp; comp++)
    {
        int diff_comp = comp - Density - 1;

        if (is_diffusive[comp])
        {
            if (visc_coef[comp] >= 0.0)
            {
                diff_cc->setVal(visc_coef[comp], diff_comp, 1, nGrow);
            }
            else
            {
                BoxLib::Abort("NavierStokes::calcDiffusivity() : must have scalar diff_coefs >= 0.0");
            }
        }
    }
}


void 
NavierStokes::getViscosity (MultiFab* viscosity[BL_SPACEDIM],
                            const Real time)
{
    //
    // Select time level to work with (N or N+1)
    //
    MultiFab* visc_cc = 0;

    const TimeLevel whichTime = which_time(State_Type,time);

    BL_ASSERT(whichTime == AmrOldTime || whichTime == AmrNewTime);

    if (whichTime == AmrOldTime)               // time N
    {
        visc_cc = viscn_cc;
    }
    else if (whichTime == AmrNewTime)          // time N+1
    {
        visc_cc = viscnp1_cc;
    }
    //
    // Fill edge-centered viscosity
    //
    for (int dir = 0; dir < BL_SPACEDIM; dir++)
    {
        for (MFIter ecMfi(*viscosity[dir]); ecMfi.isValid(); ++ecMfi)
        {
            const int i = ecMfi.index();

            center_to_edge_plain((*visc_cc)[i],(*viscosity[dir])[i],0,0,1);
        }
    }
}

void 
NavierStokes::getDiffusivity (MultiFab* diffusivity[BL_SPACEDIM],
                              const Real time,
                              const int state_comp,
                              const int dst_comp,
                              const int ncomp)
{
    BL_ASSERT(state_comp > Density);
    //
    // Pick correct diffusivity component
    //
    int diff_comp = state_comp - Density - 1;
    //
    // Select time level to work with (N or N+1)
    //
    MultiFab* diff_cc = 0;

    const TimeLevel whichTime = which_time(State_Type,time);

    BL_ASSERT(whichTime == AmrOldTime || whichTime == AmrNewTime);

    if (whichTime == AmrOldTime)               // time N
    {
        diff_cc = diffn_cc;
    }
    else if (whichTime == AmrNewTime)          // time N+1
    {
        diff_cc = diffnp1_cc;
    }
    //
    // Fill edge-centered diffusivities
    //
    for (int dir = 0; dir < BL_SPACEDIM; dir++)
    {
        for (MFIter ecMfi(*diffusivity[dir]); ecMfi.isValid(); ++ecMfi)
        {
            center_to_edge_plain((*diff_cc)[ecMfi],(*diffusivity[dir])[ecMfi],
                                 diff_comp,dst_comp,ncomp);
        }
    }
}

void
NavierStokes::center_to_edge_plain (const FArrayBox& ccfab,
                                    FArrayBox&       ecfab,
                                    int              sComp,
                                    int              dComp,
                                    int              nComp)
{
    //
    // This routine fills an edge-centered FAB from a cell-centered FAB.
    // It assumes that the data in all cells of the cell-centered FAB is
    // valid and totally ignores any concept of boundary conditions.  
    // It is assummed that the cell-centered FAB fully contains the 
    // edge-centered FAB.  If anything special needs to be done at boundaries, 
    // a varient of this routine needs to be written.  See 
    // HeatTransfer::center_to_edge_fancy().
    //
    const Box&      ccbox = ccfab.box();
    const Box&      ecbox = ecfab.box();
    const IndexType ixt   = ecbox.ixType();
    //
    // Get direction for interpolation to edges
    //
    int dir = -1;
    for (int d = 0; d < BL_SPACEDIM; d++)
        if (ixt.test(d))
            dir = d;
    //
    // Miscellanious checks
    //
    BL_ASSERT(!(ixt.cellCentered()) && !(ixt.nodeCentered()));
    BL_ASSERT(BoxLib::grow(ccbox,-BoxLib::BASISV(dir)).contains(BoxLib::enclosedCells(ecbox)));
    BL_ASSERT(sComp+nComp <= ccfab.nComp() && dComp+nComp <= ecfab.nComp());
    //
    // Shift cell-centered data to edges
    //
    Box fillBox = ccbox; 
    for (int d = 0; d < BL_SPACEDIM; d++)
        if (d != dir)
            fillBox.setRange(d, ecbox.smallEnd(d), ecbox.length(d));
    
    const int isharm = def_harm_avg_cen2edge;
    FORT_CEN2EDG(fillBox.loVect(), fillBox.hiVect(),
                 ARLIM(ccfab.loVect()), ARLIM(ccfab.hiVect()),
                 ccfab.dataPtr(sComp),
                 ARLIM(ecfab.loVect()), ARLIM(ecfab.hiVect()),
                 ecfab.dataPtr(dComp),
                 &nComp, &dir, &isharm);
}

//
// Note: this is a temporary function.  Eventually this will be moved to a
// boundary condition class.
//

static
void
getOutFlowFaces (Array<Orientation>& outFaces,
                 BCRec*              _phys_bc)
{
    outFaces.resize(0);
    for (int idir = 0; idir < BL_SPACEDIM; idir++)
    {
        if (_phys_bc->lo(idir) == Outflow)
        {
            const int len = outFaces.size();
            outFaces.resize(len+1);
            outFaces.set(len,Orientation(idir,Orientation::low));
        }

        if (_phys_bc->hi(idir) == Outflow)
        {
            const int len = outFaces.size();
            outFaces.resize(len+1);
            outFaces.set(len,Orientation(idir,Orientation::high));
        }
    }
}

void
NavierStokes::manual_tags_placement (TagBoxArray&    tags,
                                     Array<IntVect>& bf_lev)
{
    Array<Orientation> outFaces;
    getOutFlowFaces(outFaces,&phys_bc);
    if (outFaces.size()>0)
    {
        for (int i=0; i<outFaces.size(); ++i)
        {
            const Orientation& outFace = outFaces[i];
            const int oDir = outFace.coordDir();
            const Box& crse_domain = BoxLib::coarsen(geom.Domain(),bf_lev[level]);
            const int mult = (outFace.isLow() ? +1 : -1);
            if (do_refine_outflow)
            {
                //
                // Refine entire outflow boundary if new boxes within grid_tol
                // from outflow
                //
                const int grid_tol = 1;

                Box outflowBox = BoxLib::adjCell(crse_domain,outFace,grid_tol);

                outflowBox.shift(oDir,mult*grid_tol);
                //
                // Only refine if there are already tagged cells in the outflow
                // region
                //
                bool hasTags = false;
                for (MFIter tbi(tags); !hasTags && tbi.isValid(); ++tbi)
                    if (tags[tbi].numTags(outflowBox) > 0)
                        hasTags = true;
                
                if (hasTags)
                    tags.setVal(BoxArray(&outflowBox,1),TagBox::SET);
            }
            else if (do_derefine_outflow)
            {
                const int np = parent->nProper();
                //
                // Calculate the number of level 0 cells to be left uncovered
                // at the outflow.  The convoluted logic allows for the fact that
                // the number of uncovered cells must be a multiple of the level
                // blocking factor.  So, when calculating the number of coarse
                // cells below, we always round the division up.
                //
                int N_coarse_cells = Nbuf_outflow / bf_lev[0][oDir];
                if (Nbuf_outflow % bf_lev[0][oDir] != 0)
                    N_coarse_cells++;
                
                int N_level_cells = N_coarse_cells * bf_lev[0][oDir];
                
                //
                // Adjust this to get the number of cells to be left uncovered at
                // levels higher than 0
                //
                for (int i = 1; i <= level; ++i)
                {
                    /*** Calculate the minimum cells at this level ***/
                    
                    const int rat = (parent->refRatio(i-1))[oDir];
                    N_level_cells = N_level_cells * rat + np;
                    
                    /*** Calculate the required number of coarse cells ***/
                    
                    N_coarse_cells = N_level_cells / bf_lev[i][oDir];
                    if (N_level_cells % bf_lev[i][oDir] != 0)
                        N_coarse_cells++;
                    
                    /*** Calculate the corresponding number of level cells ***/
                    
                    N_level_cells = N_coarse_cells * bf_lev[i][oDir];
                }
                //
                // Untag the cells near the outflow
                //
                if (N_coarse_cells > 0)
                {
                    //
                    // Generate box at the outflow and grow it in all directions
                    // other than the outflow.  This forces outflow cells in the
                    // ghostcells in directions other that oDir to be cleared.
                    //
                    Box outflowBox = BoxLib::adjCell(crse_domain, outFace, 1);
                    for (int dir = 0; dir < BL_SPACEDIM; dir++)
                        if (dir != oDir) outflowBox.grow(dir, 1);
                    //
                    // Now, grow the box into the domain (opposite direction as
                    // outFace) the number of cells we need to clear.
                    //
                    if (outFace.isLow())
                        outflowBox.growHi(oDir, N_coarse_cells);
                    else
                        outflowBox.growLo(oDir, N_coarse_cells);
                    
                    tags.setVal(BoxArray(&outflowBox,1),TagBox::CLEAR);
                }
            }
        }
    }
}

void
NavierStokes::calcDpdt ()
{
    BL_ASSERT(state[Press_Type].descriptor()->timeType() == StateDescriptor::Point);

    MultiFab&  new_press   = get_new_data(Press_Type);
    MultiFab&  old_press   = get_old_data(Press_Type);
    MultiFab&  dpdt        = get_new_data(Dpdt_Type);
    const Real dt_for_dpdt = state[Press_Type].curTime()-state[Press_Type].prevTime();

    if (dt_for_dpdt != 0.0) 
    {
        for (MFIter mfi(dpdt); mfi.isValid(); ++mfi)
        {
            dpdt[mfi].copy(new_press[mfi],mfi.validbox(),0,mfi.validbox(),0,1);
            dpdt[mfi].minus(old_press[mfi],mfi.validbox(),0,0,1);
            dpdt[mfi].divide(dt_for_dpdt,mfi.validbox(),0,1);
        }
    }
    else
    {
        dpdt.setVal(0);
    }
}

void
NavierStokes::create_umac_grown ()
{
    if (level > 0)
    {
        BoxList bl = BoxLib::GetBndryCells(grids,1);

        BoxArray f_bnd_ba(bl);

        bl.clear();

        BoxArray c_bnd_ba = BoxArray(f_bnd_ba.size());

        for (int i = 0; i < f_bnd_ba.size(); ++i)
        {
            c_bnd_ba.set(i,Box(f_bnd_ba[i]).coarsen(crse_ratio));
            f_bnd_ba.set(i,Box(c_bnd_ba[i]).refine( crse_ratio));
        }

        for (int n = 0; n < BL_SPACEDIM; ++n)
        {
            //
            // crse_src & fine_src must have same parallel distribution.
            // We'll use the KnapSack distribution for the fine_src_ba.
            // Since fine_src_ba should contain more points, this'll lead
            // to a better distribution.
            //
            BoxArray crse_src_ba(c_bnd_ba), fine_src_ba(f_bnd_ba);

            crse_src_ba.surroundingNodes(n);
            fine_src_ba.surroundingNodes(n);

            std::vector<long> wgts;

            wgts.reserve(fine_src_ba.size());

            for (int i = 0; i < fine_src_ba.size(); i++)
                wgts.push_back(fine_src_ba[i].numPts());

            DistributionMapping dm;
            //
            // This call doesn't invoke the MinimizeCommCosts() stuff.
            // This also guarantees that these DMs won't be put into the cache.
            //
            dm.KnapSackProcessorMap(wgts,ParallelDescriptor::NProcs());

            MultiFab crse_src, fine_src;

            crse_src.define(crse_src_ba, 1, 0, dm, Fab_allocate);
            fine_src.define(fine_src_ba, 1, 0, dm, Fab_allocate);

            crse_src.setVal(1.e200);
            fine_src.setVal(1.e200);
            //
            // We want to fill crse_src from lower level u_mac including u_mac's grow cells.
            // Gotta do it in steps since parallel copy only does valid region.
            //
            {
                const MultiFab& u_macLL = getLevel(level-1).u_mac[n];

                BoxArray edge_grids = u_macLL.boxArray();
                edge_grids.grow(1);

                MultiFab u_macC(edge_grids,1,0);

                for (MFIter mfi(u_macLL); mfi.isValid(); ++mfi)
                    u_macC[mfi].copy(u_macLL[mfi]);

                crse_src.copy(u_macC);
            }

            const int Ncrse = crse_src.IndexMap().size();

#ifdef BL_USE_OMP
#pragma omp parallel for
#endif
            for (int i = 0; i < Ncrse; i++)
            {
                const int  nComp = 1;
                const int  k     = crse_src.IndexMap()[i];
                const Box& box   = crse_src[k].box();
                const int* rat   = crse_ratio.getVect();
                FORT_PC_CF_EDGE_INTERP(box.loVect(), box.hiVect(), &nComp, rat, &n,
                                       crse_src[k].dataPtr(),
                                       ARLIM(crse_src[k].loVect()),
                                       ARLIM(crse_src[k].hiVect()),
                                       fine_src[k].dataPtr(),
                                       ARLIM(fine_src[k].loVect()),
                                       ARLIM(fine_src[k].hiVect()));
            }
            crse_src.clear();
            //
            // Replace pc-interpd fine data with preferred u_mac data at
            // this level u_mac valid only on surrounding faces of valid
            // region - this op will not fill grow region.
            //
            fine_src.copy(u_mac[n]);
            //
            // Interpolate unfilled grow cells using best data from
            // surrounding faces of valid region, and pc-interpd data
            // on fine edges overlaying coarse edges.
            //
            const int Nfine = fine_src.IndexMap().size();

#ifdef BL_USE_OMP
#pragma omp parallel for
#endif
            for (int i = 0; i < Nfine; i++)
            {
                const int  nComp = 1;
                const int  k     = fine_src.IndexMap()[i];
                const Box& fbox  = fine_src[k].box();
                const int* rat   = crse_ratio.getVect();
                FORT_EDGE_INTERP(fbox.loVect(), fbox.hiVect(), &nComp, rat, &n,
                                 fine_src[k].dataPtr(),
                                 ARLIM(fine_src[k].loVect()),
                                 ARLIM(fine_src[k].hiVect()));
            }
            //
            // Make copy of of u_mac[n] covering valid+grow regions and containing no
            // grow regions itself so that we can parallel copy fine_src into it, from
            // which we can then update u_mac[n].
            //
            BoxArray edge_grids = u_mac[n].boxArray();
            edge_grids.grow(1);

            MultiFab u_macG(edge_grids,1,0);

            u_macG.copy(fine_src);
            u_macG.copy(u_mac[n]);

            for (MFIter mfi(u_macG); mfi.isValid(); ++mfi)
                u_mac[n][mfi].copy(u_macG[mfi]);
        }
    }

    for (int n = 0; n < BL_SPACEDIM; ++n)
    {
        u_mac[n].FillBoundary();
        geom.FillPeriodicBoundary(u_mac[n]);
    }
}
