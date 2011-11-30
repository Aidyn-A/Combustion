
      subroutine PROBINIT (init,name,namlen,problo,probhi)

      use probdata_module
      use eos_module, only : gamma_const
      use network   , only : network_init
      implicit none

      integer init, namlen
      integer name(namlen)
      double precision problo(2), probhi(2)

      integer untin,i

      namelist /fortin/ p_l, u_l, rho_l, p_r, u_r, rho_r, frac, idir, &
           denerr,  dengrad,  max_denerr_lev,  max_dengrad_lev, &
           velgrad,  max_velgrad_lev, &
           presserr, pressgrad,max_presserr_lev,max_pressgrad_lev

!
!     Build "probin" filename -- the name of file containing fortin namelist.
!     
      integer maxlen
      parameter (maxlen=256)
      character probin*(maxlen)

      if (namlen .gt. maxlen) then
         write(6,*) 'probin file name too long'
         stop
      end if

      do i = 1, namlen
         probin(i:i) = char(name(i))
      end do
         
! set namelist defaults

      p_l = 1.0               ! left pressure (erg/cc)
      u_l = 0.0               ! left velocity (cm/s)
      rho_l = 1.0             ! left density (g/cc)

      p_r = 0.1               ! right pressure (erg/cc)
      u_r = 0.0               ! right velocity (cm/s)
      rho_r = 0.125           ! right density (g/cc)

      idir = 1                ! direction across which to jump
      frac = 0.5              ! fraction of the domain for the interface

      denerr = 1.d20
      dengrad = 1.d20
      max_denerr_lev = -1
      max_dengrad_lev = -1

      presserr = 1.d20
      pressgrad = 1.d20
      max_presserr_lev = -1
       max_pressgrad_lev = -1

      velgrad = 1.d20
      max_velgrad_lev = -1

!     Read namelists
      untin = 9
      open(untin,file=probin(1:namlen),form='formatted',status='old')
      read(untin,fortin)
      close(unit=untin)

!     set local variable defaults -- the 'center' variables are the location of the
!     interface
      center(1) = frac*(problo(1)+probhi(1))
      center(2) = frac*(problo(2)+probhi(2))

!     compute the internal energy (erg/cc) for the left and right state
      rhoe_l = p_l/(gamma_const - 1.d0)
      rhoe_r = p_r/(gamma_const - 1.d0)

      end

! ::: -----------------------------------------------------------
! ::: This routine is called at problem setup time and is used
! ::: to initialize data on each grid.  
! ::: 
! ::: NOTE:  all arrays have one cell of ghost zones surrounding
! :::        the grid interior.  Values in these cells need not
! :::        be set here.
! ::: 
! ::: INPUTS/OUTPUTS:
! ::: 
! ::: level     => amr level of grid
! ::: time      => time at which to init data             
! ::: lo,hi     => index limits of grid interior (cell centered)
! ::: nstate    => number of state components.  You should know
! :::		   this already!
! ::: state     <=  Scalar array
! ::: delta     => cell size
! ::: xlo,xhi   => physical locations of lower left and upper
! :::              right hand corner of grid.  (does not include
! :::		   ghost region).
! ::: -----------------------------------------------------------
     subroutine ca_initdata(level,time,lo,hi,nscal, &
                            state,state_l1,state_l2,state_h1,state_h2, &
                            delta,xlo,xhi)
     use probdata_module
     use meth_params_module, only : NVAR, URHO, UMX, UMY, UEDEN, UEINT, UFS, UTEMP

     implicit none

     integer level, nscal
     integer lo(2), hi(2)
     integer state_l1,state_l2,state_h1,state_h2
     double precision xlo(2), xhi(2), time, delta(2)
     double precision state(state_l1:state_h1,state_l2:state_h2,NVAR)

     double precision xcen,ycen
     integer i,j

      do j = lo(2), hi(2)
         ycen = xlo(2) + delta(2)*(float(j-lo(2)) + 0.5d0)
         
         do i = lo(1), hi(1)
            xcen = xlo(1) + delta(1)*(float(i-lo(1)) + 0.5d0)
            
            if (idir == 1) then
               if (xcen <= center(1)) then
                  state(i,j,URHO) = rho_l
                  state(i,j,UMX) = rho_l*u_l
                  state(i,j,UMY) = 0.d0
                  state(i,j,UEDEN) = rhoe_l + 0.5*rho_l*u_l*u_l
                  state(i,j,UEINT) = rhoe_l 
               else
                  state(i,j,URHO) = rho_r
                  state(i,j,UMX) = rho_r*u_r
                  state(i,j,UMY) = 0.d0
                  state(i,j,UEDEN) = rhoe_r + 0.5*rho_r*u_r*u_r
                  state(i,j,UEINT) = rhoe_r 
               endif
               
            else if (idir == 2) then
               if (ycen <= center(2)) then
                  state(i,j,URHO) = rho_l
                  state(i,j,UMX) = 0.d0
                  state(i,j,UMY) = rho_l*u_l
                  state(i,j,UEDEN) = rhoe_l + 0.5*rho_l*u_l*u_l
                  state(i,j,UEINT) = rhoe_l 
               else
                  state(i,j,URHO) = rho_r
                  state(i,j,UMX) = 0.d0
                  state(i,j,UMY) = rho_r*u_r
                  state(i,j,UEDEN) = rhoe_r + 0.5*rho_r*u_r*u_r
                  state(i,j,UEINT) = rhoe_r
               endif
               
            else
               call bl_abort('invalid idir')
            endif

            state(i,j,UFS  ) = state(i,j,URHO)
            state(i,j,UTEMP) = 0.d0
            
         enddo
      enddo

      end subroutine ca_initdata

