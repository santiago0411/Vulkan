@echo off
D:/VulkanSDK/1.2.170.0/Bin32/glslangValidator.exe -V Shader.vert
D:/VulkanSDK/1.2.170.0/Bin32/glslangValidator.exe -V Shader.frag

MOVE vert.spv cache/vert.spv
MOVE frag.spv cache/frag.spv

PAUSE