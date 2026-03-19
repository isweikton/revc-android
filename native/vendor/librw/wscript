#! /usr/bin/env python
# encoding: utf-8

from waflib import Utils

top = '.'
PROJECT_NAME = 'vendor/librw'

def options(opt):
	# stub
	return

def configure(conf):
	conf.env.append_unique('DEFINES',[
		'RW_GL3',
		'LIBRW_SDL2',
		'USE_SDL2',
	])

def build(bld):
	source = [
		'src/anim.cpp',
		'src/base.cpp',
		'src/bmp.cpp',
		'src/camera.cpp',
		'src/charset.cpp',
		'src/clump.cpp',
		'src/d3d/d3d.cpp',
		'src/d3d/d3d8.cpp',
		'src/d3d/d3d8matfx.cpp',
		'src/d3d/d3d8render.cpp',
		'src/d3d/d3d8skin.cpp',
		'src/d3d/d3d9.cpp',
		'src/d3d/d3d9matfx.cpp',
		'src/d3d/d3d9render.cpp',
		'src/d3d/d3d9skin.cpp',
		'src/d3d/d3ddevice.cpp',
		'src/d3d/d3dimmed.cpp',
		'src/d3d/d3drender.cpp',
		'src/engine.cpp',
		'src/error.cpp',
		'src/frame.cpp',
		'src/geometry.cpp',
		'src/geoplg.cpp',
		'src/gl/gl3.cpp',
		'src/gl/gl3device.cpp',
		'src/gl/gl3immed.cpp',
		'src/gl/gl3matfx.cpp',
		'src/gl/gl3pipe.cpp',
		'src/gl/gl3raster.cpp',
		'src/gl/gl3render.cpp',
		'src/gl/gl3shader.cpp',
		'src/gl/gl3skin.cpp',
		'src/gl/glad/glad.c',
		'src/hanim.cpp',
		'src/image.cpp',
		'src/light.cpp',
		'src/lodepng/lodepng.cpp',
		'src/matfx.cpp',
		'src/ps2/pds.cpp',
		'src/pipeline.cpp',
		'src/plg.cpp',
		'src/png.cpp',
		'src/prim.cpp',
		'src/ps2/ps2.cpp',
		'src/ps2/ps2device.cpp',
		'src/ps2/ps2matfx.cpp',
		'src/ps2/ps2raster.cpp',
		'src/ps2/ps2skin.cpp',
		'src/raster.cpp',
		'src/render.cpp',
		'src/skin.cpp',
		'src/texture.cpp',
		'src/tga.cpp',
		'src/tristrip.cpp',
		'src/userdata.cpp',
		'src/uvanim.cpp',
		'src/gl/wdgl.cpp',
		'src/world.cpp',
		'src/d3d/xbox.cpp',
		'src/d3d/xboxmatfx.cpp',
		'src/d3d/xboxskin.cpp',
		'src/d3d/xboxvfmt.cpp',
	]
	
	includes = [
	]

	defines = []

	libs = ['SDL2']

	bld.stlib(
		source   = source,
		target   = 'rw',
		name	 = PROJECT_NAME,
		features = 'c cxx',
		includes = includes,
		defines  = defines,
		use	  = libs,
		subsystem = bld.env.MSVC_SUBSYSTEM,
		idx	  = bld.get_taskgen_count()
	)
