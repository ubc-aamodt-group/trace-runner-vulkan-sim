// Copyright (c) 2022, Mohammadreza Saed, Yuan Hsi Chou, Lufei Liu, Tor M. Aamodt,
// The University of British Columbia
// All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:

// Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
// Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution. Neither the name of
// The University of British Columbia nor the names of its contributors may be
// used to endorse or promote products derived from this software without
// specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
//#include "vulkan/vulkan_core.h"
#include "../mesa/include/vulkan/vulkan_core.h"
// #include "vulkan/gpgpusim_calls_from_mesa.h"
#include "../mesa/src/compiler/shader_enums.h"

#include "../gpgpu-sim_emerald/libcuda/gpgpu_context.h"
#include "../gpgpu-sim_emerald/libcuda/cuda_api_object.h"
#include "../gpgpu-sim_emerald/src/gpgpu-sim/gpu-sim.h"
#include "../gpgpu-sim_emerald/src/cuda-sim/ptx_loader.h"
#include "../gpgpu-sim_emerald/src/cuda-sim/cuda-sim.h"
#include "../gpgpu-sim_emerald/src/cuda-sim/ptx_ir.h"
#include "../gpgpu-sim_emerald/src/cuda-sim/ptx_parser.h"
#include "../gpgpu-sim_emerald/src/gpgpusim_entrypoint.h"
#include "../gpgpu-sim_emerald/src/stream_manager.h"
#include "../gpgpu-sim_emerald/src/abstract_hardware_model.h"


#include<glm/glm.hpp>

#define BOOST_FILESYSTEM_VERSION 3
#define BOOST_FILESYSTEM_NO_DEPRECATED 
#include <boost/filesystem.hpp>

// externs from gpgpusim_calls_from_mesa.h
//extern void gpgpusim_addTreelets_cpp(VkAccelerationStructureKHR accelerationStructure);
extern uint32_t gpgpusim_registerShader_cpp(char * shaderPath, uint32_t shader_type);

extern void gpgpusim_vkCmdTraceRaysKHR_cpp(
                      void *raygen_sbt,
                      void *miss_sbt,
                      void *hit_sbt,
                      void *callable_sbt,
                      bool is_indirect,
                      uint32_t launch_width,
                      uint32_t launch_height,
                      uint32_t launch_depth,
                      uint64_t launch_size_addr);

// extern void gpgpusim_setDescriptorSet_cpp(uint32_t setID, uint32_t descID, void *address, uint32_t size, VkDescriptorType type);
//extern void gpgpusim_setDescriptorSet_cpp(void *set);
extern void gpgpusim_setDescriptorSetFromLauncher_cpp(void *address, void *deviceAddress, uint32_t setID, uint32_t descID);
extern void gpgpusim_setStorageImageFromLauncher_cpp(void *address, 
                                                    void *deviceAddress, 
                                                    uint32_t setID, 
                                                    uint32_t descID, 
                                                    uint32_t width,
                                                    uint32_t height,
                                                    VkFormat format,
                                                    uint32_t VkDescriptorTypeNum,
                                                    uint32_t n_planes,
                                                    uint32_t n_samples,
                                                    VkImageTiling tiling,
                                                    uint32_t isl_tiling_mode, 
                                                    uint32_t row_pitch_B);
extern void gpgpusim_setTextureFromLauncher_cpp(void *address, 
                                                void *deviceAddress, 
                                                uint32_t setID, 
                                                uint32_t descID, 
                                                uint64_t size,
                                                uint32_t width,
                                                uint32_t height,
                                                VkFormat format,
                                                uint32_t VkDescriptorTypeNum,
                                                uint32_t n_planes,
                                                uint32_t n_samples,
                                                VkImageTiling tiling,
                                                uint32_t isl_tiling_mode,
                                                uint32_t row_pitch_B,
                                                uint32_t filter);


namespace fs = boost::filesystem;

cudaError_t gpgpusim_malloc(void **devPtr, size_t size, gpgpu_context *gpgpu_ctx = NULL)
{
    gpgpu_context *ctx;
    if (gpgpu_ctx) {
        ctx = gpgpu_ctx;
    } else {
        ctx = GPGPU_Context();
    }
    CUctx_st *context = GPGPUSim_Context(ctx);
    *devPtr = context->get_device()->get_gpgpu()->gpu_malloc(size);
    if (g_debug_execution >= 3) {
        printf("GPGPU-Sim PTX: cudaMallocing %zu bytes starting at 0x%llx..\n",
                size, (unsigned long long)*devPtr);
        ctx->api->g_mallocPtr_Size[(unsigned long long)*devPtr] = size;
    }
    if (*devPtr) {
        return cudaSuccess;
    } else {
        assert(0);
        return cudaErrorMemoryAllocation;
    }
}

