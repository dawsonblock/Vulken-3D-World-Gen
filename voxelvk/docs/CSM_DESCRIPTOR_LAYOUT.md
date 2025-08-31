
# CSM Descriptor Layout (binding 5)

## Option A — Add to your existing lighting set
```cpp
#include "src/render/csm_layout.hpp"

std::vector<VkDescriptorSetLayoutBinding> bindings = /* your current bindings */;
bindings.push_back(voxelvk::csm_shadow_binding(/*binding=*/5));

VkDescriptorSetLayoutCreateInfo lci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
lci.bindingCount = (uint32_t)bindings.size();
lci.pBindings = bindings.data();
vkCreateDescriptorSetLayout(device, &lci, nullptr, &lightingSetLayout);

// Later, write the descriptor with voxelvk::bind_csm_depth_array(...);
```

## Option B — Separate shadow set
```cpp
#include "src/render/csm_layout.hpp"
voxelvk::CSMSet csmSet;
create_csm_set(device, /*binding=*/5, csmSet);

// After building shadow map:
update_csm_set(device, csmSet, 5, csm.getDepthArrayView(), csm.getDepthSampler());

// Bind both sets at draw:
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                        /*firstSet=*/0, /*descriptorSetCount=*/2,
                        std::array<VkDescriptorSet,2>{lightingSet, csmSet.set}.data(),
                        0, nullptr);
```
