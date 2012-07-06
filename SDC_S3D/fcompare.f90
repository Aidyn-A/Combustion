! Take 2 plotfiles as input and compare them zone by zone for differences.
!
! For the comparison to take place, the grids must be identical.
!

program fcompare

  use f2kcli
  use bl_space
  use bl_error_module
  use bl_constants_module
  use bl_IO_module
  use plotfile_module
  use sort_d_module

  implicit none

  type(plotfile) :: pf_a, pf_b
  character (len=256) :: plotfile_a, plotfile_b
  integer :: unit_a, unit_b

  real(kind=dp_t), pointer :: p_a(:,:,:,:), p_b(:,:,:,:)

  integer :: lo_a(MAX_SPACEDIM), hi_a(MAX_SPACEDIM)
  integer :: lo_b(MAX_SPACEDIM), hi_b(MAX_SPACEDIM)

  integer :: flo_a(MAX_SPACEDIM), fhi_a(MAX_SPACEDIM)
  integer :: flo_b(MAX_SPACEDIM), fhi_b(MAX_SPACEDIM)

  real(kind=dp_t) :: dx_a(MAX_SPACEDIM), dx_b(MAX_SPACEDIM)

  integer :: nboxes_a, nboxes_b

  integer :: n_a, n_b
  integer, allocatable :: ivar_b(:)

  real(kind=dp_t), allocatable :: aerror(:), rerror(:)
  integer :: norm

  integer :: narg, farg
  character (len=256) :: fname

  integer :: i, j
  integer :: ii, jj, kk

  integer :: itest

  real(kind=dp_t) :: global_error


  !---------------------------------------------------------------------------
  ! process the command line arguments

  narg = command_argument_count()

  ! defaults
  norm = 0
  plotfile_a = ""
  plotfile_b = ""

  farg = 1
  do while (farg <= narg)
     call get_command_argument(farg, value = fname)
     
     select case (fname)

     case ('--infile1')
        farg = farg + 1
        call get_command_argument(farg, value = plotfile_a)

     case ('--infile2')
        farg = farg + 1
        call get_command_argument(farg, value = plotfile_b)

     case ('-n','--norm')
        farg = farg + 1
        call get_command_argument(farg, value = fname)
        read(fname, *) norm

     case default
        exit

     end select
     farg = farg + 1
  enddo

  if (len_trim(plotfile_a) == 0 .OR. len_trim(plotfile_b) == 0) then
     print *, " "
     print *, "Compare two plotfiles, zone by zone, to machine precision"
     print *, "and report the maximum absolute and relative errors for each"
     print *, "variable."
     print *, " "
     print *, "usage:"
     print *, "   fcompare --infile1 file1 --infile2 file2"
     print *, " "
     stop
  endif

  !---------------------------------------------------------------------------
  ! build the plotfiles and do initial comparisons
  
  unit_a = unit_new()
  call build(pf_a, plotfile_a, unit_a)

  unit_b = unit_new()
  call build(pf_b, plotfile_b, unit_b)

  
  ! check if they are the same dimensionality
  if (pf_a%dim /= pf_b%dim) then
     call bl_error("ERROR: plotfiles have different numbers of spatial dimensions")
  endif


  ! check if they have the same number of levels
  if (pf_a%flevel /= pf_b%flevel) then
     call bl_error("ERROR: number of levels do not match")
  endif


  ! check if the finest domains are the same size
  flo_a = lwb(plotfile_get_pd_box(pf_a, pf_a%flevel))
  fhi_a = upb(plotfile_get_pd_box(pf_a, pf_a%flevel))

  flo_b = lwb(plotfile_get_pd_box(pf_b, pf_b%flevel))
  fhi_b = upb(plotfile_get_pd_box(pf_b, pf_b%flevel))

  if ( (flo_a(1) /= flo_b(1) .OR. fhi_a(1) /= fhi_b(1)) .OR. &
      ((flo_a(2) /= flo_b(2) .OR. fhi_a(2) /= fhi_b(2)) .AND. pf_a%dim >= 2) .OR. &
      ((flo_a(3) /= flo_b(3) .OR. fhi_a(3) /= fhi_b(3)) .AND. pf_a%dim == 3) ) then
     call bl_error("ERROR: grids do not match")
  endif

  
  ! check if they have the same number of variables
  if (pf_a%nvars /= pf_b%nvars) then
     print *, " "
     print *, "WARNING: number of variables do not match"
  endif

  allocate(aerror(pf_a%nvars))
  allocate(rerror(pf_a%nvars))

  
  ! in case the variables are not in the same order, figure out the
  ! mapping between pf_a and pf_b variables
  allocate(ivar_b(pf_a%nvars))

  do n_a = 1, pf_a%nvars

     ivar_b(n_a) = -1
     do n_b = 1, pf_b%nvars

        if (pf_a%names(n_a) == pf_b%names(n_b)) then
           ivar_b(n_a) = n_b
           exit
        endif

     enddo

     if (ivar_b(n_a) == -1) then
        print *, "WARNING: variable ", trim(pf_a%names(n_a)), &
                 " not found in plotfile 2"
     endif

  enddo


  ! also print out, as a diagnostic, those variables in plotfile 1 that
  ! are not in plotfile 2
  do n_b = 1, pf_b%nvars
     itest = -1
     do n_a = 1, pf_a%nvars

        if (pf_a%names(n_a) == pf_b%names(n_b)) then
           itest = n_a
           exit
        endif

     enddo

     if (itest == -1) then
        print *, "WARNING: variable ", trim(pf_b%names(n_b)), &
                 " not found in plotfile 1"
     endif

  enddo

  !---------------------------------------------------------------------------
  ! go level-by-level and patch-by-patch and compare the data
  
