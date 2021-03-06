include "Dependencies.lua"

workspace "Vulkan"
	architecture "x86_64"
	startproject "Vulkan"

	configurations
	{
		"Debug",
		"Release"
	}

	flags
	{
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group "Dependencies"
	include "Vulkan/vendor/GLFW"
group ""

include "Vulkan"