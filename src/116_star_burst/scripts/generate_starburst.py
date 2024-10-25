#!/usr/bin/env python
# -*- coding: utf-8 -*-

import diffractsim
diffractsim.set_backend("CPU")

from diffractsim import PolychromaticField,ApertureFromImage,Lens, cf, mm, cm

F = PolychromaticField(
    spectrum=2 * cf.illuminant_d65,
    extent_x=20.0*mm, extent_y=20.0*mm,
    Nx=2048,
    Ny=2048
)

F.add(ApertureFromImage(
  "./apeture_9_0_2.png",
  image_size=(50.0/2.8*mm, 50.0/2.8*mm),
  simulation = F
))

F.add(Lens(
  f=1000*mm,
  radius=25*mm
))

F.propagate(1100*mm)

rgb = F.get_colors()
F.plot_colors(rgb, xlim=[-10*mm,10*mm], ylim=[-10*mm,10*mm])