cudaError_t gpgpusim_memcpy(void *dst, const void *src, size_t count,
                   enum cudaMemcpyKind kind, gpgpu_context *gpgpu_ctx = NULL) {
  gpgpu_context *ctx;
  if (gpgpu_ctx) {
    ctx = gpgpu_ctx;
  } else {
    ctx = GPGPU_Context();
  }

  if (kind == cudaMemcpyHostToDevice)
    ctx->the_gpgpusim->g_the_gpu->memcpy_to_gpu(dst, src, count);
  else if (kind == cudaMemcpyDeviceToHost)
    assert(0);
  else if (kind == cudaMemcpyDeviceToDevice)
    ctx->the_gpgpusim->g_the_gpu->memcpy_gpu_to_gpu(dst, src, count);
  else if (kind == cudaMemcpyDefault) {
    assert(0);
    if ((size_t)src >= GLOBAL_HEAP_START) {
      if ((size_t)dst >= GLOBAL_HEAP_START)
        ctx->the_gpgpusim->g_stream_manager->push(stream_operation(
            (size_t)src, (size_t)dst, count, 0));  // device to device
      else
        ctx->the_gpgpusim->g_stream_manager->push(
            stream_operation((size_t)src, dst, count, 0));  // device to host
    } else {
      if ((size_t)dst >= GLOBAL_HEAP_START)
        ctx->the_gpgpusim->g_stream_manager->push(
            stream_operation(src, (size_t)dst, count, 0));
      else {
        printf(
            "GPGPU-Sim PTX: cudaMemcpy - ERROR : unsupported transfer: host to "
            "host\n");
        abort();
      }
    }
  } else {
    printf("GPGPU-Sim PTX: cudaMemcpy - ERROR : unsupported cudaMemcpyKind\n");
    abort();
  }
  return cudaSuccess;
}
struct shaderInfo
{
    char shaderPath[200];
    gl_shader_stage shaderType;
    int shaderID;

    bool operator < (const shaderInfo& str) const
    {
        return (shaderID < str.shaderID);
    }
};

#define MAX_DESCRIPTOR_SETS 1
#define MAX_DESCRIPTOR_SET_BINDINGS 32
void* descriptorSets[MAX_DESCRIPTOR_SETS][MAX_DESCRIPTOR_SET_BINDINGS] = {nullptr}; // host side
void* deviceDescriptorSets[MAX_DESCRIPTOR_SETS][MAX_DESCRIPTOR_SET_BINDINGS] = {nullptr}; // device side

// ray_tracing_reflection descriptor set data structures
namespace ray_tracing_reflection
{
    struct ObjMaterial
    {
        glm::vec3 diffuse{0.7f, 0.7f, 0.7f};
        glm::vec3 specular{0.7f, 0.7f, 0.7f};
        float     shininess{0.f};
    };

    struct ObjVertex
    {
        glm::vec3 pos;
        glm::vec3 nrm;
    };

    struct ObjModelCpu
    {
        std::vector<ObjVertex> vertices;
        std::vector<uint32_t>  indices;
        std::vector<int32_t>   mat_index;
    };

    struct UniformData
    {
        glm::mat4 view_inverse;
        glm::mat4 proj_inverse;
    } uniform_data;

    struct ObjModelGpu
    {
        //uint32_t nb_indices; // not actually sent
        //uint32_t nb_vertices; // not actually sent
        void *vertex_buffer;
        void *index_buffer;
        void *mat_color_buffer;
        void *mat_index_buffer;
    };

    //void* descriptorSets[1][4];
}


long fseek_filesize(const char *filename)
{
    FILE *fp = NULL;
    long off;

    fp = fopen(filename, "r");
    if (fp == NULL)
    {
        printf("failed to fopen %s\n", filename);
        exit(EXIT_FAILURE);
    }

    if (fseek(fp, 0, SEEK_END) == -1)
    {
        printf("failed to fseek %s\n", filename);
        exit(EXIT_FAILURE);
    }

    off = ftell(fp);
    if (off == -1)
    {
        printf("failed to ftell %s\n", filename);
        exit(EXIT_FAILURE);
    }

    //printf("[*] fseek_filesize - file: %s, size: %ld\n", filename, off);

    if (fclose(fp) != 0)
    {
        printf("failed to fclose %s\n", filename);
        exit(EXIT_FAILURE);
    }

    return off;
}


std::vector<std::string> split (const std::string &s, char delim) {
    std::vector<std::string> result;
    std::stringstream ss (s);
    std::string item;

    while (getline (ss, item, delim))
    {
        result.push_back (item);
    }

    return result;
}


