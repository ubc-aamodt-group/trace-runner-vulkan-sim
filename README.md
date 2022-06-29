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
    ninja -C build/ install

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
Workloads that are provided with the trace runner: raytracing_basic, ray_tracing_reflection, raytracing_extended, RayTracingInVulkan-Scene5, RayTracingInVulkan-Scene6
Step 2 can be ignored if you are running the provided workloads.

1. In **Makefile**, change **GPGPUSIM_PATH** to the path of your gpgpusim libcudart.so
2. Only if you are dumping new workloads!! Copy the folder **gpgpusimShaders/** (that contains the translated PTX shaders, descriptor set memory dumps, and shader binding table memory dumps) to **mesa/** (if you can run Vulkan-Sim's mesa on your computer then the folder should already be there)
3. Copy your gpgpusim.config to the same location as **./vulkan_rt_runner**

If compilation fails due to undefined reference to libboost, you need to download libboost from https://www.boost.org/ and build from source, then include the path in the Makefile.

### Build and Run
    make
    ./vulkan_rt_runner <path_to_trace_folder> <workload>
    e.g.
    ./vulkan_rt_runner raytracing_basic_448x320/ raytracing_basic
    ./vulkan_rt_runner ray_tracing_reflection_448x320/ ray_tracing_reflection
    ./vulkan_rt_runner raytracing_extended_448x320/ raytracing_extended
    ./vulkan_rt_runner RayTracingInVulkan-Scene5-448x320-Baseline RayTracingInVulkan-Scene5
    ./vulkan_rt_runner RayTracingInVulkan-Scene6-448x320-Baseline RayTracingInVulkan-Scene6
