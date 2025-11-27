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

    const std::vector<const char*> m_validation_layers = {
        "VK_LAYER_KHRONOS_validation"};

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

        mptr_window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "triangle",
                                    nullptr, nullptr);
    }

    void init_vulcan() {
        create_instance();
        check_extension_support();
    }

    void main_loop() {
        while (!glfwWindowShouldClose(mptr_window)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
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
        application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application_info.pApplicationName = "triangle";
        application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        application_info.pEngineName = "no_engine";
        application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        application_info.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &application_info;

        std::vector<const char*> required_extensions =
            get_required_extensions();
        create_info.enabledExtensionCount =
            static_cast<uint32_t>(required_extensions.size());
        create_info.ppEnabledExtensionNames = required_extensions.data();

        if (ENABLE_VALIDATION_LAYERS) {
            create_info.enabledLayerCount =
                static_cast<uint32_t>(m_validation_layers.size());
            create_info.ppEnabledLayerNames = m_validation_layers.data();
        } else {
            create_info.enabledLayerCount = 0;
        }

        VkResult instance_result =
            vkCreateInstance(&create_info, nullptr, &m_instance);
        if (instance_result != VK_SUCCESS) {
            throw std::runtime_error(
                "TriangleApplication::create_instance => Failed to create a "
                "Vulkan instance.");
        }
    }

    void check_extension_support() {
        uint32_t extension_count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
                                               nullptr);

        std::vector<VkExtensionProperties> extensions(extension_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
                                               extensions.data());

        std::cout << "Available extensions:\n";
        for (const auto& extension : extensions) {
            std::cout << '\t' << extension.extensionName << '\n';
        }
    }

    bool check_validation_layer_support() {
        uint32_t layer_count;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

        std::vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count,
                                           available_layers.data());

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

        glfw_extensions =
            glfwGetRequiredInstanceExtensions(&glfw_extension_cout);

        std::vector<const char*> extensions(
            glfw_extensions, glfw_extensions + glfw_extension_cout);

        if (ENABLE_VALIDATION_LAYERS) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        };

        return extensions;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* ptr_callback_data,
        void* ptr_user_data)
    {
        std::cerr << "Validation layer: " << ptr_callback_data->pMessage
                  << std::endl;

        return VK_FALSE;
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
