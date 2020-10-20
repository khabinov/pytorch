#include <ATen/native/vulkan/ops/Common.h>
#include <torch/library.h>

namespace at {
namespace native {
namespace vulkan {
namespace ops {
namespace {

Tensor add(
    const Tensor& self_,
    const Tensor& other_,
    const Scalar alpha) {
  api::Context* const context = api::context();

  const Tensor self = self_.is_vulkan() ? self_ : self_.vulkan();
  const vTensor& v_self = convert(self);

  const Tensor other = other_.is_vulkan() ? other_ : other_.vulkan();
  const vTensor& v_other = convert(other);

  vTensor v_output(context, self.sizes().vec(), self.options());

  api::Command::Buffer command_buffer = context->command().pool.allocate();
  command_buffer.begin();

  if (v_output.has_image() && v_self.has_image() && v_other.has_image()) {
    const struct {
      uint32_t W;
      uint32_t H;
      uint32_t C;
      float alpha;
    } block {
    };

    context->dispatch(
        command_buffer,
        {
          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        },
        VK_KERNEL(add),
        {
          8, 8, 1,
        },
        v_output.extents(),
        v_output.image(command_buffer, vTensor::Access::Write),
        v_self.image(command_buffer),
        v_other.image(command_buffer),
        context->resource().pool.uniform(block).object);
  }
  else {
    // TODO: Convert all to buffer and dispatch a buffer-only kernel.
  }

  command_buffer.end();
  command_buffer.submit(context->gpu().queue);

  return convert(v_output);
}

#ifdef USE_VULKAN_API

TORCH_LIBRARY_IMPL(aten, Vulkan, m) {
  m.impl("add.Tensor", TORCH_FN(at::native::vulkan::ops::add));
}

#endif /* USE_VULKAN_API */

} // namespace
} // namespace ops
} // namespace vulkan
} // namespace native
} // namespace at