VkDescriptorType intToVkDescriptorType (int typeInInt)
{
    VkDescriptorType type;
    switch (typeInInt)
    {
        case(0):
            type =  VK_DESCRIPTOR_TYPE_SAMPLER;
            break;
        case(1):
            type =  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            break;
        case(2):
            type =  VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            break;
        case(3):
            type =  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            break;
        case(4):
            type =  VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            break;
        case(5):
            type =  VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
            break;
        case(6):
            type =  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            break;
        case(7):
            type =  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            break;
        case(8):
            type =  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            break;
        case(9):
            type =  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            break;
        case(10):
            type =  VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            break;
        case(1000138000):
            type =  VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT;
            break;
        case(1000150000):
            type =  VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            break;
        case(1000165000):
            type =  VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
            break;
        case(1000351000):
            type =  VK_DESCRIPTOR_TYPE_MUTABLE_VALVE;
            break;
        case(0x7FFFFFF):
            type =  VK_DESCRIPTOR_TYPE_MAX_ENUM;
            break;
        default:
            abort(); // should not be here!
    }
    
    return type;
}


int main(int argc, char* argv[])
{
    char *filePath;
    char *mesa_root = getenv("MESA_ROOT");
    char fullPath[500];
    
    // Read command line input for shader location
    if (argc > 1) {
        //filePath = argv[1];
        snprintf(fullPath, sizeof(fullPath), "%s", argv[1]);
    }
    // Otherwise set to default
    else {
        filePath = "gpgpusimShaders/";
        snprintf(fullPath, sizeof(fullPath), "%s%s", mesa_root, filePath);
    }

    std::string fullPathString(fullPath);
    

    // Read command line input for workload type
    char *workload;

    if (argc > 2) {
        workload = argv[2];
    }
    else {
        workload = "ray_tracing_reflection";
    }

    int export_result = setenv("VULKAN_SIM_LAUNCHER_WORKLOAD", workload, 1);
    assert(export_result == 0);

    // Recreate Descriptor Set Data
    if (!strcmp(workload, "ray_tracing_reflection")) {
        std:: cout << "ray_tracing_reflection selected" << std::endl;

        FILE *fp;
        char descPath[1000];
        //auto obj_models = new std::vector<ray_tracing_reflection::ObjModelGpu>;
        ray_tracing_reflection::ObjModelGpu* obj_models = new ray_tracing_reflection::ObjModelGpu[3];
        void* obj_models_dev;
        gpgpusim_malloc(&obj_models_dev, sizeof(ray_tracing_reflection::ObjModelGpu) * 3);
        std::vector<void*> dev_ptrs; // its 12 void* that goes in obj_models_dev

        // scene_desc (setID = 0, binding = 3)
        for (int i = 0; i < 3; i++){
            auto model = new ray_tracing_reflection::ObjModelGpu;
            void* model_dev;
            //gpgpusim_malloc(&model_dev, sizeof(ray_tracing_reflection::ObjModelGpu));
            char* line;
            size_t len;
            std::string line_string;
            std::vector<std::string> params;
            
            // buffer sizes
            snprintf(descPath, sizeof(descPath), "%s%d.scene_desc.buffer_sizes", fullPath, i);
            fp = fopen(descPath, "r");
            line = (char *)malloc(32 * sizeof(char));
            len = 0;
            getline(&line, &len, fp);
            fclose(fp);
            line_string.assign(line);
            params = split(line_string, ',');
            uint32_t vertex_buffer_size = (uint32_t) std::stoi(params[0]) * sizeof(ray_tracing_reflection::ObjVertex);
            uint32_t index_buffer_size = (uint32_t) std::stoi(params[1]) * sizeof(uint32_t);
            uint32_t mat_index_buffer_size = (uint32_t) std::stoi(params[2]) * sizeof(int32_t);
            uint32_t mat_buffer_size = (uint32_t) std::stoi(params[3]) * sizeof(ray_tracing_reflection::ObjMaterial);

            // model.nb_indices, model.nb_vertices, not actually sent
            // snprintf(descPath, sizeof(descPath), "%s%d.scene_desc.indices_vertices", fullPath, i);
            // fp = fopen(descPath, "r");
            // line = NULL;
            // len = 0;
            // getline(&line, &len, fp);
            // fclose(fp);
            // line_string.assign(line);
            // params = split(line_string, ',');
            // uint32_t nb_indices = (uint32_t) std::stoi(params[0]);
            // uint32_t nb_vertices = (uint32_t) std::stoi(params[1]);
            // model->nb_indices = nb_indices;
            // model->nb_vertices = nb_vertices;

            // model.vertex_buffer
            snprintf(descPath, sizeof(descPath), "%s%d.scene_desc.vertex_buffer", fullPath, i);
            fp = fopen(descPath, "r");
            assert(vertex_buffer_size != 0);
            model->vertex_buffer = malloc(vertex_buffer_size);
            fread(model->vertex_buffer, vertex_buffer_size / sizeof(ray_tracing_reflection::ObjVertex), sizeof(ray_tracing_reflection::ObjVertex), fp);
            fclose(fp);
            void* vertex_buffer_dev;
            gpgpusim_malloc(&vertex_buffer_dev, vertex_buffer_size);
            gpgpusim_memcpy(vertex_buffer_dev, model->vertex_buffer, vertex_buffer_size, cudaMemcpyHostToDevice);
            //gpgpusim_memcpy(model_dev + 0 * sizeof(void*), &vertex_buffer_dev, sizeof(void*), cudaMemcpyHostToDevice);
            dev_ptrs.push_back(vertex_buffer_dev);

            // model.index_buffer data
            snprintf(descPath, sizeof(descPath), "%s%d.scene_desc.index_buffer", fullPath, i);
            fp = fopen(descPath, "r");
            assert(index_buffer_size != 0);
            model->index_buffer = malloc(index_buffer_size);
            fread(model->index_buffer, index_buffer_size / sizeof(uint32_t), sizeof(uint32_t), fp);
            fclose(fp);
            void* index_buffer_dev;
            gpgpusim_malloc(&index_buffer_dev, index_buffer_size);
            gpgpusim_memcpy(index_buffer_dev, model->index_buffer, index_buffer_size, cudaMemcpyHostToDevice);
            //gpgpusim_memcpy(model_dev + 1 * sizeof(void*), &index_buffer_dev, sizeof(void*), cudaMemcpyHostToDevice);
            dev_ptrs.push_back(index_buffer_dev);

            // model.mat_index_buffer
            snprintf(descPath, sizeof(descPath), "%s%d.scene_desc.mat_index_buffer", fullPath, i);
            fp = fopen(descPath, "r");
            assert(mat_index_buffer_size != 0);
            model->mat_index_buffer = malloc(mat_index_buffer_size);
            fread(model->mat_index_buffer, mat_index_buffer_size / sizeof(int32_t), sizeof(int32_t), fp);
            fclose(fp);
            void* mat_index_buffer_dev;
            gpgpusim_malloc(&mat_index_buffer_dev, mat_index_buffer_size);
            gpgpusim_memcpy(mat_index_buffer_dev, model->mat_index_buffer, mat_index_buffer_size, cudaMemcpyHostToDevice);
            //gpgpusim_memcpy(model_dev + 2 * sizeof(void*), &mat_index_buffer_dev, sizeof(void*), cudaMemcpyHostToDevice);
            dev_ptrs.push_back(mat_index_buffer_dev);

            // model.mat_color_buffer
            snprintf(descPath, sizeof(descPath), "%s%d.scene_desc.mat_color_buffer", fullPath, i);
            fp = fopen(descPath, "r");
            assert(mat_buffer_size != 0);
            model->mat_color_buffer = malloc(mat_buffer_size);
            fread(model->mat_color_buffer, mat_buffer_size / sizeof(ray_tracing_reflection::ObjMaterial), sizeof(ray_tracing_reflection::ObjMaterial), fp);
            fclose(fp);
            void* mat_color_buffer_dev;
            gpgpusim_malloc(&mat_color_buffer_dev, mat_buffer_size);
            gpgpusim_memcpy(mat_color_buffer_dev, model->mat_color_buffer, mat_buffer_size, cudaMemcpyHostToDevice);
            //gpgpusim_memcpy(model_dev + 3 * sizeof(void*), &mat_color_buffer_dev, sizeof(void*), cudaMemcpyHostToDevice);
            dev_ptrs.push_back(mat_color_buffer_dev);

            //obj_models->push_back(*model);
            obj_models[i] = *model;
            //gpgpusim_memcpy(obj_models_dev + i * sizeof(ray_tracing_reflection::ObjModelGpu), model_dev, sizeof(ray_tracing_reflection::ObjModelGpu), cudaMemcpyDeviceToDevice);
        }

        // for (int i = 0; i < dev_ptrs.size(); i++)
        // {
            gpgpusim_memcpy(obj_models_dev, dev_ptrs.data(), sizeof(void*) * dev_ptrs.size(), cudaMemcpyHostToDevice);
        // }
        
        descriptorSets[0][3] = (void*) obj_models;
        deviceDescriptorSets[0][3] = obj_models_dev;
        gpgpusim_setDescriptorSetFromLauncher_cpp((void*) obj_models, obj_models_dev, 0, 3);
    }


    // Allocate memory for descriptor sets (uniform buffers and acceleration structure)
    for (auto &p : fs::recursive_directory_iterator(fullPathString))
    {
        if (p.path().extension() == ".vkdescrptorsetdata")
        {
            std::cout << "Loading descriptor set data: " << p.path().string() << '\n';
            char descriptorFilePath[200];
            strcpy(descriptorFilePath, p.path().string().c_str());
            
            // Parse vkdescrptorsetdata File name format: setID_descID_SizeInBytes_VkDescriptorType.vkdescrptorsetdata
            std::string filename = p.path().filename().string();
            std::vector<std::string> temp = split(filename, '.'); // gets filename without extension
            std::vector<std::string> chunks = split(temp[0], '_'); // splits up the file name into above format

            // for (auto s : chunks)
            // {
            //     std::cout << s << std::endl;
            // }

            uint32_t setID = std::stoi(chunks[0]);
            uint32_t descID = std::stoi(chunks[1]);
            void *address;
            uint32_t size = std::stoi(chunks[2]);
            VkDescriptorType type = intToVkDescriptorType(std::stoi(chunks[3]));

            uint32_t backwards_range;
            uint32_t forward_range;
            if (type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR || type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV)
            {
                backwards_range = std::stoi(chunks[4]);
                forward_range = std::stoi(chunks[5]);
                assert(size + backwards_range + forward_range != 0);
                address = malloc(size + backwards_range + forward_range); // If something breaks, its definitely here

                FILE *fp;
                fp = fopen(descriptorFilePath, "r");
                fread(address, size + backwards_range + forward_range, 1, fp);
                fclose(fp);

                void* device_ptr;
                gpgpusim_malloc(&device_ptr, size + backwards_range + forward_range);
                gpgpusim_memcpy(device_ptr, address, size + backwards_range + forward_range, cudaMemcpyHostToDevice);
                deviceDescriptorSets[setID][descID] = device_ptr + (uint64_t)backwards_range;

                //gpgpusim_setDescriptorSet_cpp(setID, descID, address + (uint64_t)desired_range, size, type);
                descriptorSets[setID][descID] = address + (uint64_t)backwards_range;
                gpgpusim_setDescriptorSetFromLauncher_cpp(address + (uint64_t)backwards_range, device_ptr + (uint64_t)backwards_range, setID, descID); // change second address back to device_ptr
            }
            else if (type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            {
                // This is moved to another section
                // address = malloc(size+ 1920*1080*sizeof(long long)); // If something breaks, its definitely here

                // FILE *fp;
                // fp = fopen(descriptorFilePath, "r");
                // fread(address, size, 1, fp);
                // fclose(fp);

                // //gpgpusim_setDescriptorSet_cpp(setID, descID, address, size, type);
                // descriptorSets[setID][descID] = address;
                // gpgpusim_setDescriptorSetFromLauncher_cpp(address, setID, descID);
            }
            else 
            {
                assert(size != 0);
                address = malloc(size); // If something breaks, its definitely here

                FILE *fp;
                fp = fopen(descriptorFilePath, "r");
                fread(address, size, 1, fp);
                fclose(fp);

                void* device_ptr;
                gpgpusim_malloc(&device_ptr, size);
                gpgpusim_memcpy(device_ptr, address, size, cudaMemcpyHostToDevice);
                deviceDescriptorSets[setID][descID] = device_ptr;

                //gpgpusim_setDescriptorSet_cpp(setID, descID, address, size, type);
                descriptorSets[setID][descID] = address;
                gpgpusim_setDescriptorSetFromLauncher_cpp(address, device_ptr, setID, descID);
            }
        }
    }


    // Acceleration Structure
    for (auto &p : fs::recursive_directory_iterator(fullPathString))
    {
        if (p.path().extension() == ".asmetadata")
        {
            std::cout << "Loading AS metadata: " << p.path().string() << '\n';
            char asMetadataFilePath[200];
            strcpy(asMetadataFilePath, p.path().string().c_str());
            
            // Parse vkdescrptorsetdata File name format: setID_descID_SizeInBytes_VkDescriptorType.vkdescrptorsetdata
            std::string filename = p.path().filename().string();
            std::vector<std::string> temp = split(filename, '.'); // gets filename without extension
            std::vector<std::string> chunks = split(temp[0], '_'); // splits up the file name into above format

            // for (auto s : chunks)
            // {
            //     std::cout << s << std::endl;
            // }

            uint32_t setID = std::stoi(chunks[0]);
            uint32_t descID = std::stoi(chunks[1]);


            FILE *fp;
            fp = fopen(asMetadataFilePath, "r");

            char* line = (char *)malloc(256 * sizeof(char));
            size_t len = 0;
            getline(&line, &len, fp);
            fclose(fp);

            std::string line_string(line);
            std::vector<std::string> params = split(line_string, ',');

            uint32_t desc_size = (uint32_t) std::stoi(params[0]);
            uint32_t VkDescriptorTypeNum = (uint32_t) std::stoi(params[1]);
            int64_t max_backwards = (int64_t) std::stoi(params[2]); // negative number
            int64_t min_backwards = (int64_t) std::stoi(params[3]); // negative number
            int64_t min_forwards = (int64_t) std::stoi(params[4]);
            int64_t max_forwards = (int64_t) std::stoi(params[5]);
            int64_t back_buffer_amount = (int64_t) std::stoi(params[6]);
            int64_t front_buffer_amount = (int64_t) std::stoi(params[7]);
            uint32_t haveBackwards = (uint32_t) std::stoi(params[8]);
            uint32_t haveForwards = (uint32_t) std::stoi(params[9]);


            // Load in top level / main chunk of AS data
            void *address;
            address = malloc(max_forwards + front_buffer_amount - max_backwards);

            char asMainFilePath[200];
            snprintf(asMainFilePath, sizeof(asMainFilePath), "%s%d_%d.asmain", argv[1], setID, descID);
            fp = fopen(asMainFilePath, "r");
            fread(address - max_backwards, desc_size, 1, fp);
            fclose(fp);

            // Load in back chunk of AS data
            if (haveBackwards)
            {
                char asBackFilePath[200];
                snprintf(asBackFilePath, sizeof(asBackFilePath), "%s%d_%d.asback", argv[1], setID, descID);
                fp = fopen(asBackFilePath, "r");
                fread(address, min_backwards - max_backwards + back_buffer_amount, 1, fp);
                fclose(fp);
            }

            // Load in front chunk of AS data
            if (haveForwards)
            {
                char asFrontFilePath[200];
                snprintf(asFrontFilePath, sizeof(asFrontFilePath), "%s%d_%d.asfront", argv[1], setID, descID);
                fp = fopen(asFrontFilePath, "r");
                fread(address - max_backwards + min_forwards, max_forwards - min_forwards + front_buffer_amount, 1, fp);
                fclose(fp);
            }

            void* device_ptr;
            gpgpusim_malloc(&device_ptr, max_forwards + front_buffer_amount - max_backwards);
            gpgpusim_memcpy(device_ptr, address, max_forwards + front_buffer_amount - max_backwards, cudaMemcpyHostToDevice);
            deviceDescriptorSets[setID][descID] = device_ptr - max_backwards;
            
            descriptorSets[setID][descID] = address - max_backwards;
            gpgpusim_setDescriptorSetFromLauncher_cpp(address - max_backwards, device_ptr - max_backwards, setID, descID); // change second address back to device_ptr
        }
    }


    // Storage Images
    for (auto &p : fs::recursive_directory_iterator(fullPathString))
    {
        if (p.path().extension() == ".vkstorageimagemetadata")
        {
            std::cout  << "Loading Storage Image metadata: " << p.path().string() << '\n';
            char storageImageFilePath[200];
            strcpy(storageImageFilePath, p.path().string().c_str());
            
            std::string filename = p.path().filename().string();
            std::vector<std::string> temp = split(filename, '.'); // gets filename without extension
            std::vector<std::string> chunks = split(temp[0], '_'); // splits up the file name into above format

            uint32_t setID = std::stoi(chunks[0]);
            uint32_t descID = std::stoi(chunks[1]);
            
            FILE *fp;
            fp = fopen(storageImageFilePath, "r");

            char* line = (char *)malloc(64 * sizeof(char));
            size_t len = 0;
            getline(&line, &len, fp);
            fclose(fp);

            std::string line_string(line);
            std::vector<std::string> params = split(line_string, ',');

            uint32_t width = (uint32_t) std::stoi(params[0]);
            uint32_t height = (uint32_t) std::stoi(params[1]);
            VkFormat format = (uint32_t) std::stoi(params[2]);
            uint32_t VkDescriptorTypeNum = (uint32_t) std::stoi(params[3]);
            uint32_t n_planes = (uint32_t) std::stoi(params[4]);
            uint32_t n_samples = (uint32_t) std::stoi(params[5]);
            VkImageTiling tiling = (uint32_t) std::stoi(params[6]);
            uint32_t isl_tiling_mode = (uint32_t) std::stoi(params[7]);
            uint32_t row_pitch_B = (uint32_t) std::stoi(params[8]);

            void *address;
            address = malloc(width * height * sizeof(long long));

            void* device_ptr;
            gpgpusim_malloc(&device_ptr, width * height * sizeof(long long));
            gpgpusim_memcpy(device_ptr, address, width * height * sizeof(long long), cudaMemcpyHostToDevice);
            deviceDescriptorSets[setID][descID] = device_ptr;
            descriptorSets[setID][descID] = address; // kinda like a descriptor set

            gpgpusim_setStorageImageFromLauncher_cpp(address, 
                                                    device_ptr, 
                                                    setID, 
                                                    descID, 
                                                    width, 
                                                    height, 
                                                    format, 
                                                    VkDescriptorTypeNum, 
                                                    n_planes, 
                                                    n_samples, 
                                                    tiling, 
                                                    isl_tiling_mode, 
                                                    row_pitch_B);
        }
    }


    // Texture Data and Metadata
    for (auto &p : fs::recursive_directory_iterator(fullPathString))
    {
        if (p.path().extension() == ".vktexturemetadata")
        {
            std::cout  << "Loading Texture metadata: " << p.path().string() << '\n';
            char textureMetadataFilePath[200];
            strcpy(textureMetadataFilePath, p.path().string().c_str());
            
            std::string filename = p.path().filename().string();
            std::vector<std::string> temp = split(filename, '.'); // gets filename without extension
            std::vector<std::string> chunks = split(temp[0], '_'); // splits up the file name into above format

            uint32_t setID = std::stoi(chunks[0]);
            uint32_t descID = std::stoi(chunks[1]);
            
            FILE *fp;
            fp = fopen(textureMetadataFilePath, "r");

            char* line = (char *)malloc(64 * sizeof(char));
            size_t len = 0;
            getline(&line, &len, fp);
            fclose(fp);

            std::string line_string(line);
            std::vector<std::string> params = split(line_string, ',');

            uint64_t size = (uint64_t) std::stoi(params[0]);
            uint32_t width = (uint32_t) std::stoi(params[1]);
            uint32_t height = (uint32_t) std::stoi(params[2]);
            VkFormat format = (uint32_t) std::stoi(params[3]);
            uint32_t VkDescriptorTypeNum = (uint32_t) std::stoi(params[4]);
            uint32_t n_planes = (uint32_t) std::stoi(params[5]);
            uint32_t n_samples = (uint32_t) std::stoi(params[6]);
            VkImageTiling tiling = (uint32_t) std::stoi(params[7]);
            uint32_t isl_tiling_mode = (uint32_t) std::stoi(params[8]);
            uint32_t row_pitch_B = (uint32_t) std::stoi(params[9]);
            uint32_t filter = (uint32_t) std::stoi(params[10]);

            void *address;
            address = malloc(size);


            // Read in texture data
            char textureDataFilePath[200];
            snprintf(textureDataFilePath, sizeof(textureDataFilePath), "%s%d_%d.vktexturedata", argv[1], setID, descID);
            fp = fopen(textureDataFilePath, "r");
            fread(address, size, 1, fp);
            fclose(fp);

            void* device_ptr;
            gpgpusim_malloc(&device_ptr, size);
            gpgpusim_memcpy(device_ptr, address, size, cudaMemcpyHostToDevice);
            deviceDescriptorSets[setID][descID] = device_ptr;
            descriptorSets[setID][descID] = address;

            gpgpusim_setTextureFromLauncher_cpp(address, 
                                                device_ptr, 
                                                setID, 
                                                descID, 
                                                size,
                                                width, 
                                                height, 
                                                format, 
                                                VkDescriptorTypeNum, 
                                                n_planes, 
                                                n_samples, 
                                                tiling, 
                                                isl_tiling_mode,
                                                row_pitch_B,
                                                filter);
        }
    }


    // Register Shaders
    std::vector<shaderInfo> shaders;

    for (auto &p : fs::recursive_directory_iterator(fullPathString))
    {
        if (p.path().extension() == ".ptx")
        {
            std::cout  << "Registering shader: " << p.path().string() << '\n';
            char shaderPath[200];
            strcpy(shaderPath, p.path().string().c_str());
            std::string filename = p.path().filename().string();
            std::vector<std::string> chunks = split(filename, '_'); 
            std::string shaderTypeString = chunks[2];
            int shaderID = std::stoi(split(chunks.back(), '.')[0]);
            //std::cout << shaderTypeString << std::endl; // Shader type string;

            gl_shader_stage shaderType;
            if (shaderTypeString == "RAYGEN") {
                shaderType = MESA_SHADER_RAYGEN;
            } else if (shaderTypeString == "ANYHIT") {
                shaderType = MESA_SHADER_ANY_HIT;
            } else if (shaderTypeString == "CLOSEST") {
                shaderType = MESA_SHADER_CLOSEST_HIT;
            } else if (shaderTypeString == "MISS") {
                shaderType = MESA_SHADER_MISS;
            } else if (shaderTypeString == "INTERSECTION") {
                shaderType = MESA_SHADER_INTERSECTION;
            } else if (shaderTypeString == "CALLABLE") {
                shaderType = MESA_SHADER_CALLABLE;
            } else {
                std::cout << "Unknown Shader Type Detected" << std::endl;
                abort();
            }

            shaderInfo shader;
            strcpy(shader.shaderPath, shaderPath);
            shader.shaderType = shaderType;
            shader.shaderID = shaderID;

            shaders.push_back(shader);
            
            //gpgpusim_registerShader_cpp(shaderPath, shaderType);
        }
    }

    std::sort(shaders.begin(), shaders.end());

    for (auto it : shaders)
    {
        gpgpusim_registerShader_cpp(it.shaderPath, it.shaderType);
    }

    // Load in Shader Binding Tables
    void *raygen_sbt;
    void *miss_sbt;
    void *hit_sbt;
    void *callable_sbt;

    for (auto &p : fs::recursive_directory_iterator(fullPathString))
    {
        long sbt_size = 64;
        if (p.path().extension() == ".raygensbt")
        {
            std::cout  << "Loading raygen sbt: " << p.path().string() << '\n';
            char sbtFilePath[200];
            strcpy(sbtFilePath, p.path().string().c_str());

            sbt_size = fseek_filesize(sbtFilePath);
            raygen_sbt = malloc(sbt_size); 
            FILE *fp;
            fp = fopen(sbtFilePath, "r");
            fread(raygen_sbt, sbt_size, 1, fp);
            fclose(fp);
        } 
        else if (p.path().extension() == ".misssbt")
        {
            std::cout  << "Loading miss sbt: " << p.path().string() << '\n';
            char sbtFilePath[200];
            strcpy(sbtFilePath, p.path().string().c_str());

            sbt_size = fseek_filesize(sbtFilePath);
            miss_sbt = malloc(sbt_size); 
            FILE *fp;
            fp = fopen(sbtFilePath, "r");
            fread(miss_sbt, sbt_size, 1, fp);
            fclose(fp);
        }
        else if (p.path().extension() == ".hitsbt")
        {
            std::cout  << "Loading hit sbt: " << p.path().string() << '\n';
            char sbtFilePath[200];
            strcpy(sbtFilePath, p.path().string().c_str());

            sbt_size = fseek_filesize(sbtFilePath);
            hit_sbt = malloc(sbt_size); 
            FILE *fp;
            fp = fopen(sbtFilePath, "r");
            fread(hit_sbt, sbt_size, 1, fp);
            fclose(fp);
        }
        else if (p.path().extension() == ".callablesbt")
        {
            std::cout  << "Loading callable sbt: " << p.path().string() << '\n';
            char sbtFilePath[200];
            strcpy(sbtFilePath, p.path().string().c_str());

            sbt_size = fseek_filesize(sbtFilePath);
            callable_sbt = malloc(sbt_size); 
            FILE *fp;
            fp = fopen(sbtFilePath, "r");
            fread(callable_sbt, sbt_size, 1, fp);
            fclose(fp);
        }
    }

    // Parse vkCmdTraceRaysKHR call parameters
    bool is_indirect;
    uint32_t launch_width;
    uint32_t launch_height;
    uint32_t launch_depth;
    uint64_t launch_size_addr;

    for (auto &p : fs::recursive_directory_iterator(fullPathString))
    {
        if (p.path().extension() == ".callparams")
        {
            std::cout  << "Loading vkCmdTraceRaysKHR call parameters: " << p.path().string() << '\n';
            char callparamsFilePath[200];
            strcpy(callparamsFilePath, p.path().string().c_str());

            FILE *fp;
            fp = fopen(callparamsFilePath, "r");
            
            char* line = (char *)malloc(64 * sizeof(char));
            size_t len = 0;
            getline(&line, &len, fp); // only 1 line in the callparams file
            //printf("%s\n", line);
            fclose(fp);
            
            std::string line_string(line);
            std::vector<std::string> params = split(line_string, ',');

            is_indirect = (bool) std::stoi(params[0]);
            launch_width = (uint32_t) std::stoi(params[1]);
            launch_height = (uint32_t) std::stoi(params[2]);
            launch_depth = (uint32_t) std::stoi(params[3]);
            launch_size_addr = (uint64_t) std::stoi(params[4]);
        }
    }

    // Invoke vkCmdTraceRaysKHR
    gpgpusim_vkCmdTraceRaysKHR_cpp(raygen_sbt,
                                   miss_sbt,
                                   hit_sbt,
                                   callable_sbt,
                                   is_indirect,
                                   launch_width,
                                   launch_height,
                                   launch_depth,
                                   launch_size_addr);


    // char first = ((char*)ptr)[0];
    // char second = ((char*)ptr)[1];
    
    //char* address = mmap((void*)0x555557b6c128, );
}