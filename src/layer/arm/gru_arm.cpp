// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "gru_arm.h"

#if __ARM_NEON
#include <arm_neon.h>
#endif // __ARM_NEON

#include "arm_activation.h"

#include <math.h>

#include "cpu.h"

namespace ncnn {

GRU_arm::GRU_arm()
{
#if __ARM_NEON
#if NCNN_ARM82
    support_fp16_storage = cpu_support_arm_asimdhp();
#endif
#endif // __ARM_NEON

#if NCNN_BF16
    support_bf16_storage = true;
#endif
}

int GRU_arm::create_pipeline(const Option& opt)
{
#if NCNN_ARM82
    if (support_fp16_storage && opt.use_fp16_storage)
    {
        return create_pipeline_fp16s(opt);
    }
#endif

#if NCNN_BF16
    if (opt.use_bf16_storage)
    {
        return create_pipeline_bf16s(opt);
    }
#endif

    // pack RUN
    int num_directions = direction == 2 ? 2 : 1;
    int size = weight_data_size / num_directions / num_output / 3;

#if __ARM_NEON
    weight_xc_data_packed.create(size * 12, num_output / 4 + num_output % 4, num_directions);
    bias_c_data_packed.create(num_output, 1, num_directions, 16u, 4);
    weight_hc_data_packed.create(num_output * 12, num_output / 4 + num_output % 4, num_directions);
#else
    weight_xc_data_packed.create(size * 3, num_output, num_directions);
    bias_c_data_packed.create(num_output, 1, num_directions, 16u, 4);
    weight_hc_data_packed.create(num_output * 3, num_output, num_directions);
#endif

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int dr = 0; dr < num_directions; dr++)
    {
        const Mat weight_xc = weight_xc_data.channel(dr);
        const Mat bias_c = bias_c_data.channel(dr);
        const Mat weight_hc = weight_hc_data.channel(dr);

        Mat weight_xc_data_packed_dr = weight_xc_data_packed.channel(dr);
        Mat bias_c_data_packed_dr = bias_c_data_packed.channel(dr);
        Mat weight_hc_data_packed_dr = weight_hc_data_packed.channel(dr);

        const float* bias_c_R = bias_c.row(0);
        const float* bias_c_U = bias_c.row(1);
        const float* bias_c_WN = bias_c.row(2);
        const float* bias_c_BN = bias_c.row(3);

        float* bias_c_RUBNWN = bias_c_data_packed_dr.row(0);

        int q = 0;
#if __ARM_NEON
        for (; q + 3 < num_output; q += 4)
        {
            bias_c_RUBNWN[0] = bias_c_R[q];
            bias_c_RUBNWN[1] = bias_c_R[q + 1];
            bias_c_RUBNWN[2] = bias_c_R[q + 2];
            bias_c_RUBNWN[3] = bias_c_R[q + 3];
            bias_c_RUBNWN[4] = bias_c_U[q];
            bias_c_RUBNWN[5] = bias_c_U[q + 1];
            bias_c_RUBNWN[6] = bias_c_U[q + 2];
            bias_c_RUBNWN[7] = bias_c_U[q + 3];
            bias_c_RUBNWN[8] = bias_c_BN[q];
            bias_c_RUBNWN[9] = bias_c_BN[q + 1];
            bias_c_RUBNWN[10] = bias_c_BN[q + 2];
            bias_c_RUBNWN[11] = bias_c_BN[q + 3];
            bias_c_RUBNWN[12] = bias_c_WN[q];
            bias_c_RUBNWN[13] = bias_c_WN[q + 1];
            bias_c_RUBNWN[14] = bias_c_WN[q + 2];
            bias_c_RUBNWN[15] = bias_c_WN[q + 3];

            bias_c_RUBNWN += 16;

            const float* weight_xc_R = weight_xc.row(num_output * 0 + q);
            const float* weight_xc_U = weight_xc.row(num_output * 1 + q);
            const float* weight_xc_N = weight_xc.row(num_output * 2 + q);

            const float* weight_xc_R_1 = weight_xc.row(num_output * 0 + q + 1);
            const float* weight_xc_U_1 = weight_xc.row(num_output * 1 + q + 1);
            const float* weight_xc_N_1 = weight_xc.row(num_output * 2 + q + 1);

            const float* weight_xc_R_2 = weight_xc.row(num_output * 0 + q + 2);
            const float* weight_xc_U_2 = weight_xc.row(num_output * 1 + q + 2);
            const float* weight_xc_N_2 = weight_xc.row(num_output * 2 + q + 2);

            const float* weight_xc_R_3 = weight_xc.row(num_output * 0 + q + 3);
            const float* weight_xc_U_3 = weight_xc.row(num_output * 1 + q + 3);
            const float* weight_xc_N_3 = weight_xc.row(num_output * 2 + q + 3);

            const float* weight_hc_R = weight_hc.row(num_output * 0 + q);
            const float* weight_hc_U = weight_hc.row(num_output * 1 + q);
            const float* weight_hc_N = weight_hc.row(num_output * 2 + q);

            const float* weight_hc_R_1 = weight_hc.row(num_output * 0 + q + 1);
            const float* weight_hc_U_1 = weight_hc.row(num_output * 1 + q + 1);
            const float* weight_hc_N_1 = weight_hc.row(num_output * 2 + q + 1);

            const float* weight_hc_R_2 = weight_hc.row(num_output * 0 + q + 2);
            const float* weight_hc_U_2 = weight_hc.row(num_output * 1 + q + 2);
            const float* weight_hc_N_2 = weight_hc.row(num_output * 2 + q + 2);

            const float* weight_hc_R_3 = weight_hc.row(num_output * 0 + q + 3);
            const float* weight_hc_U_3 = weight_hc.row(num_output * 1 + q + 3);
            const float* weight_hc_N_3 = weight_hc.row(num_output * 2 + q + 3);

            float* weight_xc_RUN = weight_xc_data_packed_dr.row(q / 4);
            float* weight_hc_RUN = weight_hc_data_packed_dr.row(q / 4);

            for (int i = 0; i < size; i++)
            {
                weight_xc_RUN[0] = weight_xc_R[i];
                weight_xc_RUN[1] = weight_xc_R_1[i];
                weight_xc_RUN[2] = weight_xc_R_2[i];
                weight_xc_RUN[3] = weight_xc_R_3[i];
                weight_xc_RUN[4] = weight_xc_U[i];
                weight_xc_RUN[5] = weight_xc_U_1[i];
                weight_xc_RUN[6] = weight_xc_U_2[i];
                weight_xc_RUN[7] = weight_xc_U_3[i];

                weight_xc_RUN += 8;
            }

            for (int i = 0; i < num_output; i++)
            {
                weight_hc_RUN[0] = weight_hc_R[i];
                weight_hc_RUN[1] = weight_hc_R_1[i];
                weight_hc_RUN[2] = weight_hc_R_2[i];
                weight_hc_RUN[3] = weight_hc_R_3[i];
                weight_hc_RUN[4] = weight_hc_U[i];
                weight_hc_RUN[5] = weight_hc_U_1[i];
                weight_hc_RUN[6] = weight_hc_U_2[i];
                weight_hc_RUN[7] = weight_hc_U_3[i];

                weight_hc_RUN += 8;
            }

            for (int i = 0; i < size; i++)
            {
                weight_xc_RUN[0] = weight_xc_N[i];
                weight_xc_RUN[1] = weight_xc_N_1[i];
                weight_xc_RUN[2] = weight_xc_N_2[i];
                weight_xc_RUN[3] = weight_xc_N_3[i];

                weight_xc_RUN += 4;
            }

            for (int i = 0; i < num_output; i++)
            {
                weight_hc_RUN[0] = weight_hc_N[i];
                weight_hc_RUN[1] = weight_hc_N_1[i];
                weight_hc_RUN[2] = weight_hc_N_2[i];
                weight_hc_RUN[3] = weight_hc_N_3[i];

                weight_hc_RUN += 4;
            }
        }
#endif // __ARM_NEON
        for (; q < num_output; q++)
        {
            bias_c_RUBNWN[0] = bias_c_R[q];
            bias_c_RUBNWN[1] = bias_c_U[q];
            bias_c_RUBNWN[2] = bias_c_BN[q];
            bias_c_RUBNWN[3] = bias_c_WN[q];

            bias_c_RUBNWN += 4;

            const float* weight_xc_R = weight_xc.row(num_output * 0 + q);
            const float* weight_xc_U = weight_xc.row(num_output * 1 + q);
            const float* weight_xc_N = weight_xc.row(num_output * 2 + q);

            const float* weight_hc_R = weight_hc.row(num_output * 0 + q);
            const float* weight_hc_U = weight_hc.row(num_output * 1 + q);
            const float* weight_hc_N = weight_hc.row(num_output * 2 + q);

#if __ARM_NEON
            float* weight_xc_RUN = weight_xc_data_packed_dr.row(q / 4 + q % 4);
            float* weight_hc_RUN = weight_hc_data_packed_dr.row(q / 4 + q % 4);
#else
            float* weight_xc_RUN = weight_xc_data_packed_dr.row(q);
            float* weight_hc_RUN = weight_hc_data_packed_dr.row(q);
#endif // __ARM_NEON

            for (int i = 0; i < size; i++)
            {
                weight_xc_RUN[0] = weight_xc_R[i];
                weight_xc_RUN[1] = weight_xc_U[i];

                weight_xc_RUN += 2;
            }

            for (int i = 0; i < num_output; i++)
            {
                weight_hc_RUN[0] = weight_hc_R[i];
                weight_hc_RUN[1] = weight_hc_U[i];

                weight_hc_RUN += 2;
            }

            for (int i = 0; i < size; i++)
            {
                weight_xc_RUN[0] = weight_xc_N[i];

                weight_xc_RUN += 1;
            }

            for (int i = 0; i < num_output; i++)
            {
                weight_hc_RUN[0] = weight_hc_N[i];

                weight_hc_RUN += 1;
            }
        }
    }

