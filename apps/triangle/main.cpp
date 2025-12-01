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
#include <stdexcept>
#include <utility>
#include <vector>
#include <optional>

class QueueFamilyIndices {
public:
    std::optional<uint32_t> graphics_family;

    bool is_complete() {
        return graphics_family.has_value();
    }
};

class TriangleApplication {
   private:

    static constexpr uint32_t WINDOW_WIDTH = 800;
    static constexpr uint32_t WINDOW_HEIGHT = 600;

#ifdef NDEBUG
    static constexpr bool ENABLE_VALIDATION_LAYERS = false;
#else
    static constexpr bool ENABLE_VALIDATION_LAYERS = true;
#endif

    GLFWwindow* mptr_window = nullptr;
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_logical_device = VK_NULL_HANDLE;
    VkQueue m_graphics_queue = VK_NULL_HANDLE;

    const std::vector<const char*> m_validation_layers = {"VK_LAYER_KHRONOS_validation"};

   public:
    void run() {
        init();
        main_loop();
        cleanup();
    }

   private:
    void init() {
        init_glfw();
        init_window();
        init_vulcan();
    }

    void cleanup() {
        if (ENABLE_VALIDATION_LAYERS) {
            proxy_destroy_debug_utils_messenger_ext(m_instance, m_debug_messenger, nullptr);
        }

        vkDestroyDevice(m_logical_device, nullptr);
        vkDestroyInstance(m_instance, nullptr);

        glfwDestroyWindow(mptr_window);
        glfwTerminate();
    }

    void init_glfw() {
        int result = glfwInit();
        if (!result) {
            throw std::runtime_error("TriangleApplication::init_glfw => Failed to initialize GLFW.");
        }
    }

    void init_window() {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        mptr_window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "triangle", nullptr, nullptr);
        if (!mptr_window) {
            glfwTerminate();
            throw std::runtime_error("TriangleApplication::init_window => Failed to create GLFW window");
        }
    }

    void init_vulcan() {
        create_instance();
        check_extension_support();
        setup_debug_messenger();
        pick_physical_device();
        create_logical_device();
    }

    void main_loop() {
        while (!glfwWindowShouldClose(mptr_window)) {
            glfwPollEvents();
        }
    }

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
        populate_application_info(application_info);

        VkInstanceCreateInfo instance_create_info{};
        populate_instance_create_info(instance_create_info, &application_info);

        std::vector<const char*> required_extensions = get_required_extensions();

        instance_create_info.enabledExtensionCount =
            static_cast<uint32_t>(required_extensions.size());
        instance_create_info.ppEnabledExtensionNames = required_extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info{};
        if (ENABLE_VALIDATION_LAYERS) {
            instance_create_info.enabledLayerCount =
                static_cast<uint32_t>(m_validation_layers.size());
            instance_create_info.ppEnabledLayerNames = m_validation_layers.data();

            populate_debug_messenger_create_info(debug_messenger_info);
            instance_create_info.pNext = &debug_messenger_info;
        } else {
            instance_create_info.enabledLayerCount = 0;
            instance_create_info.pNext = nullptr;
        }

        VkResult instance_result = vkCreateInstance(&instance_create_info, nullptr, &m_instance);
        if (instance_result != VK_SUCCESS) {
            throw std::runtime_error(
                "TriangleApplication::create_instance => Failed to create a "
                "Vulkan instance.");
        }
    }

    void populate_application_info(VkApplicationInfo& info) {
        info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        info.pApplicationName = "triangle";
        info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        info.pEngineName = "no_engine";
        info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        info.apiVersion = VK_API_VERSION_1_0;
    }

    void populate_instance_create_info(VkInstanceCreateInfo& create_info,
                                       VkApplicationInfo* application_info) {
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = application_info;
    }

    void setup_debug_messenger() {
        if (!ENABLE_VALIDATION_LAYERS) {
            return;
        }

        VkDebugUtilsMessengerCreateInfoEXT create_info{};
        populate_debug_messenger_create_info(create_info);

        if (proxy_create_debug_utils_messenger_ext(m_instance, &create_info, nullptr,
                                                   &m_debug_messenger) != VK_SUCCESS) {
            throw std::runtime_error(
                "TriangleApplication::setup_debug_messenger => failed to set "
                "up debug messenger.");
        }
    }

    void populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& create_info) {
        create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

        create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

        create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        create_info.pfnUserCallback = debug_callback;
        create_info.pUserData = nullptr;
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
        uint32_t glfw_extion_count = 0;
        const char** glfw_extensions;

        glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extion_count);

        std::vector<const char*> vulkan_extensions(glfw_extensions, glfw_extensions + glfw_extion_count);

        if (ENABLE_VALIDATION_LAYERS) {
            vulkan_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        };

        return vulkan_extensions;
    }

    // Returns true if the debug message is to aborted
    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* ptr_callback_data, void* ptr_user_data) {
        std::cerr << "Validation layer: " << ptr_callback_data->pMessage
                  << " Severity: " << message_severity << " Type: " << message_type << ptr_user_data
                  << std::endl;

        return VK_FALSE;
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

    void pick_physical_device() {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(m_instance, &count, nullptr);

        if (count == 0) {
            throw std::runtime_error(
                "TriangleApplication::pick_physical_device => Failed to find GPUs with Vulkan support.");
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
        VkPhysicalDeviceFeatures features;

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
        QueueFamilyIndices indices = find_queue_familiy_indeces(device);
        return indices.is_complete();
    }

    QueueFamilyIndices find_queue_familiy_indeces(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

        uint32_t i = 0;
        for (const auto& queue_family : queue_families) {
            if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphics_family = i;
            }

            if (indices.is_complete()) {
                break;
            }

            ++i;
        }

        return indices;
    }

    void create_logical_device() {
        QueueFamilyIndices queue_family_indices = find_queue_familiy_indeces(m_physical_device);

        VkDeviceQueueCreateInfo queue_create_info = {};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family_indices.graphics_family.value();
        queue_create_info.queueCount = 1;

        float queue_priority = 1.0f;
        queue_create_info.pQueuePriorities = &queue_priority;

        VkPhysicalDeviceFeatures physical_device_features = {};

        VkDeviceCreateInfo logical_device_create_info = {};
        logical_device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        logical_device_create_info.queueCreateInfoCount = 1;
        logical_device_create_info.pQueueCreateInfos = &queue_create_info;
        logical_device_create_info.pEnabledFeatures = &physical_device_features;

        // Previous implementations of Vulkan made a distinction between instance and device specific validation layers.
        // Backwards compatibility with older implementations of Vulkan.
        if (ENABLE_VALIDATION_LAYERS) {
            logical_device_create_info.enabledLayerCount = static_cast<uint32_t>(m_validation_layers.size());
            logical_device_create_info.ppEnabledLayerNames = m_validation_layers.data();
        } else {
            logical_device_create_info.enabledLayerCount = 0;
            logical_device_create_info.ppEnabledLayerNames = nullptr;
        }

        if (vkCreateDevice(m_physical_device, &logical_device_create_info, nullptr, &m_logical_device) != VK_SUCCESS) {
            throw std::runtime_error("TriangleApplication::create_logical_device => failed to create logical device!");
        }

        vkGetDeviceQueue(m_logical_device, queue_family_indices.graphics_family.value(), 0, &m_graphics_queue);
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
