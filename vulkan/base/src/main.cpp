
#include <iostream>
#include <vector>
#include <array>
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
    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorSet descriptor_set;
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkBuffer ssbo_buffer;
    VkDeviceMemory ssbo_memory;
    void* ssbo_write_location;
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

    void create_descriptor_pool() {
        VkDescriptorPoolSize pool_size{};
        pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        pool_size.descriptorCount = 1;

        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.poolSizeCount = 1;
        pool_info.pPoolSizes = &pool_size;
        pool_info.maxSets = 1;

        result = vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptor_pool);
    }

    void create_descriptor_set_layout() {
        VkDescriptorSetLayoutBinding layout_binding;
        layout_binding.binding = 0;
        layout_binding.descriptorCount = 1;
        layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        layout_binding.pImmutableSamplers = nullptr;
        layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &layout_binding;

        result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptor_set_layout);
    }

    void allocate_descriptor_sets() {
        VkDescriptorSetAllocateInfo alloc_info;
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = descriptor_pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &descriptor_set_layout;
        result = vkAllocateDescriptorSets(device, &alloc_info, &descriptor_set);
    }

    void create_pipeline_layout() {
        VkPipelineLayoutCreateInfo pipeline_layout_info{};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = &descriptor_set_layout;
        pipeline_layout_info.pushConstantRangeCount = 0; // Optional
        pipeline_layout_info.pPushConstantRanges = nullptr; // Optional

        result = vkCreatePipelineLayout(
            device,
            &pipeline_layout_info,
            nullptr,
            &pipeline_layout);
    }

    void create_pipeline() {
        VkPipelineShaderStageCreateInfo compute_shader_stage_create_info{};
        compute_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        compute_shader_stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        compute_shader_stage_create_info.module = shader_module;
        compute_shader_stage_create_info.pName = "main";

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = pipeline_layout;
        pipelineInfo.stage = compute_shader_stage_create_info;

        result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    }

    void create_buffer(size_t buffer_size) {
        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = buffer_size;
        buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &buffer_info, nullptr, &ssbo_buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements memory_requirements;
        vkGetBufferMemoryRequirements(device, ssbo_buffer, &memory_requirements);

        VkPhysicalDeviceMemoryProperties memory_properties;
        vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

        VkMemoryPropertyFlags required_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        uint32_t memory_type_index = 0;
        bool found_suitable_memory = false;

        for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
            bool supported = (memory_requirements.memoryTypeBits & (1 << i)) > 0;
            bool sufficient = (memory_properties.memoryTypes[i].propertyFlags & required_properties) == required_properties;

            if (supported && sufficient) {
                memory_type_index = i;
                found_suitable_memory = true;
            }
        }

        if (!found_suitable_memory) {
            result = VK_NOT_READY;
            return;
        }

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memory_requirements.size;
        allocInfo.memoryTypeIndex = memory_type_index;

        if (vkAllocateMemory(device, &allocInfo, nullptr, &ssbo_memory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        vkBindBufferMemory(device, ssbo_buffer, ssbo_memory, 0);

        result = vkCreateBuffer(device, &buffer_info, nullptr, &ssbo_buffer);
    }

    void map_buffer_memory(size_t buffer_size) {
        result = vkMapMemory(device, ssbo_memory, 0, buffer_size, 0, &ssbo_write_location);
    }

    void update_descriptor_sets(VkDeviceSize range) {
        VkDescriptorBufferInfo descriptor_buffer_info{};
        descriptor_buffer_info.buffer = ssbo_buffer;
        descriptor_buffer_info.offset = 0;
        descriptor_buffer_info.range = range;

        VkWriteDescriptorSet write_descriptor_sets;
        write_descriptor_sets.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_descriptor_sets.dstSet = descriptor_set;
        write_descriptor_sets.dstBinding = 0;
        write_descriptor_sets.dstArrayElement = 0;
        write_descriptor_sets.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write_descriptor_sets.descriptorCount = 1;
        write_descriptor_sets.pBufferInfo = &descriptor_buffer_info;
        write_descriptor_sets.pImageInfo = nullptr; // Optional
        write_descriptor_sets.pTexelBufferView = nullptr; // Optional

        vkUpdateDescriptorSets(device, 1, &write_descriptor_sets, 0, nullptr);
    }

    void destroy() {
        result = VK_NOT_READY;
        vkUnmapMemory(device, ssbo_memory);
        vkFreeMemory(device, ssbo_memory, nullptr);
        vkDestroyBuffer(device, ssbo_buffer, nullptr);
        vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
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
    // vkGetDeviceQueue can't fail?

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

    ctx.create_descriptor_pool();

    if (ctx.result != VK_SUCCESS) {
        std::cerr << "Failed to create descriptor pool" << std::endl;
        return 1;
    }

    ctx.create_descriptor_set_layout();

    if (ctx.result != VK_SUCCESS) {
        std::cerr << "Failed to create descriptor set layout" << std::endl;
        return 1;
    }

    constexpr size_t buffer_size = 1024;

    ctx.create_buffer(buffer_size);

    if (ctx.result != VK_SUCCESS) {
        std::cerr << "Failed to create buffer" << std::endl;
        return 1;
    }

    ctx.map_buffer_memory(buffer_size);

    if (ctx.result != VK_SUCCESS) {
        std::cerr << "Failed to map buffer" << std::endl;
        return 1;
    }

    ctx.allocate_descriptor_sets();

    if (ctx.result != VK_SUCCESS) {
        std::cerr << "Failed to allocate descriptor sets" << std::endl;
        return 1;
    }

    ctx.update_descriptor_sets(1024); // Cannot fail ???

    ctx.create_pipeline_layout();

    if (ctx.result != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline layout" << std::endl;
        return 1;
    }

    ctx.create_pipeline();

    if (ctx.result != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline" << std::endl;
        return 1;
    }

    ctx.destroy();
    return 0;
}
