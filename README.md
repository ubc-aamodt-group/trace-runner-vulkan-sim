This project allows you to replay vkCmdTraceRaysKHR calls using memory/data dumps from Vulkan-Sim's mesa without actually invoking mesa itself. This will assume you already have gpgpu-sim_emerald and mesa set up already.

### Setting Up The Trace Runner
Install the required packages

    sudo apt-get install libboost-all-dev

Make a folder to contain the simulator

    mkdir vulkan-sim/

Clone and Build mesa in vulkan-sim/

    git clone git@github.com:ubc-aamodt-group/mesa.git
    cd mesa
    git checkout shadow_trace_runner

In the mesa repo, edit src/intel/vulkan/meson.build so **gpgpusim_lib_dir**, **embree_header_dir**, and **embree_lib_dir** matches the path on your system.
Afterwards, run the following to build:

    meson --prefix="${PWD}/lib" build/
    meson configure build/ -Dbuildtype=debug -D b_lundef=false

Clone and Build gpgpusim_emerald in vulkan-sim/

    git clone git@github.com:ubc-aamodt-group/gpgpu-sim_emerald.git
    cd gpgpu-sim_emerald
    git checkout shadow_trace_runner
    source setup_environment debug
    make -j

Clone and build this repo, vulkan_rt_trace_runner

    git clone git@github.com:ubc-aamodt-group/vulkan_rt_trace_runner.git
    cd vulkan_rt_trace_runner
    make



### Instructions
Two workloads are provided with the trace runner: raytracing_basic and ray_tracing_reflection

1. In **Makefile**, change **GPGPUSIM_PATH** to the path of your gpgpusim libcudart.so
2. Copy the folder **gpgpusimShaders/** (that contains the translated PTX shaders, descriptor set memory dumps, and shader binding table memory dumps) to **mesa/** (if you can run Vulkan-Sim's mesa on your computer then the folder should already be there)
3. Copy your gpgpusim.config to the same location as **./vulkan_rt_runner**

### Build and Run
    make
    ./vulkan_rt_runner <path_to_trace_folder> <workload>
    e.g.
    ./vulkan_rt_runner raytracing_basic/ raytracing_basic
    ./vulkan_rt_runner ray_tracing_reflection_dump/ ray_tracing_reflection
    ./vulkan_rt_runner raytracing_extended/ raytracing_extended