    return 0;
}

static int gru(const Mat& bottom_blob, Mat& top_blob, int reverse, const Mat& weight_xc, const Mat& bias_c, const Mat& weight_hc, Mat& hidden_state, const Option& opt)
{
    int size = bottom_blob.w;
    int T = bottom_blob.h;

    int num_output = top_blob.w;

    // 2 x num_output
#if __ARM_NEON
    Mat gates(4 * 2, num_output / 4 + num_output % 4, 4u, opt.workspace_allocator);
#else
    Mat gates(2, num_output, 4u, opt.workspace_allocator);
#endif
    if (gates.empty())
        return -100;

    // unroll
    for (int t = 0; t < T; t++)
    {
        int ti = reverse ? T - 1 - t : t;

        int remain_num_output_start = 0;
#if __ARM_NEON
        int nn_num_output = num_output >> 2;
        remain_num_output_start = nn_num_output << 2;

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int qq = 0; qq < nn_num_output; qq++)
        {
            int q = qq * 4;

            const float* x = bottom_blob.row(ti);

            // gate reset update
            const float* bias_c_RUBNWN = (const float*)bias_c + q * 4;

            const float* weight_xc_RUN = weight_xc.row(q / 4);
            const float* weight_hc_RUN = weight_hc.row(q / 4);

            float32x4_t _R = vld1q_f32(bias_c_RUBNWN);
            float32x4_t _U = vld1q_f32(bias_c_RUBNWN + 4);
            float32x4_t _sum1 = vdupq_n_f32(0.f);
            float32x4_t _sum2 = vdupq_n_f32(0.f);
            float32x4_t _sum3 = vdupq_n_f32(0.f);
            float32x4_t _sum4 = vdupq_n_f32(0.f);
            float32x4_t _sum5 = vdupq_n_f32(0.f);
            float32x4_t _sum6 = vdupq_n_f32(0.f);

            int i = 0;
            for (; i + 3 < size; i += 4)
            {
                float32x4_t _xi = vld1q_f32(x + i);
                float32x4_t _weight_xc_R = vld1q_f32(weight_xc_RUN);
                float32x4_t _weight_xc_U = vld1q_f32(weight_xc_RUN + 4);
                float32x4_t _weight_xc_R_1 = vld1q_f32(weight_xc_RUN + 8);
                float32x4_t _weight_xc_U_1 = vld1q_f32(weight_xc_RUN + 12);
                float32x4_t _weight_xc_R_2 = vld1q_f32(weight_xc_RUN + 16);
                float32x4_t _weight_xc_U_2 = vld1q_f32(weight_xc_RUN + 20);
                float32x4_t _weight_xc_R_3 = vld1q_f32(weight_xc_RUN + 24);
                float32x4_t _weight_xc_U_3 = vld1q_f32(weight_xc_RUN + 28);
#if __aarch64__
                _R = vfmaq_laneq_f32(_R, _weight_xc_R, _xi, 0);
                _U = vfmaq_laneq_f32(_U, _weight_xc_U, _xi, 0);
                _sum1 = vfmaq_laneq_f32(_sum1, _weight_xc_R_1, _xi, 1);
                _sum2 = vfmaq_laneq_f32(_sum2, _weight_xc_U_1, _xi, 1);
                _sum3 = vfmaq_laneq_f32(_sum3, _weight_xc_R_2, _xi, 2);
                _sum4 = vfmaq_laneq_f32(_sum4, _weight_xc_U_2, _xi, 2);
                _sum5 = vfmaq_laneq_f32(_sum5, _weight_xc_R_3, _xi, 3);
                _sum6 = vfmaq_laneq_f32(_sum6, _weight_xc_U_3, _xi, 3);
#else
                _R = vmlaq_lane_f32(_R, _weight_xc_R, vget_low_f32(_xi), 0);
                _U = vmlaq_lane_f32(_U, _weight_xc_U, vget_low_f32(_xi), 0);
                _sum1 = vmlaq_lane_f32(_sum1, _weight_xc_R_1, vget_low_f32(_xi), 1);
                _sum2 = vmlaq_lane_f32(_sum2, _weight_xc_U_1, vget_low_f32(_xi), 1);
                _sum3 = vmlaq_lane_f32(_sum3, _weight_xc_R_2, vget_high_f32(_xi), 0);
                _sum4 = vmlaq_lane_f32(_sum4, _weight_xc_U_2, vget_high_f32(_xi), 0);
                _sum5 = vmlaq_lane_f32(_sum5, _weight_xc_R_3, vget_high_f32(_xi), 1);
                _sum6 = vmlaq_lane_f32(_sum6, _weight_xc_U_3, vget_high_f32(_xi), 1);
#endif

                weight_xc_RUN += 32;
            }
            for (; i < size; i++)
            {
                float xi = x[i];

                float32x4_t _xi = vdupq_n_f32(xi);
                float32x4_t _weight_xc_R = vld1q_f32(weight_xc_RUN);
                float32x4_t _weight_xc_U = vld1q_f32(weight_xc_RUN + 4);
                _R = vmlaq_f32(_R, _weight_xc_R, _xi);
                _U = vmlaq_f32(_U, _weight_xc_U, _xi);

                weight_xc_RUN += 8;
            }

            i = 0;
            for (; i + 3 < num_output; i += 4)
            {
                float32x4_t _h_cont = vld1q_f32((const float*)hidden_state + i);
                float32x4_t _weight_hc_R = vld1q_f32(weight_hc_RUN);
                float32x4_t _weight_hc_U = vld1q_f32(weight_hc_RUN + 4);
                float32x4_t _weight_hc_R_1 = vld1q_f32(weight_hc_RUN + 8);
                float32x4_t _weight_hc_U_1 = vld1q_f32(weight_hc_RUN + 12);
                float32x4_t _weight_hc_R_2 = vld1q_f32(weight_hc_RUN + 16);
                float32x4_t _weight_hc_U_2 = vld1q_f32(weight_hc_RUN + 20);
                float32x4_t _weight_hc_R_3 = vld1q_f32(weight_hc_RUN + 24);
                float32x4_t _weight_hc_U_3 = vld1q_f32(weight_hc_RUN + 28);
#if __aarch64__
                _R = vfmaq_laneq_f32(_R, _weight_hc_R, _h_cont, 0);
                _U = vfmaq_laneq_f32(_U, _weight_hc_U, _h_cont, 0);
                _sum1 = vfmaq_laneq_f32(_sum1, _weight_hc_R_1, _h_cont, 1);
                _sum2 = vfmaq_laneq_f32(_sum2, _weight_hc_U_1, _h_cont, 1);
                _sum3 = vfmaq_laneq_f32(_sum3, _weight_hc_R_2, _h_cont, 2);
                _sum4 = vfmaq_laneq_f32(_sum4, _weight_hc_U_2, _h_cont, 2);
                _sum5 = vfmaq_laneq_f32(_sum5, _weight_hc_R_3, _h_cont, 3);
                _sum6 = vfmaq_laneq_f32(_sum6, _weight_hc_U_3, _h_cont, 3);
#else
                _R = vmlaq_lane_f32(_R, _weight_hc_R, vget_low_f32(_h_cont), 0);
                _U = vmlaq_lane_f32(_U, _weight_hc_U, vget_low_f32(_h_cont), 0);
                _sum1 = vmlaq_lane_f32(_sum1, _weight_hc_R_1, vget_low_f32(_h_cont), 1);
                _sum2 = vmlaq_lane_f32(_sum2, _weight_hc_U_1, vget_low_f32(_h_cont), 1);
                _sum3 = vmlaq_lane_f32(_sum3, _weight_hc_R_2, vget_high_f32(_h_cont), 0);
                _sum4 = vmlaq_lane_f32(_sum4, _weight_hc_U_2, vget_high_f32(_h_cont), 0);
                _sum5 = vmlaq_lane_f32(_sum5, _weight_hc_R_3, vget_high_f32(_h_cont), 1);
                _sum6 = vmlaq_lane_f32(_sum6, _weight_hc_U_3, vget_high_f32(_h_cont), 1);
#endif

                weight_hc_RUN += 32;
            }
            for (; i < num_output; i++)
            {
                float h_cont = hidden_state[i];

                float32x4_t _h_cont = vdupq_n_f32(h_cont);
                float32x4_t _weight_hc_R = vld1q_f32(weight_hc_RUN);
                float32x4_t _weight_hc_U = vld1q_f32(weight_hc_RUN + 4);
                _R = vmlaq_f32(_R, _weight_hc_R, _h_cont);
                _U = vmlaq_f32(_U, _weight_hc_U, _h_cont);

                weight_hc_RUN += 8;
            }

            _R = vaddq_f32(_R, _sum1);
            _U = vaddq_f32(_U, _sum2);
            _sum3 = vaddq_f32(_sum3, _sum5);
            _sum4 = vaddq_f32(_sum4, _sum6);
            _R = vaddq_f32(_R, _sum3);
            _U = vaddq_f32(_U, _sum4);

            // sigmoid(R)
            // sigmoid(U)
            _R = sigmoid_ps(_R);
            _U = sigmoid_ps(_U);

            // gate new
            float32x4_t _N = vld1q_f32(bias_c_RUBNWN + 8);
            _sum1 = vdupq_n_f32(0.f);
            _sum2 = vdupq_n_f32(0.f);
            _sum3 = vdupq_n_f32(0.f);

            i = 0;
            for (; i + 3 < num_output; i += 4)
            {
                float32x4_t _h_cont = vld1q_f32((const float*)hidden_state + i);
                float32x4_t _weight_hc_N = vld1q_f32(weight_hc_RUN);
                float32x4_t _weight_hc_N_1 = vld1q_f32(weight_hc_RUN + 4);
                float32x4_t _weight_hc_N_2 = vld1q_f32(weight_hc_RUN + 8);
                float32x4_t _weight_hc_N_3 = vld1q_f32(weight_hc_RUN + 12);
#if __aarch64__
                _N = vfmaq_laneq_f32(_N, _weight_hc_N, _h_cont, 0);
                _sum1 = vfmaq_laneq_f32(_sum1, _weight_hc_N_1, _h_cont, 1);
                _sum2 = vfmaq_laneq_f32(_sum2, _weight_hc_N_2, _h_cont, 2);
                _sum3 = vfmaq_laneq_f32(_sum3, _weight_hc_N_3, _h_cont, 3);
#else
                _N = vmlaq_lane_f32(_N, _weight_hc_N, vget_low_f32(_h_cont), 0);
                _sum1 = vmlaq_lane_f32(_sum1, _weight_hc_N_1, vget_low_f32(_h_cont), 1);
                _sum2 = vmlaq_lane_f32(_sum2, _weight_hc_N_2, vget_high_f32(_h_cont), 0);
                _sum3 = vmlaq_lane_f32(_sum3, _weight_hc_N_3, vget_high_f32(_h_cont), 1);
#endif

                weight_hc_RUN += 16;
            }
            for (; i < num_output; i++)
            {
                float h_cont = hidden_state[i];

                float32x4_t _h_cont = vdupq_n_f32(h_cont);
                float32x4_t _weight_hc_N = vld1q_f32(weight_hc_RUN);
                _N = vmlaq_f32(_N, _weight_hc_N, _h_cont);

                weight_hc_RUN += 4;
            }

            _N = vaddq_f32(_N, _sum1);
            _sum2 = vaddq_f32(_sum2, _sum3);
            _N = vaddq_f32(_N, _sum2);

            _N = vmlaq_f32(vld1q_f32(bias_c_RUBNWN + 12), _R, _N);
            _sum1 = vdupq_n_f32(0.f);
            _sum2 = vdupq_n_f32(0.f);
            _sum3 = vdupq_n_f32(0.f);

            i = 0;
            for (; i + 3 < size; i += 4)
            {
                float32x4_t _xi = vld1q_f32(x + i);
                float32x4_t _weight_xc_N = vld1q_f32(weight_xc_RUN);
                float32x4_t _weight_xc_N_1 = vld1q_f32(weight_xc_RUN + 4);
                float32x4_t _weight_xc_N_2 = vld1q_f32(weight_xc_RUN + 8);
                float32x4_t _weight_xc_N_3 = vld1q_f32(weight_xc_RUN + 12);
#if __aarch64__
                _N = vfmaq_laneq_f32(_N, _weight_xc_N, _xi, 0);
                _sum1 = vfmaq_laneq_f32(_sum1, _weight_xc_N_1, _xi, 1);
                _sum2 = vfmaq_laneq_f32(_sum2, _weight_xc_N_2, _xi, 2);
                _sum3 = vfmaq_laneq_f32(_sum3, _weight_xc_N_3, _xi, 3);
#else
                _N = vmlaq_lane_f32(_N, _weight_xc_N, vget_low_f32(_xi), 0);
                _sum1 = vmlaq_lane_f32(_sum1, _weight_xc_N_1, vget_low_f32(_xi), 1);
                _sum2 = vmlaq_lane_f32(_sum2, _weight_xc_N_2, vget_high_f32(_xi), 0);
                _sum3 = vmlaq_lane_f32(_sum3, _weight_xc_N_3, vget_high_f32(_xi), 1);
#endif

                weight_xc_RUN += 16;
            }
            for (; i < size; i++)
            {
                float xi = x[i];

                float32x4_t _xi = vdupq_n_f32(xi);
                float32x4_t _weight_xc_N = vld1q_f32(weight_xc_RUN);
                _N = vmlaq_f32(_N, _weight_xc_N, _xi);

                weight_xc_RUN += 4;
            }

            _N = vaddq_f32(_N, _sum1);
            _sum2 = vaddq_f32(_sum2, _sum3);
            _N = vaddq_f32(_N, _sum2);

            // tanh(N)
            _N = tanh_ps(_N);

            float* gates_data = gates.row(q / 4);

            vst1q_f32(gates_data, _U);
            vst1q_f32(gates_data + 4, _N);
        }
#endif // __ARM_NEON
        #pragma omp parallel for num_threads(opt.num_threads)
        for (int q = remain_num_output_start; q < num_output; q++)
        {
            const float* x = bottom_blob.row(ti);

            // gate reset update
            const float* bias_c_RUBNWN = (const float*)bias_c + q * 4;

#if __ARM_NEON
            const float* weight_xc_RUN = weight_xc.row(q / 4 + q % 4);
            const float* weight_hc_RUN = weight_hc.row(q / 4 + q % 4);
#else
            const float* weight_xc_RUN = weight_xc.row(q);
            const float* weight_hc_RUN = weight_hc.row(q);
#endif

            float R = bias_c_RUBNWN[0];
            float U = bias_c_RUBNWN[1];

            for (int i = 0; i < size; i++)
            {
                float xi = x[i];

                R += weight_xc_RUN[0] * xi;
                U += weight_xc_RUN[1] * xi;

                weight_xc_RUN += 2;
            }

            for (int i = 0; i < num_output; i++)
            {
                float h_cont = hidden_state[i];

                R += weight_hc_RUN[0] * h_cont;
                U += weight_hc_RUN[1] * h_cont;

                weight_hc_RUN += 2;
            }

            // sigmoid(R)
            // sigmoid(U)
            R = 1.f / (1.f + exp(-R));
            U = 1.f / (1.f + exp(-U));

            // gate new
            float N = bias_c_RUBNWN[2];

            for (int i = 0; i < num_output; i++)
            {
                float h_cont = hidden_state[i];

                N += weight_hc_RUN[0] * h_cont;

                weight_hc_RUN += 1;
            }

            N = bias_c_RUBNWN[3] + R * N;

            for (int i = 0; i < size; i++)
            {
                float xi = x[i];

                N += weight_xc_RUN[0] * xi;

                weight_xc_RUN += 1;
            }

            // tanh(N)
            N = tanh(N);

#if __ARM_NEON
            float* gates_data = gates.row(q / 4 + q % 4);
#else
            float* gates_data = gates.row(q);
#endif

            gates_data[0] = U;
            gates_data[1] = N;
        }

        // h_t := (1 - update) .* new + update .* h_{t-1}
        float* output_data = top_blob.row(ti);

        float* hidden_ptr = hidden_state;

#if __ARM_NEON
        nn_num_output = num_output >> 2;
        remain_num_output_start = nn_num_output << 2;

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int qq = 0; qq < nn_num_output; qq++)
        {
            int q = qq * 4;

            const float* gates_data = gates.row(q / 4);

            float32x4_t _U = vld1q_f32(gates_data);
            float32x4_t _N = vld1q_f32(gates_data + 4);

            float32x4_t _H = vaddq_f32(vmulq_f32(vsubq_f32(vdupq_n_f32(1.f), _U), _N), vmulq_f32(_U, vld1q_f32(hidden_ptr + q)));

            vst1q_f32(hidden_ptr + q, _H);
            vst1q_f32(output_data + q, _H);
        }
