      subroutine advance(vel_old,vel_new,scal_old,scal_new,
     $                   I_R_old,I_R_new,press_old,press_new,
     $                   divu_old,divu_new,dsdt,beta_old,beta_new,
     $                   dx,dt,time)

      implicit none
      include 'spec.h'
      real*8   vel_new(-1:nx  )
      real*8   vel_old(-1:nx  )
      real*8  scal_new(-1:nx  ,nscal)
      real*8  scal_old(-1:nx  ,nscal)
      real*8 press_new(0 :nx  )
      real*8 press_old(0 :nx  )
      real*8   I_R_new(0:nx-1,0:maxspec)
      real*8   I_R_old(0:nx-1,0:maxspec)
      real*8    macvel(0 :nx  )
      real*8   veledge(0 :nx  )
      real*8      aofs(0 :nx-1,nscal)
      real*8  divu_old(0 :nx-1)
      real*8  divu_new(0 :nx-1)
      real*8  divu_tmp(0 :nx-1)
      real*8  beta_old(-1:nx,nscal)
      real*8  beta_new(-1:nx,nscal)
      real*8    mu_old(-1:nx)
      real*8    mu_new(-1:nx)
      real*8      dsdt(0 :nx-1)
      real*8        gp(0 :nx-1)
      real*8   rhohalf(0 :nx-1)
      real*8    tforce(0 :nx-1,nscal)
      real*8      visc(0 :nx-1)
      real*8        cp(0 :nx-1)
      real*8 x
      real*8 dx
      real*8 dt
      real*8 time
      real*8 be_cn_theta
      real*8 theta
      real*8 vel_theta
      
      real*8    diff_old(0:nx-1,nscal)
      real*8    diff_hat(0:nx-1,nscal)
      real*8    diff_new(0:nx-1,nscal)
      real*8   const_src(0:nx-1,nscal)
      real*8 lin_src_old(0:nx-1,nscal)
      real*8 lin_src_new(0:nx-1,nscal)
      integer misdc
      
      integer i,n,ispec
      integer iunit
      
      real*8 divu_max
      real*8     alpha(0:nx-1)
      real*8       Rhs(0:nx-1,nscal)
      real*8      dRhs(0:nx-1,0:maxspec)
      real*8   vel_Rhs(0:nx-1)
      real*8 rhocp_old,  rhocp
      real*8 Tmid
      real*8   pthermo(-1:nx  )
      real*8    Ydot_max, Y(maxspec)
      real*8 RWRK, cpmix, sum
      integer IWRK, is, rho_flag
      
C CEG debugging FIXME
      real*8 ptherm(-1:nx)
      real*8 dummy
      integer j, imax, imin
      real*8  Schange(-1:nx  ,nscal)
      real*8  change_max(nscal)
      real*8  change_min(nscal)


      print *,'advance: at start of time step'
      be_cn_theta = 1.d0
c     
c*****************************************************************
c     Create MAC velocities.
c*****************************************************************
c          
      do i = 0,nx-1
         gp(i) = (press_old(i+1) - press_old(i)) / dx
      enddo

c      call minmax_scal(nx,scal_old)
      print *,'... predict edge velocities'
C CEG:: this fills ghost cells for vel_old
      call pre_mac_predict(vel_old,scal_old,gp,
     $                     macvel,dx,dt,time)
      
      call compute_pthermo(scal_old,pthermo)

      do i = 0,nx-1
         divu_tmp(i) = divu_old(i) + 0.5d0 * dt * dsdt(i)
      enddo
      divu_max = ABS(divu_tmp(0))
      do i = 1,nx-1
         divu_max = MAX(divu_max,ABS(divu_tmp(i)))
      enddo
      print *,'DIVU norm old = ',divu_max 
      call add_dpdt(pthermo,divu_tmp,macvel,dx,dt)
      divu_max = ABS(divu_tmp(0))
      do i = 1,nx-1
         divu_max = MAX(divu_max,ABS(divu_tmp(i)))
      enddo
      print *,'DIVU norm new = ',divu_max 
      call macproj(nx,macvel,divu_tmp,dx)

c
c*****************************************************************
c     
C CEG:: make sure to get diffusivities at time n (old time)
      call calc_diffusivities(scal_old,beta_old,mu_old,dx,time)

      if (use_strang) then
         call strang_advance(macvel,scal_old,scal_new,
     $                   I_R_old,I_R_new,beta_old,beta_new,
     $                   dx,dt,time)
      else
         if (use_temp_eqn) then
            call advance_temp(macvel,scal_old,scal_new,
     $                   I_R_old,I_R_new,beta_old,beta_new,
     $                   dx,dt,time)
         else
            print *,'... evolving without using temp eqn'
            print *,'... creating the diffusive terms with old data'

C     CEG:: each one of these functions first calls set_bc(scal_old)
C     maybe should change this
            call get_temp_visc_terms(scal_old,beta_old,
     &           diff_old(0,Temp),dx,time)
            call get_spec_visc_terms(scal_old,beta_old,
     &           diff_old(0,FirstSpec),dx,time)
            call get_rhoh_visc_terms(scal_old,beta_old,
     &           diff_old(0,RhoH),dx,time)
            
