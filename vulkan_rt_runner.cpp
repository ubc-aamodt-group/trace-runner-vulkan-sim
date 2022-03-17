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
extern void gpgpusim_setDescriptorSetFromLauncher_cpp(void *address, uint32_t setID, uint32_t descID);
extern void gpgpusim_setStorageImageFromLauncher_cpp(void *address, 
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
                                                uint32_t isl_tiling_mode);


namespace fs = boost::filesystem;


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

void* descriptorSets[1][10] = {nullptr};

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

        // scene_desc (setID = 0, binding = 3)
        for (int i = 0; i < 3; i++){
            auto model = new ray_tracing_reflection::ObjModelGpu;
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

            // model.index_buffer data
            snprintf(descPath, sizeof(descPath), "%s%d.scene_desc.index_buffer", fullPath, i);
            fp = fopen(descPath, "r");
            assert(index_buffer_size != 0);
            model->index_buffer = malloc(index_buffer_size);
            fread(model->index_buffer, index_buffer_size / sizeof(uint32_t), sizeof(uint32_t), fp);
            fclose(fp);

            // model.mat_index_buffer
            snprintf(descPath, sizeof(descPath), "%s%d.scene_desc.mat_index_buffer", fullPath, i);
            fp = fopen(descPath, "r");
            assert(mat_index_buffer_size != 0);
            model->mat_index_buffer = malloc(mat_index_buffer_size);
            fread(model->mat_index_buffer, mat_index_buffer_size / sizeof(int32_t), sizeof(int32_t), fp);
            fclose(fp);

            // model.mat_color_buffer
            snprintf(descPath, sizeof(descPath), "%s%d.scene_desc.mat_color_buffer", fullPath, i);
            fp = fopen(descPath, "r");
            assert(mat_buffer_size != 0);
            model->mat_color_buffer = malloc(mat_buffer_size);
            fread(model->mat_color_buffer, mat_buffer_size / sizeof(ray_tracing_reflection::ObjMaterial), sizeof(ray_tracing_reflection::ObjMaterial), fp);
            fclose(fp);

            //obj_models->push_back(*model);
            obj_models[i] = *model;
        }

        descriptorSets[0][3] = (void*) obj_models;
        gpgpusim_setDescriptorSetFromLauncher_cpp((void*) obj_models, 0, 3);
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

                //gpgpusim_setDescriptorSet_cpp(setID, descID, address + (uint64_t)desired_range, size, type);
                descriptorSets[setID][descID] = address + (uint64_t)backwards_range;
                gpgpusim_setDescriptorSetFromLauncher_cpp(address + (uint64_t)backwards_range, setID, descID);
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

                //gpgpusim_setDescriptorSet_cpp(setID, descID, address, size, type);
                descriptorSets[setID][descID] = address;
                gpgpusim_setDescriptorSetFromLauncher_cpp(address, setID, descID);
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


            // Load in top level / main chunk of AS data
            void *address;
            address = malloc(max_forwards + front_buffer_amount - max_backwards);

            char asMainFilePath[200];
            snprintf(asMainFilePath, sizeof(asMainFilePath), "%s%d_%d.asmain", argv[1], setID, descID);
            fp = fopen(asMainFilePath, "r");
            fread(address - max_backwards, desc_size, 1, fp);
            fclose(fp);

            // Load in back chunk of AS data
            char asBackFilePath[200];
            snprintf(asBackFilePath, sizeof(asBackFilePath), "%s%d_%d.asback", argv[1], setID, descID);
            fp = fopen(asBackFilePath, "r");
            fread(address, min_backwards - max_backwards + back_buffer_amount, 1, fp);
            fclose(fp);

            // Load in front chunk of AS data
            char asFrontFilePath[200];
            snprintf(asFrontFilePath, sizeof(asFrontFilePath), "%s%d_%d.asfront", argv[1], setID, descID);
            fp = fopen(asFrontFilePath, "r");
            fread(address - max_backwards + min_forwards, max_forwards - min_forwards + front_buffer_amount, 1, fp);
            fclose(fp);
            
            descriptorSets[setID][descID] = address - max_backwards;
            gpgpusim_setDescriptorSetFromLauncher_cpp(address - max_backwards, setID, descID);
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

            descriptorSets[setID][descID] = address; // kinda like a descriptor set
            gpgpusim_setStorageImageFromLauncher_cpp(address, 
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

            void *address;
            address = malloc(size);


            // Read in texture data
            char textureDataFilePath[200];
            snprintf(textureDataFilePath, sizeof(textureDataFilePath), "%s%d_%d.vktexturedata", argv[1], setID, descID);
            fp = fopen(textureDataFilePath, "r");
            fread(address, size, 1, fp);
            fclose(fp);

            gpgpusim_setTextureFromLauncher_cpp(address, 
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
                                                isl_tiling_mode);
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
        int sbt_size = 64;
        if (p.path().extension() == ".raygensbt")
        {
            std::cout  << "Loading raygen sbt: " << p.path().string() << '\n';
            char sbtFilePath[200];
            strcpy(sbtFilePath, p.path().string().c_str());

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

// int main(int argc, char* argv[])
// {
//     char *filePath;
//     char *mesa_root = getenv("MESA_ROOT");
    
//     // Read command line input for shader location
//     if (argc > 1) {
//         filePath = argv[1];
//     }
//     // Otherwise set to default
//     else {
//         filePath = "gpgpusimShaders/";
//     }

//     char fullPath[200];
//     snprintf(fullPath, sizeof(fullPath), "%s%s", mesa_root, filePath);

//     // Register Shaders
//     std::string fullPathString(fullPath);

//     for (auto &p : fs::recursive_directory_iterator(fullPathString))
//     {
//         if (p.path().extension() == ".ptx")
//         {
//             std::cout  << "Registering shader: " << p.path().string() << '\n';
//             char shaderPath[200];
//             strcpy(shaderPath, p.path().string().c_str());
//             std::string filename = p.path().filename().string();
//             std::vector<std::string> chunks = split(filename, '_'); 
//             std::string shaderTypeString = chunks[2];
//             //std::cout << shaderTypeString << std::endl; // Shader type string

//             gl_shader_stage shaderType;
//             if (shaderTypeString == "RAYGEN") {
//                 shaderType = MESA_SHADER_RAYGEN;
//             } else if (shaderTypeString == "ANYHIT") {
//                 shaderType = MESA_SHADER_ANY_HIT;
//             } else if (shaderTypeString == "CLOSEST") {
//                 shaderType = MESA_SHADER_CLOSEST_HIT;
//             } else if (shaderTypeString == "MISS") {
//                 shaderType = MESA_SHADER_MISS;
//             } else if (shaderTypeString == "INTERSECTION") {
//                 shaderType = MESA_SHADER_INTERSECTION;
//             } else if (shaderTypeString == "CALLABLE") {
//                 shaderType = MESA_SHADER_CALLABLE;
//             } else {
//                 std::cout << "Unknown Shader Type Detected" << std::endl;
//                 abort();
//             }

//             gpgpusim_registerShader_cpp(shaderPath, shaderType);
//         }
//     }

//     // Allocate memory for descriptor sets (uniform buffers and acceleration structure)
//     // am i only getting the top level and missing the bottom level? Do i need to rebuild the tree in the launcher?
//     // I think the old addresses from mesa embedded inside is causing the segfault
//     for (auto &p : fs::recursive_directory_iterator(fullPathString))
//     {
//         if (p.path().extension() == ".vkdescrptorsetdata")
//         {
//             std::cout << "Loading descriptor set data: " << p.path().string() << '\n';
//             char descriptorFilePath[200];
//             strcpy(descriptorFilePath, p.path().string().c_str());
            
//             // Parse vkdescrptorsetdata File name format: setID_descID_SizeInBytes_VkDescriptorType.vkdescrptorsetdata
//             std::string filename = p.path().filename().string();
//             std::vector<std::string> temp = split(filename, '.'); // gets filename without extension
//             std::vector<std::string> chunks = split(temp[0], '_'); // splits up the file name into above format

//             // for (auto s : chunks)
//             // {
//             //     std::cout << s << std::endl;
//             // }

//             uint32_t setID = std::stoi(chunks[0]);
//             uint32_t descID = std::stoi(chunks[1]);
//             void *address;
//             uint32_t size = std::stoi(chunks[2]);
//             VkDescriptorType type = intToVkDescriptorType(std::stoi(chunks[3]));

//             uint32_t desired_range;
//             if (type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR || type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV)
//             {
//                 desired_range = std::stoi(chunks[4]);
//                 address = malloc(size + desired_range); // If something breaks, its definitely here

//                 FILE *fp;
//                 fp = fopen(descriptorFilePath, "r");
//                 fread(address, size + desired_range, 1, fp);
//                 fclose(fp);

//                 //gpgpusim_setDescriptorSet_cpp(setID, descID, address + (uint64_t)desired_range, size, type);
//             }
//             else if (type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
//             {
//                 address = malloc(size+ 1920*1080*sizeof(long long)); // If something breaks, its definitely here

//                 FILE *fp;
//                 fp = fopen(descriptorFilePath, "r");
//                 fread(address, size, 1, fp);
//                 fclose(fp);

//                 //gpgpusim_setDescriptorSet_cpp(setID, descID, address, size, type);
//             }
//             else 
//             {
//                 address = malloc(size); // If something breaks, its definitely here

//                 FILE *fp;
//                 fp = fopen(descriptorFilePath, "r");
//                 fread(address, size, 1, fp);
//                 fclose(fp);

//                 //gpgpusim_setDescriptorSet_cpp(setID, descID, address, size, type);
//             }
//         }
//     }

//     // Load in Shader Binding Tables
//     void *raygen_sbt;
//     void *miss_sbt;
//     void *hit_sbt;
//     void *callable_sbt;

//     for (auto &p : fs::recursive_directory_iterator(fullPathString))
//     {
//         if (p.path().extension() == ".raygensbt")
//         {
//             std::cout  << "Loading raygen sbt: " << p.path().string() << '\n';
//             char sbtFilePath[200];
//             strcpy(sbtFilePath, p.path().string().c_str());

//             int sbt_size = 32; // Capped at 32 bytes, dunno if correct
//             raygen_sbt = malloc(sbt_size); 
//             FILE *fp;
//             fp = fopen(sbtFilePath, "r");
//             fread(raygen_sbt, sbt_size, 1, fp);
//             fclose(fp);
//         } 
//         else if (p.path().extension() == ".misssbt")
//         {
//             std::cout  << "Loading miss sbt: " << p.path().string() << '\n';
//             char sbtFilePath[200];
//             strcpy(sbtFilePath, p.path().string().c_str());

//             int sbt_size = 32; // Capped at 32 bytes, dunno if correct
//             miss_sbt = malloc(sbt_size); 
//             FILE *fp;
//             fp = fopen(sbtFilePath, "r");
//             fread(miss_sbt, sbt_size, 1, fp);
//             fclose(fp);
//         }
//         else if (p.path().extension() == ".hitsbt")
//         {
//             std::cout  << "Loading hit sbt: " << p.path().string() << '\n';
//             char sbtFilePath[200];
//             strcpy(sbtFilePath, p.path().string().c_str());

//             int sbt_size = 32; // Capped at 32 bytes, dunno if correct
//             hit_sbt = malloc(sbt_size); 
//             FILE *fp;
//             fp = fopen(sbtFilePath, "r");
//             fread(hit_sbt, sbt_size, 1, fp);
//             fclose(fp);
//         }
//         else if (p.path().extension() == ".callablesbt")
//         {
//             std::cout  << "Loading callable sbt: " << p.path().string() << '\n';
//             char sbtFilePath[200];
//             strcpy(sbtFilePath, p.path().string().c_str());

//             int sbt_size = 32; // Capped at 32 bytes, dunno if correct
//             callable_sbt = malloc(sbt_size); 
//             FILE *fp;
//             fp = fopen(sbtFilePath, "r");
//             fread(callable_sbt, sbt_size, 1, fp);
//             fclose(fp);
//         }
//     }

//     // Parse vkCmdTraceRaysKHR call parameters
//     bool is_indirect;
//     uint32_t launch_width;
//     uint32_t launch_height;
//     uint32_t launch_depth;
//     uint64_t launch_size_addr;

//     for (auto &p : fs::recursive_directory_iterator(fullPathString))
//     {
//         if (p.path().extension() == ".callparams")
//         {
//             std::cout  << "Loading vkCmdTraceRaysKHR call parameters: " << p.path().string() << '\n';
//             char callparamsFilePath[200];
//             strcpy(callparamsFilePath, p.path().string().c_str());

//             FILE *fp;
//             fp = fopen(callparamsFilePath, "r");
            
//             char* line = NULL;
//             size_t len = 0;
//             getline(&line, &len, fp); // only 1 line in the callparams file
//             //printf("%s\n", line);
//             fclose(fp);
            
//             std::string line_string(line);
//             std::vector<std::string> params = split(line_string, ',');

//             is_indirect = (bool) std::stoi(params[0]);
//             launch_width = (uint32_t) std::stoi(params[1]);
//             launch_height = (uint32_t) std::stoi(params[2]);
//             launch_depth = (uint32_t) std::stoi(params[3]);
//             launch_size_addr = (uint64_t) std::stoi(params[4]);
//         }
//     }

//     // Invoke vkCmdTraceRaysKHR
//     gpgpusim_vkCmdTraceRaysKHR_cpp(raygen_sbt,
//                                    miss_sbt,
//                                    hit_sbt,
//                                    callable_sbt,
//                                    is_indirect,
//                                    launch_width,
//                                    launch_height,
//                                    launch_depth,
//                                    launch_size_addr);

//     // Free the descriptor sets

//     // TODO: Figure out how to dump data separately for each kernel call
// }