#endif // __ARM_NEON
        #pragma omp parallel for num_threads(opt.num_threads)
        for (int q = remain_num_output_start; q < num_output; q++)
        {
#if __ARM_NEON
            const float* gates_data = gates.row(q / 4 + q % 4);
#else
            const float* gates_data = gates.row(q);
#endif

            float U = gates_data[0];
            float N = gates_data[1];

            float H = (1 - U) * N + U * hidden_ptr[q];

            hidden_ptr[q] = H;
            output_data[q] = H;
        }
    }

    return 0;
}

int GRU_arm::forward(const Mat& bottom_blob, Mat& top_blob, const Option& opt) const
{
    int elembits = bottom_blob.elembits();

#if NCNN_ARM82
    if (support_fp16_storage && opt.use_fp16_storage && elembits == 16)
    {
        if (opt.use_fp16_arithmetic)
            return forward_fp16sa(bottom_blob, top_blob, opt);
        else
            return forward_fp16s(bottom_blob, top_blob, opt);
    }
#endif

#if NCNN_BF16
    if (opt.use_bf16_storage && elembits == 16)
        return forward_bf16s(bottom_blob, top_blob, opt);
#endif

    int T = bottom_blob.h;

    int num_directions = direction == 2 ? 2 : 1;

    // initial hidden state
    Mat hidden(num_output, 4u, opt.workspace_allocator);
    if (hidden.empty())
        return -100;
    hidden.fill(0.f);

    top_blob.create(num_output * num_directions, T, 4u, opt.blob_allocator);
    if (top_blob.empty())
        return -100;

    // Uni directional
    if (direction == 0 || direction == 1)
    {
        int ret = gru(bottom_blob, top_blob, direction, weight_xc_data_packed.channel(0), bias_c_data_packed.channel(0), weight_hc_data_packed.channel(0), hidden, opt);
        if (ret != 0)
            return ret;
    }

    if (direction == 2)
    {
        Mat top_blob_forward(num_output, T, 4u, opt.workspace_allocator);
        if (top_blob_forward.empty())
            return -100;

        Mat top_blob_reverse(num_output, T, 4u, opt.workspace_allocator);
        if (top_blob_reverse.empty())
            return -100;

        int ret0 = gru(bottom_blob, top_blob_forward, 0, weight_xc_data_packed.channel(0), bias_c_data_packed.channel(0), weight_hc_data_packed.channel(0), hidden, opt);
        if (ret0 != 0)
            return ret0;

        hidden.fill(0.0f);

        int ret1 = gru(bottom_blob, top_blob_reverse, 1, weight_xc_data_packed.channel(1), bias_c_data_packed.channel(1), weight_hc_data_packed.channel(1), hidden, opt);
        if (ret1 != 0)
            return ret1;

        // concat w
        for (int i = 0; i < T; i++)
        {
            const float* pf = top_blob_forward.row(i);
            const float* pr = top_blob_reverse.row(i);
            float* ptr = top_blob.row(i);

            memcpy(ptr, pf, num_output * sizeof(float));
            memcpy(ptr + num_output, pr, num_output * sizeof(float));
        }
    }

    return 0;
}

