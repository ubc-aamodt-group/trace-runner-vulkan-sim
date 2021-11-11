#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include "../mesa/include/vulkan/vulkan_core.h"
#include "../mesa/src/intel/vulkan/gpgpusim_calls_from_mesa.h"
#include "../mesa/src/compiler/shader_enums.h"

#define BOOST_FILESYSTEM_VERSION 3
#define BOOST_FILESYSTEM_NO_DEPRECATED 
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;


off_t fsize(const char *filename) {
    struct stat st; 

    if (stat(filename, &st) == 0)
        return st.st_size;

    return -1; 
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


int main ()
{
    char *mesa_root = getenv("MESA_ROOT");
    const char *filePath = "gpgpusimShaders/";

    char fullPath[200];
    snprintf(fullPath, sizeof(fullPath), "%s%s", mesa_root, filePath);

    // Register Shaders
    std::string fullPathString(fullPath);

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
            //std::cout << shaderTypeString << std::endl; // Shader type string

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

            gpgpusim_registerShader_cpp(shaderPath, shaderType);
        }
    }

    // Allocate memory for descriptor sets (uniform buffers and acceleration structure)
    for (auto &p : fs::recursive_directory_iterator(fullPathString))
    {
        if (p.path().extension() == ".vkdescrptorsetdata")
        {
            std::cout  << "Loading descriptor set data: " << p.path().string() << '\n';
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

            address = malloc(size); // If something breaks, its definitely here

            FILE *fp;
            fp = fopen(descriptorFilePath, "r");
            fread(address, size, 1, fp);
            fclose(fp);

            gpgpusim_setDescriptorSet_cpp(setID, descID, address, size, type);
        }
    }

    // Invoke vkCmdTraceRays
    // gpgpusim_vkCmdTraceRaysKHR(
    //                   void *raygen_sbt,
    //                   void *miss_sbt,
    //                   void *hit_sbt,
    //                   void *callable_sbt,
    //                   bool is_indirect,
    //                   uint32_t launch_width,
    //                   uint32_t launch_height,
    //                   uint32_t launch_depth,
    //                   uint64_t launch_size_addr);

    // Free the descriptor sets


    // TODO: Figure out how to dump data separately for each kernel call
}