// $Id: SHODotAction.cc,v 1.8 2008/10/20 14:26:32 mgilbert Exp $
/*  Copyright (C) 2004-2006 John B. Shumway, Jr.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "SHODotAction.h"
#include <blitz/tinyvec.h>
#include "MultiLevelSampler.h"
#include "Beads.h"
#include "SuperCell.h"
#include "Paths.h"

SHODotAction::SHODotAction(const double tau, const double t, const double v0,
                           const double k)
  : tau(tau), t(t), v0(v0), k(k) {
std::cout << "thick,v0,mw2: " << t << "," << v0 << "," << k << std::endl;
}

double SHODotAction::getActionDifference(const MultiLevelSampler& sampler,
                                         const int level) {
  const Beads<NDIM>& sectionBeads=sampler.getSectionBeads();
  const Beads<NDIM>& movingBeads=sampler.getMovingBeads();
  const SuperCell& cell=sampler.getSuperCell();
  const int nStride=(int)pow(2,level);
  const int nSlice=sectionBeads.getNSlice();
  const IArray& index=sampler.getMovingIndex(); 
  const int nMoving=index.size();
  double deltaAction=0;
#if NDIM==3
  double ktstride = 0.5*k*tau*nStride;
#endif
  for (int islice=nStride; islice<nSlice-nStride; islice+=nStride) {
    for (int iMoving=0; iMoving<nMoving; ++iMoving) {
      const int i=index(iMoving);
      // Add action for moving beads.
      Vec delta=movingBeads(iMoving,islice);
      cell.pbc(delta);
#if NDIM==3
      deltaAction+=ktstride*(delta[0]*delta[0]+delta[1]*delta[1]);
      deltaAction+=(fabs(delta[2])>0.5*t)?v0*tau*nStride:0;
#else
      deltaAction+=(fabs(delta[0])>0.5*t)?v0*tau*nStride:0;
#endif
      // Subtract action for old beads.
      delta=sectionBeads(i,islice);
      cell.pbc(delta);
#if NDIM==3
      deltaAction-=ktstride*(delta[0]*delta[0]+delta[1]*delta[1]);
      deltaAction-=(fabs(delta[2])>0.5*t)?v0*tau*nStride:0;
#else
      deltaAction-=(fabs(delta[0])>0.5*t)?v0*tau*nStride:0;
#endif
    }
  }
  return deltaAction;
}

double SHODotAction::getTotalAction(const Paths& paths, 
    const int level) const {
  return 0;
}

void SHODotAction::getBeadAction(const Paths& paths, int ipart, int islice,
    double& u, double& utau, double& ulambda, Vec &fm, Vec &fp) const {
  Vec delta=paths(ipart,islice);
#if NDIM==3
  utau=0.5*k*(delta[0]*delta[0]+delta[1]*delta[1]);
  utau+=(fabs(delta[2])>0.5*t)?v0:0;
#else
  utau=(fabs(delta[0])>0.5*t)?v0:0;
#endif
  u=utau*tau;
}