int GRU_arm::forward(const std::vector<Mat>& bottom_blobs, std::vector<Mat>& top_blobs, const Option& opt) const
{
    const Mat& bottom_blob = bottom_blobs[0];
    int elembits = bottom_blob.elembits();

#if NCNN_ARM82
    if (support_fp16_storage && opt.use_fp16_storage && elembits == 16)
    {
        if (opt.use_fp16_arithmetic)
            return forward_fp16sa(bottom_blobs, top_blobs, opt);
        else
            return forward_fp16s(bottom_blobs, top_blobs, opt);
    }
#endif

#if NCNN_BF16
    if (opt.use_bf16_storage && elembits == 16)
        return forward_bf16s(bottom_blobs, top_blobs, opt);
#endif

    int T = bottom_blob.h;
    int num_directions = direction == 2 ? 2 : 1;

    Mat hidden;
    Allocator* hidden_allocator = top_blobs.size() == 2 ? opt.blob_allocator : opt.workspace_allocator;
    if (bottom_blobs.size() == 2)
    {
        hidden = bottom_blobs[1].clone(hidden_allocator);
    }
    else
    {
        hidden.create(num_output, num_directions, 4u, hidden_allocator);
        if (hidden.empty())
            return -100;
        hidden.fill(0.f);
    }

    Mat& top_blob = top_blobs[0];
    top_blob.create(num_output * num_directions, T, 4u, opt.blob_allocator);
    if (top_blob.empty())
        return -100;

    // Uni directional
    if (direction == 0 || direction == 1)
    {
        int ret = gru(bottom_blob, top_blob, direction, weight_xc_data_packed.channel(0), bias_c_data_packed.channel(0), weight_hc_data_packed.channel(0), hidden, opt);
        if (ret != 0)
            return ret;
    }

    if (direction == 2)
    {
        Mat top_blob_forward(num_output, T, 4u, opt.workspace_allocator);
        if (top_blob_forward.empty())
            return -100;

        Mat top_blob_reverse(num_output, T, 4u, opt.workspace_allocator);
        if (top_blob_reverse.empty())
            return -100;

        Mat hidden0 = hidden.row_range(0, 1);
        int ret0 = gru(bottom_blob, top_blob_forward, 0, weight_xc_data_packed.channel(0), bias_c_data_packed.channel(0), weight_hc_data_packed.channel(0), hidden0, opt);
        if (ret0 != 0)
            return ret0;

        Mat hidden1 = hidden.row_range(1, 1);
        int ret1 = gru(bottom_blob, top_blob_reverse, 1, weight_xc_data_packed.channel(1), bias_c_data_packed.channel(1), weight_hc_data_packed.channel(1), hidden1, opt);
        if (ret1 != 0)
            return ret1;

        // concat w
        for (int i = 0; i < T; i++)
        {
            const float* pf = top_blob_forward.row(i);
            const float* pr = top_blob_reverse.row(i);
            float* ptr = top_blob.row(i);

            memcpy(ptr, pf, num_output * sizeof(float));
            memcpy(ptr + num_output, pr, num_output * sizeof(float));
        }
    }

    if (top_blobs.size() == 2)
    {
        top_blobs[1] = hidden;
    }

    return 0;
}

