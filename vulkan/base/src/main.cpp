
#include <iostream>
#include <vector>
#include <fstream>
#include <filesystem>
#include <optional>
#include <vulkan/vulkan.h>

std::optional<std::vector<char>> read_binary_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        return std::nullopt;
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();
    return buffer;
}


struct VkContext {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    std::optional<uint32_t> compute_family_index_opt = std::nullopt;
    VkDevice device;
    VkQueue compute_queue;
    VkShaderModule shader_module;
    VkDescriptorSetLayout descriptor_set_layout;
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkResult result = VK_NOT_READY;

    void create_instance(bool enable_validation_layers) {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Vulkan getting started";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        std::vector<const char*> extensions = {};
        std::vector<const char*> layers = {};

        if (enable_validation_layers) {
            layers.push_back("VK_LAYER_KHRONOS_validation");
        };

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        createInfo.enabledExtensionCount = extensions.size();
        if (!extensions.empty()) {
            createInfo.ppEnabledExtensionNames = extensions.data();
        }

        createInfo.enabledLayerCount = layers.size();
        if (!layers.empty()) {
            createInfo.ppEnabledLayerNames = layers.data();
        }

        result = vkCreateInstance(&createInfo, nullptr, &instance);

        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create instance" << std::endl;
        }
    }

    void pick_physical_device() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            std::cerr << "Failed to find any GPUs with Vulkan support" << std::endl;
            result = VK_NOT_READY;
            return;
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (auto& device : devices) {
            VkPhysicalDeviceProperties deviceProperties;
            VkPhysicalDeviceFeatures deviceFeatures;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
            vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

            if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                physical_device = device;
            }
        }

        if (physical_device == VK_NULL_HANDLE) {
            result = VK_NOT_READY;
        }
    }

    void find_queue_families() {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            // Note: Cop-out to look for both COMPUTE and GRAPHICS bits, queues with only COMPUTE are more hard core
            if (!compute_family_index_opt.has_value()
                && (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) > 0
                && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) > 0) {
                compute_family_index_opt = i;
            }
            i++;
        }
    }

    void create_device() {
        if (compute_family_index_opt.has_value() && result == VK_SUCCESS) {
            float queuePriority = 1.0f;
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = compute_family_index_opt.value();
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;

            VkPhysicalDeviceFeatures deviceFeatures{};

            VkDeviceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

            createInfo.pQueueCreateInfos = &queueCreateInfo;
            createInfo.queueCreateInfoCount = 1;

            createInfo.pEnabledFeatures = &deviceFeatures;

            std::vector<const char*> extensions = {};

            createInfo.enabledExtensionCount = extensions.size();
            if (!extensions.empty()) {
                createInfo.ppEnabledExtensionNames = extensions.data();
            }

            result = vkCreateDevice(physical_device, &createInfo, nullptr, &device);
        } else {
            result = VK_NOT_READY;
        }
    }

    void get_compute_queue() {
        vkGetDeviceQueue(device, compute_family_index_opt.value(), 0, &compute_queue);
    }

    void create_shader_module(std::vector<char> code) {
        VkShaderModuleCreateInfo shader_module_create_info{};
        shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_module_create_info.codeSize = code.size();
        shader_module_create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shader_module;
        result = vkCreateShaderModule(
            device,
            &shader_module_create_info,
            nullptr,
            &shader_module);
    }

    void create_pipeline_layout() {
        VkPipelineShaderStageCreateInfo compute_shader_stage_create_info{};
        compute_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        compute_shader_stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        compute_shader_stage_create_info.module = shader_module;
        compute_shader_stage_create_info.pName = "main";

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0; // Optional
        pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

        result = vkCreatePipelineLayout(
            device,
            &pipelineLayoutInfo,
            nullptr,
            &pipeline_layout);
    }

    void destroy() {
        result = VK_NOT_READY;
        vkDestroyShaderModule(device, shader_module, nullptr);
        vkDestroyPipeline(device, pipeline, nullptr);
        vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
    }
};



int main(int argc, char** argv) {
    VkContext ctx;

    ctx.create_instance(true);

    if (ctx.result != VK_SUCCESS) {
        std::cerr << "Failed to create instance" << std::endl;
        return 1;
    }

    ctx.pick_physical_device();

    if (ctx.result != VK_SUCCESS) {
        std::cerr << "Failed to pick physical device" << std::endl;
        return 1;
    }

    ctx.find_queue_families();

    if (ctx.result != VK_SUCCESS) {
        std::cerr << "Failed to find queue families" << std::endl;
        return 1;
    }

    ctx.create_device();

    if (ctx.result != VK_SUCCESS) {
        std::cerr << "Failed to create device" << std::endl;
        return 1;
    }

    ctx.get_compute_queue();

    if (ctx.result != VK_SUCCESS) {
        std::cerr << "Failed to get compute queue" << std::endl;
        return 1;
    }

    auto shader_code_opt = read_binary_file(VULKAN_GS_DEFAULT_compute_SHADER_PATH);

    if (!shader_code_opt.has_value()) {
        std::cerr << "Failed to read " << VULKAN_GS_DEFAULT_compute_SHADER_PATH << std::endl;
        return 1;
    }

    ctx.create_shader_module(shader_code_opt.value());

    if (ctx.result != VK_SUCCESS) {
        std::cerr << "Failed to create shader module" << std::endl;
        return 1;
    }

    ctx.destroy();
    return 0;
}