c*****************************************************************
            
            print *,'... computing aofs with D(old) + R(guess)'

            do i = 0,nx-1
               do n = 1,Nspec
                  Y(n) = scal_old(i,FirstSpec+n-1) / scal_old(i,Density)
               enddo
               call CKCPBS(scal_old(i,Temp),Y,IWRK,RWRK,cpmix)
               rhocp_old = cpmix * scal_old(i,Density)
               diff_old(i,Temp) = diff_old(i,Temp)/rhocp_old

               do n = 1,Nspec
                  is = FirstSpec + n - 1
                  tforce(i,is) = diff_old(i,is) + I_R_new(i,n)
               enddo
               tforce(i,Temp) = diff_old(i,Temp) + I_R_new(i,0)
            enddo
            
            call scal_aofs(scal_old,macvel,aofs,tforce,dx,dt,time)

c*****************************************************************

            print *,'... update rho'
            call update_rho(scal_old,scal_new,aofs,dx,dt)

            if (predict_temp_for_coeffs .eq. 1) then
               print *,'... predict temp with old coeffs'
               rho_flag = 1
               theta = 0.5d0
C     CEG:: beta_new comes into advance() with the same value as 
C     beta_old (the one we just calculated)
               call update_temp(scal_old,scal_new,aofs,
     $              alpha,beta_old,beta_new,I_R_new(0,0),
     $              Rhs(0,Temp),dx,dt,theta,time)
C     CEG:: just uses RHS and overwrites snew
C     does not fill ghost cells
               call cn_solve(scal_new,alpha,beta_old,Rhs(0,Temp),
     $              dx,dt,Temp,theta,rho_flag)

               call get_hmix_given_T_RhoY(scal_new,dx)      

               print *,'... compute new coeffs'
               call calc_diffusivities(scal_new,beta_new,mu_new,dx,
     &                                 time+dt)
            else
               print *,'... set new coeffs to old values for predictor'
               do n=1,nscal
                  do i=-1,nx
                     scal_new(i,Temp) = scal_old(i,Temp)
                     beta_new(i,n) = beta_old(i,n)
                  enddo
               enddo
            endif

c*****************************************************************
            print *,'... do predictor for species (MISDC terms=0)'
            do i=0,nx-1
               dRhs(i,0) = 0.0d0
               do n=1,Nspec
                  dRhs(i,n) = dt*I_R_new(i,n)
               enddo
            enddo
            call update_spec(scal_old,scal_new,aofs,alpha,beta_old,
     &           dRhs(0,1),Rhs(0,FirstSpec),dx,dt,be_cn_theta,time)

            rho_flag = 2
            do n=1,Nspec
               is = FirstSpec + n - 1
               call cn_solve(scal_new,alpha,beta_new,Rhs(0,is),
     $              dx,dt,is,be_cn_theta,rho_flag)
            enddo

C CEG:: trying NOT doing SDC with rhoH.  just do the old CN solve instead
            theta = 0.5d0
            print *,'... do predictor for rhoh (MISDC terms=0)'
            call update_rhoh(scal_old,scal_new,aofs,alpha,beta_old,
     &           dRhs(0,0),Rhs(0,RhoH),dx,dt,theta,time)
            rho_flag = 2
            call cn_solve(scal_new,alpha,beta_new,Rhs(0,RhoH),
     $           dx,dt,RhoH,theta,rho_flag)

            call rhoh_to_temp(scal_new)

            print *,'...   extract D sources'
            if (be_cn_theta .ne. 0.d0) then
               do i = 0,nx-1
                  diff_new(i,RhoH) = (
     $                 (scal_new(i,RhoH)-scal_old(i,RhoH))/dt 
     $                 - aofs(i,RhoH) -
     $                 (1.d0-theta)*diff_old(i,RhoH) )/theta
                  do n=1,Nspec
                     is = FirstSpec + n - 1
                     diff_new(i,is) = (
     $                    (scal_new(i,is)-scal_old(i,is))/dt 
     $                    - aofs(i,is) - I_R_new(i,n) - 
     $                    (1.d0-be_cn_theta)*diff_old(i,is) 
     $                    )/be_cn_theta
                  enddo
               enddo
            else
               print *,'ERROR:: be_cn_theta=0.0d0 case not coded yet'
               stop
            endif

            if (nochem_hack) then
               print *,'WARNING! doing nochem_hack--skipping reactions'
            else
               print *,'... react with A+D sources, reset I_R_new'
               do n = 1,nscal
                  do i = 0,nx-1
                     const_src(i,n) =     aofs(i,n)
                     lin_src_old(i,n) = diff_old(i,n)
                     lin_src_new(i,n) = diff_new(i,n)
                  enddo
               enddo

               call strang_chem(scal_old,scal_new,
     $              const_src,lin_src_old,lin_src_new,
     $              I_R_new,dt)
            endif

C     CEG debugging FIXME
C     
C     Find the estimated change in S over the timestep
C     
            do n = 1,nscal
               change_max(n) = 0.d0
               change_min(n) = 0.d0
            enddo
            do i = 0,nx-1
               do n = 1,nscal
                  Schange(i,n) = scal_new(i,n) - scal_old(i,n)
                  change_max(n) = MAX(change_max(n),Schange(i,n))
                  change_min(n) = MIN(change_min(n),Schange(i,n))
               enddo 
            enddo
            write(*,*)
            write(*,*)'Change in S over the timestep'
            write(*,*)'index      min      max'
            do n = 1,nscal
               write(*,*)n,MAX(ABS(change_min(n)),ABS(change_max(n)))
            enddo
            do i = 0,nx-1
               do n = 1,nscal
                  Schange(i,n) = scal_new(i,n)
               enddo
            enddo
 1008       FORMAT((I5,1X),(E22.15,1X))      
