QING MAO PASS 6 - FULL FOOTPRINT

Problem fixed:
The Landscape itself was ~10 km square, but the meaningful Qing Mao terrain occupied
only about half its width and half its height, i.e. roughly one quarter of the area.

Pass 6 magnifies the authored Qing Mao terrain by ~1.72x in each horizontal axis.
This makes the mountain/foothill system occupy most of the Landscape.

IMPORT:
QingMao_Heightmap_2017_FullFootprint.png

Resolution:
2017 x 2017

Section Size:
63 x 63 Quads
Sections Per Component:
2 x 2
Components:
16 x 16

Location:
X 0
Y 0
Z 70000

Scale:
X 500
Y 500
Z 350

Use the ordinary/non-World-Partition level workflow that imported correctly.

VISIBLE ANCHORS:
After importing, run:
place_qingmao_visible_anchors.py

This version:
- finds the actual Landscape actor bounds
- maps normalized canon anchors into those real bounds
- raycasts down to the terrain
- creates visible cylinder marker posts with Outliner labels

It no longer assumes the Landscape is centered exactly where the generator expected.