#if NCNN_BF16
static int gru_bf16s(const Mat& bottom_blob, Mat& top_blob, int reverse, const Mat& weight_xc, const Mat& bias_c, const Mat& weight_hc, Mat& hidden_state, const Option& opt)
{
    int size = bottom_blob.w;
    int T = bottom_blob.h;

    int num_output = top_blob.w;

    // 2 x num_output
#if __ARM_NEON
    Mat gates(4 * 2, num_output / 4 + num_output % 4, 4u, opt.workspace_allocator);
#else
    Mat gates(2, num_output, 4u, opt.workspace_allocator);
#endif
    if (gates.empty())
        return -100;

    // unroll
    for (int t = 0; t < T; t++)
    {
        int ti = reverse ? T - 1 - t : t;

        int remain_num_output_start = 0;
#if __ARM_NEON
        int nn_num_output = num_output >> 2;
        remain_num_output_start = nn_num_output << 2;

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int qq = 0; qq < nn_num_output; qq++)
        {
            int q = qq * 4;

            const unsigned short* x = bottom_blob.row<const unsigned short>(ti);

            // gate reset update
            const unsigned short* bias_c_RUBNWN = (const unsigned short*)bias_c + q * 4;

            const unsigned short* weight_xc_RUN = weight_xc.row<const unsigned short>(q / 4);
            const unsigned short* weight_hc_RUN = weight_hc.row<const unsigned short>(q / 4);

            float32x4_t _R = vcvt_f32_bf16(vld1_u16(bias_c_RUBNWN));
            float32x4_t _U = vcvt_f32_bf16(vld1_u16(bias_c_RUBNWN + 4));
            float32x4_t _sum1 = vdupq_n_f32(0.f);
            float32x4_t _sum2 = vdupq_n_f32(0.f);
            float32x4_t _sum3 = vdupq_n_f32(0.f);
            float32x4_t _sum4 = vdupq_n_f32(0.f);
            float32x4_t _sum5 = vdupq_n_f32(0.f);
            float32x4_t _sum6 = vdupq_n_f32(0.f);

            int i = 0;
            for (; i + 3 < size; i += 4)
            {
                float32x4_t _xi = vcvt_f32_bf16(vld1_u16(x + i));
                float32x4_t _weight_xc_R = vcvt_f32_bf16(vld1_u16(weight_xc_RUN));
                float32x4_t _weight_xc_U = vcvt_f32_bf16(vld1_u16(weight_xc_RUN + 4));
                float32x4_t _weight_xc_R_1 = vcvt_f32_bf16(vld1_u16(weight_xc_RUN + 8));
                float32x4_t _weight_xc_U_1 = vcvt_f32_bf16(vld1_u16(weight_xc_RUN + 12));
                float32x4_t _weight_xc_R_2 = vcvt_f32_bf16(vld1_u16(weight_xc_RUN + 16));
                float32x4_t _weight_xc_U_2 = vcvt_f32_bf16(vld1_u16(weight_xc_RUN + 20));
                float32x4_t _weight_xc_R_3 = vcvt_f32_bf16(vld1_u16(weight_xc_RUN + 24));
                float32x4_t _weight_xc_U_3 = vcvt_f32_bf16(vld1_u16(weight_xc_RUN + 28));
#if __aarch64__
                _R = vfmaq_laneq_f32(_R, _weight_xc_R, _xi, 0);
                _U = vfmaq_laneq_f32(_U, _weight_xc_U, _xi, 0);
                _sum1 = vfmaq_laneq_f32(_sum1, _weight_xc_R_1, _xi, 1);
                _sum2 = vfmaq_laneq_f32(_sum2, _weight_xc_U_1, _xi, 1);
                _sum3 = vfmaq_laneq_f32(_sum3, _weight_xc_R_2, _xi, 2);
                _sum4 = vfmaq_laneq_f32(_sum4, _weight_xc_U_2, _xi, 2);
                _sum5 = vfmaq_laneq_f32(_sum5, _weight_xc_R_3, _xi, 3);
                _sum6 = vfmaq_laneq_f32(_sum6, _weight_xc_U_3, _xi, 3);
#else
                _R = vmlaq_lane_f32(_R, _weight_xc_R, vget_low_f32(_xi), 0);
                _U = vmlaq_lane_f32(_U, _weight_xc_U, vget_low_f32(_xi), 0);
                _sum1 = vmlaq_lane_f32(_sum1, _weight_xc_R_1, vget_low_f32(_xi), 1);
                _sum2 = vmlaq_lane_f32(_sum2, _weight_xc_U_1, vget_low_f32(_xi), 1);
                _sum3 = vmlaq_lane_f32(_sum3, _weight_xc_R_2, vget_high_f32(_xi), 0);
                _sum4 = vmlaq_lane_f32(_sum4, _weight_xc_U_2, vget_high_f32(_xi), 0);
                _sum5 = vmlaq_lane_f32(_sum5, _weight_xc_R_3, vget_high_f32(_xi), 1);
                _sum6 = vmlaq_lane_f32(_sum6, _weight_xc_U_3, vget_high_f32(_xi), 1);
#endif

                weight_xc_RUN += 32;
            }
            for (; i < size; i++)
            {
                unsigned short xi = x[i];

                float32x4_t _xi = vcvt_f32_bf16(vdup_n_u16(xi));
                float32x4_t _weight_xc_R = vcvt_f32_bf16(vld1_u16(weight_xc_RUN));
                float32x4_t _weight_xc_U = vcvt_f32_bf16(vld1_u16(weight_xc_RUN + 4));
                _R = vmlaq_f32(_R, _weight_xc_R, _xi);
                _U = vmlaq_f32(_U, _weight_xc_U, _xi);

                weight_xc_RUN += 8;
            }

            i = 0;
            for (; i + 3 < num_output; i += 4)
            {
                float32x4_t _h_cont = vld1q_f32((const float*)hidden_state + i);
                float32x4_t _weight_hc_R = vcvt_f32_bf16(vld1_u16(weight_hc_RUN));
                float32x4_t _weight_hc_U = vcvt_f32_bf16(vld1_u16(weight_hc_RUN + 4));
                float32x4_t _weight_hc_R_1 = vcvt_f32_bf16(vld1_u16(weight_hc_RUN + 8));
                float32x4_t _weight_hc_U_1 = vcvt_f32_bf16(vld1_u16(weight_hc_RUN + 12));
                float32x4_t _weight_hc_R_2 = vcvt_f32_bf16(vld1_u16(weight_hc_RUN + 16));
                float32x4_t _weight_hc_U_2 = vcvt_f32_bf16(vld1_u16(weight_hc_RUN + 20));
                float32x4_t _weight_hc_R_3 = vcvt_f32_bf16(vld1_u16(weight_hc_RUN + 24));
                float32x4_t _weight_hc_U_3 = vcvt_f32_bf16(vld1_u16(weight_hc_RUN + 28));
#if __aarch64__
                _R = vfmaq_laneq_f32(_R, _weight_hc_R, _h_cont, 0);
                _U = vfmaq_laneq_f32(_U, _weight_hc_U, _h_cont, 0);
                _sum1 = vfmaq_laneq_f32(_sum1, _weight_hc_R_1, _h_cont, 1);
                _sum2 = vfmaq_laneq_f32(_sum2, _weight_hc_U_1, _h_cont, 1);
                _sum3 = vfmaq_laneq_f32(_sum3, _weight_hc_R_2, _h_cont, 2);
                _sum4 = vfmaq_laneq_f32(_sum4, _weight_hc_U_2, _h_cont, 2);
                _sum5 = vfmaq_laneq_f32(_sum5, _weight_hc_R_3, _h_cont, 3);
                _sum6 = vfmaq_laneq_f32(_sum6, _weight_hc_U_3, _h_cont, 3);
#else
                _R = vmlaq_lane_f32(_R, _weight_hc_R, vget_low_f32(_h_cont), 0);
                _U = vmlaq_lane_f32(_U, _weight_hc_U, vget_low_f32(_h_cont), 0);
                _sum1 = vmlaq_lane_f32(_sum1, _weight_hc_R_1, vget_low_f32(_h_cont), 1);
                _sum2 = vmlaq_lane_f32(_sum2, _weight_hc_U_1, vget_low_f32(_h_cont), 1);
                _sum3 = vmlaq_lane_f32(_sum3, _weight_hc_R_2, vget_high_f32(_h_cont), 0);
                _sum4 = vmlaq_lane_f32(_sum4, _weight_hc_U_2, vget_high_f32(_h_cont), 0);
                _sum5 = vmlaq_lane_f32(_sum5, _weight_hc_R_3, vget_high_f32(_h_cont), 1);
                _sum6 = vmlaq_lane_f32(_sum6, _weight_hc_U_3, vget_high_f32(_h_cont), 1);
#endif

                weight_hc_RUN += 32;
            }
            for (; i < num_output; i++)
            {
                float h_cont = hidden_state[i];

                float32x4_t _h_cont = vdupq_n_f32(h_cont);
                float32x4_t _weight_hc_R = vcvt_f32_bf16(vld1_u16(weight_hc_RUN));
                float32x4_t _weight_hc_U = vcvt_f32_bf16(vld1_u16(weight_hc_RUN + 4));
                _R = vmlaq_f32(_R, _weight_hc_R, _h_cont);
                _U = vmlaq_f32(_U, _weight_hc_U, _h_cont);

                weight_hc_RUN += 8;
            }

            _R = vaddq_f32(_R, _sum1);
            _U = vaddq_f32(_U, _sum2);
            _sum3 = vaddq_f32(_sum3, _sum5);
            _sum4 = vaddq_f32(_sum4, _sum6);
            _R = vaddq_f32(_R, _sum3);
            _U = vaddq_f32(_U, _sum4);

            // sigmoid(R)
            // sigmoid(U)
            _R = sigmoid_ps(_R);
            _U = sigmoid_ps(_U);

            // gate new
            float32x4_t _N = vcvt_f32_bf16(vld1_u16(bias_c_RUBNWN + 8));
            _sum1 = vdupq_n_f32(0.f);
            _sum2 = vdupq_n_f32(0.f);
            _sum3 = vdupq_n_f32(0.f);

            i = 0;
            for (; i + 3 < num_output; i += 4)
            {
                float32x4_t _h_cont = vld1q_f32((const float*)hidden_state + i);
                float32x4_t _weight_hc_N = vcvt_f32_bf16(vld1_u16(weight_hc_RUN));
                float32x4_t _weight_hc_N_1 = vcvt_f32_bf16(vld1_u16(weight_hc_RUN + 4));
                float32x4_t _weight_hc_N_2 = vcvt_f32_bf16(vld1_u16(weight_hc_RUN + 8));
                float32x4_t _weight_hc_N_3 = vcvt_f32_bf16(vld1_u16(weight_hc_RUN + 12));
#if __aarch64__
                _N = vfmaq_laneq_f32(_N, _weight_hc_N, _h_cont, 0);
                _sum1 = vfmaq_laneq_f32(_sum1, _weight_hc_N_1, _h_cont, 1);
                _sum2 = vfmaq_laneq_f32(_sum2, _weight_hc_N_2, _h_cont, 2);
                _sum3 = vfmaq_laneq_f32(_sum3, _weight_hc_N_3, _h_cont, 3);
#else
                _N = vmlaq_lane_f32(_N, _weight_hc_N, vget_low_f32(_h_cont), 0);
                _sum1 = vmlaq_lane_f32(_sum1, _weight_hc_N_1, vget_low_f32(_h_cont), 1);
                _sum2 = vmlaq_lane_f32(_sum2, _weight_hc_N_2, vget_high_f32(_h_cont), 0);
                _sum3 = vmlaq_lane_f32(_sum3, _weight_hc_N_3, vget_high_f32(_h_cont), 1);
#endif

                weight_hc_RUN += 16;
            }
            for (; i < num_output; i++)
            {
                float h_cont = hidden_state[i];

                float32x4_t _h_cont = vdupq_n_f32(h_cont);
                float32x4_t _weight_hc_N = vcvt_f32_bf16(vld1_u16(weight_hc_RUN));
                _N = vmlaq_f32(_N, _weight_hc_N, _h_cont);

                weight_hc_RUN += 4;
            }

            _N = vaddq_f32(_N, _sum1);
            _sum2 = vaddq_f32(_sum2, _sum3);
            _N = vaddq_f32(_N, _sum2);

            _N = vmlaq_f32(vcvt_f32_bf16(vld1_u16(bias_c_RUBNWN + 12)), _R, _N);
            _sum1 = vdupq_n_f32(0.f);
            _sum2 = vdupq_n_f32(0.f);
            _sum3 = vdupq_n_f32(0.f);

            i = 0;
            for (; i + 3 < size; i += 4)
            {
                float32x4_t _xi = vcvt_f32_bf16(vld1_u16(x + i));
                float32x4_t _weight_xc_N = vcvt_f32_bf16(vld1_u16(weight_xc_RUN));
                float32x4_t _weight_xc_N_1 = vcvt_f32_bf16(vld1_u16(weight_xc_RUN + 4));
                float32x4_t _weight_xc_N_2 = vcvt_f32_bf16(vld1_u16(weight_xc_RUN + 8));
                float32x4_t _weight_xc_N_3 = vcvt_f32_bf16(vld1_u16(weight_xc_RUN + 12));
#if __aarch64__
                _N = vfmaq_laneq_f32(_N, _weight_xc_N, _xi, 0);
                _sum1 = vfmaq_laneq_f32(_sum1, _weight_xc_N_1, _xi, 1);
                _sum2 = vfmaq_laneq_f32(_sum2, _weight_xc_N_2, _xi, 2);
                _sum3 = vfmaq_laneq_f32(_sum3, _weight_xc_N_3, _xi, 3);
#else
                _N = vmlaq_lane_f32(_N, _weight_xc_N, vget_low_f32(_xi), 0);
                _sum1 = vmlaq_lane_f32(_sum1, _weight_xc_N_1, vget_low_f32(_xi), 1);
                _sum2 = vmlaq_lane_f32(_sum2, _weight_xc_N_2, vget_high_f32(_xi), 0);
                _sum3 = vmlaq_lane_f32(_sum3, _weight_xc_N_3, vget_high_f32(_xi), 1);
#endif

                weight_xc_RUN += 16;
            }
            for (; i < size; i++)
            {
                unsigned short xi = x[i];

                float32x4_t _xi = vcvt_f32_bf16(vdup_n_u16(xi));
                float32x4_t _weight_xc_N = vcvt_f32_bf16(vld1_u16(weight_xc_RUN));
                _N = vmlaq_f32(_N, _weight_xc_N, _xi);

                weight_xc_RUN += 4;
            }

            _N = vaddq_f32(_N, _sum1);
            _sum2 = vaddq_f32(_sum2, _sum3);
            _N = vaddq_f32(_N, _sum2);

            // tanh(N)
            _N = tanh_ps(_N);

            float* gates_data = gates.row(q / 4);

            vst1q_f32(gates_data, _U);
            vst1q_f32(gates_data + 4, _N);
        }
#endif // __ARM_NEON
        #pragma omp parallel for num_threads(opt.num_threads)
        for (int q = remain_num_output_start; q < num_output; q++)
        {
            const unsigned short* x = bottom_blob.row<const unsigned short>(ti);

            // gate reset update
            const unsigned short* bias_c_RUBNWN = (const unsigned short*)bias_c + q * 4;

#if __ARM_NEON
            const unsigned short* weight_xc_RUN = weight_xc.row<const unsigned short>(q / 4 + q % 4);
            const unsigned short* weight_hc_RUN = weight_hc.row<const unsigned short>(q / 4 + q % 4);
#else
            const unsigned short* weight_xc_RUN = weight_xc.row<const unsigned short>(q);
            const unsigned short* weight_hc_RUN = weight_hc.row<const unsigned short>(q);
#endif

            float R = bfloat16_to_float32(bias_c_RUBNWN[0]);
            float U = bfloat16_to_float32(bias_c_RUBNWN[1]);

            for (int i = 0; i < size; i++)
            {
                float xi = bfloat16_to_float32(x[i]);

                R += bfloat16_to_float32(weight_xc_RUN[0]) * xi;
                U += bfloat16_to_float32(weight_xc_RUN[1]) * xi;

                weight_xc_RUN += 2;
            }

            for (int i = 0; i < num_output; i++)
            {
                float h_cont = hidden_state[i];

                R += bfloat16_to_float32(weight_hc_RUN[0]) * h_cont;
                U += bfloat16_to_float32(weight_hc_RUN[1]) * h_cont;

                weight_hc_RUN += 2;
            }

            // sigmoid(R)
            // sigmoid(U)
            R = 1.f / (1.f + exp(-R));
            U = 1.f / (1.f + exp(-U));

            // gate new
            float N = bfloat16_to_float32(bias_c_RUBNWN[2]);

            for (int i = 0; i < num_output; i++)
            {
                float h_cont = hidden_state[i];

                N += bfloat16_to_float32(weight_hc_RUN[0]) * h_cont;

                weight_hc_RUN += 1;
            }

            N = bfloat16_to_float32(bias_c_RUBNWN[3]) + R * N;

            for (int i = 0; i < size; i++)
            {
                float xi = bfloat16_to_float32(x[i]);

                N += bfloat16_to_float32(weight_xc_RUN[0]) * xi;

                weight_xc_RUN += 1;
            }

            // tanh(N)
            N = tanh(N);

#if __ARM_NEON
            float* gates_data = gates.row(q / 4 + q % 4);
#else
            float* gates_data = gates.row(q);
#endif

            gates_data[0] = U;
            gates_data[1] = N;
        }

        // h_t := (1 - update) .* new + update .* h_{t-1}
        unsigned short* output_data = top_blob.row<unsigned short>(ti);

        float* hidden_ptr = hidden_state;

#if __ARM_NEON
        nn_num_output = num_output >> 2;
        remain_num_output_start = nn_num_output << 2;

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int qq = 0; qq < nn_num_output; qq++)
        {
            int q = qq * 4;

            const float* gates_data = gates.row(q / 4);

            float32x4_t _U = vld1q_f32(gates_data);
            float32x4_t _N = vld1q_f32(gates_data + 4);

            float32x4_t _H = vaddq_f32(vmulq_f32(vsubq_f32(vdupq_n_f32(1.f), _U), _N), vmulq_f32(_U, vld1q_f32(hidden_ptr + q)));

            vst1q_f32(hidden_ptr + q, _H);
            vst1_u16(output_data + q, vcvt_bf16_f32(_H));
        }