C----------------------------------------------------------------
C----------------------------------------------------------------
            do misdc = 1, misdc_iterMAX
               print *,'... doing SDC iter ',misdc

               print *,'... create new diff_hat from current state'
               call calc_diffusivities(scal_new,beta_new,mu_new,
     &              dx,time+dt)
               call get_temp_visc_terms(scal_new,beta_new,
     &              diff_hat(0,Temp),dx,time+dt)
               call get_spec_visc_terms(scal_new,beta_new,
     &              diff_hat(0,FirstSpec),dx,time+dt)

               do i = 0,nx-1
                  do n = 1,Nspec
                     Y(n) =scal_new(i,FirstSpec+n-1)/scal_new(i,Density)
                  enddo
                  call CKCPBS(scal_new(i,Temp),Y,IWRK,RWRK,cpmix)
                  rhocp = cpmix * scal_new(i,Density)
                  diff_hat(i,Temp) = diff_hat(i,Temp)/rhocp
                  do n = 1,Nspec
                     ispec = FirstSpec + n - 1
                     tforce(i,ispec) = I_R_new(i,n)
     &                    + 0.5d0*(diff_old(i,ispec)+diff_hat(i,ispec))
                  enddo
                  tforce(i,Temp) = I_R_new(i,0)
     &                 + 0.5d0*(diff_old(i,Temp)+diff_hat(i,Temp))
               enddo
               
               print *,'... compute A with updated D+R source'
               call scal_aofs(scal_old,macvel,aofs,tforce,dx,dt,time)

               print *,'... update rho'
               call update_rho(scal_old,scal_new,aofs,dx,dt)

c*****************************************************************

               print *,'... update D for species with A + R + MISDC(D)'
               do i=0,nx-1
                  do n=1,Nspec
                     is = FirstSpec + n - 1
                     dRhs(i,n) = dt*(I_R_new(i,n) 
     &                    + 0.5d0*(diff_old(i,is) - diff_hat(i,is)))
                  enddo
                  dRhs(i,0) = 0.d0
               enddo
               call update_spec(scal_old,scal_new,aofs,alpha,beta_old,
     &              dRhs(0,1),Rhs(0,FirstSpec),dx,dt,be_cn_theta,time)
               rho_flag = 2
               do n=1,Nspec
                  is = FirstSpec + n - 1
                  call cn_solve(scal_new,alpha,beta_new,Rhs(0,is),
     $                 dx,dt,is,be_cn_theta,rho_flag)
               enddo

               print *,'... update D for rhoh with A + R + MISDC(D)'
               call update_rhoh(scal_old,scal_new,aofs,alpha,beta_old,
     &              dRhs(0,0),Rhs(0,RhoH),dx,dt,theta,time)
               rho_flag = 2
               call cn_solve(scal_new,alpha,beta_new,Rhs(0,RhoH),
     $              dx,dt,RhoH,theta,rho_flag)
               print *,'... create new temp from new RhoH, spec'
               call rhoh_to_temp(scal_new)

               print *,'... create diff_new from RhoH & spec solutions'
               if (be_cn_theta .ne. 0.d0) then
                  do i = 0,nx-1
                     diff_new(i,RhoH) = (
     $                    (scal_new(i,RhoH)-scal_old(i,RhoH))/dt 
     $                    - aofs(i,RhoH) - dRhs(i,0)/dt - 
     $                    (1.d0-theta)*diff_old(i,RhoH) 
     $                    )/theta
                     do n=1,Nspec
                        is = FirstSpec + n - 1
                        diff_new(i,is) = (
     $                       (scal_new(i,is)-scal_old(i,is))/dt 
     $                       - aofs(i,is) - dRhs(i,n)/dt - 
     $                       (1.d0-be_cn_theta)*diff_old(i,is) 
     $                       )/be_cn_theta
                     enddo
                  enddo
               else
                  print *,'ERROR:: be_cn_theta=0.0d0 case not coded yet'
                  stop
               endif
               
               if (nochem_hack) then
                  print *,'WARNING: SDC nochem_hack--skipping reactions'
               else
                  print *,'... react with const and linear sources'
                  do n = 1,nscal
                     do i = 0,nx-1
                        const_src(i,RhoH) = aofs(i,RhoH)
                        lin_src_old(i,RhoH) = diff_old(i,RhoH)
                        lin_src_new(i,RhoH) = diff_new(i,RhoH)

                        const_src(i,n) = aofs(i,n)
     $                       + diff_new(i,n) - diff_hat(i,n)
                        lin_src_old(i,n) = diff_old(i,n)
                        lin_src_new(i,n) = diff_hat(i,n)
                     enddo                     
                  enddo
                  
                  call strang_chem(scal_old,scal_new,
     $                 const_src,lin_src_old,lin_src_new,
     $                 I_R_new,dt)
               endif

c*****************************************************************
c     End of MISDC iterations
c*****************************************************************
C     CEG debugging FIXME
C     
C     Find the size of the correction, ie the change in S_new
C     
               do n = 1,nscal
                  change_max(n) = 0.d0
                  change_min(n) = 0.d0
               enddo
               do i = 0,nx-1
                  do n = 1,nscal
                     Schange(i,n) = scal_new(i,n) - Schange(i,n)
                     change_max(n) = MAX(change_max(n),Schange(i,n))
                     change_min(n) = MIN(change_min(n),Schange(i,n))
                  enddo 
               enddo
               write(*,*)
               write(*,*)'Size of the correction (Change in S_new)'
               write(*,*)'index      min      max'
               do n = 1,nscal
