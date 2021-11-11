#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include "../mesa/src/intel/vulkan/gpgpusim_calls_from_mesa.h"
#include "../mesa/src/compiler/shader_enums.h"

#define BOOST_FILESYSTEM_VERSION 3
#define BOOST_FILESYSTEM_NO_DEPRECATED 
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;


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


int main ()
{
    FILE *fp;
    char *mesa_root = getenv("MESA_ROOT");
    const char *filePath = "gpgpusimShaders/";
    const char *descExtension = ".vkdescrptorsetdata";
    const char *ptxExtension = ".ptx";

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
            std::cout << shaderTypeString << std::endl; // Shader type string

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
    

    // Invoke 
}