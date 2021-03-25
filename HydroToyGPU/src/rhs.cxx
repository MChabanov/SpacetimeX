#include <fixmath.hxx>
#include <loop_device.hxx>

#include <cctk.h>
#include <cctk_Arguments_Checked.h>
#include <cctk_Parameters.h>

// #include <array>
// #include <cassert>
// #include <cmath>
// #include <iostream>

namespace HydroToyGPU {
using namespace std;
using namespace Loop;

extern "C" void HydroToyGPU_RHS(CCTK_ARGUMENTS) {
  DECLARE_CCTK_ARGUMENTS_HydroToyGPU_RHS;
  DECLARE_CCTK_PARAMETERS;

  const array<CCTK_REAL, dim> dx = {CCTK_DELTA_SPACE(0), CCTK_DELTA_SPACE(1),
                                    CCTK_DELTA_SPACE(2)};
  const array<CCTK_REAL, dim> dx1 = {1 / dx[0], 1 / dx[1], 1 / dx[2]};
  // const CCTK_REAL dV = dx[0] * dx[1] * dx[2]; // cell volume
  // const CCTK_REAL dV1 = 1 / dV;

  // Cell centred grid functions
  const GridDescBaseDevice grid(cctkGH);
  constexpr array<int, dim> cell_centred = {1, 1, 1};
  const GF3D2layout gf_layout(cctkGH, cell_centred);

  const GF3D2<CCTK_REAL> gf_rhsrho(gf_layout, rhsrho);
  const GF3D2<CCTK_REAL> gf_rhsmomx(gf_layout, rhsmomx);
  const GF3D2<CCTK_REAL> gf_rhsmomy(gf_layout, rhsmomy);
  const GF3D2<CCTK_REAL> gf_rhsmomz(gf_layout, rhsmomz);
  const GF3D2<CCTK_REAL> gf_rhsetot(gf_layout, rhsetot);

  // Fluxes in x direction
  const GF3D2layout gf_fxlayout(cctkGH, {0, 1, 1});
  const GF3D2<const CCTK_REAL> gf_fxrho(gf_fxlayout, fxrho);
  const GF3D2<const CCTK_REAL> gf_fxmomx(gf_fxlayout, fxmomx);
  const GF3D2<const CCTK_REAL> gf_fxmomy(gf_fxlayout, fxmomy);
  const GF3D2<const CCTK_REAL> gf_fxmomz(gf_fxlayout, fxmomz);
  const GF3D2<const CCTK_REAL> gf_fxetot(gf_fxlayout, fxetot);

  // Fluxes in y direction
  const GF3D2layout gf_fylayout(cctkGH, {1, 0, 1});
  const GF3D2<const CCTK_REAL> gf_fyrho(gf_fylayout, fyrho);
  const GF3D2<const CCTK_REAL> gf_fymomx(gf_fylayout, fymomx);
  const GF3D2<const CCTK_REAL> gf_fymomy(gf_fylayout, fymomy);
  const GF3D2<const CCTK_REAL> gf_fymomz(gf_fylayout, fymomz);
  const GF3D2<const CCTK_REAL> gf_fyetot(gf_fylayout, fyetot);

  // Fluxes in z direction
  const GF3D2layout gf_fzlayout(cctkGH, {1, 1, 0});
  const GF3D2<const CCTK_REAL> gf_fzrho(gf_fzlayout, fzrho);
  const GF3D2<const CCTK_REAL> gf_fzmomx(gf_fzlayout, fzmomx);
  const GF3D2<const CCTK_REAL> gf_fzmomy(gf_fzlayout, fzmomy);
  const GF3D2<const CCTK_REAL> gf_fzmomz(gf_fzlayout, fzmomz);
  const GF3D2<const CCTK_REAL> gf_fzetot(gf_fzlayout, fzetot);

  // Transport
  // dt rho + d_i (rho vel^i) = 0
  // dt mom_j + d_i (mom_j vel^i) = 0
  // dt etot + d_i (etot vel^i) = 0

  const auto calcupdate =
      [=] CCTK_ATTRIBUTE_ALWAYS_INLINE CCTK_HOST CCTK_DEVICE(
          CCTK_REAL fx_m, CCTK_REAL fx_p, CCTK_REAL fy_m, CCTK_REAL fy_p,
          CCTK_REAL fz_m, CCTK_REAL fz_p) {
        // return -dV1 * (fx_p - fx_m + fy_p - fy_m + fz_p - fz_m);
        return -dx1[0] * (fx_p - fx_m) - dx1[1] * (fy_p - fy_m) -
               dx1[2] * (fz_p - fz_m);
      };

  constexpr auto DI = PointDesc::DI;
  grid.loop_int_device<1, 1, 1>(
      grid.nghostzones, [=] CCTK_ATTRIBUTE_ALWAYS_INLINE CCTK_HOST CCTK_DEVICE(
                            const PointDesc &p) {
        // Neighbouring "plus" and "minus" face indices in the x, y, and z
        // directions
        const auto Imx = p.I;
        const auto Imy = p.I;
        const auto Imz = p.I;
        const auto Ipx = p.I + DI[0];
        const auto Ipy = p.I + DI[1];
        const auto Ipz = p.I + DI[2];

        gf_rhsrho(p.I) =
            calcupdate(gf_fxrho(Imx), gf_fxrho(Ipx), gf_fyrho(Imy),
                       gf_fyrho(Ipy), gf_fzrho(Imz), gf_fzrho(Ipz));
        gf_rhsmomx(p.I) =
            calcupdate(gf_fxmomx(Imx), gf_fxmomx(Ipx), gf_fymomx(Imy),
                       gf_fymomx(Ipy), gf_fzmomx(Imz), gf_fzmomx(Ipz));
        gf_rhsmomy(p.I) =
            calcupdate(gf_fxmomy(Imx), gf_fxmomy(Ipx), gf_fymomy(Imy),
                       gf_fymomy(Ipy), gf_fzmomy(Imz), gf_fzmomy(Ipz));
        gf_rhsmomz(p.I) =
            calcupdate(gf_fxmomz(Imx), gf_fxmomz(Ipx), gf_fymomz(Imy),
                       gf_fymomz(Ipy), gf_fzmomz(Imz), gf_fzmomz(Ipz));
        gf_rhsetot(p.I) =
            calcupdate(gf_fxetot(Imx), gf_fxetot(Ipx), gf_fyetot(Imy),
                       gf_fyetot(Ipy), gf_fzetot(Imz), gf_fzetot(Ipz));
      });

  // Fill boundary and ghost cells (TODO: avoid this)
  grid.loop_bnd_device<1, 1, 1>(
      grid.nghostzones, [=] CCTK_ATTRIBUTE_ALWAYS_INLINE CCTK_HOST CCTK_DEVICE(
                            const PointDesc &p) {
        gf_rhsrho(p.I) = 0;
        gf_rhsmomx(p.I) = 0;
        gf_rhsmomy(p.I) = 0;
        gf_rhsmomz(p.I) = 0;
        gf_rhsetot(p.I) = 0;
      });
  grid.loop_ghosts_device<1, 1, 1>(
      grid.nghostzones, [=] CCTK_ATTRIBUTE_ALWAYS_INLINE CCTK_HOST CCTK_DEVICE(
                            const PointDesc &p) {
        gf_rhsrho(p.I) = 0;
        gf_rhsmomx(p.I) = 0;
        gf_rhsmomy(p.I) = 0;
        gf_rhsmomz(p.I) = 0;
        gf_rhsetot(p.I) = 0;
      });
}

} // namespace HydroToyGPU