#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef RACER_SHADER_DIR
#define RACER_SHADER_DIR "shaders"
#endif

struct QueueFamilies
{
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;

    bool complete() const
    {
        return graphics.has_value() && present.has_value();
    }
};

struct PushData
{
    float width;
    float height;
    float time;
    float steering;
    float speed;
    float score;
};

class VulkanRacer
{
public:
    void run()
    {
        initWindow();
        initVulkan();
        gameLoop();
        cleanup();
    }

private:
    SDL_Window* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent{};
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> imageViews;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    VkSemaphore renderFinished = VK_NULL_HANDLE;
    VkFence inFlight = VK_NULL_HANDLE;
    bool framebufferResized = false;
    float elapsed = 0.0f;
    float steering = 0.0f;
    float speed = 0.18f;
    float score = 0.0f;

    void check(VkResult result, const char* message)
    {
        if (result != VK_SUCCESS)
            throw std::runtime_error(message);
    }

    void initWindow()
    {
        if (!SDL_Init(SDL_INIT_VIDEO))
            throw std::runtime_error(SDL_GetError());

        window = SDL_CreateWindow(
            "VULKAN RACER | SDL3 + Vulkan 1.4",
            1920,
            1080,
            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
        );

        if (!window)
            throw std::runtime_error(SDL_GetError());
    }

