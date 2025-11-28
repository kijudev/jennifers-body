#include <GLFW/glfw3.h>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <vector>

class TriangleApplication {
   private:
    static constexpr uint32_t WINDOW_WIDTH = 800;
    static constexpr uint32_t WINDOW_HEIGHT = 600;

#ifdef NDEBUG
    static constexpr bool ENABLE_VALIDATION_LAYERS = false;
#else
    static constexpr bool ENABLE_VALIDATION_LAYERS = true;
#endif

    GLFWwindow* mptr_window;
    VkInstance m_instance;
    VkDebugUtilsMessengerEXT m_debug_messenger;

    const std::vector<const char*> m_validation_layers = {"VK_LAYER_KHRONOS_validation"};

   public:
    void run() {
        init_window();
        init_vulcan();
        main_loop();
        cleanup();
    }

   private:
    void init_window() {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        mptr_window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "triangle", nullptr, nullptr);
    }

    void init_vulcan() {
        create_instance();
        check_extension_support();
        setup_debug_messenger();
    }

    void main_loop() {
        while (!glfwWindowShouldClose(mptr_window)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
        if (ENABLE_VALIDATION_LAYERS) {
            proxy_destroy_debug_utils_messenger_ext(m_instance, m_debug_messenger, nullptr);
        }

        vkDestroyInstance(m_instance, nullptr);

        glfwDestroyWindow(mptr_window);
        glfwTerminate();
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
        }

        VkResult instance_result = vkCreateInstance(&instance_create_info, nullptr, &m_instance);
        if (instance_result != VK_SUCCESS) {
            throw std::runtime_error(
                "TriangleApplication::create_instance => Failed to create a "
                "Vulkan instance.");
        }
    }

    void populate_application_info(VkApplicationInfo& application_info) {
        application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application_info.pApplicationName = "triangle";
        application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        application_info.pEngineName = "no_engine";
        application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        application_info.apiVersion = VK_API_VERSION_1_0;
    }

    void populate_instance_create_info(VkInstanceCreateInfo& instance_create_info,
                                       VkApplicationInfo* application_info) {
        instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_create_info.pApplicationInfo = application_info;
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
        uint32_t extension_count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

        std::vector<VkExtensionProperties> extensions(extension_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());

        std::cout << "Available extensions:\n";
        for (const auto& extension : extensions) {
            std::cout << '\t' << extension.extensionName << '\n';
        }
    }

    bool check_validation_layer_support() {
        uint32_t layer_count;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

        std::vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

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
        uint32_t glfw_extension_cout = 0;
        const char** glfw_extensions;

        glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_cout);

        std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_cout);

        if (ENABLE_VALIDATION_LAYERS) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        };

        return extensions;
    }

    // Returns true if the debug message is to aborted
    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* ptr_callback_data, void* ptr_user_data) {
        std::cerr << "Validation layer: " << ptr_callback_data->pMessage << std::endl;

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
        VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger,
        const VkAllocationCallbacks* ptr_allocator) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance, debug_messenger, ptr_allocator);
        }
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
