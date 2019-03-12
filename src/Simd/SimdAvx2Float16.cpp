/*
* Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2018 Yermalayeu Ihar.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#include "Simd/SimdMemory.h"
#include "Simd/SimdExtract.h"
#include "Simd/SimdStore.h"
#include "Simd/SimdArray.h"

namespace Simd
{
#ifdef SIMD_AVX2_ENABLE    
    namespace Avx2
    {
        template<bool align> SIMD_INLINE void Float32ToFloat16(const float * src, uint16_t * dst)
        {
            Sse2::Store<align>((__m128i*)dst, _mm256_cvtps_ph(Avx::Load<align>(src), 0));
        }

        template <bool align> void Float32ToFloat16(const float * src, size_t size, uint16_t * dst)
        {
            assert(size >= F);
            if (align)
                assert(Aligned(src) && Aligned(dst));

            size_t fullAlignedSize = Simd::AlignLo(size, QF);
            size_t partialAlignedSize = Simd::AlignLo(size, F);

            size_t i = 0;
            for (; i < fullAlignedSize; i += QF)
            {
                Float32ToFloat16<align>(src + i + F * 0, dst + i + F * 0);
                Float32ToFloat16<align>(src + i + F * 1, dst + i + F * 1);
                Float32ToFloat16<align>(src + i + F * 2, dst + i + F * 2);
                Float32ToFloat16<align>(src + i + F * 3, dst + i + F * 3);
            }
            for (; i < partialAlignedSize; i += F)
                Float32ToFloat16<align>(src + i, dst + i);
            if (partialAlignedSize != size)
                Float32ToFloat16<false>(src + size - F, dst + size - F);
        }

        void Float32ToFloat16(const float * src, size_t size, uint16_t * dst)
        {
            if (Aligned(src) && Aligned(dst))
                Float32ToFloat16<true>(src, size, dst);
            else
                Float32ToFloat16<false>(src, size, dst);
        }

        template<bool align> SIMD_INLINE void Float16ToFloat32(const uint16_t * src, float * dst)
        {
            Avx::Store<align>(dst, _mm256_cvtph_ps(Sse2::Load<align>((__m128i*)src)));
        }

        template <bool align> void Float16ToFloat32(const uint16_t * src, size_t size, float * dst)
        {
            assert(size >= F);
            if (align)
                assert(Aligned(src) && Aligned(dst));

            size_t fullAlignedSize = Simd::AlignLo(size, QF);
            size_t partialAlignedSize = Simd::AlignLo(size, F);

            size_t i = 0;
            for (; i < fullAlignedSize; i += QF)
            {
                Float16ToFloat32<align>(src + i + F * 0, dst + i + F * 0);
                Float16ToFloat32<align>(src + i + F * 1, dst + i + F * 1);
                Float16ToFloat32<align>(src + i + F * 2, dst + i + F * 2);
                Float16ToFloat32<align>(src + i + F * 3, dst + i + F * 3);
            }
            for (; i < partialAlignedSize; i += F)
                Float16ToFloat32<align>(src + i, dst + i);
            if (partialAlignedSize != size)
                Float16ToFloat32<false>(src + size - F, dst + size - F);
        }

        void Float16ToFloat32(const uint16_t * src, size_t size, float * dst)
        {
            if (Aligned(src) && Aligned(dst))
                Float16ToFloat32<true>(src, size, dst);
            else
                Float16ToFloat32<false>(src, size, dst);
        }

        template <bool align> SIMD_INLINE void SquaredDifferenceSum16f(const uint16_t * a, const uint16_t * b, size_t offset, __m256 & sum)
        {
            __m256 _a = _mm256_cvtph_ps(Sse2::Load<align>((__m128i*)(a + offset)));
            __m256 _b = _mm256_cvtph_ps(Sse2::Load<align>((__m128i*)(b + offset)));
            __m256 _d = _mm256_sub_ps(_a, _b);
            sum = _mm256_fmadd_ps(_d, _d, sum);
        }

        template <bool align> SIMD_INLINE void SquaredDifferenceSum16f(const uint16_t * a, const uint16_t * b, size_t size, float * sum)
        {
            assert(size >= F);
            if (align)
                assert(Aligned(a) && Aligned(b));

            size_t partialAlignedSize = AlignLo(size, F);
            size_t fullAlignedSize = AlignLo(size, QF);
            size_t i = 0;
            __m256 sums[4] = { _mm256_setzero_ps(), _mm256_setzero_ps(), _mm256_setzero_ps(), _mm256_setzero_ps() };
            if (fullAlignedSize)
            {
                for (; i < fullAlignedSize; i += QF)
                {
                    SquaredDifferenceSum16f<align>(a, b, i + F * 0, sums[0]);
                    SquaredDifferenceSum16f<align>(a, b, i + F * 1, sums[1]);
                    SquaredDifferenceSum16f<align>(a, b, i + F * 2, sums[2]);
                    SquaredDifferenceSum16f<align>(a, b, i + F * 3, sums[3]);
                }
                sums[0] = _mm256_add_ps(_mm256_add_ps(sums[0], sums[1]), _mm256_add_ps(sums[2], sums[3]));
            }
            for (; i < partialAlignedSize; i += F)
                SquaredDifferenceSum16f<align>(a, b, i, sums[0]);
            if (partialAlignedSize != size)
            {
                __m256 mask = RightNotZero(size - partialAlignedSize);
                __m256 _a = _mm256_cvtph_ps(Sse2::Load<false>((__m128i*)(a + size - F)));
                __m256 _b = _mm256_cvtph_ps(Sse2::Load<false>((__m128i*)(b + size - F)));
                __m256 _d = _mm256_and_ps(_mm256_sub_ps(_a, _b), mask);
                sums[0] = _mm256_fmadd_ps(_d, _d, sums[0]);
            }
            *sum = Avx::ExtractSum(sums[0]);
        }

        void SquaredDifferenceSum16f(const uint16_t * a, const uint16_t * b, size_t size, float * sum)
        {
            if (Aligned(a) && Aligned(b))
                SquaredDifferenceSum16f<true>(a, b, size, sum);
            else
                SquaredDifferenceSum16f<false>(a, b, size, sum);
        }

        template<bool align> void CosineDistance16f(const uint16_t * a, const uint16_t * b, size_t size, float * distance)
        {
            if (align)
                assert(Aligned(a) && Aligned(b));

            size_t partialAlignedSize = AlignLo(size, F);
            size_t fullAlignedSize = AlignLo(size, DF);
            size_t i = 0;
            __m256 _aa[2] = { _mm256_setzero_ps(), _mm256_setzero_ps() };
            __m256 _ab[2] = { _mm256_setzero_ps(), _mm256_setzero_ps() };
            __m256 _bb[2] = { _mm256_setzero_ps(), _mm256_setzero_ps() };
            if (fullAlignedSize)
            {
                for (; i < fullAlignedSize; i += DF)
                {
                    __m256 a0 = _mm256_cvtph_ps(Sse2::Load<align>((__m128i*)(a + i) + 0));
                    __m256 b0 = _mm256_cvtph_ps(Sse2::Load<align>((__m128i*)(b + i) + 0));
                    _aa[0] = _mm256_fmadd_ps(a0, a0, _aa[0]);
                    _ab[0] = _mm256_fmadd_ps(a0, b0, _ab[0]);
                    _bb[0] = _mm256_fmadd_ps(b0, b0, _bb[0]);
                    __m256 a1 = _mm256_cvtph_ps(Sse2::Load<align>((__m128i*)(a + i) + 1));
                    __m256 b1 = _mm256_cvtph_ps(Sse2::Load<align>((__m128i*)(b + i) + 1));
                    _aa[1] = _mm256_fmadd_ps(a1, a1, _aa[1]);
                    _ab[1] = _mm256_fmadd_ps(a1, b1, _ab[1]);
                    _bb[1] = _mm256_fmadd_ps(b1, b1, _bb[1]);
                }
                _aa[0] = _mm256_add_ps(_aa[0], _aa[1]);
                _ab[0] = _mm256_add_ps(_ab[0], _ab[1]);
                _bb[0] = _mm256_add_ps(_bb[0], _bb[1]);
            }
            for (; i < partialAlignedSize; i += F)
            {
                __m256 a0 = _mm256_cvtph_ps(Sse2::Load<align>((__m128i*)(a + i) + 0));
                __m256 b0 = _mm256_cvtph_ps(Sse2::Load<align>((__m128i*)(b + i) + 0));
                _aa[0] = _mm256_fmadd_ps(a0, a0, _aa[0]);
                _ab[0] = _mm256_fmadd_ps(a0, b0, _ab[0]);
                _bb[0] = _mm256_fmadd_ps(b0, b0, _bb[0]);
            }
            if (partialAlignedSize != size)
            {
                __m256 mask = RightNotZero(size - partialAlignedSize);
                __m256 a0 = _mm256_and_ps(mask, _mm256_cvtph_ps(Sse2::Load<align>((__m128i*)(a + size - F))));
                __m256 b0 = _mm256_and_ps(mask, _mm256_cvtph_ps(Sse2::Load<align>((__m128i*)(b + size - F))));
                _aa[0] = _mm256_fmadd_ps(a0, a0, _aa[0]);
                _ab[0] = _mm256_fmadd_ps(a0, b0, _ab[0]);
                _bb[0] = _mm256_fmadd_ps(b0, b0, _bb[0]);
            }
            float aa = Avx::ExtractSum(_aa[0]), ab = Avx::ExtractSum(_ab[0]), bb = Avx::ExtractSum(_bb[0]);
            *distance = 1.0f - ab / ::sqrt(aa*bb);
        }

        void CosineDistance16f(const uint16_t * a, const uint16_t * b, size_t size, float * distance)
        {
            if (Aligned(a) && Aligned(b))
                CosineDistance16f<true>(a, b, size, distance);
            else
                CosineDistance16f<false>(a, b, size, distance);
        }

        static void Squares(size_t M, size_t K, const uint16_t * const * A, float * squares)
        {
            size_t M4 = AlignLo(M, 4);
            size_t KF = AlignLo(K, F);
            __m256 mask = RightNotZero(K - KF);
            size_t i = 0;
            for (; i < M4; i += 4)
            {
                __m256 sums[4] = { _mm256_setzero_ps(), _mm256_setzero_ps(), _mm256_setzero_ps(), _mm256_setzero_ps() };
                for (size_t k = 0; k < KF; k += F)
                {
                    __m256 a0 = _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(A[i + 0] + k)));
                    __m256 a1 = _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(A[i + 1] + k)));
                    __m256 a2 = _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(A[i + 2] + k)));
                    __m256 a3 = _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(A[i + 3] + k)));
                    sums[0] = _mm256_fmadd_ps(a0, a0, sums[0]);
                    sums[1] = _mm256_fmadd_ps(a1, a1, sums[1]);
                    sums[2] = _mm256_fmadd_ps(a2, a2, sums[2]);
                    sums[3] = _mm256_fmadd_ps(a3, a3, sums[3]);
                }
                if (KF < F)
                {
                    size_t k = K - F;
                    __m256 a0 = _mm256_and_ps(mask, _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(A[i + 0] + k))));
                    __m256 a1 = _mm256_and_ps(mask, _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(A[i + 1] + k))));
                    __m256 a2 = _mm256_and_ps(mask, _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(A[i + 2] + k))));
                    __m256 a3 = _mm256_and_ps(mask, _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(A[i + 3] + k))));
                    sums[0] = _mm256_fmadd_ps(a0, a0, sums[0]);
                    sums[1] = _mm256_fmadd_ps(a1, a1, sums[1]);
                    sums[2] = _mm256_fmadd_ps(a2, a2, sums[2]);
                    sums[3] = _mm256_fmadd_ps(a3, a3, sums[3]);
                }
                _mm_storeu_ps(squares + i, Extract4Sums(sums));
            }
            for (; i < M; i += 1)
            {
                __m256 sum = _mm256_setzero_ps();
                for (size_t k = 0; k < KF; k += F)
                {
                    __m256 a = _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(A[i] + k)));
                    sum = _mm256_fmadd_ps(a, a, sum);
                }
                if (KF < F)
                {
                    size_t k = K - F;
                    __m256 a = _mm256_and_ps(mask, _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(A[i] + k))));
                    sum = _mm256_fmadd_ps(a, a, sum);
                }
                squares[i] = Avx::ExtractSum(sum);
            }
        }

        SIMD_INLINE __m256 Tail(size_t tail)
        {
            const int32_t mask[DF] = { 0, 0, 0, 0, 0, 0, 0, 0 , -1, -1, -1, -1, -1, -1, -1, -1 };
            return _mm256_loadu_ps((float*)(mask + tail));
        }

        static void MicroCosineDistances3x4(size_t K, const uint16_t * const * A, const uint16_t * const * B, const float * aa, const float * bb, float * distances, size_t stride)
        {
            size_t K8 = K & (~7);
            __m256 c00 = _mm256_setzero_ps();
            __m256 c01 = _mm256_setzero_ps();
            __m256 c02 = _mm256_setzero_ps();
            __m256 c03 = _mm256_setzero_ps();
            __m256 c10 = _mm256_setzero_ps();
            __m256 c11 = _mm256_setzero_ps();
            __m256 c12 = _mm256_setzero_ps();
            __m256 c13 = _mm256_setzero_ps();
            __m256 c20 = _mm256_setzero_ps();
            __m256 c21 = _mm256_setzero_ps();
            __m256 c22 = _mm256_setzero_ps();
            __m256 c23 = _mm256_setzero_ps();
            __m256 a0, a1, a2, b0;
            for (size_t k = 0; k < K8; k += 8)
            {
                a0 = _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(A[0] + k)));
                a1 = _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(A[1] + k)));
                a2 = _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(A[2] + k)));
                b0 = _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(B[0] + k)));
                c00 = _mm256_fmadd_ps(a0, b0, c00);
                c10 = _mm256_fmadd_ps(a1, b0, c10);
                c20 = _mm256_fmadd_ps(a2, b0, c20);
                b0 = _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(B[1] + k)));
                c01 = _mm256_fmadd_ps(a0, b0, c01);
                c11 = _mm256_fmadd_ps(a1, b0, c11);
                c21 = _mm256_fmadd_ps(a2, b0, c21);
                b0 = _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(B[2] + k)));
                c02 = _mm256_fmadd_ps(a0, b0, c02);
                c12 = _mm256_fmadd_ps(a1, b0, c12);
                c22 = _mm256_fmadd_ps(a2, b0, c22);
                b0 = _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(B[3] + k)));
                c03 = _mm256_fmadd_ps(a0, b0, c03);
                c13 = _mm256_fmadd_ps(a1, b0, c13);
                c23 = _mm256_fmadd_ps(a2, b0, c23);
            }
            if (K8 < K)
            {
                size_t k = K - 8;
                __m256 tail = Tail(K - K8);
                a0 = _mm256_and_ps(tail, _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(A[0] + k))));
                a1 = _mm256_and_ps(tail, _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(A[1] + k))));
                a2 = _mm256_and_ps(tail, _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(A[2] + k))));
                b0 = _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(B[0] + k)));
                c00 = _mm256_fmadd_ps(a0, b0, c00);
                c10 = _mm256_fmadd_ps(a1, b0, c10);
                c20 = _mm256_fmadd_ps(a2, b0, c20);
                b0 = _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(B[1] + k)));
                c01 = _mm256_fmadd_ps(a0, b0, c01);
                c11 = _mm256_fmadd_ps(a1, b0, c11);
                c21 = _mm256_fmadd_ps(a2, b0, c21);
                b0 = _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(B[2] + k)));
                c02 = _mm256_fmadd_ps(a0, b0, c02);
                c12 = _mm256_fmadd_ps(a1, b0, c12);
                c22 = _mm256_fmadd_ps(a2, b0, c22);
                b0 = _mm256_cvtph_ps(_mm_loadu_si128((__m128i*)(B[3] + k)));
                c03 = _mm256_fmadd_ps(a0, b0, c03);
                c13 = _mm256_fmadd_ps(a1, b0, c13);
                c23 = _mm256_fmadd_ps(a2, b0, c23);
            }
            __m128 _bb = _mm_loadu_ps(bb);
            __m128 _1 = _mm_set1_ps(1.0f);
            _mm_storeu_ps(distances + 0 * stride, _mm_fnmadd_ps(_mm_rsqrt_ps(_mm_mul_ps(_bb, _mm_set1_ps(aa[0]))), Extract4Sums(c00, c01, c02, c03), _1));
            _mm_storeu_ps(distances + 1 * stride, _mm_fnmadd_ps(_mm_rsqrt_ps(_mm_mul_ps(_bb, _mm_set1_ps(aa[1]))), Extract4Sums(c10, c11, c12, c13), _1));
            _mm_storeu_ps(distances + 2 * stride, _mm_fnmadd_ps(_mm_rsqrt_ps(_mm_mul_ps(_bb, _mm_set1_ps(aa[2]))), Extract4Sums(c20, c21, c22, c23), _1));
        }

        static void MacroCosineDistances(size_t M, size_t N, size_t K, const uint16_t * const * A, const uint16_t * const * B, const float * aa, const float * bb, float * distances, size_t stride)
        {
            for (size_t i = 0; i < M; i += 3)
            {
                for (size_t j = 0; j < N; j += 4)
                    MicroCosineDistances3x4(K, A + i, B + j, aa + i, bb + j, distances + j, stride);
                distances += 3 * stride;
            }
        }

        void CosineDistancesMxNa16f(size_t M, size_t N, size_t K, const uint16_t * const * A, const uint16_t * const * B, float * distances)
        {
            const size_t L2 = 256 * 1024;
            size_t M3 = AlignLoAny(M, 3);
            size_t N4 = AlignLo(N, 4);
            size_t mN = AlignLoAny(L2 / 2 / K, 4);
            size_t mM = AlignLoAny(L2 / 2 / K, 3);
            Array32f aa(M), bb(N);
            for (size_t i = 0; i < M3; i += mM)
            {
                size_t dM = Simd::Min(M3, i + mM) - i;
                Squares(dM, K, A + i, aa.data + i);
                for (size_t j = 0; j < N4; j += mN)
                {
                    size_t dN = Simd::Min(N4, j + mN) - j;
                    if(i == 0)
                        Squares(dN, K, B + j, bb.data + j);
                    MacroCosineDistances(dM, dN, K, A + i, B + j, aa.data + i, bb.data + j, distances + i * N + j, N);
                }
                for (size_t j = N4; j < N; ++j)
                    CosineDistance16f(A[i], B[j], K, distances + i * N + j);
            }
            for (size_t i = M3; i < M; i ++)
                for (size_t j = 0; j < N; ++j)
                    CosineDistance16f(A[i], B[j], K, distances + i * N + j);
        }
    }
#endif// SIMD_AVX2_ENABLE
}
