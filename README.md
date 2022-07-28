# ctraft 
A simple proof-of-concept, barebones minecraft engine clone running on Nintendo 3DS. Written mostly as a practice while learning `libctru` & `citro3d`. 

## Specifics
`ctraft` uses simplex noise to generate a basic terrain split into 16³ chunks, which are separately triangulated into meshes. 

Chunk generation and meshing are serviced asynchronously on a separate thread. Old 3ds only has a 30% timeslice available on the second core, so New3DS is recommended. 

## Resources & Credits
Based on the homebrew [devkitPro toolchain](https://devkitpro.org/).

Terrain generation using [OpenSimplex2 noise](https://github.com/Auburn/FastNoiseLite).

Included textures are taken from the WTFPL-licensed  [Good Morning Craft](https://www.curseforge.com/minecraft/texture-packs/good-morning-craft).