C     write(*,*)n,change_min(n),change_max(n)
                  write(*,*)n,MAX(ABS(change_min(n)),ABS(change_max(n)))
               enddo
               do i = 0,nx-1
                  do n = 1,nscal
                     Schange(i,n) = scal_new(i,n)
                  enddo
               enddo

            enddo

         endif
C end if(use temp eqn)
         endif
C end strang vs SDC
            
      call calc_diffusivities(scal_new,beta_new,mu_new,dx,time+dt)
      call calc_divu(scal_new,beta_new,I_R_new,divu_new,dx,time+dt)

      do i = 0,nx-1
         rhohalf(i) = 0.5d0*(scal_old(i,Density)+scal_new(i,Density))
         dsdt(i) = (divu_new(i) - divu_old(i)) / dt
      enddo         
C debugging FIXME
C$$$ 1007 FORMAT((I5,1X),(E22.15,1X))      
C$$$         open(UNIT=11, FILE='dsdt.dat', STATUS = 'REPLACE')
C$$$         write(11,*)'# 256 2'
C$$$         do i=0,nx-1
C$$$            write(11,1007) i, dsdt(i)
C$$$         enddo
C$$$         close(11)
C$$$         write(*,*)'divu update'
C$$$         stop
CCCCCCCCCCCCC

      print *,'... update velocities'

      vel_theta = 0.5d0
C get velocity visc terms to use as a forcing term for advection
      call get_vel_visc_terms(vel_old,mu_old,visc,dx,time)
      do i = 0, nx-1
         visc(i) = visc(i)/scal_old(i,Density)
      enddo

      call vel_edge_states(vel_old,scal_old(-1,Density),gp,
     $                     macvel,veledge,dx,dt,time,visc)
      
      call update_vel(vel_old,vel_new,gp,rhohalf,
     &                macvel,veledge,alpha,mu_old,
     &                vel_Rhs,dx,dt,vel_theta,time)

      if (initial_iter .eq. 1) then
         call get_vel_visc_terms(vel_old,mu_old,visc,dx,time)
         do i = 0, nx-1
            vel_new(i) = vel_new(i) + visc(i)*dt/rhohalf(i)
         enddo
      else
         rho_flag = 1
         call cn_solve(vel_new,alpha,mu_new,vel_Rhs,
     $        dx,dt,1,vel_theta,rho_flag)
      endif

      print *,'...nodal projection...'
C CEG:: LMC has another var initial_step, and doesn't do the proj
C       until after all num_init_iters are done
      if (initial_iter .eq. 0) then
         call project(vel_old,vel_new,rhohalf,divu_new,
     $        press_old,press_new,dx,dt)
      endif
CCCCCCCCCCC debugging FIXME
 1006 FORMAT((I5,1X),11(E22.15,1X))      
         call compute_pthermo(scal_new,ptherm)
         open(UNIT=11, FILE='new.dat', STATUS = 'REPLACE')
         write(11,*)'# 256 12'
         do j=0,nx-1
            do n = 1,Nspec
               Y(n) = scal_new(j,FirstSpec+n-1)*1.d3
            enddo
            write(11,1006) j, vel_new(j)*1.d-2, 
     &                     scal_new(j,Density)*1.d3,
     &                     (Y(n),n=1,Nspec),
     $                     scal_new(j,RhoH)*1.d-1,
     $                     scal_new(j,Temp),
     $                     ptherm(j)*1.d-1
         enddo
         close(11)
C         stop
CCCCCCCCCCCCC      
      end

      subroutine advance_temp (macvel,scal_old,scal_new,
     $                   I_R_old,I_R_new,
     $                   beta_old,beta_new,dx,dt,time)

      implicit none
      include 'spec.h'
      real*8  scal_new(-1:nx  ,nscal)
      real*8  scal_old(-1:nx  ,nscal)
      real*8   I_R_new(0:nx-1,0:maxspec)
      real*8   I_R_old(0:nx-1,0:maxspec)
      real*8    macvel(0 :nx  )
      real*8      aofs(0 :nx-1,nscal)
      real*8  beta_old(-1:nx,nscal)
      real*8  beta_new(-1:nx,nscal)
      real*8  mu_dummy(-1:nx)
      real*8   rhohalf(0 :nx-1)
      real*8    tforce(0 :nx-1,nscal)
      real*8      visc(0 :nx-1)
      real*8        cp(0 :nx-1)
      real*8 dx
      real*8 dt
      real*8 time
      real*8 be_cn_theta
      real*8 theta
      
      real*8    diff_old(0:nx-1,nscal)
      real*8    diff_new(0:nx-1,nscal)
      real*8    diff_hat(0:nx-1,nscal)
      real*8   const_src(0:nx-1,nscal)
      real*8 lin_src_old(0:nx-1,nscal)
      real*8 lin_src_new(0:nx-1,nscal)
      
      integer i,n,ispec
      integer iunit
      
      real*8 divu_max
      real*8     alpha(0:nx-1)
      real*8       Rhs(0:nx-1,nscal)
      real*8      dRhs(0:nx-1,0:maxspec)
      real*8 rhocp_old, rhocp
      real*8 Tmid
      real*8   pthermo(-1:nx  )
      real*8    Ydot_max, Y(maxspec)
      real*8 RWRK, cpmix, sum
      integer IWRK, is, rho_flag
      integer misdc

