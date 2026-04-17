// commnads to create spirv files from shader & some info about spriv compliations

// command for vertex
C:\Users\morphed\AppData\Local\Android\Sdk\ndk\29.0.14206865\shader-tools\windows-x86_64\glslc.exe -fshader-stage=vertex --target-env=vulkan1.1 -o shader.vert.spv shader.vert
// command for fragment
C:\Users\morphed\AppData\Local\Android\Sdk\ndk\29.0.14206865\shader-tools\windows-x86_64\glslc.exe -fshader-stage=fragment --target-env=vulkan1.1 -o shader.frag.spv shader.frag

--> -fshader-stage asks for shader type/stage
vertex
fragment
compute
geometry
tesscontrol
tesseval

--> --target-env : asks for vul API version (latest is 1.3 supported)