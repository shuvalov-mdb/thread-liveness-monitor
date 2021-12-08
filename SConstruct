# build with `scons --debug-build` for debug.
AddOption(
    '--debug-build',
    action='store_true',
    help='debug build',
    default=False)

env = Environment()

if GetOption('debug_build'):
    env.ParseFlags('-DDEBUG')
    variant_dir = 'build/debug'
else:
    variant_dir = 'build/release'

env.SConscript('src/SConscript', variant_dir=variant_dir, duplicate=0, exports='env')