#endif // __ARM_NEON
        #pragma omp parallel for num_threads(opt.num_threads)
        for (int q = remain_num_output_start; q < num_output; q++)
        {
#if __ARM_NEON
            const float* gates_data = gates.row(q / 4 + q % 4);
#else
            const float* gates_data = gates.row(q);
#endif

            float U = gates_data[0];
            float N = gates_data[1];

            float H = (1 - U) * N + U * hidden_ptr[q];

            hidden_ptr[q] = H;
            output_data[q] = float32_to_bfloat16(H);
        }
    }

    return 0;
}

int GRU_arm::create_pipeline_bf16s(const Option& opt)
{
    // pack RUN
    int num_directions = direction == 2 ? 2 : 1;
    int size = weight_data_size / num_directions / num_output / 3;

#if __ARM_NEON
    weight_xc_data_packed.create(size * 12, num_output / 4 + num_output % 4, num_directions, 2u, 1);
    bias_c_data_packed.create(num_output, 1, num_directions, 8u, 4);
    weight_hc_data_packed.create(num_output * 12, num_output / 4 + num_output % 4, num_directions, 2u, 1);
#else
    weight_xc_data_packed.create(size * 3, num_output, num_directions, 2u, 1);
    bias_c_data_packed.create(num_output, 1, num_directions, 8u, 4);
    weight_hc_data_packed.create(num_output * 3, num_output, num_directions, 2u, 1);
#endif

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int dr = 0; dr < num_directions; dr++)
    {
        const Mat weight_xc = weight_xc_data.channel(dr);
        const Mat bias_c = bias_c_data.channel(dr);
        const Mat weight_hc = weight_hc_data.channel(dr);

        Mat weight_xc_data_packed_dr = weight_xc_data_packed.channel(dr);
        Mat bias_c_data_packed_dr = bias_c_data_packed.channel(dr);
        Mat weight_hc_data_packed_dr = weight_hc_data_packed.channel(dr);

        const float* bias_c_R = bias_c.row(0);
        const float* bias_c_U = bias_c.row(1);
        const float* bias_c_WN = bias_c.row(2);
        const float* bias_c_BN = bias_c.row(3);

        unsigned short* bias_c_RUBNWN = bias_c_data_packed_dr.row<unsigned short>(0);

        int q = 0;
#if __ARM_NEON
        for (; q + 3 < num_output; q += 4)
        {
            bias_c_RUBNWN[0] = float32_to_bfloat16(bias_c_R[q]);
            bias_c_RUBNWN[1] = float32_to_bfloat16(bias_c_R[q + 1]);
            bias_c_RUBNWN[2] = float32_to_bfloat16(bias_c_R[q + 2]);
            bias_c_RUBNWN[3] = float32_to_bfloat16(bias_c_R[q + 3]);
            bias_c_RUBNWN[4] = float32_to_bfloat16(bias_c_U[q]);
            bias_c_RUBNWN[5] = float32_to_bfloat16(bias_c_U[q + 1]);
            bias_c_RUBNWN[6] = float32_to_bfloat16(bias_c_U[q + 2]);
            bias_c_RUBNWN[7] = float32_to_bfloat16(bias_c_U[q + 3]);
            bias_c_RUBNWN[8] = float32_to_bfloat16(bias_c_BN[q]);
            bias_c_RUBNWN[9] = float32_to_bfloat16(bias_c_BN[q + 1]);
            bias_c_RUBNWN[10] = float32_to_bfloat16(bias_c_BN[q + 2]);
            bias_c_RUBNWN[11] = float32_to_bfloat16(bias_c_BN[q + 3]);
            bias_c_RUBNWN[12] = float32_to_bfloat16(bias_c_WN[q]);
            bias_c_RUBNWN[13] = float32_to_bfloat16(bias_c_WN[q + 1]);
            bias_c_RUBNWN[14] = float32_to_bfloat16(bias_c_WN[q + 2]);
            bias_c_RUBNWN[15] = float32_to_bfloat16(bias_c_WN[q + 3]);

            bias_c_RUBNWN += 16;

            const float* weight_xc_R = weight_xc.row(num_output * 0 + q);
            const float* weight_xc_U = weight_xc.row(num_output * 1 + q);
            const float* weight_xc_N = weight_xc.row(num_output * 2 + q);

            const float* weight_xc_R_1 = weight_xc.row(num_output * 0 + q + 1);
            const float* weight_xc_U_1 = weight_xc.row(num_output * 1 + q + 1);
            const float* weight_xc_N_1 = weight_xc.row(num_output * 2 + q + 1);

            const float* weight_xc_R_2 = weight_xc.row(num_output * 0 + q + 2);
            const float* weight_xc_U_2 = weight_xc.row(num_output * 1 + q + 2);
            const float* weight_xc_N_2 = weight_xc.row(num_output * 2 + q + 2);

            const float* weight_xc_R_3 = weight_xc.row(num_output * 0 + q + 3);
            const float* weight_xc_U_3 = weight_xc.row(num_output * 1 + q + 3);
            const float* weight_xc_N_3 = weight_xc.row(num_output * 2 + q + 3);

            const float* weight_hc_R = weight_hc.row(num_output * 0 + q);
            const float* weight_hc_U = weight_hc.row(num_output * 1 + q);
            const float* weight_hc_N = weight_hc.row(num_output * 2 + q);

            const float* weight_hc_R_1 = weight_hc.row(num_output * 0 + q + 1);
            const float* weight_hc_U_1 = weight_hc.row(num_output * 1 + q + 1);
            const float* weight_hc_N_1 = weight_hc.row(num_output * 2 + q + 1);

            const float* weight_hc_R_2 = weight_hc.row(num_output * 0 + q + 2);
            const float* weight_hc_U_2 = weight_hc.row(num_output * 1 + q + 2);
            const float* weight_hc_N_2 = weight_hc.row(num_output * 2 + q + 2);

            const float* weight_hc_R_3 = weight_hc.row(num_output * 0 + q + 3);
            const float* weight_hc_U_3 = weight_hc.row(num_output * 1 + q + 3);
            const float* weight_hc_N_3 = weight_hc.row(num_output * 2 + q + 3);

            unsigned short* weight_xc_RUN = weight_xc_data_packed_dr.row<unsigned short>(q / 4);
            unsigned short* weight_hc_RUN = weight_hc_data_packed_dr.row<unsigned short>(q / 4);

            for (int i = 0; i < size; i++)
            {
                weight_xc_RUN[0] = float32_to_bfloat16(weight_xc_R[i]);
                weight_xc_RUN[1] = float32_to_bfloat16(weight_xc_R_1[i]);
                weight_xc_RUN[2] = float32_to_bfloat16(weight_xc_R_2[i]);
                weight_xc_RUN[3] = float32_to_bfloat16(weight_xc_R_3[i]);
                weight_xc_RUN[4] = float32_to_bfloat16(weight_xc_U[i]);
                weight_xc_RUN[5] = float32_to_bfloat16(weight_xc_U_1[i]);
                weight_xc_RUN[6] = float32_to_bfloat16(weight_xc_U_2[i]);
                weight_xc_RUN[7] = float32_to_bfloat16(weight_xc_U_3[i]);

                weight_xc_RUN += 8;
            }

            for (int i = 0; i < num_output; i++)
            {
                weight_hc_RUN[0] = float32_to_bfloat16(weight_hc_R[i]);
                weight_hc_RUN[1] = float32_to_bfloat16(weight_hc_R_1[i]);
                weight_hc_RUN[2] = float32_to_bfloat16(weight_hc_R_2[i]);
                weight_hc_RUN[3] = float32_to_bfloat16(weight_hc_R_3[i]);
                weight_hc_RUN[4] = float32_to_bfloat16(weight_hc_U[i]);
                weight_hc_RUN[5] = float32_to_bfloat16(weight_hc_U_1[i]);
                weight_hc_RUN[6] = float32_to_bfloat16(weight_hc_U_2[i]);
                weight_hc_RUN[7] = float32_to_bfloat16(weight_hc_U_3[i]);

                weight_hc_RUN += 8;
            }

            for (int i = 0; i < size; i++)
            {
                weight_xc_RUN[0] = float32_to_bfloat16(weight_xc_N[i]);
                weight_xc_RUN[1] = float32_to_bfloat16(weight_xc_N_1[i]);
                weight_xc_RUN[2] = float32_to_bfloat16(weight_xc_N_2[i]);
                weight_xc_RUN[3] = float32_to_bfloat16(weight_xc_N_3[i]);

                weight_xc_RUN += 4;
            }

            for (int i = 0; i < num_output; i++)
            {
                weight_hc_RUN[0] = float32_to_bfloat16(weight_hc_N[i]);
                weight_hc_RUN[1] = float32_to_bfloat16(weight_hc_N_1[i]);
                weight_hc_RUN[2] = float32_to_bfloat16(weight_hc_N_2[i]);
                weight_hc_RUN[3] = float32_to_bfloat16(weight_hc_N_3[i]);

                weight_hc_RUN += 4;
            }
        }
#endif // __ARM_NEON
        for (; q < num_output; q++)
        {
            bias_c_RUBNWN[0] = float32_to_bfloat16(bias_c_R[q]);
            bias_c_RUBNWN[1] = float32_to_bfloat16(bias_c_U[q]);
            bias_c_RUBNWN[2] = float32_to_bfloat16(bias_c_BN[q]);
            bias_c_RUBNWN[3] = float32_to_bfloat16(bias_c_WN[q]);

            bias_c_RUBNWN += 4;

            const float* weight_xc_R = weight_xc.row(num_output * 0 + q);
            const float* weight_xc_U = weight_xc.row(num_output * 1 + q);
            const float* weight_xc_N = weight_xc.row(num_output * 2 + q);

            const float* weight_hc_R = weight_hc.row(num_output * 0 + q);
            const float* weight_hc_U = weight_hc.row(num_output * 1 + q);
            const float* weight_hc_N = weight_hc.row(num_output * 2 + q);

#if __ARM_NEON
            unsigned short* weight_xc_RUN = weight_xc_data_packed_dr.row<unsigned short>(q / 4 + q % 4);
            unsigned short* weight_hc_RUN = weight_hc_data_packed_dr.row<unsigned short>(q / 4 + q % 4);
#else
            unsigned short* weight_xc_RUN = weight_xc_data_packed_dr.row<unsigned short>(q);
            unsigned short* weight_hc_RUN = weight_hc_data_packed_dr.row<unsigned short>(q);
#endif // __ARM_NEON

            for (int i = 0; i < size; i++)
            {
                weight_xc_RUN[0] = float32_to_bfloat16(weight_xc_R[i]);
                weight_xc_RUN[1] = float32_to_bfloat16(weight_xc_U[i]);

                weight_xc_RUN += 2;
            }

            for (int i = 0; i < num_output; i++)
            {
                weight_hc_RUN[0] = float32_to_bfloat16(weight_hc_R[i]);
                weight_hc_RUN[1] = float32_to_bfloat16(weight_hc_U[i]);

                weight_hc_RUN += 2;
            }

            for (int i = 0; i < size; i++)
            {
                weight_xc_RUN[0] = float32_to_bfloat16(weight_xc_N[i]);

                weight_xc_RUN += 1;
            }

            for (int i = 0; i < num_output; i++)
            {
                weight_hc_RUN[0] = float32_to_bfloat16(weight_hc_N[i]);

                weight_hc_RUN += 1;
            }
        }
    }

    return 0;
}

