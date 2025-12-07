#include <algorithm>
#include <cstddef>
#include <fstream>
#include <ios>
#include <limits>
#include <set>
#include <string>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

class QueueFamilyIndices {
   public:
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family;

    bool is_complete() { return graphics_family.has_value() && present_family.has_value(); }
};

class SwapChainSupportDetails {
   public:
    VkSurfaceCapabilitiesKHR        capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   present_modes;
};

class TriangleApplication {
   private:
    static constexpr uint32_t WINDOW_WIDTH  = 800;
    static constexpr uint32_t WINDOW_HEIGHT = 600;

#ifdef NDEBUG
    static constexpr bool ENABLE_VALIDATION_LAYERS = false;
#else
    static constexpr bool ENABLE_VALIDATION_LAYERS = true;
#endif

    GLFWwindow*              m_window                = nullptr;
    VkInstance               m_instance              = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debug_messenger       = VK_NULL_HANDLE;
    VkSurfaceKHR             m_surface               = VK_NULL_HANDLE;
    VkPhysicalDevice         m_physical_device       = VK_NULL_HANDLE;
    VkDevice                 m_logical_device        = VK_NULL_HANDLE;
    VkQueue                  m_graphics_queue        = VK_NULL_HANDLE;
    VkQueue                  m_presentation_queue    = VK_NULL_HANDLE;
    VkSwapchainKHR           m_swapchain             = VK_NULL_HANDLE;
    std::vector<VkImage>     m_swapchain_images      = {};
    VkFormat                 m_swapchain_format      = VK_FORMAT_UNDEFINED;
    VkExtent2D               m_swapchain_extent      = {};
    std::vector<VkImageView> m_swapchain_image_views = {};

    const std::vector<const char*> m_validation_layers = {"VK_LAYER_KHRONOS_validation"};
    const std::vector<const char*> m_device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

   public:
    void run() {
        init();
        main_loop();
        cleanup();
    }

   private:
    /* ---- Initialization and lifecycle ---- */

    void init() {
        init_glfw();
        init_window();
        init_vulcan();
    }

    void init_glfw() {
        if (!glfwInit()) {
            throw std::runtime_error(
                "TriangleApplication::init_glfw => Failed to initialize GLFW.");
        }
    }

