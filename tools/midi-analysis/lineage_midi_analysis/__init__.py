"""Offline MIDI groove-variation analysis (Lineage's design doc:
f214f85b-lineagemidianalysisdesign.md). Mines a folder of drum MIDI files
into a vocabulary.json describing how real performances vary a base
pattern — timing shifts, embellishments, omissions, substitutions,
density changes, and fills — for Lineage's mutation/rule engine to sample
from later. This package only produces the data file; nothing here runs
inside the plugin.
"""
