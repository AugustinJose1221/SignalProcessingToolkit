# A caller outside the tree, building against the library once it is installed.
#
# WHY THIS EXISTS. Install rules that nobody consumed are install rules that do
# not work. This is the smallest thing that proves the whole road: a project
# that knows nothing of this repository beyond find_package, and a program that
# fails rather than prints if the library came out wrong.
cmake_minimum_required(VERSION 3.17)

project(FfittConsumer C)

find_package(ffitt REQUIRED)

add_executable(use use.c)
target_link_libraries(use ffitt::ffitt)