int GRU_arm::forward_bf16s(const Mat& bottom_blob, Mat& top_blob, const Option& opt) const
{
    int T = bottom_blob.h;

    int num_directions = direction == 2 ? 2 : 1;

    // initial hidden state
    Mat hidden(num_output, 4u, opt.workspace_allocator);
    if (hidden.empty())
        return -100;
    hidden.fill(0.f);

    top_blob.create(num_output * num_directions, T, 2u, opt.blob_allocator);
    if (top_blob.empty())
        return -100;

    // Uni directional
    if (direction == 0 || direction == 1)
    {
        int ret = gru_bf16s(bottom_blob, top_blob, direction, weight_xc_data_packed.channel(0), bias_c_data_packed.channel(0), weight_hc_data_packed.channel(0), hidden, opt);
        if (ret != 0)
            return ret;
    }

    if (direction == 2)
    {
        Mat top_blob_forward(num_output, T, 2u, opt.workspace_allocator);
        if (top_blob_forward.empty())
            return -100;

        Mat top_blob_reverse(num_output, T, 2u, opt.workspace_allocator);
        if (top_blob_reverse.empty())
            return -100;

        int ret0 = gru_bf16s(bottom_blob, top_blob_forward, 0, weight_xc_data_packed.channel(0), bias_c_data_packed.channel(0), weight_hc_data_packed.channel(0), hidden, opt);
        if (ret0 != 0)
            return ret0;

        hidden.fill(0.f);

        int ret1 = gru_bf16s(bottom_blob, top_blob_reverse, 1, weight_xc_data_packed.channel(1), bias_c_data_packed.channel(1), weight_hc_data_packed.channel(1), hidden, opt);
        if (ret1 != 0)
            return ret1;

        // concat w
        for (int i = 0; i < T; i++)
        {
            const unsigned short* pf = top_blob_forward.row<const unsigned short>(i);
            const unsigned short* pr = top_blob_reverse.row<const unsigned short>(i);
            unsigned short* ptr = top_blob.row<unsigned short>(i);

            memcpy(ptr, pf, num_output * sizeof(unsigned short));
            memcpy(ptr + num_output, pr, num_output * sizeof(unsigned short));
        }
    }

    return 0;
}