    void initVulkan()
    {
        createInstance();
        if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface))
            throw std::runtime_error(SDL_GetError());
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapchain();
        createImageViews();
        createPipeline();
        createCommandPool();
        createCommandBuffers();
        createSyncObjects();
    }

    void createInstance()
    {
        uint32_t count = 0;
        const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&count);
        if (!extensions)
            throw std::runtime_error(SDL_GetError());

        VkApplicationInfo app{};
        app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app.pApplicationName = "Vulkan Racer";
        app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app.pEngineName = "Vulkan Racer Engine";
        app.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app.apiVersion = VK_API_VERSION_1_4;

        VkInstanceCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        info.pApplicationInfo = &app;
        info.enabledExtensionCount = count;
        info.ppEnabledExtensionNames = extensions;

        check(vkCreateInstance(&info, nullptr, &instance), "Could not create Vulkan instance");
    }

    QueueFamilies findQueueFamilies(VkPhysicalDevice candidate)
    {
        QueueFamilies result;
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, families.data());

        for (uint32_t i = 0; i < count; ++i)
        {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                result.graphics = i;

            VkBool32 supported = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface, &supported);
            if (supported)
                result.present = i;

            if (result.complete())
                break;
        }
        return result;
    }

    bool supportsSwapchain(VkPhysicalDevice candidate)
    {
        uint32_t count = 0;
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> extensions(count);
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &count, extensions.data());

        return std::any_of(extensions.begin(), extensions.end(), [](const auto& ext)
        {
            return std::strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
        });
    }

    void pickPhysicalDevice()
    {
        uint32_t count = 0;
        check(vkEnumeratePhysicalDevices(instance, &count, nullptr), "No Vulkan GPU found");
        if (count == 0)
            throw std::runtime_error("No Vulkan-capable GPU found");

        std::vector<VkPhysicalDevice> devices(count);
        check(vkEnumeratePhysicalDevices(instance, &count, devices.data()), "Could not enumerate GPUs");

        for (VkPhysicalDevice candidate : devices)
        {
            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(candidate, &props);

            VkPhysicalDeviceVulkan13Features features13{};
            features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
            VkPhysicalDeviceFeatures2 features2{};
            features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features2.pNext = &features13;
            vkGetPhysicalDeviceFeatures2(candidate, &features2);

            if (VK_VERSION_MAJOR(props.apiVersion) < 1 ||
                (VK_VERSION_MAJOR(props.apiVersion) == 1 && VK_VERSION_MINOR(props.apiVersion) < 3))
                continue;

            if (!features13.dynamicRendering || !supportsSwapchain(candidate))
                continue;

            if (!findQueueFamilies(candidate).complete())
                continue;

            physicalDevice = candidate;
            std::cout << "GPU: " << props.deviceName << '\n';
            break;
        }

        if (physicalDevice == VK_NULL_HANDLE)
            throw std::runtime_error("No GPU supports Vulkan 1.3 dynamic rendering and swapchains");
    }

    void createLogicalDevice()
    {
        QueueFamilies indices = findQueueFamilies(physicalDevice);
        std::vector<uint32_t> uniqueFamilies = {
            indices.graphics.value(),
            indices.present.value()
        };
        std::sort(uniqueFamilies.begin(), uniqueFamilies.end());
        uniqueFamilies.erase(std::unique(uniqueFamilies.begin(), uniqueFamilies.end()), uniqueFamilies.end());

        float priority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queues;
        for (uint32_t family : uniqueFamilies)
        {
            VkDeviceQueueCreateInfo queue{};
            queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue.queueFamilyIndex = family;
            queue.queueCount = 1;
            queue.pQueuePriorities = &priority;
            queues.push_back(queue);
        }

        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features13.dynamicRendering = VK_TRUE;

        const char* extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        VkDeviceCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        info.pNext = &features13;
        info.queueCreateInfoCount = static_cast<uint32_t>(queues.size());
        info.pQueueCreateInfos = queues.data();
        info.enabledExtensionCount = 1;
        info.ppEnabledExtensionNames = extensions;

        check(vkCreateDevice(physicalDevice, &info, nullptr, &device), "Could not create logical device");
        vkGetDeviceQueue(device, indices.graphics.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.present.value(), 0, &presentQueue);
    }

    void createSwapchain()
    {
        VkSurfaceCapabilitiesKHR caps{};
        check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &caps),
              "Could not query surface capabilities");

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

        VkSurfaceFormatKHR chosen = formats.front();
        for (const auto& format : formats)
        {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                chosen = format;
                break;
            }
        }

        uint32_t modeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, nullptr);
        std::vector<VkPresentModeKHR> modes(modeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, modes.data());

        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (auto mode : modes)
        {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                presentMode = mode;
                break;
            }
        }

        int pixelWidth = 0;
        int pixelHeight = 0;
        SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight);

        if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            swapchainExtent = caps.currentExtent;
        }
        else
        {
            swapchainExtent.width = std::clamp(
                static_cast<uint32_t>(std::max(pixelWidth, 1)),
                caps.minImageExtent.width,
                caps.maxImageExtent.width
            );
            swapchainExtent.height = std::clamp(
                static_cast<uint32_t>(std::max(pixelHeight, 1)),
                caps.minImageExtent.height,
                caps.maxImageExtent.height
            );
        }

        uint32_t imageCount = caps.minImageCount + 1;
        if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
            imageCount = caps.maxImageCount;

        QueueFamilies indices = findQueueFamilies(physicalDevice);
        uint32_t familyIndices[] = { indices.graphics.value(), indices.present.value() };

        VkSwapchainCreateInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface = surface;
        info.minImageCount = imageCount;
        info.imageFormat = chosen.format;
        info.imageColorSpace = chosen.colorSpace;
        info.imageExtent = swapchainExtent;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        if (indices.graphics != indices.present)
        {
            info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            info.queueFamilyIndexCount = 2;
            info.pQueueFamilyIndices = familyIndices;
        }
        else
        {
            info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        info.preTransform = caps.currentTransform;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = presentMode;
        info.clipped = VK_TRUE;

        check(vkCreateSwapchainKHR(device, &info, nullptr, &swapchain), "Could not create swapchain");
        swapchainFormat = chosen.format;

        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
        swapchainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());
    }

    void createImageViews()
    {
        imageViews.resize(swapchainImages.size());

        for (size_t i = 0; i < swapchainImages.size(); ++i)
        {
            VkImageViewCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.image = swapchainImages[i];
            info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            info.format = swapchainFormat;
            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            info.subresourceRange.baseMipLevel = 0;
            info.subresourceRange.levelCount = 1;
            info.subresourceRange.baseArrayLayer = 0;
            info.subresourceRange.layerCount = 1;
            check(vkCreateImageView(device, &info, nullptr, &imageViews[i]), "Could not create image view");
        }
    }

    std::vector<char> readFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file)
            throw std::runtime_error("Could not open shader: " + path);

        size_t size = static_cast<size_t>(file.tellg());
        std::vector<char> data(size);
        file.seekg(0);
        file.read(data.data(), static_cast<std::streamsize>(size));
        return data;
    }

    VkShaderModule createShaderModule(const std::vector<char>& code)
    {
        VkShaderModuleCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = code.size();
        info.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule module = VK_NULL_HANDLE;
        check(vkCreateShaderModule(device, &info, nullptr, &module), "Could not create shader module");
        return module;
    }

    void createPipeline()
    {
        auto vertCode = readFile(std::string(RACER_SHADER_DIR) + "/racer.vert.spv");
        auto fragCode = readFile(std::string(RACER_SHADER_DIR) + "/racer.frag.spv");

        VkShaderModule vert = createShaderModule(vertCode);
        VkShaderModule frag = createShaderModule(fragCode);

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vert;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = frag;
        stages[1].pName = "main";

        VkPushConstantRange push{};
        push.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        push.offset = 0;
        push.size = sizeof(PushData);

        VkPipelineLayoutCreateInfo layout{};
        layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout.pushConstantRangeCount = 1;
        layout.pPushConstantRanges = &push;
        check(vkCreatePipelineLayout(device, &layout, nullptr, &pipelineLayout),
              "Could not create pipeline layout");

        VkPipelineRenderingCreateInfo rendering{};
        rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachmentFormats = &swapchainFormat;

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo assembly{};
        assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewport{};
        viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments = &blendAttachment;

        VkDynamicState dynamicStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamic{};
        dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates = dynamicStates;

        VkGraphicsPipelineCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        info.pNext = &rendering;
        info.stageCount = 2;
        info.pStages = stages;
        info.pVertexInputState = &vertexInput;
        info.pInputAssemblyState = &assembly;
        info.pViewportState = &viewport;
        info.pRasterizationState = &raster;
        info.pMultisampleState = &multisample;
        info.pColorBlendState = &blend;
        info.pDynamicState = &dynamic;
        info.layout = pipelineLayout;

        VkResult result = vkCreateGraphicsPipelines(
            device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline
        );

        vkDestroyShaderModule(device, frag, nullptr);
        vkDestroyShaderModule(device, vert, nullptr);
        check(result, "Could not create graphics pipeline");
    }

    void createCommandPool()
    {
        QueueFamilies indices = findQueueFamilies(physicalDevice);
        VkCommandPoolCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        info.queueFamilyIndex = indices.graphics.value();
        check(vkCreateCommandPool(device, &info, nullptr, &commandPool),
              "Could not create command pool");
    }

    void createCommandBuffers()
    {
        commandBuffers.resize(swapchainImages.size());

        VkCommandBufferAllocateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.commandPool = commandPool;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

        check(vkAllocateCommandBuffers(device, &info, commandBuffers.data()),
              "Could not allocate command buffers");
    }

    void createSyncObjects()
    {
        VkSemaphoreCreateInfo semaphore{};
        semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence{};
        fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        check(vkCreateSemaphore(device, &semaphore, nullptr, &imageAvailable),
              "Could not create image semaphore");
        check(vkCreateSemaphore(device, &semaphore, nullptr, &renderFinished),
              "Could not create render semaphore");
        check(vkCreateFence(device, &fence, nullptr, &inFlight),
              "Could not create frame fence");
    }

    void recreateSwapchain()
    {
        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(window, &width, &height);

        while (width == 0 || height == 0)
        {
            SDL_WaitEvent(nullptr);
            SDL_GetWindowSizeInPixels(window, &width, &height);
        }

        vkDeviceWaitIdle(device);

        for (auto view : imageViews)
            vkDestroyImageView(device, view, nullptr);

        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkFreeCommandBuffers(device, commandPool,
                             static_cast<uint32_t>(commandBuffers.size()),
                             commandBuffers.data());

        imageViews.clear();
        swapchainImages.clear();
        commandBuffers.clear();

        createSwapchain();
        createImageViews();
        createCommandBuffers();
        framebufferResized = false;
    }

    void recordCommandBuffer(VkCommandBuffer command, uint32_t imageIndex)
    {
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        check(vkBeginCommandBuffer(command, &begin), "Could not begin command buffer");

        VkImageMemoryBarrier toColor{};
        toColor.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toColor.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toColor.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        toColor.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toColor.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toColor.image = swapchainImages[imageIndex];
        toColor.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toColor.subresourceRange.levelCount = 1;
        toColor.subresourceRange.layerCount = 1;
        toColor.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        vkCmdPipelineBarrier(
            command,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, 0, nullptr, 0, nullptr, 1, &toColor
        );

        VkClearValue clear{};
        clear.color = {{0.015f, 0.025f, 0.045f, 1.0f}};

        VkRenderingAttachmentInfo attachment{};
        attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        attachment.imageView = imageViews[imageIndex];
        attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.clearValue = clear;

        VkRenderingInfo rendering{};
        rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering.renderArea.extent = swapchainExtent;
        rendering.layerCount = 1;
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachments = &attachment;

        vkCmdBeginRendering(command, &rendering);

        VkViewport viewport{};
        viewport.width = static_cast<float>(swapchainExtent.width);
        viewport.height = static_cast<float>(swapchainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.extent = swapchainExtent;

        vkCmdSetViewport(command, 0, 1, &viewport);
        vkCmdSetScissor(command, 0, 1, &scissor);
        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        PushData data{
            static_cast<float>(swapchainExtent.width),
            static_cast<float>(swapchainExtent.height),
            elapsed,
            steering,
            speed,
            score
        };

        vkCmdPushConstants(
            command,
            pipelineLayout,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(PushData),
            &data
        );

        vkCmdDraw(command, 3, 1, 0, 0);
        vkCmdEndRendering(command);

        VkImageMemoryBarrier toPresent{};
        toPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toPresent.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toPresent.image = swapchainImages[imageIndex];
        toPresent.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toPresent.subresourceRange.levelCount = 1;
        toPresent.subresourceRange.layerCount = 1;
        toPresent.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        vkCmdPipelineBarrier(
            command,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, nullptr, 0, nullptr, 1, &toPresent
        );

        check(vkEndCommandBuffer(command), "Could not end command buffer");
    }

    void drawFrame()
    {
        check(vkWaitForFences(device, 1, &inFlight, VK_TRUE, UINT64_MAX),
              "Could not wait for frame fence");

        uint32_t imageIndex = 0;
        VkResult acquired = vkAcquireNextImageKHR(
            device, swapchain, UINT64_MAX, imageAvailable, VK_NULL_HANDLE, &imageIndex
        );

        if (acquired == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreateSwapchain();
            return;
        }

        if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR)
            check(acquired, "Could not acquire swapchain image");

        check(vkResetFences(device, 1, &inFlight), "Could not reset frame fence");
        check(vkResetCommandBuffer(commandBuffers[imageIndex], 0), "Could not reset command buffer");
        recordCommandBuffer(commandBuffers[imageIndex], imageIndex);

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &imageAvailable;
        submit.pWaitDstStageMask = &waitStage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &commandBuffers[imageIndex];
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &renderFinished;

        check(vkQueueSubmit(graphicsQueue, 1, &submit, inFlight), "Could not submit draw command");

        VkPresentInfoKHR present{};
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &renderFinished;
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain;
        present.pImageIndices = &imageIndex;

        VkResult result = vkQueuePresentKHR(presentQueue, &present);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized)
            recreateSwapchain();
        else
            check(result, "Could not present swapchain image");
    }

    void gameLoop()
    {
        bool running = true;
        uint64_t previous = SDL_GetPerformanceCounter();
        const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());

        while (running)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT)
                    running = false;
                if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
                    framebufferResized = true;
            }

            uint64_t now = SDL_GetPerformanceCounter();
            float dt = static_cast<float>((now - previous) / frequency);
            previous = now;
            dt = std::min(dt, 0.05f);

            const bool* keys = SDL_GetKeyboardState(nullptr);

            if (keys[SDL_SCANCODE_ESCAPE])
                running = false;

            float steerInput = 0.0f;
            if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])
                steerInput -= 1.0f;
            if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT])
                steerInput += 1.0f;

            steering += steerInput * dt * (1.5f + speed);
            steering *= std::pow(0.90f, dt * 60.0f);
            steering = std::clamp(steering, -1.0f, 1.0f);

            if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])
                speed += dt * 0.34f;
            else
                speed -= dt * 0.055f;

            if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])
                speed -= dt * 0.55f;

            speed = std::clamp(speed, 0.0f, 1.0f);
            elapsed += dt * (0.35f + speed * 2.8f);
            score += dt * speed * 100.0f;

            if (std::abs(steering) > 0.88f)
                speed = std::max(0.12f, speed - dt * 0.22f);

            drawFrame();
        }

        vkDeviceWaitIdle(device);
    }

    void cleanup()
    {
        if (device != VK_NULL_HANDLE)
            vkDeviceWaitIdle(device);

        if (device != VK_NULL_HANDLE)
        {
            if (inFlight) vkDestroyFence(device, inFlight, nullptr);
            if (renderFinished) vkDestroySemaphore(device, renderFinished, nullptr);
            if (imageAvailable) vkDestroySemaphore(device, imageAvailable, nullptr);
            if (commandPool) vkDestroyCommandPool(device, commandPool, nullptr);
            if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
            if (pipelineLayout) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

            for (auto view : imageViews)
                vkDestroyImageView(device, view, nullptr);

            if (swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr);
            vkDestroyDevice(device, nullptr);
        }

        if (surface && instance)
            SDL_Vulkan_DestroySurface(instance, surface, nullptr);

        if (instance)
            vkDestroyInstance(instance, nullptr);

        if (window)
            SDL_DestroyWindow(window);

        SDL_Quit();
    }
};

int main()
{
    try
    {
        VulkanRacer game;
        game.run();
    }
    catch (const std::exception& error)
    {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
