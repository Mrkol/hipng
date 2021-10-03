# HipNg -- the hip and cool game engine

Being hip and cool means that this project tries to use the most modern and fashionable stuff:

 * C++20: coroutines and concepts (and eventually modules when cmake support gets better)
 * Unified executors
 * Flecs
 * Vulkan
 * Other stuff which isn't as hip and cool as these ones

The only manually managed dependency is vulkan. Figure out how to get it onto your system on your own.
All other dependencies are automagically fetched from github using CPM.cmake which itself is automagically fetched as well.
If some libraries are already available on the system CPM should find them on it's own.
Specifying a CPM_SOURCE_CACHE environment variable should prevent it from redownloading stuff all the time.