998 format(1x,a24,2x,a20,   2x,a20)
999 format(1x,70("-"))

  write (*,*) " "
  write (*,998) "variable name", "absolute error", "relative error"
  write (*,999)

  do i = 1, pf_a%flevel

     aerror(:) = ZERO
     rerror(:) = ZERO

     ! make sure the dx's agree
     dx_a = plotfile_get_dx(pf_a, i)
     dx_b = plotfile_get_dx(pf_b, i)  

     if ((dx_a(1) /= dx_b(1)) .OR. &
         (pf_a%dim >= 2 .AND. dx_a(2) /= dx_b(2)) .OR. &
         (pf_a%dim == 3 .AND. dx_a(3) /= dx_b(3))) then
        call bl_error("ERROR: grid dx does not match")
     endif


     ! make sure the number of boxes agree
     nboxes_a = nboxes(pf_a, i)
     nboxes_b = nboxes(pf_b, i)
     
     if (nboxes_a /= nboxes_b) then
        call bl_error("ERROR: number of boxes do not match")
     endif

     do j = 1, nboxes_a


        ! make sure that the grids match
        lo_a = lwb(get_box(pf_a, i, j))
        hi_a = upb(get_box(pf_a, i, j))

        lo_b = lwb(get_box(pf_b, i, j))
        hi_b = upb(get_box(pf_b, i, j))

        if ( (lo_a(1) /= lo_b(1) .OR. hi_a(1) /= hi_b(1)) .OR. &
             (pf_a%dim >= 2 .AND. (lo_a(2) /= lo_b(2) .OR. hi_a(2) /= hi_b(2))) .OR. &
             (pf_a%dim == 3 .AND. (lo_a(3) /= lo_b(3) .OR. hi_a(3) /= hi_b(3))) ) then
           call bl_error("ERROR: grids do not match")
        endif
        


        ! loop over the variables.  Take plotfile_a to be the one defining
        ! the list of variables, and bind them one-by-one.  Don't assume that
        ! the variables are in the same order in plotfile_b.

        do n_a = 1, pf_a%nvars

           n_b = ivar_b(n_a)
           if (n_b == -1) cycle

           call fab_bind_comp_vec(pf_a, i, j, (/ n_a /) )
           call fab_bind_comp_vec(pf_b, i, j, (/ n_b /) )

           p_a => dataptr(pf_a, i, j)
           p_b => dataptr(pf_b, i, j)

           select case (pf_a%dim)

           case (1)

              do ii = lo_a(1), hi_a(1)
                 if (norm == 0) then
                    aerror(n_a) = max(aerror(n_a), &
                                      abs(p_a(ii,1,1,1) - p_b(ii,1,1,1)))
                    
                    rerror(n_a) = max(rerror(n_a), &
                                      abs(p_a(ii,1,1,1) - p_b(ii,1,1,1)) / &
                                      abs(p_a(ii,1,1,1)))
                 else
                    aerror(n_a) = aerror(n_a) + &
                         (abs(p_a(ii,1,1,1) - p_b(ii,1,1,1)))**norm

                    rerror(n_a) = rerror(n_a) + &
                         (abs(p_a(ii,1,1,1) - p_b(ii,1,1,1)) / &
                          abs(p_a(ii,1,1,1)))**norm
                 endif
              enddo

           case (2)

              do jj = lo_a(2), hi_a(2)
                 do ii = lo_a(1), hi_a(1)

                    if (norm == 0) then
                       aerror(n_a) = max(aerror(n_a), &
                                         abs(p_a(ii,jj,1,1) - p_b(ii,jj,1,1)))
                    
                       rerror(n_a) = max(rerror(n_a), &
                                         abs(p_a(ii,jj,1,1) - &
                                             p_b(ii,jj,1,1)) / &
                                         abs(p_a(ii,jj,1,1)))
                    else
                       aerror(n_a) = aerror(n_a) + &
                            (abs(p_a(ii,jj,1,1) - p_b(ii,jj,1,1)))**norm

                       rerror(n_a) = rerror(n_a) + &
                            (abs(p_a(ii,jj,1,1) - p_b(ii,jj,1,1)) / &
                             abs(p_a(ii,jj,1,1)))**norm
                    endif

                 enddo
              enddo

           case (3)

              do kk = lo_a(3), hi_a(3)
                 do jj = lo_a(2), hi_a(2)
                    do ii = lo_a(1), hi_a(1)

                       if (norm == 0) then
                          aerror(n_a) = max(aerror(n_a), &
                                            abs(p_a(ii,jj,kk,1) - &
                                                p_b(ii,jj,kk,1)))
                    
                          rerror(n_a) = max(rerror(n_a), &
                                            abs(p_a(ii,jj,kk,1) - &
                                                p_b(ii,jj,kk,1)) / &
                                            abs(p_a(ii,jj,kk,1)))
                       else
                          aerror(n_a) = aerror(n_a) + &
                               (abs(p_a(ii,jj,kk,1) - &
                                    p_b(ii,jj,kk,1)))**norm

                          rerror(n_a) = rerror(n_a) + &
                               (abs(p_a(ii,jj,kk,1) - &
                                    p_b(ii,jj,kk,1)) / &
                                abs(p_a(ii,jj,kk,1)))**norm
                       endif

                    enddo
                 enddo
              enddo

           end select


           call fab_unbind(pf_a, i, j)
           call fab_unbind(pf_b, i, j)

        enddo  ! variable loop

     enddo  ! boxes loop

     ! normalize
     if (norm > 0) then

        do n_a = 1, pf_a%nvars
           aerror(n_a) = aerror(n_a)*product(dx_a(1:pf_a%dim))
           aerror(n_a) = aerror(n_a)**(ONE/real(norm,dp_t))

           rerror(n_a) = rerror(n_a)*product(dx_a(1:pf_a%dim))
           rerror(n_a) = rerror(n_a)**(ONE/real(norm,dp_t))
        enddo

     endif


     !------------------------------------------------------------------------
     ! print out the comparison report for this level
     
1000 format(1x,"level = ", i2)
1001 format(1x,a24,2x,g20.10,2x,g20.10)
1002 format(1x,a24,2x,a42)

     write (*,1000) i

     do n_a = 1, pf_a%nvars
        if (ivar_b(n_a) == -1) then
           write (*,1002) pf_a%names(n_a), "< variable not present in both files > "
        else
           write (*,1001) pf_a%names(n_a), aerror(n_a), rerror(n_a)
        endif
     enddo

     if (i == 1) then
        global_error = maxval(aerror(:))
     else
        global_error = max(global_error, maxval(aerror(:)))
     endif

  enddo  ! level loop

  if (global_error == ZERO) then
     print *, "PLOTFILES AGREE"
  endif

end program fcompare