C CEG debugging FIXME
      real*8 ptherm(-1:nx)
      integer j
      real*8  Schange(-1:nx  ,nscal)
      real*8  change_max(nscal)
      real*8  change_min(nscal)


      be_cn_theta = 1.0d0

      print *,'... evolving using temperature equation'
      print *,'... creating the diffusive terms with old data'

C CEG:: each one of these functions first calls set_bc(scal_old)
C   maybe should change this
      call get_temp_visc_terms(scal_old,beta_old,
     &                         diff_old(0,Temp),dx,time)
      call get_spec_visc_terms(scal_old,beta_old,
     &                         diff_old(0,FirstSpec),dx,time)
      call get_rhoh_visc_terms(scal_old,beta_old,
     &                         diff_old(0,RhoH),dx,time)
      
c*****************************************************************
      
      print *,'... computing aofs with D(old) + R(guess)'

      do i = 0,nx-1
         do n = 1,Nspec
            Y(n) = scal_old(i,FirstSpec+n-1) / scal_old(i,Density)
         enddo
         call CKCPBS(scal_old(i,Temp),Y,IWRK,RWRK,cpmix)
         rhocp_old = cpmix * scal_old(i,Density)
         diff_old(i,Temp) = diff_old(i,Temp)/rhocp_old

         do n = 1,Nspec
            is = FirstSpec + n - 1
            tforce(i,is) = diff_old(i,is) + I_R_new(i,n)
         enddo
         tforce(i,Temp) = diff_old(i,Temp) + I_R_new(i,0)
      enddo
       
      call scal_aofs(scal_old,macvel,aofs,tforce,dx,dt,time)

c*****************************************************************

      print *,'... update rho'
      call update_rho(scal_old,scal_new,aofs,dx,dt)

c*****************************************************************
c     Either do c-n solve for new T prior to computing new 
c     coeffs, or simply start by copying from previous time step


      if (predict_temp_for_coeffs .eq. 1) then
         print *,'... predict temp with old coeffs'
         rho_flag = 1
         theta = 0.5d0
C CEG:: beta_new comes into advance() with the same value as 
C       beta_old (the one we just calculated)
         call update_temp(scal_old,scal_new,aofs,
     $                    alpha,beta_old,beta_new,I_R_new(0,0),
     $                    Rhs(0,Temp),dx,dt,theta,time)
C CEG:: just uses RHS and overwrites snew
C does not fill ghost cells
         call cn_solve(scal_new,alpha,beta_old,Rhs(0,Temp),
     $                 dx,dt,Temp,theta,rho_flag)

         call get_hmix_given_T_RhoY(scal_new,dx)      

         print *,'... compute new coeffs'
         call calc_diffusivities(scal_new,beta_new,mu_dummy,dx,time+dt)
      else
         print *,'... set new coeffs to old values for predictor'
         do n=1,nscal
            do i=-1,nx
               scal_new(i,Temp) = scal_old(i,Temp)
               beta_new(i,n) = beta_old(i,n)
            enddo
         enddo
      endif

c*****************************************************************
      print *,'... do predictor for species (MISDC terms=0)'
      do i=0,nx-1
         dRhs(i,0) = 0.0d0
         do n=1,Nspec
            dRhs(i,n) = dt*I_R_new(i,n)
         enddo
      enddo
      call update_spec(scal_old,scal_new,aofs,alpha,beta_old,
     &     dRhs(0,1),Rhs(0,FirstSpec),dx,dt,be_cn_theta,time)

      rho_flag = 2
      do n=1,Nspec
         is = FirstSpec + n - 1
         call cn_solve(scal_new,alpha,beta_new,Rhs(0,is),
     $                 dx,dt,is,be_cn_theta,rho_flag)
      enddo

      print *,'... do predictor for rhoh (MISDC terms=0)'
      call update_rhoh(scal_old,scal_new,aofs,alpha,beta_old,
     &     dRhs(0,0),Rhs(0,RhoH),dx,dt,be_cn_theta,time)
      rho_flag = 2
      call cn_solve(scal_new,alpha,beta_new,Rhs(0,RhoH),
     $              dx,dt,RhoH,be_cn_theta,rho_flag)

      call rhoh_to_temp(scal_new)

      print *,'...   extract D sources'
      if (be_cn_theta .ne. 0.d0) then
         do i = 0,nx-1
            diff_new(i,RhoH) = (
     $           (scal_new(i,RhoH)-scal_old(i,RhoH))/dt 
     $           - aofs(i,RhoH) -
     $           (1.d0-be_cn_theta)*diff_old(i,RhoH) )/be_cn_theta
            do n=1,Nspec
               is = FirstSpec + n - 1
               diff_new(i,is) = (
     $              (scal_new(i,is)-scal_old(i,is))/dt 
     $              - aofs(i,is) - I_R_new(i,n) - 
     $              (1.d0-be_cn_theta)*diff_old(i,is) )/be_cn_theta
            enddo
         enddo
      else
         print *,'ERROR:: not set up to work with be_cn_theta=0.0d0'
         stop
      endif

      if (nochem_hack) then
         write(*,*)'WARNING! doing nochem_hack--skipping reactions'
      else
         print *,'... react with A+D sources, reset I_R_new'
         do n = 1,nscal
            do i = 0,nx-1
               const_src(i,n) =     aofs(i,n)
               lin_src_old(i,n) = diff_old(i,n)
               lin_src_new(i,n) = diff_new(i,n)
            enddo
         enddo

         call strang_chem(scal_old,scal_new,
     $                    const_src,lin_src_old,lin_src_new,
     $                    I_R_new,dt)
      endif

