#define EIGEN_USE_THREADS

#include <iostream>

#include "correlation_kernel.h"
#include "pad.h"
#include "interpolation.h"

#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/register_types.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/platform/types.h"

namespace tensorflow {
typedef Eigen::GpuDevice GPUDevice;

template<typename Device>
class CorrelationGradKernel : public OpKernel {
  public:
    explicit CorrelationGradKernel(OpKernelConstruction *ctx) : OpKernel(ctx) {
      // Get the attributes
      OP_REQUIRES_OK(ctx, ctx->GetAttr("kernel_size", &kernel_size));
      OP_REQUIRES_OK(ctx, ctx->GetAttr("max_displacement", &max_displacement));
      OP_REQUIRES_OK(ctx, ctx->GetAttr("stride_1", &stride_1));
      OP_REQUIRES_OK(ctx, ctx->GetAttr("stride_2", &stride_2));
      OP_REQUIRES_OK(ctx, ctx->GetAttr("pad", &pad));
      OP_REQUIRES_OK(ctx, ctx->GetAttr("rate", &rate));

      OP_REQUIRES(ctx, kernel_size % 2 != 0, errors::InvalidArgument("kernel_size must be odd"));
    }

    void Compute(OpKernelContext *ctx) override {
      // Get the input images and verify their dimensions
      const Tensor& gradients_t = ctx->input(0);
      const Tensor& input_a_t   = ctx->input(1);
      const Tensor& input_b_t   = ctx->input(2);

      OP_REQUIRES(ctx, input_a_t.dims() == 4, errors::InvalidArgument("input_a must have rank 4"));
      OP_REQUIRES(ctx, input_b_t.dims() == 4, errors::InvalidArgument("input_b must have rank 4"));

      // Get dimensions of input
      const int batch_size          = input_a_t.dim_size(0);
      const int in_height           = input_a_t.dim_size(1);
      const int in_width            = input_a_t.dim_size(2);
      const int in_channels         = input_a_t.dim_size(3);
      const int in_count_per_sample = in_height * in_width * in_channels;
      const int padded_height       = in_height + 2 * pad;
      const int padded_width        = in_width + 2 * pad;

      //// For Interpolate
      int interpolated_height = (int)rate * (in_height - 1) + 1;
      int interpolated_width = (int)rate * (in_width - 1) + 1;
      int padded_interpolated_height = interpolated_height + 2 * pad;
      int padded_interpolated_width = interpolated_width + 2 * pad;
      const int in_count_per_sample_b = interpolated_height * interpolated_width * in_channels;

//      std::cout << "interpolated_height : " << interpolated_height << "\n";
//      std::cout << "interpolated_width : " << interpolated_width << "\n";
//      std::cout << "padded_interpolated_height : " << padded_interpolated_height << "\n";
//      std::cout << "padded_interpolated_width : " << padded_interpolated_width << "\n";
//      std::cout << "in_count_per_sample_b : " << in_count_per_sample_b << "\n";
      ////

      // The size of unreachable border region on each side
      const int kernel_radius = (kernel_size - 1) / 2;
      const int border_size   = max_displacement + kernel_radius;

      // Calculate the output dimensions
      const int out_height = ceil((float)(padded_height - border_size * 2) / stride_1);
      const int out_width  = ceil((float)(padded_width - border_size * 2) / stride_1);

      // const int neighborhood_grid_radius = max_displacement / (int)stride_2;
      const int neighborhood_grid_radius = max_displacement * (int)rate;
      const int neighborhood_grid_radius_a = max_displacement;
      const int neighborhood_grid_width  = neighborhood_grid_radius * 2 + 1;
      const int neighborhood_grid_width_a  = neighborhood_grid_radius_a * 2 + 1;
      const int out_channels             = neighborhood_grid_width * neighborhood_grid_width;

      // Allocate the memory for the outputs
      Tensor *output_a_gradient_t;
      OP_REQUIRES_OK(ctx, ctx->allocate_output(0, input_a_t.shape(), &output_a_gradient_t));
      Tensor *output_b_gradient_t;
      OP_REQUIRES_OK(ctx, ctx->allocate_output(1, input_b_t.shape(), &output_b_gradient_t));

      // Get the tensors
      auto gradients         = gradients_t.tensor<float, 4>();
      auto input_a           = input_a_t.tensor<float, 4>();
      auto input_b           = input_b_t.tensor<float, 4>();
      auto output_a_gradient = output_a_gradient_t->tensor<float, 4>();
      auto output_b_gradient = output_b_gradient_t->tensor<float, 4>();

      // Create temporary tensors for padded inputs
      Tensor padded_input_a_t;
      Tensor interpolated_input_b_t;
      Tensor padded_interpolated_input_b_t;

      OP_REQUIRES_OK(ctx,
                     ctx->allocate_temp(DataTypeToEnum<float>::value,
                                        TensorShape({ batch_size, padded_height, padded_width, in_channels }),
                                        &padded_input_a_t));
      OP_REQUIRES_OK(ctx,
                     ctx->allocate_temp(DataTypeToEnum<float>::value,
                                        TensorShape({ batch_size, interpolated_height, interpolated_width, in_channels }),
                                        &interpolated_input_b_t));
      OP_REQUIRES_OK(ctx,
                     ctx->allocate_temp(DataTypeToEnum<float>::value,
                                        TensorShape({ batch_size, padded_interpolated_height, padded_interpolated_width, in_channels }),
                                        &padded_interpolated_input_b_t));
      auto padded_input_a = padded_input_a_t.tensor<float, 4>();
      auto interpolated_input_b = interpolated_input_b_t.tensor<float, 4>();
      auto padded_interpolated_input_b = padded_interpolated_input_b_t.tensor<float, 4>();

      //// Use Interpolation function
      Interpolation(ctx->eigen_device<Device>(),
          input_b.data(),
          batch_size,
          in_height,
          in_width,
          in_channels,
          interpolated_height,
          interpolated_width,
          rate,
          interpolated_input_b.data()
          );
      ////

      // Pad the inputs
      Pad(ctx->eigen_device<Device>(),
          input_a.data(),
          batch_size,
          in_height,
          in_width,
          in_channels,
          padded_height,
          padded_width,
          padded_input_a.data());
      Pad(ctx->eigen_device<Device>(),
          interpolated_input_b.data(),
          batch_size,
          interpolated_height,
          interpolated_width,
          in_channels,
          padded_interpolated_height,
          padded_interpolated_width,
          padded_interpolated_input_b.data()
          );

      CorrelationGradA(ctx->eigen_gpu_device(),
                       batch_size,
                       out_width,
                       out_height,
                       out_channels,
                       max_displacement,
                       neighborhood_grid_radius,
                       neighborhood_grid_width,
                       kernel_radius,
                       (int)stride_1,
                       (int)stride_2,
                       in_width,
                       in_height,
                       padded_width,
                       padded_height,
                       in_channels,
                       in_count_per_sample,
                       pad,
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                       interpolated_width,
                       interpolated_height,
                       padded_interpolated_width,
                       padded_interpolated_height,
                       in_count_per_sample_b,
                       rate,
                       neighborhood_grid_radius_a,
                       neighborhood_grid_width_a,
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                       padded_interpolated_input_b.data(),
                       gradients.data(),
                       output_a_gradient.data());

      CorrelationGradB(ctx->eigen_gpu_device(),
                       batch_size,
                       out_width,
                       out_height,
                       out_channels,
                       max_displacement,
                       neighborhood_grid_radius,
                       neighborhood_grid_width,
                       kernel_radius,
                       (int)stride_1,
                       (int)stride_2,
                       in_width,
                       in_height,
                       padded_width,
                       padded_height,
                       in_channels,
                       in_count_per_sample,
                       pad,
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                       interpolated_width,
                       interpolated_height,
                       padded_interpolated_width,
                       padded_interpolated_height,
                       in_count_per_sample_b,
                       rate,
                       neighborhood_grid_radius_a,
                       neighborhood_grid_width_a,
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                       padded_input_a.data(),
                       gradients.data(),
                       output_b_gradient.data());
    }

  private:
    int kernel_size;
    int max_displacement;
    float stride_1;
    float stride_2;
    float rate;
    int pad;
};

REGISTER_KERNEL_BUILDER(Name("CorrelationGrad")
                        .Device(DEVICE_GPU),
                        CorrelationGradKernel<GPUDevice>)
} // end namespace tensorflow
