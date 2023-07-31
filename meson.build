project('SocketLib', 'cpp', default_options : ['c_std=c11', 'cpp_std=c++20'])

add_global_arguments('-DDEBUG=true', language : 'cpp')
add_global_arguments('-DFMT_HEADER_ONLY', language : 'cpp')





# Compiler flags for enabling profiling (if option is true)
if get_option('enable_profiling')
    # Find and link against gperftools/profiler
    gperftools_dep = [] #dependency('google-pprof')

    add_global_link_arguments('-lprofiler', language: 'cpp')
    add_global_arguments('-DPROFILE_BUILD', language: 'cpp')
else
    gperftools_dep = []
endif

# For release builds, set the optimization level to -O3 and add -lprofiler
if get_option('buildtype') == 'release'
  add_global_arguments('-O3', language: 'cpp')
endif

#add_project_arguments(['-stdlib=libc++'], language : 'cpp')
#add_project_link_arguments(['-stdlib=libc++','-lstdc++fs'], language : 'cpp')
thread_dep = dependency('threads')

libDir = include_directories('shared')
mainDir = include_directories('test/include')

fmtInclude = include_directories('extern/includes/fmt/fmt/include')

libSocketLib = shared_library('socket_lib', [
'src/Socket.cpp',
'src/ServerSocket.cpp',
'src/SocketHandler.cpp',
'src/Message.cpp',
'src/ClientSocket.cpp',
'src/SocketLogger.cpp',
], include_directories : [libDir, fmtInclude], override_options : ['cpp_std=c++20'], dependencies : thread_dep, cpp_args: ['-DDEBUG'])
libSocketLib_dep = declare_dependency(link_with : libSocketLib, include_directories : [libDir, fmtInclude])


testSrc = ['test/src/main.cpp', 'test/src/ServerSocketTest.cpp', 'test/src/ClientSocketTest.cpp']

executable('SocketLibMain', testSrc, include_directories : mainDir, dependencies : [thread_dep, libSocketLib_dep, gperftools_dep], override_options : ['cpp_std=c++20'])
