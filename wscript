#! /usr/bin/env python
# encoding: utf-8
# klaymen1n

from __future__ import print_function
from waflib import Logs, Context, Configure
import os
import sys

VERSION = '1.0'
APPNAME = 're:vc'
top = '.'

Context.Context.line_just = 55

projects=['vendor/librw', 'src']

@Configure.conf
def check_pkg(conf, package, uselib_store, fragment, *k, **kw):
	errormsg = '{0} not available! Install {0} development package. Also you may need to set PKG_CONFIG_PATH environment variable'.format(package)
	confmsg = 'Checking for \'{0}\' sanity'.format(package)
	errormsg2 = '{0} isn\'t installed correctly. Make sure you installed proper development package for target architecture'.format(package)

	try:
		conf.check_cfg(package=package, args='--cflags --libs', uselib_store=uselib_store, *k, **kw )
	except conf.errors.ConfigurationError:
		conf.fatal(errormsg)

	try:
		conf.check_cxx(fragment=fragment, use=uselib_store, msg=confmsg, *k, **kw)
	except conf.errors.ConfigurationError:
		conf.fatal(errormsg2)

@Configure.conf
def get_taskgen_count(self):
	try: idx = self.tg_idx_count
	except: idx = 0 # don't set tg_idx_count to not increase counter
	return idx


def options(opt):
	grp = opt.add_option_group('Common options')
	
	grp.add_option('--disable-warns', action = 'store_true', dest = 'DISABLE_WARNS', default = False,
		help = 'disable warinigs while building [default: %default]')

	grp.add_option('-4', '--32bits', action = 'store_true', dest = 'TARGET32', default = False,
		help = 'allow targetting 32-bit engine(Linux/Windows/OSX x86 only) [default: %default]')

	grp.add_option('-D', '--debug-engine', action = 'store_true', dest = 'DEBUG_ENGINE', default = False,
		help = 'build with -DDEBUG [default: %default]')
	
	grp.add_option('--sanitize', action = 'store', dest = 'SANITIZE', default = '',
		help = 'build with sanitizers [default: %default]')
	
	opt.load('subproject compiler_optimizations')

	opt.add_subproject(projects)

	opt.load('xcompile compiler_cxx compiler_c clang_compilation_database waf_unit_test subproject strip_on_install')
	if sys.platform == 'win32':
		opt.load('msvc msdev msvs')
	opt.load('reconfigure')

def check_deps(conf):

	if conf.env.DEST_OS != 'android':
		if conf.env.DEST_OS != 'win32':
			conf.check_cfg(package='sdl2', uselib_store='SDL2', args=['--cflags', '--libs'])
			conf.check_cfg(package='openal', uselib_store='OPENAL', args=['--cflags', '--libs'])
			conf.check_cfg(package='libmpg123', uselib_store='MPG123', args=['--cflags', '--libs'])
			conf.check_cfg(package='sndfile', uselib_store='SNDFILE', args=['--cflags', '--libs'])
	else:
		conf.check(lib='SDL2', uselib_store='SDL2')
		#if conf.env.DEST_CPU != 'aarch64':
			#conf.check(lib='unwind', uselib_store='UNWIND')
			#conf.check(lib='crypto', uselib_store='CRYPTO')
			#conf.check(lib='ssl', uselib_store='SSL')
		conf.check(lib='android_support', uselib_store='ANDROID_SUPPORT')
		conf.check(lib='openal', uselib_store='OPENAL')
		conf.check(lib='mpg123')
		conf.check(lib='unwind', uselib_store='UNWIND')


def configure(conf):
	
	conf.load('subproject xcompile compiler_c compiler_cxx gccdeps gitversion clang_compilation_database waf_unit_test enforce_pic strip_on_install')
	conf.load('fwgslib reconfigure compiler_optimizations')
	conf.env.BIT32_MANDATORY = conf.options.TARGET32
	if conf.env.BIT32_MANDATORY:
		Logs.info('WARNING: will build engine for 32-bit target')
		conf.load('force_32bit')

	if conf.options.DEBUG_ENGINE:
		conf.env.append_unique('DEFINES', [
			'DEBUG'
		])
		
	if conf.env.DEST_OS == 'android':
		conf.env.append_unique('DEFINES', [
			'ANDROID=1', '_ANDROID=1',
			'LINUX=1', '_LINUX=1',
			'POSIX=1', '_POSIX=1',
			'GNUC',
			'NO_HOOK_MALLOC',
			'_DLL_EXT=.so',
			'AL_LIBTYPE_STATIC'
		])

	if conf.options.DISABLE_WARNS:
		compiler_optional_flags = ['-w']
	else:
		compiler_optional_flags = [
			'-Wall',
			'-fdiagnostics-color=always',
			'-Wcast-align',
			'-Wuninitialized',
			'-Winit-self',
			'-Wstrict-aliasing',
			'-Wno-reorder',
			'-Wno-unknown-pragmas',
			'-Wno-unused-function',
			'-Wno-unused-but-set-variable',
			'-Wno-unused-value',
			'-Wno-unused-variable',
			'-Wno-unused-command-line-argument',
			'-faligned-new',
		]

	c_compiler_optional_flags = [
		'-fnonconst-initializers' # owcc
	]

	cflags, linkflags = conf.get_optimization_flags()


	flags = []
	
	if conf.options.SANITIZE:
		flags += ['-fsanitize=%s'%conf.options.SANITIZE]
	
	if conf.env.DEST_OS == 'android':
		flags += [
			'-I'+os.path.abspath('.')+'/vendor/SDL',
			'-I'+os.path.abspath('.')+'/vendor/openal-soft/include/',
			'-I'+os.path.abspath('.')+'/vendor/mpg123/include',
			'-llog'
		]
	
	flags += ['-pipe', '-fPIC', '-L'+os.path.abspath('.')+'/lib/'+conf.env.DEST_OS+'/'+conf.env.DEST_CPU+'/']
	flags += ['-pthread']
	flags += ['-funwind-tables', '-g']
	
	cflags += flags
	linkflags += flags

	cxxflags = list(cflags)
	cxxflags += ['-std=c++11','-fpermissive']

	if conf.env.COMPILER_CC == 'gcc':
		conf.define('COMPILER_GCC', 1)
		
	conf.check_cc(cflags=cflags, linkflags=linkflags, msg='Checking for required C flags')
	conf.check_cxx(cxxflags=cxxflags, linkflags=linkflags, msg='Checking for required C++ flags')
	
	conf.env.append_unique('CFLAGS', cflags)
	conf.env.append_unique('CXXFLAGS', cxxflags)
	conf.env.append_unique('LINKFLAGS', linkflags)

	cxxflags += conf.filter_cxxflags(compiler_optional_flags, cflags)
	cflags += conf.filter_cflags(compiler_optional_flags + c_compiler_optional_flags, cflags)

	conf.env.append_unique('CFLAGS', cflags)
	conf.env.append_unique('CXXFLAGS', cxxflags)
	conf.env.append_unique('LINKFLAGS', linkflags)

	if conf.env.DEST_OS == 'android':
		conf.env.PREFIX = conf.env.PREFIX.replace('/lib', '') #fuck this
		conf.env.LIBDIR = conf.env.BINDIR = conf.env.PREFIX
	

	check_deps( conf )
	conf.add_subproject(projects)

def build(bld):
	if bld.env.DEST_OS == 'android':
		sdl_path = os.path.join('lib', bld.env.DEST_OS, bld.env.DEST_CPU, 'libSDL2.so')
		bld.install_files(bld.env.LIBDIR, [sdl_path])
	bld.add_subproject(projects)
