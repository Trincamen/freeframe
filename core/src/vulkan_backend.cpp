#include <volk.h>

#include <string>
#include <vector>

#include "freeframe/format.hpp"
#include "freeframe/gpu_backend.hpp"

namespace ff {

namespace {

GpuType toGpuType(VkPhysicalDeviceType t) {
    switch (t) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return GpuType::Discrete;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return GpuType::Integrated;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return GpuType::Virtual;
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return GpuType::Cpu;
    default: return GpuType::Other;
    }
}

class VulkanBackend final : public IGpuBackend {
public:
    GpuApi api() const override { return GpuApi::Vulkan; }

    Result<std::vector<GpuInfo>> enumerateAdapters() override {
        using R = Result<std::vector<GpuInfo>>;

        if (volkInitialize() != VK_SUCCESS) {
            return R::failure("Vulkan loader (vulkan-1.dll / libvulkan.so.1) not found - "
                              "install/update the GPU driver");
        }

        // Request 1.2, or 1.0 if the loader is older than 1.1 (no vkEnumerateInstanceVersion).
        uint32_t wanted = VK_API_VERSION_1_2;
        const uint32_t loaderVersion = volkGetInstanceVersion();
        if (loaderVersion < VK_API_VERSION_1_1) {
            wanted = VK_API_VERSION_1_0;
        }

        VkApplicationInfo app{};
        app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app.pApplicationName = "freeframe";
        app.applicationVersion = 1;
        app.pEngineName = "FreeFrame";
        app.engineVersion = 1;
        app.apiVersion = wanted;

        VkInstanceCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ci.pApplicationInfo = &app;

        VkInstance instance = VK_NULL_HANDLE;
        const VkResult cr = vkCreateInstance(&ci, nullptr, &instance);
        if (cr != VK_SUCCESS) {
            return R::failure("vkCreateInstance failed (VkResult " + std::to_string(static_cast<int>(cr)) + ")");
        }
        volkLoadInstance(instance);

        uint32_t count = 0;
        VkResult er = vkEnumeratePhysicalDevices(instance, &count, nullptr);
        std::vector<VkPhysicalDevice> devices;
        if (er == VK_SUCCESS && count > 0) {
            devices.resize(count);
            er = vkEnumeratePhysicalDevices(instance, &count, devices.data());
            if (er == VK_SUCCESS || er == VK_INCOMPLETE) {
                devices.resize(count);
                er = VK_SUCCESS;
            }
        }

        std::vector<GpuInfo> result;
        if (er == VK_SUCCESS) {
            for (VkPhysicalDevice dev : devices) {
                VkPhysicalDeviceProperties props{};
                vkGetPhysicalDeviceProperties(dev, &props);
                VkPhysicalDeviceMemoryProperties mem{};
                vkGetPhysicalDeviceMemoryProperties(dev, &mem);

                uint64_t local = 0;
                for (uint32_t i = 0; i < mem.memoryHeapCount; ++i) {
                    if (mem.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                        local += mem.memoryHeaps[i].size;
                    }
                }

                GpuInfo info;
                info.name = props.deviceName;
                info.api = GpuApi::Vulkan;
                info.type = toGpuType(props.deviceType);
                info.vendorId = props.vendorID;
                info.deviceId = props.deviceID;
                info.dedicatedVideoMemoryBytes = local;
                info.driverVersion = decodeDriverVersion(props.vendorID, props.driverVersion);
                info.apiVersion = decodeVulkanApiVersion(props.apiVersion);
                result.push_back(std::move(info));
            }
        }

        vkDestroyInstance(instance, nullptr);

        if (er != VK_SUCCESS) {
            return R::failure("vkEnumeratePhysicalDevices failed (VkResult " +
                              std::to_string(static_cast<int>(er)) + ")");
        }
        if (result.empty()) {
            return R::failure("no Vulkan devices found");
        }
        return R::success(std::move(result));
    }
};

} // namespace

std::unique_ptr<IGpuBackend> createVulkanBackend() {
    return std::make_unique<VulkanBackend>();
}

} // namespace ff
