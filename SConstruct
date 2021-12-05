env = Environment()
env.VariantDir('build', 'src')

env.SConscript(['build/SConscript',
                'build/third_party/SConscript'], exports='env')