    void init_window() {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        m_window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "triangle", nullptr, nullptr);
        if (!m_window) {
            glfwTerminate();
            throw std::runtime_error(
                "TriangleApplication::init_window => Failed to create GLFW window");
        }
    }

    void init_vulcan() {
        create_instance();
        check_extension_support();
        setup_debug_messenger();
        create_surface();
        pick_physical_device();
        create_logical_device();
        create_graphics_pipleline();
    }

    void main_loop() {
        while (!glfwWindowShouldClose(m_window)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
        for (auto image_view : m_swapchain_image_views) {
            vkDestroyImageView(m_logical_device, image_view, nullptr);
        }

        vkDestroySwapchainKHR(m_logical_device, m_swapchain, nullptr);
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        vkDestroyDevice(m_logical_device, nullptr);

        if (ENABLE_VALIDATION_LAYERS) {
            proxy_destroy_debug_utils_messenger_ext(m_instance, m_debug_messenger, nullptr);
        }

        vkDestroyInstance(m_instance, nullptr);

        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    /* ---- Instance creation and validation layer helpers ---- */

    // Creates a vulkan instance
    // Creates vulcan create info
    // Sets up validation layers
    void create_instance() {
        if (ENABLE_VALIDATION_LAYERS && !(check_validation_layer_support())) {
            throw std::runtime_error(
                "TriangleApplication::create_instance => validation layers "
                "requested, but not available.");
        }

        VkApplicationInfo application_info{};
        application_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application_info.pApplicationName   = "triangle";
        application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        application_info.pEngineName        = "no_engine";
        application_info.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        application_info.apiVersion         = VK_API_VERSION_1_0;

        VkInstanceCreateInfo application_create_info{};
        application_create_info.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        application_create_info.pApplicationInfo = &application_info;

        std::vector<const char*> required_extensions = get_required_extensions();

        application_create_info.enabledExtensionCount =
            static_cast<uint32_t>(required_extensions.size());
        application_create_info.ppEnabledExtensionNames = required_extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info{};
        if (ENABLE_VALIDATION_LAYERS) {
            application_create_info.enabledLayerCount =
                static_cast<uint32_t>(m_validation_layers.size());
            application_create_info.ppEnabledLayerNames = m_validation_layers.data();

            debug_messenger_create_info.sType =
                VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debug_messenger_create_info.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debug_messenger_create_info.messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debug_messenger_create_info.pfnUserCallback = debug_callback;
            debug_messenger_create_info.pUserData       = nullptr;
            application_create_info.pNext               = &debug_messenger_create_info;
        } else {
            application_create_info.enabledLayerCount = 0;
            application_create_info.pNext             = nullptr;
        }

        if (vkCreateInstance(&application_create_info, nullptr, &m_instance) != VK_SUCCESS) {
            throw std::runtime_error(
                "TriangleApplication::create_instance => Failed to create a "
                "Vulkan instance.");
        }
    }

    void create_swapchain() {
        SwapChainSupportDetails support_details =
            query_swapchain_support_details(m_physical_device);

        VkSurfaceFormatKHR surface_format =
            choose_swapchain_surface_format(support_details.formats);
        VkPresentModeKHR present_mode =
            choose_swapchain_present_mode(support_details.present_modes);
        VkExtent2D extent = choose_swapchain_extent(support_details.capabilities);

        uint32_t image_count = support_details.capabilities.minImageCount + 1;

        if (support_details.capabilities.maxImageCount > 0 &&
            image_count > support_details.capabilities.maxImageCount) {
            image_count = support_details.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR create_info = {};
        create_info.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface                  = m_surface;
        create_info.minImageCount            = image_count;
        create_info.imageFormat              = surface_format.format;
        create_info.imageColorSpace          = surface_format.colorSpace;
        create_info.imageExtent              = extent;
        create_info.imageArrayLayers         = 1;
        create_info.imageUsage               = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices queue_family_indices = find_queue_familiy_indices(m_physical_device);
        uint32_t queue_family_indices_array[]   = {queue_family_indices.graphics_family.value(),
                                                   queue_family_indices.present_family.value()};

        if (queue_family_indices.graphics_family.value() ==
            queue_family_indices.present_family.value()) {
            create_info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
            create_info.queueFamilyIndexCount = 1;
            create_info.pQueueFamilyIndices   = &queue_family_indices.graphics_family.value();
        } else {
            create_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            create_info.queueFamilyIndexCount = 2;
            create_info.pQueueFamilyIndices   = queue_family_indices_array;
        }

        create_info.preTransform   = support_details.capabilities.currentTransform;
        create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        create_info.presentMode    = present_mode;
        create_info.clipped        = VK_TRUE;
        create_info.oldSwapchain   = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(m_logical_device, &create_info, nullptr, &m_swapchain) !=
            VK_SUCCESS) {
            throw std::runtime_error(
                "TriangleApplication::create_swap_chain => failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(m_logical_device, m_swapchain, &image_count, nullptr);
        m_swapchain_images.resize(image_count);
        vkGetSwapchainImagesKHR(m_logical_device, m_swapchain, &image_count,
                                m_swapchain_images.data());

        m_swapchain_format = surface_format.format;
        m_swapchain_extent = extent;
    }

    void check_extension_support() {
        uint32_t count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);

        std::vector<VkExtensionProperties> extensions(count);
        vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data());

        std::cout << "TriangleApplication::check_extension_support => Available extensions:\n";
        for (const auto& extension : extensions) {
            std::cout << '\t' << extension.extensionName << '\n';
        }
    }

    bool check_validation_layer_support() {
        uint32_t count;
        vkEnumerateInstanceLayerProperties(&count, nullptr);

        std::vector<VkLayerProperties> available_layers(count);
        vkEnumerateInstanceLayerProperties(&count, available_layers.data());

        for (const char* layer_name : m_validation_layers) {
            bool layer_found = false;

            for (const auto& layer_properties : available_layers) {
                if (std::strcmp(layer_name, layer_properties.layerName) == 0) {
                    layer_found = true;
                    break;
                }
            }

            if (!layer_found) {
                return false;
            }
        }

        return true;
    }

    std::vector<const char*> get_required_extensions() {
        uint32_t     glfw_extion_count = 0;
        const char** glfw_extensions;

        glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extion_count);

        std::vector<const char*> vulkan_extensions(glfw_extensions,
                                                   glfw_extensions + glfw_extion_count);

        if (ENABLE_VALIDATION_LAYERS) {
            vulkan_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        };

        return vulkan_extensions;
    }

    /* ---- Debug messenger helpers and proxies ---- */

    // Returns true if the debug message is to aborted
    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
        VkDebugUtilsMessageTypeFlagsEXT             message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* ptr_callback_data, void* ptr_user_data) {
        std::cerr << "Validation layer: " << ptr_callback_data->pMessage
                  << " Severity: " << message_severity << " Type: " << message_type << ptr_user_data
                  << std::endl;

        return VK_FALSE;
    }

    void setup_debug_messenger() {
        if (!ENABLE_VALIDATION_LAYERS) {
            return;
        }

        VkDebugUtilsMessengerCreateInfoEXT create_info{};
        create_info.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        create_info.pfnUserCallback = debug_callback;
        create_info.pUserData       = nullptr;

        if (proxy_create_debug_utils_messenger_ext(m_instance, &create_info, nullptr,
                                                   &m_debug_messenger) != VK_SUCCESS) {
            throw std::runtime_error(
                "TriangleApplication::setup_debug_messenger => failed to set "
                "up debug messenger.");
        }
    }

    static VkResult proxy_create_debug_utils_messenger_ext(
        VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* ptr_create_info,
        const VkAllocationCallbacks* ptr_allocator, VkDebugUtilsMessengerEXT* ptr_debug_messenger) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(instance, ptr_create_info, ptr_allocator, ptr_debug_messenger);
        } else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    static void proxy_destroy_debug_utils_messenger_ext(
        VkInstance instance, VkDebugUtilsMessengerEXT messenger,
        const VkAllocationCallbacks* ptr_allocator) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance, messenger, ptr_allocator);
        }
    }

    void create_surface() {
        if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS) {
            throw std::runtime_error(
                "TriangleApplication::create_surface => failed to create window surface!");
        }
    }

    void pick_physical_device() {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(m_instance, &count, nullptr);

        if (count == 0) {
            throw std::runtime_error(
                "TriangleApplication::pick_physical_device => Failed to find GPUs with Vulkan "
                "support.");
        }

        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

        std::multimap<uint32_t, VkPhysicalDevice> candidates;

        for (const auto& device : devices) {
            uint32_t score = rate_physical_device(device);
            candidates.insert(std::make_pair(score, device));
        }

        if (candidates.empty()) {
            throw std::runtime_error(
                "TriangleApplication::pick_physical_device => Failed to find a suitable GPU.");
        }

        if (candidates.rbegin()->first > 0) {
            m_physical_device = candidates.rbegin()->second;
        } else {
            throw std::runtime_error(
                "TriangleApplication::pick_physical_device => Failed to find a suitable GPU.");
        }

        if (m_physical_device == VK_NULL_HANDLE) {
            throw std::runtime_error(
                "TriangleApplication::pick_physical_device => Failed to find a suitable GPU.");
        }
    }

    uint32_t rate_physical_device(VkPhysicalDevice device) {
        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceFeatures   features;

        vkGetPhysicalDeviceFeatures(device, &features);
        vkGetPhysicalDeviceProperties(device, &properties);

        int score = 0;

        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        }

        score += properties.limits.maxImageDimension2D;

        if (!features.geometryShader) {
            return 0;
        }

        if (!is_physical_device_suitable(device)) {
            return 0;
        }

        return score;
    }

    bool is_physical_device_suitable(VkPhysicalDevice device) {
        QueueFamilyIndices queue_family_indices = find_queue_familiy_indices(device);

        bool are_extensions_supported = check_physical_device_extension_support(device);
        bool is_swap_chain_adequate =
            are_extensions_supported ? check_swapchain_support(device) : false;

        return queue_family_indices.is_complete() && are_extensions_supported &&
               is_swap_chain_adequate;
    }

    bool check_physical_device_extension_support(VkPhysicalDevice device) {
        uint32_t extension_count;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

        std::vector<VkExtensionProperties> available_extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count,
                                             available_extensions.data());

        std::set<std::string> required_extensions(m_device_extensions.begin(),
                                                  m_device_extensions.end());

        for (const auto& extension : available_extensions) {
            required_extensions.erase(extension.extensionName);
        }

        return required_extensions.empty();
    }

    bool check_swapchain_support(VkPhysicalDevice physical_device) {
        SwapChainSupportDetails details = query_swapchain_support_details(physical_device);
        return !details.formats.empty() && !details.present_modes.empty();
    }

    QueueFamilyIndices find_queue_familiy_indices(VkPhysicalDevice physical_device) {
        QueueFamilyIndices queue_family_indices{};

        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count,
                                                 queue_families.data());

        uint32_t i = 0;
        for (const auto& queue_family : queue_families) {
            if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                queue_family_indices.graphics_family = i;
            }

            VkBool32 has_presentation_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, m_surface,
                                                 &has_presentation_support);
            if (has_presentation_support) {
                queue_family_indices.present_family = i;
            }

            if (queue_family_indices.is_complete()) {
                break;
            }

            ++i;
        }

        return queue_family_indices;
    }

    void create_logical_device() {
        QueueFamilyIndices queue_family_indices = find_queue_familiy_indices(m_physical_device);

        std::vector<VkDeviceQueueCreateInfo> queue_create_infos{};
        std::set<uint32_t> unique_queue_families = {queue_family_indices.graphics_family.value(),
                                                    queue_family_indices.present_family.value()};

        float queue_priority = 1.0f;

        for (const uint32_t queue_family : unique_queue_families) {
            VkDeviceQueueCreateInfo queue_create_info = {};
            queue_create_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex        = queue_family;
            queue_create_info.queueCount              = 1;
            queue_create_info.pQueuePriorities        = &queue_priority;
            queue_create_infos.push_back(queue_create_info);
        }

        VkPhysicalDeviceFeatures physical_device_features = {};

        VkDeviceCreateInfo logical_device_create_info = {};
        logical_device_create_info.sType              = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        logical_device_create_info.queueCreateInfoCount =
            static_cast<uint32_t>(queue_create_infos.size());
        logical_device_create_info.pQueueCreateInfos = queue_create_infos.data();
        logical_device_create_info.pEnabledFeatures  = &physical_device_features;
        logical_device_create_info.enabledExtensionCount =
            static_cast<uint32_t>(m_device_extensions.size());
        logical_device_create_info.ppEnabledExtensionNames = m_device_extensions.data();

        // Previous implementations of Vulkan made a distinction between instance and device
        // specific validation layers. Backwards compatibility with older implementations of Vulkan.
        if (ENABLE_VALIDATION_LAYERS) {
            logical_device_create_info.enabledLayerCount =
                static_cast<uint32_t>(m_validation_layers.size());
            logical_device_create_info.ppEnabledLayerNames = m_validation_layers.data();
        } else {
            logical_device_create_info.enabledLayerCount   = 0;
            logical_device_create_info.ppEnabledLayerNames = nullptr;
        }

        if (vkCreateDevice(m_physical_device, &logical_device_create_info, nullptr,
                           &m_logical_device) != VK_SUCCESS) {
            throw std::runtime_error(
                "TriangleApplication::create_logical_device => failed to create logical device!");
        }

        vkGetDeviceQueue(m_logical_device, queue_family_indices.graphics_family.value(), 0,
                         &m_graphics_queue);
        vkGetDeviceQueue(m_logical_device, queue_family_indices.present_family.value(), 0,
                         &m_presentation_queue);
    }

    SwapChainSupportDetails query_swapchain_support_details(VkPhysicalDevice physical_device) {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, m_surface,
                                                  &details.capabilities);

        uint32_t format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, m_surface, &format_count, nullptr);

        if (format_count != 0) {
            details.formats.resize(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, m_surface, &format_count,
                                                 details.formats.data());
        }

        uint32_t present_mode_count;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, m_surface, &present_mode_count,
                                                  nullptr);

        if (present_mode_count != 0) {
            details.present_modes.resize(present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                physical_device, m_surface, &present_mode_count, details.present_modes.data());
        }

        return details;
    }

    VkSurfaceFormatKHR choose_swapchain_surface_format(
        const std::vector<VkSurfaceFormatKHR>& formats) {
        for (const auto& format : formats) {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }

        if (!formats.empty()) {
            return formats[0];
        }

        throw std::runtime_error(
            "TriangleApplication::choose_swap_chain_surface_format => Failed to find a suitable "
            "swap chain surface format.");
    }

    VkPresentModeKHR choose_swapchain_present_mode(
        const std::vector<VkPresentModeKHR>& present_modes) {
        for (const auto& present_mode : present_modes) {
            if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return present_mode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D choose_swapchain_extent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            int width{};
            int height{};

            glfwGetFramebufferSize(m_window, &width, &height);
            VkExtent2D extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

            extent.width  = std::clamp(extent.width, capabilities.minImageExtent.width,
                                       capabilities.maxImageExtent.width);
            extent.height = std::clamp(extent.height, capabilities.minImageExtent.height,
                                       capabilities.maxImageExtent.height);

            return extent;
        }
    }

    void create_image_views() {
        m_swapchain_image_views.resize(m_swapchain_images.size());

        for (size_t i = 0; i < m_swapchain_image_views.size(); ++i) {
            VkImageViewCreateInfo create_info           = {};
            create_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            create_info.image                           = m_swapchain_images[i];
            create_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            create_info.format                          = m_swapchain_format;
            create_info.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            create_info.subresourceRange.baseMipLevel   = 0;
            create_info.subresourceRange.levelCount     = 1;
            create_info.subresourceRange.baseArrayLayer = 0;
            create_info.subresourceRange.layerCount     = 1;

            if (vkCreateImageView(m_logical_device, &create_info, nullptr,
                                  &m_swapchain_image_views[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image views!");
            }
        }
    }

    void create_graphics_pipleline() {
        std::vector<char> vert_shader_code = read_file("shaders/vert.spv");
        std::vector<char> frag_shader_code = read_file("shaders/frag.spv");
    }

    static std::vector<char> read_file(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("TriangleApplication::read_file => failed to open file!");
        }

        size_t            file_size = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(file_size);

        file.seekg(0);
        file.read(buffer.data(), file_size);
        file.close();

        return buffer;
    }
};

int main() {
    TriangleApplication application;

    try {
        application.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
