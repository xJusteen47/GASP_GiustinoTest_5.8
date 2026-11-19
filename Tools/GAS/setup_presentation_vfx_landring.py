"""Rebuilds only NS_GSTN_LandRing (smoke redo A1 iterations), for the capture's -Pre: setup_presentation_vfx_puff,setup_presentation_vfx_landring"""
import importlib

import setup_presentation_vfx as V


def run():
    importlib.reload(V)
    del V.ERRORS[:]
    s = V.land_ring()
    V.L.compile_and_wait(s)
    V.EAL.save_loaded_asset(s)
    return f"LandRing rebuilt, errors={V.ERRORS}"
