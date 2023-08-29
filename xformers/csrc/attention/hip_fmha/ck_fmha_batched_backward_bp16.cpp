#include <ck.hpp>
#include <stdexcept>

#include "ck_fmha_batched_backward.h"

void batched_backward_bp16(BatchedBackwardParams& param, hipStream_t stream) {
  if (param.custom_mask_type == 0) {
    if (param.has_attn_bias)
      batched_backward_masktype_attnbias_dispatched<ck::bhalf_t, 0, true>(
          param, stream);
    else
      batched_backward_masktype_attnbias_dispatched<ck::bhalf_t, 0, false>(
          param, stream);
  } else if (param.custom_mask_type == 1) {
    if (param.has_attn_bias)
      batched_backward_masktype_attnbias_dispatched<ck::bhalf_t, 1, true>(
          param, stream);
    else
      batched_backward_masktype_attnbias_dispatched<ck::bhalf_t, 1, false>(
          param, stream);
  } else if (param.custom_mask_type == 2) {
    if (param.has_attn_bias)
      batched_backward_masktype_attnbias_dispatched<ck::bhalf_t, 2, true>(
          param, stream);
    else
      batched_backward_masktype_attnbias_dispatched<ck::bhalf_t, 2, false>(
          param, stream);
  } else
    throw std::runtime_error("Invalid custom_mask_type value");
};