! ::: -----------------------------------------------------------

     subroutine ca_hypfill(adv,adv_l1,adv_l2,adv_h1,adv_h2, &
                           domlo,domhi,delta,xlo,time,bc)
 
     use meth_params_module, only : NVAR

     implicit none
     include 'bc_types.fi'
     integer adv_l1,adv_l2,adv_h1,adv_h2
     integer bc(2,2,*)
     integer domlo(2), domhi(2)
     double precision delta(2), xlo(2), time
     double precision adv(adv_l1:adv_h1,adv_l2:adv_h2,NVAR)

     integer n

      do n = 1,NVAR
         call filcc(adv(adv_l1,adv_l2,n), &
              adv_l1,adv_l2,adv_h1,adv_h2, &
              domlo,domhi,delta,xlo,bc(1,1,n))
      enddo

      do n = 1,NVAR

!        XLO
         if ( bc(1,1,n).eq.EXT_DIR .and. adv_l1.lt.domlo(1)) then
            print *,'SHOULD NEVER GET HERE bc(1,1,n) .eq. EXT_DIR) '
            stop
         end if

!        XHI
         if ( bc(1,2,n).eq.EXT_DIR .and. adv_l1.lt.domlo(1)) then
            print *,'SHOULD NEVER GET HERE bc(1,2,n) .eq. EXT_DIR) '
            stop
         end if

!        YLO
         if ( bc(2,1,n).eq.EXT_DIR .and. adv_l2.lt.domlo(2)) then
            print *,'SHOULD NEVER GET HERE bc(2,1,n) .eq. EXT_DIR) '
            stop
         end if

!        YHI
         if ( bc(2,2,n).eq.EXT_DIR .and. adv_l2.lt.domlo(2)) then
            print *,'SHOULD NEVER GET HERE bc(2,2,n) .eq. EXT_DIR) '
            stop
         end if

      end do

      end subroutine ca_hypfill

! ::: -----------------------------------------------------------

      subroutine ca_denfill(adv,adv_l1,adv_l2,adv_h1,adv_h2, &
           domlo,domhi,delta,xlo,time,bc)

      implicit none
      include 'bc_types.fi'
      integer adv_l1,adv_l2,adv_h1,adv_h2
      integer bc(2,2,*)
      integer domlo(2), domhi(2)
      double precision delta(2), xlo(2), time
      double precision adv(adv_l1:adv_h1,adv_l2:adv_h2)

!     Note: this function should not be needed, technically, but is provided
!     to filpatch because there are many times in the algorithm when just
!     the density is needed.  We try to rig up the filling so that the same
!     function is called here and in hypfill where all the states are filled.

      call filcc(adv,adv_l1,adv_l2,adv_h1,adv_h2,domlo,domhi,delta,xlo,bc)

!     XLO
      if ( bc(1,1,1).eq.EXT_DIR .and. adv_l1.lt.domlo(1)) then
         print *,'SHOULD NEVER GET HERE bc(1,1,1) .eq. EXT_DIR) '
         stop
      end if

!     XHI
      if ( bc(1,2,1).eq.EXT_DIR .and. adv_l1.lt.domlo(1)) then
         print *,'SHOULD NEVER GET HERE bc(1,2,1) .eq. EXT_DIR) '
         stop
      end if

!     YLO
      if ( bc(2,1,1).eq.EXT_DIR .and. adv_l2.lt.domlo(2)) then
         print *,'SHOULD NEVER GET HERE bc(2,1,1) .eq. EXT_DIR) '
         stop
      end if

!     YHI
      if ( bc(2,2,1).eq.EXT_DIR .and. adv_l2.lt.domlo(2)) then
         print *,'SHOULD NEVER GET HERE bc(2,2,1) .eq. EXT_DIR) '
         stop
      end if

      end subroutine ca_denfill

! ::: -----------------------------------------------------------
 
      subroutine ca_reactfill(react,react_l1,react_l2,&
           react_h1,react_h2, &
           domlo,domhi,delta,xlo,time,bc)
 
      use probdata_module
      implicit none
      include 'bc_types.fi'
 
      integer :: react_l1,react_l2,react_h1,react_h2
      integer :: bc(2,2,*)
      integer :: domlo(2), domhi(2)
      double precision delta(2), xlo(2), time
      double precision react(react_l1:react_h1,react_l2:react_h2)
 
      call filcc(react,react_l1,react_l2,react_h1,react_h2,domlo,domhi,delta,xlo,bc)
 
      end subroutine ca_reactfill