C CEG debugging FIXME
C
C Find the estimated change in S over the timestep
C
      do n = 1,nscal
         change_max(n) = 0.d0
         change_min(n) = 0.d0
      enddo
      do i = 0,nx-1
         do n = 1,nscal
            Schange(i,n) = scal_new(i,n) - scal_old(i,n)
            change_max(n) = MAX(change_max(n),Schange(i,n))
            change_min(n) = MIN(change_min(n),Schange(i,n))
         enddo 
      enddo
      write(*,*)
      write(*,*)'Change in S over the timestep'
      write(*,*)'index      min      max'
      do n = 1,nscal
         write(*,*)n,MAX(ABS(change_min(n)),ABS(change_max(n)))
      enddo
      do i = 0,nx-1
         do n = 1,nscal
            Schange(i,n) = scal_new(i,n)
         enddo
      enddo
 1008 FORMAT((I5,1X),(E22.15,1X))      
C----------------------------------------------------------------
C----------------------------------------------------------------
      do misdc = 1, misdc_iterMAX
         print *,'... doing SDC iter ',misdc

         print *,'... create new diff_hat from current state'
         call calc_diffusivities(scal_new,beta_new,mu_dummy,
     &                           dx,time+dt)
         call get_temp_visc_terms(scal_new,beta_new,
     &                            diff_hat(0,Temp),dx,time+dt)
         call get_spec_visc_terms(scal_new,beta_new,
     &                            diff_hat(0,FirstSpec),dx,time+dt)

         do i = 0,nx-1
            do n = 1,Nspec
               Y(n) = scal_new(i,FirstSpec+n-1) / scal_new(i,Density)
            enddo
            call CKCPBS(scal_new(i,Temp),Y,IWRK,RWRK,cpmix)
            rhocp = cpmix * scal_new(i,Density)
            diff_hat(i,Temp) = diff_hat(i,Temp)/rhocp
C     save a copy of diff_new(RhoH)
            diff_hat(i,RhoH) = diff_new(i,RhoH)
            do n = 1,Nspec
               ispec = FirstSpec + n - 1
               tforce(i,ispec) = I_R_new(i,n)
     &              + 0.5d0*(diff_old(i,ispec)+diff_hat(i,ispec))
            enddo
            tforce(i,Temp) = I_R_new(i,0)
     &              + 0.5d0*(diff_old(i,Temp)+diff_hat(i,Temp))
         enddo
         
         print *,'... compute A with updated D+R source'
         call scal_aofs(scal_old,macvel,aofs,tforce,dx,dt,time)

         print *,'... update rho'
         call update_rho(scal_old,scal_new,aofs,dx,dt)

c*****************************************************************

         print *,'... update D for species with A + R + MISDC(D)'
         do i=0,nx-1
            do n=1,Nspec
               is = FirstSpec + n - 1
               dRhs(i,n) = dt*(I_R_new(i,n) 
     &              + 0.5d0*(diff_old(i,is) - diff_hat(i,is)))
            enddo
            dRhs(i,0) = dt*(
     &           + 0.5d0*(diff_old(i,RhoH) - diff_new(i,RhoH)))
         enddo
         call update_spec(scal_old,scal_new,aofs,alpha,beta_old,
     &        dRhs(0,1),Rhs(0,FirstSpec),dx,dt,be_cn_theta,time)
         rho_flag = 2
         do n=1,Nspec
            is = FirstSpec + n - 1
            call cn_solve(scal_new,alpha,beta_new,Rhs(0,is),
     $                    dx,dt,is,be_cn_theta,rho_flag)
         enddo

         print *,'... update D for rhoh with A + R + MISDC(D)'
         call update_rhoh(scal_old,scal_new,aofs,alpha,beta_old,
     &        dRhs(0,0),Rhs(0,RhoH),dx,dt,be_cn_theta,time)
         rho_flag = 2
         call cn_solve(scal_new,alpha,beta_new,Rhs(0,RhoH),
     $                 dx,dt,RhoH,be_cn_theta,rho_flag)
         print *,'... create new temp from new RhoH, spec'
         call rhoh_to_temp(scal_new)

         print *,'... create diff_new from RhoH and spec solutions'
         if (be_cn_theta .ne. 0.d0) then
            do i = 0,nx-1
               diff_new(i,RhoH) = (
     $              (scal_new(i,RhoH)-scal_old(i,RhoH))/dt 
     $              - aofs(i,RhoH) - dRhs(i,0)/dt - 
     $              (1.d0-be_cn_theta)*diff_old(i,RhoH) )/be_cn_theta
               do n=1,Nspec
                  is = FirstSpec + n - 1
                  diff_new(i,is) = (
     $                 (scal_new(i,is)-scal_old(i,is))/dt 
     $                 - aofs(i,is) - dRhs(i,n)/dt - 
     $                 (1.d0-be_cn_theta)*diff_old(i,is) )/be_cn_theta
               enddo
            enddo
         else
            print *,'ERROR:: not set up to work with be_cn_theta=0.0d0'
            stop
         endif
 
         if (nochem_hack) then
            write(*,*)'WARNING:: SDC nochem_hack--skipping reactions'
         else
            print *,'... react with const and linear sources'
            do n = 1,nscal
               do i = 0,nx-1
                  
                  const_src(i,n) = aofs(i,n)
     $                 + diff_new(i,n) - diff_hat(i,n)
                  lin_src_old(i,n) = diff_old(i,n)
                  lin_src_new(i,n) = diff_hat(i,n)
               enddo
            enddo
            
            call strang_chem(scal_old,scal_new,
     $           const_src,lin_src_old,lin_src_new,
     $           I_R_new,dt)
         endif

