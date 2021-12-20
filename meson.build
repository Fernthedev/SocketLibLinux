project('SocketLib', 'cpp', default_options : ['c_std=c11', 'cpp_std=c++20', 'optimization=3'])

add_global_arguments('-DDEBUG=true', language : 'cpp')
add_global_arguments('-DFMT_HEADER_ONLY', language : 'cpp')

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
libSocketLibDep = declare_dependency(link_with : libSocketLib,
  include_directories : [libDir, fmtInclude])


testSrc = ['test/src/main.cpp', 'test/src/ServerSocketTest.cpp']

executable('SocketLibMain', testSrc, include_directories : mainDir, dependencies : [thread_dep, libSocketLibDep])
