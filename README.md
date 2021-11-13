This project allows you to replay vkCmdTraceRaysKHR calls using memory/data dumps from Vulkan-Sim's mesa without actually invoking mesa itself. This will assume you already have gpgpu-sim_emerald and mesa set up already.

### Requirements
    sudo apt-get install libboost-all-dev

### Instructions
1. In **Makefile**, change **GPGPUSIM_PATH** to the path of your gpgpusim libcudart.so
2. Copy the folder **gpgpusimShaders/** (that contains the translated PTX shaders, descriptor set memory dumps, and shader binding table memory dumps) to **mesa/** (if you can run Vulkan-Sim's mesa on your computer then the folder should already be there)
3. Copy your gpgpusim.config to the same location as **./vulkan_rt_runner**

A sample dump folder and gpgpusim.config can be found under the folder **sample_dump**. Please set it like the following.

    cp sample_dump/gpgpusim.config .
    cp -r sample_dump/gpgpusimShaders ../mesa/.

### Build and Run
    make
    ./vulkan_rt_runner
