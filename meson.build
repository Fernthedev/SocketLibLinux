project('SocketLib', 'cpp', default_options : ['c_std=c11', 'cpp_std=c++20','-DDEBUG=true'])

#add_project_arguments(['-stdlib=libc++'], language : 'cpp')
#add_project_link_arguments(['-stdlib=libc++','-lstdc++fs'], language : 'cpp')
thread_dep = dependency('threads')

libDir = include_directories('shared')
mainDir = include_directories('test/include')

libSocketLib = shared_library('socket_lib', [
'src/Socket.cpp',
'src/ServerSocket.cpp',
'src/SocketHandler.cpp',
'src/Message.cpp'
], include_directories : libDir, override_options : ['cpp_std=c++20'], dependencies : thread_dep, cpp_args: ['-DDEBUG'])
libSocketLibDep = declare_dependency(link_with : libSocketLib,
  include_directories : libDir)


testSrc = ['test/src/main.cpp', 'test/src/ServerSocketTest.cpp']

executable('SocketLibMain', testSrc, include_directories : mainDir, dependencies : [thread_dep, libSocketLibDep])