int GRU_arm::forward_bf16s(const std::vector<Mat>& bottom_blobs, std::vector<Mat>& top_blobs, const Option& opt) const
{
    const Mat& bottom_blob = bottom_blobs[0];
    int T = bottom_blob.h;
    int num_directions = direction == 2 ? 2 : 1;

    Mat hidden;
    Allocator* hidden_allocator = top_blobs.size() == 2 ? opt.blob_allocator : opt.workspace_allocator;
    if (bottom_blobs.size() == 2)
    {
        Option opt_cast = opt;
        opt_cast.blob_allocator = hidden_allocator;
        cast_bfloat16_to_float32(bottom_blobs[1], hidden, opt_cast);
    }
    else
    {
        hidden.create(num_output, num_directions, 4u, hidden_allocator);
        if (hidden.empty())
            return -100;
        hidden.fill(0.f);
    }

    Mat& top_blob = top_blobs[0];
    top_blob.create(num_output * num_directions, T, 2u, opt.blob_allocator);
    if (top_blob.empty())
        return -100;

    // Uni directional
    if (direction == 0 || direction == 1)
    {
        int ret = gru_bf16s(bottom_blob, top_blob, direction, weight_xc_data_packed.channel(0), bias_c_data_packed.channel(0), weight_hc_data_packed.channel(0), hidden, opt);
        if (ret != 0)
            return ret;
    }

    if (direction == 2)
    {
        Mat top_blob_forward(num_output, T, 2u, opt.workspace_allocator);
        if (top_blob_forward.empty())
            return -100;

        Mat top_blob_reverse(num_output, T, 2u, opt.workspace_allocator);
        if (top_blob_reverse.empty())
            return -100;

        Mat hidden0 = hidden.row_range(0, 1);
        int ret0 = gru_bf16s(bottom_blob, top_blob_forward, 0, weight_xc_data_packed.channel(0), bias_c_data_packed.channel(0), weight_hc_data_packed.channel(0), hidden0, opt);
        if (ret0 != 0)
            return ret0;

        Mat hidden1 = hidden.row_range(1, 1);
        int ret1 = gru_bf16s(bottom_blob, top_blob_reverse, 1, weight_xc_data_packed.channel(1), bias_c_data_packed.channel(1), weight_hc_data_packed.channel(1), hidden1, opt);
        if (ret1 != 0)
            return ret1;

        // concat w
        for (int i = 0; i < T; i++)
        {
            const unsigned short* pf = top_blob_forward.row<const unsigned short>(i);
            const unsigned short* pr = top_blob_reverse.row<const unsigned short>(i);
            unsigned short* ptr = top_blob.row<unsigned short>(i);

            memcpy(ptr, pf, num_output * sizeof(unsigned short));
            memcpy(ptr + num_output, pr, num_output * sizeof(unsigned short));
        }
    }

    if (top_blobs.size() == 2)
    {
        cast_float32_to_bfloat16(hidden, top_blobs[1], opt);
    }

    return 0;
}
#endif // NCNN_BF16

} // namespace ncnn
