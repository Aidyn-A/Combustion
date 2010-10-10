
       subroutine ca_react_state(lo,hi, &
                                 s_in ,so_l1,so_l2,so_h1,so_h2,&
                                 s_out,sn_l1,sn_l2,sn_h1,sn_h2,&
                                 reaction_terms,r_l1,r_l2,r_h1,r_h2, &
                                 time,dt_react)

      use eos_module
      use network           , only : nspec, naux
      use meth_params_module, only : NVAR, URHO, UMX, UMY, UEDEN, UEINT, UTEMP, UFS, UFX, &
                                     small_dens, small_temp, allow_negative_energy
      use castro_burner_module

      implicit none

      integer lo(2),hi(2)
      integer so_l1,so_h1,so_l2,so_h2
      integer sn_l1,sn_h1,sn_l2,sn_h2
      integer  r_l1, r_h1, r_l2, r_h2
      double precision s_in (so_l1:so_h1,so_l2:so_h2,NVAR)
      double precision s_out(sn_l1:sn_h1,sn_l2:sn_h2,NVAR)
      double precision reaction_terms(r_l1:r_h1,r_l2:r_h2,nspec+2)
      double precision time,dt_react

      integer          :: i,j,n
      integer          :: pt_index(2)
      double precision :: rho, rhoInv, u, v, ke, e_in, e_out, T_in, T_out
      double precision :: x_in(nspec+naux), x_out(nspec+naux)
      double precision :: dummy_gam, dummy_pres, dummy_c, dummy_dpdr, dummy_dpde

      do j = lo(2), hi(2)
      do i = lo(1), hi(1)

        if (s_in(i,j,URHO) .gt. small_dens) then

           ! Define T from e
           rho           = s_in(i,j,URHO)
           rhoInv        = 1.d0 / rho
           u             = s_in(i,j,UMX)*rhoInv
           v             = s_in(i,j,UMY)*rhoInv
           ke            = 0.5d0 * (u**2 + v**2)
           T_in          = s_in(i,j,UTEMP)
           x_in(1:nspec) = s_in(i,j,UFS:UFS+nspec-1) * rhoInv
           if (naux > 0) &
             x_in(nspec+1:nspec+naux)  = s_in(i,j,UFX:UFX+naux-1) * rhoInv

           ! NEW -- Define e from (rho e) *NOT* from (rho E)
           e_in          = s_in(i,j,UEINT) * rhoInv

           pt_index(1) = i
           pt_index(2) = j

           if (allow_negative_energy.eq.0 .and. e_in .le. 0.d0) then
              print *,'... e negative in react_state: ',i,e_in
              T_in = max(T_in, small_temp)
              call eos_given_RTX(e_in, dummy_pres , rho, T_in, x_in, pt_index)
              if (e_in .lt. 0.d0) then
                 print *,'... call to eos_given_RTX with small_temp still gives negative e ',e_in
                 call bl_error("Error:: React_2d.f90 :: ca_react_state")
              else
                 print *,'... able to re-set using eos_given_RTX with small_temp ',e_in
              end if
           end if

           ! Use this call to define T_in
           call eos_given_ReX(dummy_gam, dummy_pres , dummy_c, T_in, &
                              dummy_dpdr, dummy_dpde, rho, e_in, x_in, pt_index)

           call burner(rho, T_in, x_in, e_in, dt_react, time, x_out, e_out)

           ! Make sure that species emerge in the proper range: [0,1]
           do n = 1, nspec
             x_out(n) = max(min(x_out(n),1.d0),0.d0)
           end do

           reaction_terms(i,j,1:nspec) = reaction_terms(i,j,1:nspec) + (x_out(1:nspec) - x_in(1:nspec))
           reaction_terms(i,j,nspec+1) = reaction_terms(i,j,nspec+1) +     (e_out - e_in)
           reaction_terms(i,j,nspec+2) = reaction_terms(i,j,nspec+2) + rho*(e_out - e_in)

           s_out(i,j,URHO)            = rho
           s_out(i,j,UEINT)           = rho * e_out
           s_out(i,j,UEDEN)           = rho * (e_out + ke)
           s_out(i,j,UFS:UFS+nspec-1) = rho * x_out(1:nspec)
           s_out(i,j,UFX:UFX+naux -1) = s_in(i,j,UFX:UFX+naux-1)

           if (e_out .lt. 0.d0) then
             print *,'REACT:NEGATIVE e_out from burner ',i,j,e_out
             call bl_error("Error:: React_2d.f90 :: ca_react_state")
           end if

           ! Use this call to define T_out
           ! We initialize T_out = T_in so the eos will have an initial gues
           T_out = T_in
           call eos_given_ReX(dummy_gam, dummy_pres , dummy_c, T_out, &
                              dummy_dpdr, dummy_dpde, rho, e_out, x_out, pt_index)

           s_out(i,j,UTEMP)           = T_out

        else

           s_out(i,j,1:NVAR)          = s_in(i,j,1:NVAR)

        end if

      end do
      end do

      end subroutine ca_react_state

