# ctraft 

![preview](https://github.com/kejran/ctraft/blob/master/screenshot.png?raw=true)

A simple proof-of-concept, barebones (and shameless) minecraft engine clone running on Nintendo 3DS. Written mostly as a practice while learning `libctru` & `citro3d`. 

### Releases
Latest `.3dsx` CI build can be downloaded [from this link.](https://github.com/kejran/ctraft/releases/latest/download/ctraft.3dsx)

You can download it directly via FBI:

![qr](https://user-images.githubusercontent.com/6992153/182961923-7c319f98-7c38-48c6-8501-1533be1df431.png)


This code goes hard. Feel free to screenshot.

### How do you pronounce _ctraft_?
I do not.

## Specifics
`ctraft` uses simplex noise to generate a basic terrain split into 16³ chunks, which are separately triangulated into meshes. 

Currently, terrain surface is generated from a 2d simplex heightmap. Ore is placed below, based on a 3d noise density, while caves are carved on string intersections of two simplex 3d isosurfaces. Above the surface, grass and trees are generated depending on a basic hash values. 

Chunk generation and meshing are serviced asynchronously on a separate thread. Old 3ds only has a 30% timeslice available on the second core, so New3DS is recommended. 

Currently no mesh optimization is being done, although vertex data is shared between faces.

There is no proper light support - but a simple static, vertex-based ambient occlusion is being generated during meshing.

## Resources & Credits
Based on the homebrew [devkitPro toolchain](https://devkitpro.org/).

Terrain generation using [OpenSimplex2 noise](https://github.com/Auburn/FastNoiseLite).

OpenSimplex2 and [isosurface intersection cave generation](https://github.com/KdotJPG/Cave-Tech-Demo) by [K.jpg](https://github.com/KdotJPG)

Included textures are taken from the WTFPL-licensed  [Good Morning Craft](https://www.curseforge.com/minecraft/texture-packs/good-morning-craft).
