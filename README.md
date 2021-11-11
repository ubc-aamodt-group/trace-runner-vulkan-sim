This project allows you to replay vkCmdTraceRaysKHR calls using memory/data dumps from Vulkan-Sim's mesa without actually invoking mesa itself.

### Requirements
    sudo apt-get install libboost-all-dev

### Instructions
1. In **Makefile**, change **GPGPUSIM_PATH** to the path of your gpgpusim libcudart.so
2. Copy the folder **gpgpusimShaders/** (that contains the translated PTX shaders, descriptor set memory dumps, and shader binding table memory dumps) to **mesa/** (if you can run Vulkan-Sim's mesa on your computer then the folder should already be there)

### Build and Run with the following
    make
    ./vulkan_rt_runner
