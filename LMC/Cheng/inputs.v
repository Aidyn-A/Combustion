DistributionMapping.verbose=1
DistributionMapping.do_not_minimize_comm_costs=0

#ht.plot_auxDiags=1

#amr.regrid_on_restart = 1

max_step  =  6000000 
stop_time = 4.00

proj.do_outflow_bcs = 0
mac.do_outflow_bcs = 0

ns.do_fillPatchUMAC=1
ns.zeroBndryVisc=1
ns.do_OT_radiation=1
ns.do_OT_radiation=0

proj.proj_2 = 1
ns.num_divu_iters =3

ns.max_grid_size_chem = 64
ns.max_grid_size_chem = 32
ns.max_grid_size_chem = 16

ht.plot_rhoY=0
#ht.plot_molefrac=1
ht.plot_molefrac=0
ht.plot_massfrac=1
ht.plot_ydot=0
ns.use_chemeq2=0
amr.derive_plot_vars=mag_vort rhoRT

#ns.verbose_vode = 1
#amr.restart=chk0060

geometry.coord_sys = 0  # 0 => cart, 1 => RZ

geometry.prob_lo   =  -0.20 -0.20 -0.00
geometry.prob_hi   =  +0.20 +0.20 +0.40
geometry.prob_lo   =  -0.06 -0.06 -0.00
geometry.prob_hi   =  +0.06 +0.06 +0.12

mg.usecg = 1
cg.v = 1
cg.v = 0
cg.isExpert=1
mg.v = 1
mg.v = 0
cg.maxiter = 1000
mg.maxiter = 1000
mg.nu_0 = 1
mg.nu_1 = 4
mg.nu_2 = 4
mg.nu_f = 40
cg.unstable_criterion = 100
ns.htt_tempmin=1.0
ns.htt_tempmax=2.5
ns.v = 1
#mac.v = 1
proj.v = 1

mg.cg_solver=1
cg.cg_solver=1

amr.n_cell    = 16 16 16
amr.n_cell    = 32 32 32
amr.n_cell    = 64 64 64
amr.n_cell    = 192 192 128
amr.n_cell    = 192 192 192
amr.n_cell    = 96 96 96

amr.v=1
amr.max_level =  1            # maximum level number allowed
amr.max_level =  3            # maximum level number allowed
amr.max_level =  2            # maximum level number allowed
amr.ref_ratio       = 2 2 2 2 # refinement ratio
amr.regrid_int      = 2       # how often to regrid
amr.n_error_buf     = 1 1 1 1 # number of buffer cells in error est
amr.grid_eff        = 0.9     # what constitutes an efficient grid
amr.grid_eff        = 0.7     # what constitutes an efficient grid
amr.blocking_factor = 8       # block factor in grid generation
amr.check_file      = chk     # root name of checkpoint file
amr.check_int       = 1     # number of timesteps between checkpoints
amr.plot_file       = plt
amr.plot_int        = 5
amr.grid_log        = grdlog  # name of grid logging file
amr.max_grid_size   = 64
amr.max_grid_size   = 32
#amr.derive_plot_vars=ALL

amr.probin_file = probin.v # This will default to file "probin" if not set

ns.dt_cutoff       = 5.e-10   # level 0 timestep below which we halt
ns.visc_tol        = 1.0e-14  # tolerence for viscous solves
ns.visc_abs_tol    = 1.0e-14  # tolerence for viscous solves
ns.cfl            = 0.7       # cfl number for hyperbolic system
ns.init_shrink    = 0.3       # scale back initial timestep
ns.change_max     = 1.1       # scale back initial timestep
ns.vel_visc_coef   = 1.983e-5
ns.temp_cond_coef  = 2.6091e-5
ns.scal_diff_coefs = -0.01
ns.variable_vel_visc  = 1
ns.variable_scal_diff = 1
ns.init_iter      = 3        # number of init iters to def pressure
ns.gravity        = 0        # body force  (gravity in MKS units)
ns.gravity        = -9.81    # body force  (gravity in MKS units)
ns.sum_interval   = 1        # timesteps between computing mass
ns.do_reflux      = 1        # 1 => do refluxing
ns.do_mac_proj    = 1        # 1 => do MAC projection

ns.do_sync_proj   = 1        # 1 => do Sync Project
ns.do_MLsync_proj = 1
ns.do_divu_sync = 0
ns.divu_relax_factor   = 0.0

ns.be_cn_theta = 0.5
ns.S_in_vel_diffusion = 1

ns.do_temp = 1

ns.do_diffuse_sync = 1
ns.do_reflux_visc  = 1

ns.divu_ceiling = 1
ns.divu_dt_factor = .4
ns.min_rho_divu_ceiling = .01

ns.chemfile        = drm19.dat
ns.chemoutfile     = chem.out
ns.thermfile       = thermo12.dat
ns.tranfile        = transport12.dat
ns.tranfile        = tran.asc.drm19

ns.fuelName        = CH4
ns.oxidizerName    = O2
ns.flameTracName   = CH4
ns.flameTracName   = HCO
ns.unity_Le = 0

ns.dpdt_option = 0

#ns.prandtl = .70
#ns.schmidt = .70
#ns.constant_mu_val = 0.05
#ns.constant_lambda_val = 0.0714286
#ns.constant_rhoD_val = 0.0714286

ns.lo_bc          = 2 2 1
ns.hi_bc          = 2 2 2

# >>>>>>>>>>>>>  BC FLAGS <<<<<<<<<<<<<<<<
# 0 = Interior           3 = Symmetry
# 1 = Inflow             4 = SlipWall
# 2 = Outflow            5 = NoSlipWall

godunov.slope_order = 4

diffuse.use_cg_solve = 0
diffuse.max_order = 4
diffuse.tensor_max_order = 4
diffuse.use_tensor_cg_solve = 0
diffuse.v = 1
diffuse.Rhs_in_abs_tol = 1

proj.proj_tol       = 1.0e-1  # tolerence for projections
proj.proj_tol       = 1.0e-11  # tolerence for projections
proj.proj_tol       = 1.0e-9  # tolerence for projections
proj.sync_tol       = 1.0e-8  # tolerence for projections
proj.rho_wgt_vel_proj = 0      # 0 => const den proj, 1 => rho weighted
proj.Pcode          = 0
proj.filter_factor  = 0.0
proj.divu_minus_s_factor = .5
proj.divu_minus_s_factor = 0.

mac.mac_tol        = 1.0e-12  # tolerence for mac projections
mac.mac_sync_tol   = 1.0e-9   # tolerence for mac SYNC projection
mac.mac_abs_tol    = 1.0e-14
mac.use_cg_solve   = 1

fab.format = NATIVE

DistributionMapping.strategy = KNAPSACK

# ns.cdf_prefix = DebugFiles/test
#
# StationData.vars     -- Names of StateData components to output
# StationData.coord    -- BL_SPACEDIM array of Reals
# StationData.coord    -- the next one
# StationData.coord    -- ditto ...
#
# e.g.
#
#StationData.vars  = pressure
#StationData.coord = 0.001 0.001
#StationData.coord = 0.011 0.0021
#StationData.coord = 0.0005 0.005
#StationData.coord = 0.00123 0.00123
#StationData.coord = 0.0023 0.00153
#StationData.coord = 0.00234 0.00234