c*****************************************************************
c       End of MISDC iterations
c*****************************************************************
C     CEG debugging FIXME
C     
C     Find the size of the correction, ie the change in S_new
C     
         do n = 1,nscal
            change_max(n) = 0.d0
            change_min(n) = 0.d0
         enddo
         do i = 0,nx-1
            do n = 1,nscal
               Schange(i,n) = scal_new(i,n) - Schange(i,n)
               change_max(n) = MAX(change_max(n),Schange(i,n))
               change_min(n) = MIN(change_min(n),Schange(i,n))
            enddo 
         enddo
         write(*,*)
         write(*,*)'Size of the correction (Change in S_new)'
         write(*,*)'index      min      max'
         do n = 1,nscal
C            write(*,*)n,change_min(n),change_max(n)
            write(*,*)n,MAX(ABS(change_min(n)),ABS(change_max(n)))
         enddo
         do i = 0,nx-1
            do n = 1,nscal
               Schange(i,n) = scal_new(i,n)
            enddo
         enddo

      enddo

      end 

      subroutine strang_advance (macvel,scal_old,scal_new,
     $                   I_R_old,I_R_new,
     $                   beta_old,beta_new,dx,dt,time)

      implicit none
      include 'spec.h'
      real*8  scal_new(-1:nx  ,nscal)
      real*8  scal_old(-1:nx  ,nscal)
      real*8   I_R_new(0:nx-1,0:maxspec)
      real*8   I_R_old(0:nx-1,0:maxspec)
      real*8    macvel(0 :nx  )
      real*8      aofs(0 :nx-1,nscal)
      real*8  beta_old(-1:nx,nscal)
      real*8  beta_new(-1:nx,nscal)
      real*8  mu_dummy(-1:nx)
      real*8   rhohalf(0 :nx-1)
      real*8    tforce(0 :nx-1,nscal)
      real*8      visc(0 :nx-1)
      real*8        cp(0 :nx-1)
      real*8 dx
      real*8 dt
      real*8 time
      real*8 be_cn_theta
      real*8 theta
      
      real*8    diff_old(0:nx-1,nscal)
      real*8   const_src(0:nx-1,nscal)
      real*8 lin_src_old(0:nx-1,nscal)
      real*8 lin_src_new(0:nx-1,nscal)
      
      integer i,n,ispec
      integer iunit
      
      real*8 divu_max
      real*8     alpha(0:nx-1)
      real*8       Rhs(0:nx-1,nscal)
      real*8      dRhs(0:nx-1,0:maxspec)
      real*8 rhocp_old
      real*8 Tmid
      real*8   pthermo(-1:nx  )
      real*8    Ydot_max, Y(maxspec)
      real*8 RWRK, cpmix, sum
      integer IWRK, is, rho_flag

C CEG debugging FIXME
      real*8 ptherm(-1:nx)
      integer j
      

      be_cn_theta = 0.5d0

      if (nochem_hack) then
         print *,'WARNING! doing nochem_hack...'
         do n = 1,nscal
            do i = 0,nx-1
               I_R_new(i,n) = 0.d0
            enddo
         enddo
      else
         print *,'... react for dt/2;  set I_R_new'
         do n = 1,nscal
            do i = 0,nx-1
               const_src(i,n) =   0.d0
               lin_src_old(i,n) = 0.d0
               lin_src_new(i,n) = 0.d0
            enddo
         enddo
         call strang_chem(scal_old,scal_new,
     $                    const_src,lin_src_old,lin_src_new,
     $                    I_R_new,dt/2.d0)
         do j=0,nx-1
            do n = FirstSpec,LastSpec
               scal_old(j,n) = scal_new(j,n)
            enddo
            scal_old(j,Temp) = scal_new(j,Temp)
         enddo
      endif
c     
c*****************************************************************
c     
      print *,'... creating the diffusive terms with old data'
C CEG:: fixme??
      call calc_diffusivities(scal_old,beta_old,mu_dummy,dx,time)
      call calc_diffusivities(scal_new,beta_new,mu_dummy,dx,time)

C CEG:: each one of these functions first calls set_bc(scal_old)
C   maybe should change this
      call get_temp_visc_terms(scal_old,beta_old,
     &                         diff_old(0,Temp),dx,time)
      call get_spec_visc_terms(scal_old,beta_old,
     &                         diff_old(0,FirstSpec),dx,time)
      call get_rhoh_visc_terms(scal_old,beta_old,
     &                         diff_old(0,RhoH),dx,time)
            
      print *,'... computing aofs with D(old)'

      do i = 0,nx-1
         do n = 1,Nspec
            Y(n) = scal_old(i,FirstSpec+n-1) / scal_old(i,Density)
         enddo
         call CKCPBS(scal_old(i,Temp),Y,IWRK,RWRK,cpmix)
         rhocp_old = cpmix * scal_old(i,Density)
         diff_old(i,Temp) = diff_old(i,Temp)/rhocp_old

         do n = 1,Nspec
            is = FirstSpec + n - 1
            tforce(i,is) = diff_old(i,is)
         enddo
         tforce(i,Temp) = diff_old(i,Temp)
      enddo
       
      call scal_aofs(scal_old,macvel,aofs,tforce,dx,dt,time)

      print *,'... update rho'
      call update_rho(scal_old,scal_new,aofs,dx,dt)

      call calc_diffusivities(scal_new,beta_new,mu_dummy,dx,time)

