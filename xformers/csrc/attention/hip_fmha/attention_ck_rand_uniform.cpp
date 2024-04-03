/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <ATen/ATen.h>
#include <ATen/cuda/CUDAContext.h>
#include <ATen/cuda/CUDAGeneratorImpl.h>
#include <c10/core/TensorOptions.h>
#include <torch/library.h>
#include <torch/types.h>
#include <ATen/cuda/CUDAGraphsUtils.cuh>

#include <ck/ck.hpp>
#include <ck/host_utility/kernel_launch.hpp>

#include "ck_tiled_fmha_rand_uniform_kernel.hpp"

namespace {

/**
 * generate a tensor with random uniform values. only used for testing, not much
 * attention is paid to performance
 */
at::Tensor rand_uniform_int(
    double dropout_prob,
    const at::Tensor& out_pattern) // [Batches, num_head, query_len, key_len]
{
  int B = out_pattern.size(0);
  int num_heads = out_pattern.size(1);
  int M = out_pattern.size(2);
  int N = out_pattern.size(3);

  // at::cuda::CUDAGuard device_guard(out_pattern.device());
  hipStream_t stream = at::cuda::getCurrentHIPStream().stream();

  at::CUDAGeneratorImpl* gen =
      at::get_generator_or_default<at::CUDAGeneratorImpl>(
          c10::nullopt, at::cuda::detail::getDefaultCUDAGenerator());

  at::PhiloxCudaState rng_engine_inputs;
  {
    std::lock_guard<std::mutex> lock(gen->mutex_);
    rng_engine_inputs = gen->philox_cuda_state(B * num_heads * M * N);
  }

  const auto seeds = at::cuda::philox::unpack(rng_engine_inputs);

  int64_t philox_seed = std::get<0>(seeds);
  int64_t philox_offset = std::get<1>(seeds);

  at::Tensor randvals;

  randvals = at::empty(
      {B, num_heads, M, N}, out_pattern.options().dtype(at::ScalarType::Int));

  {
    using FmhaRandUniformKernel_ =
        FmhaRandUniformKernel<128, 64, 32, int32_t, false>;

    const auto kargs = FmhaRandUniformKernel_::MakeKargs(
        randvals.data_ptr(),
        M,
        N,
        num_heads,
        B,
        static_cast<int>(randvals.stride(2)),
        static_cast<int>(randvals.stride(3)),
        static_cast<int>(randvals.stride(1)),
        static_cast<int>(randvals.stride(0)),
        {philox_seed, philox_offset});

    dim3 kGridSize = FmhaRandUniformKernel_::GridSize(B, num_heads, M, N);
    constexpr dim3 kBlockSize = FmhaRandUniformKernel_::BlockSize();
    constexpr ck::index_t kBlockPerCu = FmhaRandUniformKernel_::kBlockPerCu;

    (void)launch_kernel<kBlockSize.x, kBlockPerCu>(
        StreamConfig{stream, false},
        FmhaRandUniformKernel_{},
        kGridSize,
        kBlockSize,
        0,
        kargs);
  }

  (void)hipStreamSynchronize(stream);

  return randvals;
} // namespace

} // namespace

TORCH_LIBRARY_IMPL(xformers, CUDA, m) {
  m.impl(
      TORCH_SELECTIVE_NAME("xformers::_ck_rand_uniform"),
      TORCH_FN(rand_uniform_int));
}