c*****************************************************************
c     Either do c-n solve for new T prior to computing new 
c     coeffs, or simply start by copying from previous time step
      if (predict_temp_for_coeffs .eq. 1) then
         print *,'... predict temp with old coeffs'
         rho_flag = 1
         theta = 0.5d0
         do i=0,nx-1
            dRhs(i,0) = 0.0d0
         enddo
         call update_temp(scal_old,scal_new,aofs,
     $                    alpha,beta_old,beta_new,dRhs(0,0),
     $                    Rhs(0,Temp),dx,dt,theta,time)
C CEG:: just uses RHS and overwrites snew
C does not fill ghost cells
         call cn_solve(scal_new,alpha,beta_old,Rhs(0,Temp),
     $                 dx,dt,Temp,theta,rho_flag)

         call get_hmix_given_T_RhoY(scal_new,dx)      

         print *,'... compute new coeffs'
         call calc_diffusivities(scal_new,beta_new,mu_dummy,dx,time+dt)
      else
         print *,'... set new coeffs to old values for predictor'
         do n=1,nscal
            do i=-1,nx
               scal_new(i,Temp) = scal_old(i,Temp)
               beta_new(i,n) = beta_old(i,n)
            enddo
         enddo
      endif

      print *,'... do predictor for species'
      do i=0,nx-1
         dRhs(i,0) = 0.0d0
         do n=1,Nspec
            dRhs(i,n) = 0.d0
         enddo
      enddo
      call update_spec(scal_old,scal_new,aofs,alpha,beta_old,
     &     dRhs(0,1),Rhs(0,FirstSpec),dx,dt,be_cn_theta,time)

      rho_flag = 2
      do n=1,Nspec
         is = FirstSpec + n - 1
         call cn_solve(scal_new,alpha,beta_new,Rhs(0,is),
     $                 dx,dt,is,be_cn_theta,rho_flag)
      enddo


      print *,'... do predictor for rhoh (MISDC terms=0)'
      call update_rhoh(scal_old,scal_new,aofs,alpha,beta_old,
     &     dRhs(0,0),Rhs(0,RhoH),dx,dt,be_cn_theta,time)
      rho_flag = 2
      call cn_solve(scal_new,alpha,beta_new,Rhs(0,RhoH),
     $              dx,dt,RhoH,be_cn_theta,rho_flag)

      call rhoh_to_temp(scal_new)

C----------------------------------------------------------------
C   Corrector

      print *,'... compute new coeffs'
      call calc_diffusivities(scal_new,beta_new,mu_dummy,dx,time+dt)

      call update_spec(scal_old,scal_new,aofs,alpha,beta_old,
     &                 dRhs(0,1),Rhs(0,FirstSpec),dx,dt,
     &                 be_cn_theta,time)
      rho_flag = 2
      do n=1,Nspec
         is = FirstSpec + n - 1
         call cn_solve(scal_new,alpha,beta_new,Rhs(0,is),
     $                 dx,dt,is,be_cn_theta,rho_flag)
      enddo
         
      print *,'... do predictor for rhoh (MISDC terms=0)'
      call update_rhoh(scal_old,scal_new,aofs,alpha,beta_old,
     &                 dRhs(0,0),Rhs(0,RhoH),dx,dt,be_cn_theta,time)
      rho_flag = 2
      call cn_solve(scal_new,alpha,beta_new,Rhs(0,RhoH),
     $              dx,dt,RhoH,be_cn_theta,rho_flag)
      call rhoh_to_temp(scal_new)

      if (nochem_hack) then
         print *,'WARNING! doing nochem_hack...'
      else
         do j=0,nx-1
            do n = FirstSpec,LastSpec
               scal_old(j,n) = scal_new(j,n)
            enddo
            scal_old(j,Temp) = scal_new(j,Temp)
            scal_old(j,Density) = scal_new(j,Density)
         enddo

         call strang_chem(scal_old,scal_new,
     $                    const_src,lin_src_old,lin_src_new,
     $                    I_R_new,dt/2.d0)
      endif

CCCCCCCCCCC debugging FIXME
C$$$ 1006 FORMAT((I5,1X),11(E22.15,1X))      
C$$$         call compute_pthermo(scal_new,ptherm)
C$$$         open(UNIT=11, FILE='corr.dat', STATUS = 'REPLACE')
C$$$         write(11,*)'# 256 12'
C$$$         do j=0,nx-1
C$$$            do n = 1,Nspec
C$$$               Y(n) = scal_new(j,FirstSpec+n-1)*1.d3
C$$$            enddo
C$$$            write(11,1006) j, macvel(j)*1.d-2, 
C$$$     &                     scal_new(j,Density)*1.d3,
C$$$     &                     (Y(n),n=1,Nspec),
C$$$     $                     scal_new(j,RhoH)*1.d-1,
C$$$     $                     scal_new(j,Temp),
C$$$     $                     ptherm(j)*1.d-1
C$$$         enddo
C$$$         close(11)
C$$$         write(*,*)'AFTER end'
CCCCCCCCCCCCC      

      end
