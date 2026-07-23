// Copyright 2016 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

/* Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "core/fpdfapi/page/cpdf_colorspace.h"

#include <math.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/fpdfapi/page/cpdf_devicecs.h"
#include "core/fpdfapi/page/cpdf_docpagedata.h"
#include "core/fpdfapi/page/cpdf_function.h"
#include "core/fpdfapi/page/cpdf_iccprofile.h"
#include "core/fpdfapi/page/cpdf_indexedcs.h"
#include "core/fpdfapi/page/cpdf_pattern.h"
#include "core/fpdfapi/page/cpdf_patterncs.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_object.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/fpdf_parser_utility.h"
#include "core/fxcodec/fx_codec.h"
#include "core/fxcodec/icc/icc_transform.h"
#include "core/fxcrt/check.h"
#include "core/fxcrt/check_op.h"
#include "core/fxcrt/compiler_specific.h"
#include "core/fxcrt/containers/contains.h"
#include "core/fxcrt/data_vector.h"
#include "core/fxcrt/fx_2d_size.h"
#include "core/fxcrt/fx_safe_types.h"
#include "core/fxcrt/fx_system.h"
#include "core/fxcrt/maybe_owned.h"
#include "core/fxcrt/notreached.h"
#include "core/fxcrt/scoped_set_insertion.h"
#include "core/fxcrt/stl_util.h"
#include "core/fxcrt/zip.h"
#include "core/fxge/dib/fx_dib.h"

namespace {

constexpr auto kSRGBSamples1 = std::to_array<const uint8_t>({
    0,   3,   6,   10,  13,  15,  18,  20,  22,  23,  25,  27,  28,  30,  31,
    32,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,
    48,  49,  49,  50,  51,  52,  53,  53,  54,  55,  56,  56,  57,  58,  58,
    59,  60,  61,  61,  62,  62,  63,  64,  64,  65,  66,  66,  67,  67,  68,
    68,  69,  70,  70,  71,  71,  72,  72,  73,  73,  74,  74,  75,  76,  76,
    77,  77,  78,  78,  79,  79,  79,  80,  80,  81,  81,  82,  82,  83,  83,
    84,  84,  85,  85,  85,  86,  86,  87,  87,  88,  88,  88,  89,  89,  90,
    90,  91,  91,  91,  92,  92,  93,  93,  93,  94,  94,  95,  95,  95,  96,
    96,  97,  97,  97,  98,  98,  98,  99,  99,  99,  100, 100, 101, 101, 101,
    102, 102, 102, 103, 103, 103, 104, 104, 104, 105, 105, 106, 106, 106, 107,
    107, 107, 108, 108, 108, 109, 109, 109, 110, 110, 110, 110, 111, 111, 111,
    112, 112, 112, 113, 113, 113, 114, 114, 114, 115, 115, 115, 115, 116, 116,
    116, 117, 117, 117, 118, 118, 118, 118, 119, 119, 119, 120,
});

constexpr auto kSRGBSamples2 = std::to_array<const uint8_t>({
    120, 121, 122, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135,
    136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 148, 149,
    150, 151, 152, 153, 154, 155, 155, 156, 157, 158, 159, 159, 160, 161, 162,
    163, 163, 164, 165, 166, 167, 167, 168, 169, 170, 170, 171, 172, 173, 173,
    174, 175, 175, 176, 177, 178, 178, 179, 180, 180, 181, 182, 182, 183, 184,
    185, 185, 186, 187, 187, 188, 189, 189, 190, 190, 191, 192, 192, 193, 194,
    194, 195, 196, 196, 197, 197, 198, 199, 199, 200, 200, 201, 202, 202, 203,
    203, 204, 205, 205, 206, 206, 207, 208, 208, 209, 209, 210, 210, 211, 212,
    212, 213, 213, 214, 214, 215, 215, 216, 216, 217, 218, 218, 219, 219, 220,
    220, 221, 221, 222, 222, 223, 223, 224, 224, 225, 226, 226, 227, 227, 228,
    228, 229, 229, 230, 230, 231, 231, 232, 232, 233, 233, 234, 234, 235, 235,
    236, 236, 237, 237, 238, 238, 238, 239, 239, 240, 240, 241, 241, 242, 242,
    243, 243, 244, 244, 245, 245, 246, 246, 246, 247, 247, 248, 248, 249, 249,
    250, 250, 251, 251, 251, 252, 252, 253, 253, 254, 254, 255, 255,
});

static const uint8_t g_ContribTable_layer_0[256] = {
    0,  0,  0,  0,  0,  // 0-4
    1,  1,  1,  1,  1,  // 5-9
    2,  2,  2,  2,  2,  // 10-14
    3,  3,  3,  3,  3,  // 15-19
    4,  4,  4,  4,  4,  // 20-24
    5,  5,  5,  5,  5,  // 25-29
    6,  6,  6,  6,  6,  // 30-34
    7,  7,  7,  7,  7,  // 35-39
    8,  8,  8,  8,  8,  // 40-44
    9,  9,  9,  9,  9,  // 45-49
    10, 10, 10, 10, 10, // 50-54
    11, 11, 11, 11, 11, // 55-59
    12, 12, 12, 12, 12, // 60-64
    13, 13, 13, 13, 13, // 65-69
    14, 14, 14, 14, 14, // 70-74
    15, 15, 15, 15, 15, // 75-79
    16, 16, 16, 16, 16, // 80-84
    17, 17, 17, 17, 17, // 85-89
    18, 18, 18, 18, 18, // 90-94
    19, 19, 19, 19, 19, // 95-99
    20, 20, 20, 20, 20, // 100-104
    21, 21, 21, 21, 21, // 105-109
    22, 22, 22, 22, 22, // 110-114
    23, 23, 23, 23, 23, // 115-119
    24, 24, 24, 24, 24, // 120-124
    25, 25, 25, 25, 25, // 125-129
    26, 26, 26, 26, 26, // 130-134
    27, 27, 27, 27, 27, // 135-139
    28, 28, 28, 28, 28, // 140-144
    29, 29, 29, 29, 29, // 145-149
    30, 30, 30, 30, 30, // 150-154
    31, 31, 31, 31, 31, // 155-159
    32, 32, 32, 32, 32, // 160-164
    33, 33, 33, 33, 33, // 165-169
    34, 34, 34, 34, 34, // 170-174
    35, 35, 35, 35, 35, // 175-179
    36, 36, 36, 36, 36, // 180-184
    37, 37, 37, 37, 37, // 185-189
    38, 38, 38, 38, 38, // 190-194
    39, 39, 39, 39, 39, // 195-199
    40, 40, 40, 40, 40, // 200-204
    41, 41, 41, 41, 41, // 205-209
    42, 42, 42, 42, 42, // 210-214
    43, 43, 43, 43, 43, // 215-219
    44, 44, 44, 44, 44, // 220-224
    45, 45, 45, 45, 45, // 225-229
    46, 46, 46, 46, 46, // 230-234
    47, 47, 47, 47, 47, // 235-239
    48, 48, 48, 48, 48, // 240-244
    49, 49, 49, 49, 49, // 245-249
    50, 50, 50, 50, 50, // 250-254
    51                  // 255
};

static const uint32_t g_ContribTable_layer_1[256] = {
    0,    0,    0,    0,    0,    // 0-4
    52,   52,   52,   52,   52,   // 5-9
    104,  104,  104,  104,  104,  // 10-14
    156,  156,  156,  156,  156,  // 15-19
    208,  208,  208,  208,  208,  // 20-24
    260,  260,  260,  260,  260,  // 25-29
    312,  312,  312,  312,  312,  // 30-34
    364,  364,  364,  364,  364,  // 35-39
    416,  416,  416,  416,  416,  // 40-44
    468,  468,  468,  468,  468,  // 45-49
    520,  520,  520,  520,  520,  // 50-54
    572,  572,  572,  572,  572,  // 55-59
    624,  624,  624,  624,  624,  // 60-64
    676,  676,  676,  676,  676,  // 65-69
    728,  728,  728,  728,  728,  // 70-74
    780,  780,  780,  780,  780,  // 75-79
    832,  832,  832,  832,  832,  // 80-84
    884,  884,  884,  884,  884,  // 85-89
    936,  936,  936,  936,  936,  // 90-94
    988,  988,  988,  988,  988,  // 95-99
    1040, 1040, 1040, 1040, 1040, // 100-104
    1092, 1092, 1092, 1092, 1092, // 105-109
    1144, 1144, 1144, 1144, 1144, // 110-114
    1196, 1196, 1196, 1196, 1196, // 115-119
    1248, 1248, 1248, 1248, 1248, // 120-124
    1300, 1300, 1300, 1300, 1300, // 125-129
    1352, 1352, 1352, 1352, 1352, // 130-134
    1404, 1404, 1404, 1404, 1404, // 135-139
    1456, 1456, 1456, 1456, 1456, // 140-144
    1508, 1508, 1508, 1508, 1508, // 145-149
    1560, 1560, 1560, 1560, 1560, // 150-154
    1612, 1612, 1612, 1612, 1612, // 155-159
    1664, 1664, 1664, 1664, 1664, // 160-164
    1716, 1716, 1716, 1716, 1716, // 165-169
    1768, 1768, 1768, 1768, 1768, // 170-174
    1820, 1820, 1820, 1820, 1820, // 175-179
    1872, 1872, 1872, 1872, 1872, // 180-184
    1924, 1924, 1924, 1924, 1924, // 185-189
    1976, 1976, 1976, 1976, 1976, // 190-194
    2028, 2028, 2028, 2028, 2028, // 195-199
    2080, 2080, 2080, 2080, 2080, // 200-204
    2132, 2132, 2132, 2132, 2132, // 205-209
    2184, 2184, 2184, 2184, 2184, // 210-214
    2236, 2236, 2236, 2236, 2236, // 215-219
    2288, 2288, 2288, 2288, 2288, // 220-224
    2340, 2340, 2340, 2340, 2340, // 225-229
    2392, 2392, 2392, 2392, 2392, // 230-234
    2444, 2444, 2444, 2444, 2444, // 235-239
    2496, 2496, 2496, 2496, 2496, // 240-244
    2548, 2548, 2548, 2548, 2548, // 245-249
    2600, 2600, 2600, 2600, 2600, // 250-254
    2652                          // 255
};

static const uint32_t g_ContribTable_layer_2[256] = {
    0,       0,       0,       0,       0,       // 0-4
    2704,    2704,    2704,    2704,    2704,    // 5-9
    5408,    5408,    5408,    5408,    5408,    // 10-14
    8112,    8112,    8112,    8112,    8112,    // 15-19
    10816,   10816,   10816,   10816,   10816,   // 20-24
    13520,   13520,   13520,   13520,   13520,   // 25-29
    16224,   16224,   16224,   16224,   16224,   // 30-34
    18928,   18928,   18928,   18928,   18928,   // 35-39
    21632,   21632,   21632,   21632,   21632,   // 40-44
    24336,   24336,   24336,   24336,   24336,   // 45-49
    27040,   27040,   27040,   27040,   27040,   // 50-54
    29744,   29744,   29744,   29744,   29744,   // 55-59
    32448,   32448,   32448,   32448,   32448,   // 60-64
    35152,   35152,   35152,   35152,   35152,   // 65-69
    37856,   37856,   37856,   37856,   37856,   // 70-74
    40560,   40560,   40560,   40560,   40560,   // 75-79
    43264,   43264,   43264,   43264,   43264,   // 80-84
    45968,   45968,   45968,   45968,   45968,   // 85-89
    48672,   48672,   48672,   48672,   48672,   // 90-94
    51376,   51376,   51376,   51376,   51376,   // 95-99
    54080,   54080,   54080,   54080,   54080,   // 100-104
    56784,   56784,   56784,   56784,   56784,   // 105-109
    59488,   59488,   59488,   59488,   59488,   // 110-114
    62192,   62192,   62192,   62192,   62192,   // 115-119
    64896,   64896,   64896,   64896,   64896,   // 120-124
    67600,   67600,   67600,   67600,   67600,   // 125-129
    70304,   70304,   70304,   70304,   70304,   // 130-134
    73008,   73008,   73008,   73008,   73008,   // 135-139
    75712,   75712,   75712,   75712,   75712,   // 140-144
    78416,   78416,   78416,   78416,   78416,   // 145-149
    81120,   81120,   81120,   81120,   81120,   // 150-154
    83824,   83824,   83824,   83824,   83824,   // 155-159
    86528,   86528,   86528,   86528,   86528,   // 160-164
    89232,   89232,   89232,   89232,   89232,   // 165-169
    91936,   91936,   91936,   91936,   91936,   // 170-174
    94640,   94640,   94640,   94640,   94640,   // 175-179
    97344,   97344,   97344,   97344,   97344,   // 180-184
    100048,  100048,  100048,  100048,  100048,  // 185-189
    102752,  102752,  102752,  102752,  102752,  // 190-194
    105456,  105456,  105456,  105456,  105456,  // 195-199
    108160,  108160,  108160,  108160,  108160,  // 200-204
    110864,  110864,  110864,  110864,  110864,  // 205-209
    113568,  113568,  113568,  113568,  113568,  // 210-214
    116272,  116272,  116272,  116272,  116272,  // 215-219
    118976,  118976,  118976,  118976,  118976,  // 220-224
    121680,  121680,  121680,  121680,  121680,  // 225-229
    124384,  124384,  124384,  124384,  124384,  // 230-234
    127088,  127088,  127088,  127088,  127088,  // 235-239
    129792,  129792,  129792,  129792,  129792,  // 240-244
    132496,  132496,  132496,  132496,  132496,  // 245-249
    135200,  135200,  135200,  135200,  135200,  // 250-254
    137904                                       // 255
};

static const uint32_t g_ContribTable_layer_3[256] = {
    0,         0,         0,         0,         0,         // 0-4
    140608,    140608,    140608,    140608,    140608,    // 5-9
    281216,    281216,    281216,    281216,    281216,    // 10-14
    421824,    421824,    421824,    421824,    421824,    // 15-19
    562432,    562432,    562432,    562432,    562432,    // 20-24
    703040,    703040,    703040,    703040,    703040,    // 25-29
    843648,    843648,    843648,    843648,    843648,    // 30-34
    984256,    984256,    984256,    984256,    984256,    // 35-39
    1124864,   1124864,   1124864,   1124864,   1124864,   // 40-44
    1265472,   1265472,   1265472,   1265472,   1265472,   // 45-49
    1406080,   1406080,   1406080,   1406080,   1406080,   // 50-54
    1546688,   1546688,   1546688,   1546688,   1546688,   // 55-59
    1687296,   1687296,   1687296,   1687296,   1687296,   // 60-64
    1827904,   1827904,   1827904,   1827904,   1827904,   // 65-69
    1968512,   1968512,   1968512,   1968512,   1968512,   // 70-74
    2109120,   2109120,   2109120,   2109120,   2109120,   // 75-79
    2249728,   2249728,   2249728,   2249728,   2249728,   // 80-84
    2390336,   2390336,   2390336,   2390336,   2390336,   // 85-89
    2530944,   2530944,   2530944,   2530944,   2530944,   // 90-94
    2671552,   2671552,   2671552,   2671552,   2671552,   // 95-99
    2812160,   2812160,   2812160,   2812160,   2812160,   // 100-104
    2952768,   2952768,   2952768,   2952768,   2952768,   // 105-109
    3093376,   3093376,   3093376,   3093376,   3093376,   // 110-114
    3233984,   3233984,   3233984,   3233984,   3233984,   // 115-119
    3374592,   3374592,   3374592,   3374592,   3374592,   // 120-124
    3515200,   3515200,   3515200,   3515200,   3515200,   // 125-129
    3655808,   3655808,   3655808,   3655808,   3655808,   // 130-134
    3796416,   3796416,   3796416,   3796416,   3796416,   // 135-139
    3937024,   3937024,   3937024,   3937024,   3937024,   // 140-144
    4077632,   4077632,   4077632,   4077632,   4077632,   // 145-149
    4218240,   4218240,   4218240,   4218240,   4218240,   // 150-154
    4358848,   4358848,   4358848,   4358848,   4358848,   // 155-159
    4499456,   4499456,   4499456,   4499456,   4499456,   // 160-164
    4640064,   4640064,   4640064,   4640064,   4640064,   // 165-169
    4780672,   4780672,   4780672,   4780672,   4780672,   // 170-174
    4921280,   4921280,   4921280,   4921280,   4921280,   // 175-179
    5061888,   5061888,   5061888,   5061888,   5061888,   // 180-184
    5202496,   5202496,   5202496,   5202496,   5202496,   // 185-189
    5343104,   5343104,   5343104,   5343104,   5343104,   // 190-194
    5483712,   5483712,   5483712,   5483712,   5483712,   // 195-199
    5624320,   5624320,   5624320,   5624320,   5624320,   // 200-204
    5764928,   5764928,   5764928,   5764928,   5764928,   // 205-209
    5905536,   5905536,   5905536,   5905536,   5905536,   // 210-214
    6046144,   6046144,   6046144,   6046144,   6046144,   // 215-219
    6186752,   6186752,   6186752,   6186752,   6186752,   // 220-224
    6327360,   6327360,   6327360,   6327360,   6327360,   // 225-229
    6467968,   6467968,   6467968,   6467968,   6467968,   // 230-234
    6608576,   6608576,   6608576,   6608576,   6608576,   // 235-239
    6749184,   6749184,   6749184,   6749184,   6749184,   // 240-244
    6889792,   6889792,   6889792,   6889792,   6889792,   // 245-249
    7030400,   7030400,   7030400,   7030400,   7030400,   // 250-254
    7171008                                                // 255
};

constexpr size_t kBlackWhitePointCount = 3;

void GetDefaultBlackPoint(pdfium::span<float> pPoints) {
  static constexpr float kDefaultValue = 0.0f;
  for (size_t i = 0; i < kBlackWhitePointCount; ++i) {
    pPoints[i] = kDefaultValue;
  }
}

void GetBlackPoint(const CPDF_Dictionary* dict, pdfium::span<float> pPoints) {
  RetainPtr<const CPDF_Array> pParam = dict->GetArrayFor("BlackPoint");
  if (!pParam || pParam->size() != kBlackWhitePointCount) {
    GetDefaultBlackPoint(pPoints);
    return;
  }

  // Check to make sure all values are non-negative.
  for (size_t i = 0; i < kBlackWhitePointCount; ++i) {
    pPoints[i] = pParam->GetFloatAt(i);
    if (pPoints[i] < 0) {
      GetDefaultBlackPoint(pPoints);
      return;
    }
  }
}

bool GetWhitePoint(const CPDF_Dictionary* dict, pdfium::span<float> pPoints) {
  RetainPtr<const CPDF_Array> pParam = dict->GetArrayFor("WhitePoint");
  if (!pParam || pParam->size() != kBlackWhitePointCount) {
    return false;
  }

  for (size_t i = 0; i < kBlackWhitePointCount; ++i) {
    pPoints[i] = pParam->GetFloatAt(i);
  }
  return pPoints[0] > 0.0f && pPoints[1] == 1.0f && pPoints[2] > 0.0f;
}

class CPDF_CalGray final : public CPDF_ColorSpace {
 public:
  CONSTRUCT_VIA_MAKE_RETAIN;
  ~CPDF_CalGray() override;

  // CPDF_ColorSpace:
  std::optional<FX_RGB_STRUCT<float>> GetRGB(
      pdfium::span<const float> pBuf) const override;
  uint32_t v_Load(CPDF_Document* doc,
                  const CPDF_Array* pArray,
                  std::set<const CPDF_Object*>* pVisited) override;
  void TranslateImageLine(pdfium::span<uint8_t> dest_span,
                          pdfium::span<const uint8_t> src_span,
                          int pixels,
                          int image_width,
                          int image_height,
                          bool bTransMask) const override;

 private:
  static constexpr float kDefaultGamma = 1.0f;

  CPDF_CalGray();

  float gamma_ = kDefaultGamma;
  std::array<float, kBlackWhitePointCount> white_point_ = {{1.0f, 1.0f, 1.0f}};
  std::array<float, kBlackWhitePointCount> black_point_ = {{0.0f, 0.0f, 0.0f}};
};

class CPDF_CalRGB final : public CPDF_ColorSpace {
 public:
  CONSTRUCT_VIA_MAKE_RETAIN;
  ~CPDF_CalRGB() override;

  // CPDF_ColorSpace:
  std::optional<FX_RGB_STRUCT<float>> GetRGB(
      pdfium::span<const float> pBuf) const override;
  void TranslateImageLine(pdfium::span<uint8_t> dest_span,
                          pdfium::span<const uint8_t> src_span,
                          int pixels,
                          int image_width,
                          int image_height,
                          bool bTransMask) const override;
  uint32_t v_Load(CPDF_Document* doc,
                  const CPDF_Array* pArray,
                  std::set<const CPDF_Object*>* pVisited) override;

 private:
  static constexpr size_t kGammaCount = 3;
  static constexpr size_t kMatrixCount = 9;

  CPDF_CalRGB();

  std::array<float, kBlackWhitePointCount> white_point_ = {{1.0f, 1.0f, 1.0f}};
  std::array<float, kBlackWhitePointCount> black_point_ = {{0.0f, 0.0f, 0.0f}};
  std::optional<std::array<float, kGammaCount>> gamma_;
  std::optional<std::array<float, kMatrixCount>> matrix_;
};

class CPDF_LabCS final : public CPDF_ColorSpace {
 public:
  CONSTRUCT_VIA_MAKE_RETAIN;
  ~CPDF_LabCS() override;

  // CPDF_ColorSpace:
  std::optional<FX_RGB_STRUCT<float>> GetRGB(
      pdfium::span<const float> pBuf) const override;
  void GetDefaultValue(int iComponent,
                       float* value,
                       float* min,
                       float* max) const override;
  void TranslateImageLine(pdfium::span<uint8_t> dest_span,
                          pdfium::span<const uint8_t> src_span,
                          int pixels,
                          int image_width,
                          int image_height,
                          bool bTransMask) const override;
  uint32_t v_Load(CPDF_Document* doc,
                  const CPDF_Array* pArray,
                  std::set<const CPDF_Object*>* pVisited) override;

 private:
  static constexpr size_t kRangesCount = 4;

  CPDF_LabCS();

  std::array<float, kBlackWhitePointCount> white_point_ = {{1.0f, 1.0f, 1.0f}};
  std::array<float, kBlackWhitePointCount> black_point_ = {{0.0f, 0.0f, 0.0f}};
  std::array<float, kRangesCount> ranges_ = {};
};

class CPDF_ICCBasedCS final : public CPDF_BasedCS {
 public:
  CONSTRUCT_VIA_MAKE_RETAIN;
  ~CPDF_ICCBasedCS() override;

  // CPDF_ColorSpace:
  std::optional<FX_RGB_STRUCT<float>> GetRGB(
      pdfium::span<const float> pBuf) const override;
  RetainPtr<CPDF_IccProfile> GetIccProfile() const override;
  void TranslateImageLine(pdfium::span<uint8_t> dest_span,
                          pdfium::span<const uint8_t> src_span,
                          int pixels,
                          int image_width,
                          int image_height,
                          bool bTransMask) const override;
  bool IsNormal() const override;
  uint32_t v_Load(CPDF_Document* doc,
                  const CPDF_Array* pArray,
                  std::set<const CPDF_Object*>* pVisited) override;

 private:
  CPDF_ICCBasedCS();

  // If no valid ICC profile or using sRGB, try looking for an alternate.
  bool FindAlternateProfile(CPDF_Document* doc,
                            const CPDF_Dictionary* dict,
                            std::set<const CPDF_Object*>* pVisited,
                            uint32_t nExpectedComponents);
  static RetainPtr<CPDF_ColorSpace> GetStockAlternateProfile(
      uint32_t nComponents);
  static std::vector<float> GetRanges(const CPDF_Dictionary* dict,
                                      uint32_t nComponents);

  RetainPtr<CPDF_IccProfile> profile_;
  mutable DataVector<uint8_t> cache_;
  std::vector<float> ranges_;
};

class CPDF_SeparationCS final : public CPDF_BasedCS {
 public:
  CONSTRUCT_VIA_MAKE_RETAIN;
  ~CPDF_SeparationCS() override;

  // CPDF_ColorSpace:
  std::optional<FX_RGB_STRUCT<float>> GetRGB(
      pdfium::span<const float> pBuf) const override;
  void GetDefaultValue(int iComponent,
                       float* value,
                       float* min,
                       float* max) const override;
  uint32_t v_Load(CPDF_Document* doc,
                  const CPDF_Array* pArray,
                  std::set<const CPDF_Object*>* pVisited) override;

 private:
  CPDF_SeparationCS();

  bool is_none_type_ = false;
  std::unique_ptr<const CPDF_Function> func_;
};

class CPDF_DeviceNCS final : public CPDF_BasedCS {
 public:
  CONSTRUCT_VIA_MAKE_RETAIN;
  ~CPDF_DeviceNCS() override;

  // CPDF_ColorSpace:
  std::optional<FX_RGB_STRUCT<float>> GetRGB(
      pdfium::span<const float> pBuf) const override;
  void GetDefaultValue(int iComponent,
                       float* value,
                       float* min,
                       float* max) const override;
  uint32_t v_Load(CPDF_Document* doc,
                  const CPDF_Array* pArray,
                  std::set<const CPDF_Object*>* pVisited) override;

 private:
  CPDF_DeviceNCS();

  std::unique_ptr<const CPDF_Function> func_;
};

class Vector_3by1 {
 public:
  Vector_3by1() : a(0.0f), b(0.0f), c(0.0f) {}

  Vector_3by1(float a1, float b1, float c1) : a(a1), b(b1), c(c1) {}

  float a;
  float b;
  float c;
};

class Matrix_3by3 {
 public:
  Matrix_3by3()
      : a(0.0f),
        b(0.0f),
        c(0.0f),
        d(0.0f),
        e(0.0f),
        f(0.0f),
        g(0.0f),
        h(0.0f),
        i(0.0f) {}

  Matrix_3by3(float a1,
              float b1,
              float c1,
              float d1,
              float e1,
              float f1,
              float g1,
              float h1,
              float i1)
      : a(a1), b(b1), c(c1), d(d1), e(e1), f(f1), g(g1), h(h1), i(i1) {}

  Matrix_3by3 Inverse() {
    float det = a * (e * i - f * h) - b * (i * d - f * g) + c * (d * h - e * g);
    if (fabs(det) < std::numeric_limits<float>::epsilon()) {
      return Matrix_3by3();
    }

    return Matrix_3by3(
        (e * i - f * h) / det, -(b * i - c * h) / det, (b * f - c * e) / det,
        -(d * i - f * g) / det, (a * i - c * g) / det, -(a * f - c * d) / det,
        (d * h - e * g) / det, -(a * h - b * g) / det, (a * e - b * d) / det);
  }

  Matrix_3by3 Multiply(const Matrix_3by3& m) {
    return Matrix_3by3(a * m.a + b * m.d + c * m.g, a * m.b + b * m.e + c * m.h,
                       a * m.c + b * m.f + c * m.i, d * m.a + e * m.d + f * m.g,
                       d * m.b + e * m.e + f * m.h, d * m.c + e * m.f + f * m.i,
                       g * m.a + h * m.d + i * m.g, g * m.b + h * m.e + i * m.h,
                       g * m.c + h * m.f + i * m.i);
  }

  Vector_3by1 TransformVector(const Vector_3by1& v) {
    return Vector_3by1(a * v.a + b * v.b + c * v.c, d * v.a + e * v.b + f * v.c,
                       g * v.a + h * v.b + i * v.c);
  }

  float a;
  float b;
  float c;
  float d;
  float e;
  float f;
  float g;
  float h;
  float i;
};

float RGB_Conversion(float colorComponent) {
  colorComponent = std::clamp(colorComponent, 0.0f, 1.0f);
  int scale = std::max(static_cast<int>(colorComponent * 1023), 0);
  if (scale < 192) {
    return kSRGBSamples1[scale] / 255.0f;
  }
  return kSRGBSamples2[scale / 4 - 48] / 255.0f;
}

FX_RGB_STRUCT<float> XYZ_to_sRGB(float X, float Y, float Z) {
  const float R1 = 3.2410f * X - 1.5374f * Y - 0.4986f * Z;
  const float G1 = -0.9692f * X + 1.8760f * Y + 0.0416f * Z;
  const float B1 = 0.0556f * X - 0.2040f * Y + 1.0570f * Z;
  return {RGB_Conversion(R1), RGB_Conversion(G1), RGB_Conversion(B1)};
}

FX_RGB_STRUCT<float> XYZ_to_sRGB_WhitePoint(float X,
                                            float Y,
                                            float Z,
                                            float Xw,
                                            float Yw,
                                            float Zw) {
  // The following RGB_xyz is based on
  // sRGB value {Rx,Ry}={0.64, 0.33}, {Gx,Gy}={0.30, 0.60}, {Bx,By}={0.15, 0.06}

  static constexpr float Rx = 0.64f;
  static constexpr float Ry = 0.33f;
  static constexpr float Gx = 0.30f;
  static constexpr float Gy = 0.60f;
  static constexpr float Bx = 0.15f;
  static constexpr float By = 0.06f;
  Matrix_3by3 RGB_xyz(Rx, Gx, Bx, Ry, Gy, By, 1 - Rx - Ry, 1 - Gx - Gy,
                      1 - Bx - By);
  Vector_3by1 whitePoint(Xw, Yw, Zw);
  Vector_3by1 XYZ(X, Y, Z);

  Vector_3by1 RGB_Sum_XYZ = RGB_xyz.Inverse().TransformVector(whitePoint);
  Matrix_3by3 RGB_SUM_XYZ_DIAG(RGB_Sum_XYZ.a, 0, 0, 0, RGB_Sum_XYZ.b, 0, 0, 0,
                               RGB_Sum_XYZ.c);
  Matrix_3by3 M = RGB_xyz.Multiply(RGB_SUM_XYZ_DIAG);
  Vector_3by1 RGB = M.Inverse().TransformVector(XYZ);

  return {RGB_Conversion(RGB.a), RGB_Conversion(RGB.b), RGB_Conversion(RGB.c)};
}

class StockColorSpaces {
 public:
  StockColorSpaces()
      : gray_(pdfium::MakeRetain<CPDF_DeviceCS>(
            CPDF_ColorSpace::Family::kDeviceGray)),
        rgb_(pdfium::MakeRetain<CPDF_DeviceCS>(
            CPDF_ColorSpace::Family::kDeviceRGB)),
        cmyk_(pdfium::MakeRetain<CPDF_DeviceCS>(
            CPDF_ColorSpace::Family::kDeviceCMYK)),
        pattern_(pdfium::MakeRetain<CPDF_PatternCS>()) {
    pattern_->InitializeStockPattern();
  }
  StockColorSpaces(const StockColorSpaces&) = delete;
  StockColorSpaces& operator=(const StockColorSpaces&) = delete;
  ~StockColorSpaces() = default;

  RetainPtr<CPDF_ColorSpace> GetStockCS(CPDF_ColorSpace::Family family) {
    if (family == CPDF_ColorSpace::Family::kDeviceGray) {
      return gray_;
    }
    if (family == CPDF_ColorSpace::Family::kDeviceRGB) {
      return rgb_;
    }
    if (family == CPDF_ColorSpace::Family::kDeviceCMYK) {
      return cmyk_;
    }
    if (family == CPDF_ColorSpace::Family::kPattern) {
      return pattern_;
    }
    NOTREACHED();
  }

 private:
  RetainPtr<CPDF_DeviceCS> gray_;
  RetainPtr<CPDF_DeviceCS> rgb_;
  RetainPtr<CPDF_DeviceCS> cmyk_;
  RetainPtr<CPDF_PatternCS> pattern_;
};

StockColorSpaces* g_stock_colorspaces = nullptr;

}  // namespace

PatternValue::PatternValue() = default;

PatternValue::PatternValue(const PatternValue& that) = default;

PatternValue::~PatternValue() = default;

void PatternValue::SetComps(pdfium::span<const float> comps) {
  fxcrt::Copy(comps, comps_);
}

// static
void CPDF_ColorSpace::InitializeGlobals() {
  CHECK(!g_stock_colorspaces);
  g_stock_colorspaces = new StockColorSpaces();
}

// static
void CPDF_ColorSpace::DestroyGlobals() {
  delete g_stock_colorspaces;
  g_stock_colorspaces = nullptr;
}

// static
RetainPtr<CPDF_ColorSpace> CPDF_ColorSpace::GetStockCS(Family family) {
  return g_stock_colorspaces->GetStockCS(family);
}

// static
RetainPtr<CPDF_ColorSpace> CPDF_ColorSpace::GetStockCSForName(
    const ByteString& name) {
  if (name == "DeviceRGB" || name == "RGB") {
    return GetStockCS(Family::kDeviceRGB);
  }
  if (name == "DeviceGray" || name == "G") {
    return GetStockCS(Family::kDeviceGray);
  }
  if (name == "DeviceCMYK" || name == "CMYK") {
    return GetStockCS(Family::kDeviceCMYK);
  }
  if (name == "Pattern") {
    return GetStockCS(Family::kPattern);
  }
  return nullptr;
}

// static
RetainPtr<CPDF_ColorSpace> CPDF_ColorSpace::Load(
    CPDF_Document* doc,
    const CPDF_Object* pObj,
    std::set<const CPDF_Object*>* pVisited) {
  if (!pObj) {
    return nullptr;
  }

  if (pdfium::Contains(*pVisited, pObj)) {
    return nullptr;
  }

  ScopedSetInsertion insertion(pVisited, pObj);

  if (pObj->IsName()) {
    return GetStockCSForName(pObj->GetString());
  }

  if (const CPDF_Stream* pStream = pObj->AsStream()) {
    RetainPtr<const CPDF_Dictionary> dict = pStream->GetDict();
    CPDF_DictionaryLocker locker(std::move(dict));
    for (const auto& it : locker) {
      RetainPtr<const CPDF_Name> pValue = ToName(it.second);
      if (pValue) {
        RetainPtr<CPDF_ColorSpace> pRet =
            GetStockCSForName(pValue->GetString());
        if (pRet) {
          return pRet;
        }
      }
    }
    return nullptr;
  }

  const CPDF_Array* pArray = pObj->AsArray();
  if (!pArray || pArray->IsEmpty()) {
    return nullptr;
  }

  RetainPtr<const CPDF_Object> pFamilyObj = pArray->GetDirectObjectAt(0);
  if (!pFamilyObj) {
    return nullptr;
  }

  ByteString familyname = pFamilyObj->GetString();
  if (pArray->size() == 1) {
    return GetStockCSForName(familyname);
  }

  RetainPtr<CPDF_ColorSpace> pCS =
      CPDF_ColorSpace::AllocateColorSpace(familyname.AsStringView());
  if (!pCS) {
    return nullptr;
  }

  pCS->array_.Reset(pArray);
  pCS->components_ = pCS->v_Load(doc, pArray, pVisited);
  if (pCS->components_ == 0) {
    return nullptr;
  }

  return pCS;
}

// static
RetainPtr<CPDF_ColorSpace> CPDF_ColorSpace::AllocateColorSpace(
    ByteStringView bsFamilyName) {
  switch (bsFamilyName.GetID()) {
    case FXBSTR_ID('C', 'a', 'l', 'G'):
      return pdfium::MakeRetain<CPDF_CalGray>();
    case FXBSTR_ID('C', 'a', 'l', 'R'):
      return pdfium::MakeRetain<CPDF_CalRGB>();
    case FXBSTR_ID('L', 'a', 'b', 0):
      return pdfium::MakeRetain<CPDF_LabCS>();
    case FXBSTR_ID('I', 'C', 'C', 'B'):
      return pdfium::MakeRetain<CPDF_ICCBasedCS>();
    case FXBSTR_ID('I', 'n', 'd', 'e'):
    case FXBSTR_ID('I', 0, 0, 0):
      return pdfium::MakeRetain<CPDF_IndexedCS>();
    case FXBSTR_ID('S', 'e', 'p', 'a'):
      return pdfium::MakeRetain<CPDF_SeparationCS>();
    case FXBSTR_ID('D', 'e', 'v', 'i'):
      return pdfium::MakeRetain<CPDF_DeviceNCS>();
    case FXBSTR_ID('P', 'a', 't', 't'):
      return pdfium::MakeRetain<CPDF_PatternCS>();
    default:
      return nullptr;
  }
}

// static
uint32_t CPDF_ColorSpace::ComponentsForFamily(Family family) {
  switch (family) {
    case Family::kDeviceGray:
      return 1;
    case Family::kDeviceRGB:
      return 3;
    case Family::kDeviceCMYK:
      return 4;
    default:
      NOTREACHED();
  }
}

std::vector<float> CPDF_ColorSpace::CreateBufAndSetDefaultColor() const {
  DCHECK(family_ != Family::kPattern);

  float min;
  float max;
  std::vector<float> buf(components_);
  for (uint32_t i = 0; i < components_; i++) {
    GetDefaultValue(i, &buf[i], &min, &max);
  }

  return buf;
}

uint32_t CPDF_ColorSpace::ComponentCount() const {
  return components_;
}

// Returns nullptr because only the CPDF_ICCBasedCS subclass supports ICC
// profiles. CPDF_ICCBasedCS overrides this method to return valid ICC
// profile data.
RetainPtr<CPDF_IccProfile> CPDF_ColorSpace::GetIccProfile() const {
  return nullptr;
}

void CPDF_ColorSpace::GetDefaultValue(int iComponent,
                                      float* value,
                                      float* min,
                                      float* max) const {
  *value = 0.0f;
  *min = 0.0f;
  *max = 1.0f;
}

void CPDF_ColorSpace::TranslateImageLine(pdfium::span<uint8_t> dest_span,
                                         pdfium::span<const uint8_t> src_span,
                                         int pixels,
                                         int image_width,
                                         int image_height,
                                         bool bTransMask) const {
  // Only applies to CMYK colorspaces. None of the colorspaces that use this
  // generic base implementation are CMYK.
  CHECK(!bTransMask);

  uint8_t* dest_buf = dest_span.data();
  const uint8_t* src_buf = src_span.data();
  std::vector<float> src(components_);
  const int divisor = family_ != Family::kIndexed ? 255 : 1;
  UNSAFE_TODO({
    for (int i = 0; i < pixels; i++) {
      for (uint32_t j = 0; j < components_; j++) {
        src[j] = static_cast<float>(*src_buf++) / divisor;
      }
      auto rgb = GetRGBOrZerosOnError(src);
      *dest_buf++ = static_cast<int32_t>(rgb.blue * 255);
      *dest_buf++ = static_cast<int32_t>(rgb.green * 255);
      *dest_buf++ = static_cast<int32_t>(rgb.red * 255);
    }
  });
}

void CPDF_ColorSpace::EnableStdConversion(bool bEnabled) {
  if (bEnabled) {
    std_conversion_++;
  } else if (std_conversion_) {
    std_conversion_--;
  }
}

bool CPDF_ColorSpace::IsNormal() const {
  return GetFamily() == Family::kDeviceGray ||
         GetFamily() == Family::kDeviceRGB ||
         GetFamily() == Family::kDeviceCMYK ||
         GetFamily() == Family::kCalGray || GetFamily() == Family::kCalRGB;
}

const CPDF_PatternCS* CPDF_ColorSpace::AsPatternCS() const {
  return nullptr;
}

const CPDF_IndexedCS* CPDF_ColorSpace::AsIndexedCS() const {
  return nullptr;
}

CPDF_ColorSpace::CPDF_ColorSpace(Family family) : family_(family) {}

CPDF_ColorSpace::~CPDF_ColorSpace() = default;

void CPDF_ColorSpace::SetComponentsForStockCS(uint32_t nComponents) {
  components_ = nComponents;
}

CPDF_CalGray::CPDF_CalGray() : CPDF_ColorSpace(Family::kCalGray) {}

CPDF_CalGray::~CPDF_CalGray() = default;

uint32_t CPDF_CalGray::v_Load(CPDF_Document* doc,
                              const CPDF_Array* pArray,
                              std::set<const CPDF_Object*>* pVisited) {
  RetainPtr<const CPDF_Dictionary> dict = pArray->GetDictAt(1);
  if (!dict) {
    return 0;
  }

  if (!GetWhitePoint(dict.Get(), white_point_)) {
    return 0;
  }

  GetBlackPoint(dict.Get(), black_point_);

  gamma_ = dict->GetFloatFor("Gamma");
  if (gamma_ == 0) {
    gamma_ = kDefaultGamma;
  }
  return 1;
}

std::optional<FX_RGB_STRUCT<float>> CPDF_CalGray::GetRGB(
    pdfium::span<const float> pBuf) const {
  const float gray = pBuf[0];
  return FX_RGB_STRUCT<float>{gray, gray, gray};
}

void CPDF_CalGray::TranslateImageLine(pdfium::span<uint8_t> dest_span,
                                      pdfium::span<const uint8_t> src_span,
                                      int pixels,
                                      int image_width,
                                      int image_height,
                                      bool bTransMask) const {
  CHECK(!bTransMask);  // Only applies to CMYK colorspaces.

  auto gray_span = src_span.first(static_cast<size_t>(pixels));
  auto rgb_span = fxcrt::reinterpret_span<FX_RGB_STRUCT<uint8_t>>(dest_span);
  for (auto [gray_ref, rgb_ref] : fxcrt::Zip(gray_span, rgb_span)) {
    // Compiler can not conclude that src/dest don't overlap.
    const uint8_t pix = gray_ref;
    rgb_ref.red = pix;
    rgb_ref.green = pix;
    rgb_ref.blue = pix;
  }
}

CPDF_CalRGB::CPDF_CalRGB() : CPDF_ColorSpace(Family::kCalRGB) {}

CPDF_CalRGB::~CPDF_CalRGB() = default;

uint32_t CPDF_CalRGB::v_Load(CPDF_Document* doc,
                             const CPDF_Array* pArray,
                             std::set<const CPDF_Object*>* pVisited) {
  RetainPtr<const CPDF_Dictionary> dict = pArray->GetDictAt(1);
  if (!dict) {
    return 0;
  }

  if (!GetWhitePoint(dict.Get(), white_point_)) {
    return 0;
  }

  GetBlackPoint(dict.Get(), black_point_);

  RetainPtr<const CPDF_Array> pGamma = dict->GetArrayFor("Gamma");
  if (pGamma) {
    auto& gamma = gamma_.emplace();
    for (size_t i = 0; i < std::size(gamma); ++i) {
      gamma[i] = pGamma->GetFloatAt(i);
    }
  }

  RetainPtr<const CPDF_Array> pMatrix = dict->GetArrayFor("Matrix");
  if (pMatrix) {
    auto& matrix = matrix_.emplace();
    for (size_t i = 0; i < std::size(matrix); ++i) {
      matrix[i] = pMatrix->GetFloatAt(i);
    }
  }
  return 3;
}

std::optional<FX_RGB_STRUCT<float>> CPDF_CalRGB::GetRGB(
    pdfium::span<const float> pBuf) const {
  float a = pBuf[0];
  float b = pBuf[1];
  float c = pBuf[2];
  if (gamma_.has_value()) {
    const auto& gamma = gamma_.value();
    a = powf(a, gamma[0]);
    b = powf(b, gamma[1]);
    c = powf(c, gamma[2]);
  }
  float x;
  float y;
  float z;
  if (matrix_.has_value()) {
    const auto& matrix = matrix_.value();
    x = matrix[0] * a + matrix[3] * b + matrix[6] * c;
    y = matrix[1] * a + matrix[4] * b + matrix[7] * c;
    z = matrix[2] * a + matrix[5] * b + matrix[8] * c;
  } else {
    x = a;
    y = b;
    z = c;
  }
  return XYZ_to_sRGB_WhitePoint(x, y, z, white_point_[0], white_point_[1],
                                white_point_[2]);
}

void CPDF_CalRGB::TranslateImageLine(pdfium::span<uint8_t> dest_span,
                                     pdfium::span<const uint8_t> src_span,
                                     int pixels,
                                     int image_width,
                                     int image_height,
                                     bool bTransMask) const {
  CHECK(!bTransMask);  // Only applies to CMYK colorspaces.
  fxcodec::ReverseRGB(dest_span, src_span, pixels);
}

CPDF_LabCS::CPDF_LabCS() : CPDF_ColorSpace(Family::kLab) {}

CPDF_LabCS::~CPDF_LabCS() = default;

void CPDF_LabCS::GetDefaultValue(int iComponent,
                                 float* value,
                                 float* min,
                                 float* max) const {
  DCHECK_LT(iComponent, 3);

  if (iComponent > 0) {
    float range_min = ranges_[iComponent * 2 - 2];
    float range_max = ranges_[iComponent * 2 - 1];
    if (range_min <= range_max) {
      *min = range_min;
      *max = range_max;
      *value = std::clamp(0.0f, *min, *max);
      return;
    }
  }

  *min = 0.0f;
  *max = 100.0f;
  *value = 0.0f;
}

uint32_t CPDF_LabCS::v_Load(CPDF_Document* doc,
                            const CPDF_Array* pArray,
                            std::set<const CPDF_Object*>* pVisited) {
  RetainPtr<const CPDF_Dictionary> dict = pArray->GetDictAt(1);
  if (!dict) {
    return 0;
  }

  if (!GetWhitePoint(dict.Get(), white_point_)) {
    return 0;
  }

  GetBlackPoint(dict.Get(), black_point_);

  RetainPtr<const CPDF_Array> pParam = dict->GetArrayFor("Range");
  static constexpr std::array<float, kRangesCount> kDefaultRanges = {
      {-100.0f, 100.0f, -100.0f, 100.0f}};
  for (size_t i = 0; i < std::size(kDefaultRanges); ++i) {
    ranges_[i] = pParam ? pParam->GetFloatAt(i) : kDefaultRanges[i];
  }
  return 3;
}

std::optional<FX_RGB_STRUCT<float>> CPDF_LabCS::GetRGB(
    pdfium::span<const float> pBuf) const {
  float Lstar = pBuf[0];
  float astar = pBuf[1];
  float bstar = pBuf[2];
  float M = (Lstar + 16.0f) / 116.0f;
  float L = M + astar / 500.0f;
  float N = M - bstar / 200.0f;
  float X;
  float Y;
  float Z;
  if (L < 0.2069f) {
    X = 0.957f * 0.12842f * (L - 0.1379f);
  } else {
    X = 0.957f * L * L * L;
  }

  if (M < 0.2069f) {
    Y = 0.12842f * (M - 0.1379f);
  } else {
    Y = M * M * M;
  }

  if (N < 0.2069f) {
    Z = 1.0889f * 0.12842f * (N - 0.1379f);
  } else {
    Z = 1.0889f * N * N * N;
  }

  return XYZ_to_sRGB(X, Y, Z);
}

void CPDF_LabCS::TranslateImageLine(pdfium::span<uint8_t> dest_span,
                                    pdfium::span<const uint8_t> src_span,
                                    int pixels,
                                    int image_width,
                                    int image_height,
                                    bool bTransMask) const {
  CHECK(!bTransMask);  // Only applies to CMYK colorspaces.

  auto bgr_span = fxcrt::reinterpret_span<FX_BGR_STRUCT<uint8_t>>(dest_span);
  auto lab_span =
      fxcrt::reinterpret_span<const FX_LAB_STRUCT<uint8_t>>(src_span).first(
          static_cast<size_t>(pixels));
  for (auto [lab_ref, bgr_ref] : fxcrt::Zip(lab_span, bgr_span)) {
    const float lab[3] = {
        static_cast<float>(lab_ref.lightness_star * 100) / 255.0f,
        static_cast<float>(lab_ref.a_star - 128),
        static_cast<float>(lab_ref.b_star - 128),
    };
    // Better code than the equivalent GetRGBOrZerosOnError() since that
    // is implemented in a base class and can't devirtualize the GetRGB()
    // call despite this class being marked final.
    auto rgb = GetRGB(lab).value_or(FX_RGB_STRUCT<float>{});
    bgr_ref.blue = static_cast<int32_t>(rgb.blue * 255);
    bgr_ref.green = static_cast<int32_t>(rgb.green * 255);
    bgr_ref.red = static_cast<int32_t>(rgb.red * 255);
  }
}

CPDF_ICCBasedCS::CPDF_ICCBasedCS() : CPDF_BasedCS(Family::kICCBased) {}

CPDF_ICCBasedCS::~CPDF_ICCBasedCS() = default;

uint32_t CPDF_ICCBasedCS::v_Load(CPDF_Document* doc,
                                 const CPDF_Array* pArray,
                                 std::set<const CPDF_Object*>* pVisited) {
  RetainPtr<const CPDF_Stream> pStream = pArray->GetStreamAt(1);
  if (!pStream) {
    return 0;
  }

  // The PDF 1.7 spec says the number of components must be valid. While some
  // PDF viewers tolerate invalid values, Acrobat does not, so be consistent
  // with Acrobat and reject bad values.
  RetainPtr<const CPDF_Dictionary> dict = pStream->GetDict();
  const int32_t nDictComponents = dict->GetIntegerFor("N");
  if (!fxcodec::IccTransform::IsValidIccComponents(nDictComponents)) {
    return 0;
  }

  // Safe to cast, as the value just got validated.
  const uint32_t nComponents = static_cast<uint32_t>(nDictComponents);
  profile_ = CPDF_DocPageData::FromDocument(doc)->GetIccProfile(pStream);
  if (!profile_) {
    return 0;
  }

  // If PDFium does not understand the ICC profile format at all, or if it's
  // SRGB, a profile PDFium recognizes but does not support well, then try the
  // alternate profile.
  if (!profile_->IsSupported() &&
      !FindAlternateProfile(doc, dict.Get(), pVisited, nComponents)) {
    // If there is no alternate profile, use a stock profile as mentioned in
    // the PDF 1.7 spec in table 4.16 in the "Alternate" key description.
    DCHECK(!base_cs_);
    base_cs_ = GetStockAlternateProfile(nComponents);
  }

  // TODO(crbug.com/pdfium/2136): Use this data to clamp color components.
  ranges_ = GetRanges(dict.Get(), nComponents);
  return nComponents;
}

std::optional<FX_RGB_STRUCT<float>> CPDF_ICCBasedCS::GetRGB(
    pdfium::span<const float> pBuf) const {
  if (profile_->IsSRGB()) {
    return FX_RGB_STRUCT<float>{pBuf[0], pBuf[1], pBuf[2]};
  }
  if (profile_->IsSupported()) {
    float rgb[3];
    profile_->Translate(pBuf.first(ComponentCount()), rgb);
    return FX_RGB_STRUCT<float>{rgb[0], rgb[1], rgb[2]};
  }
  if (base_cs_) {
    return base_cs_->GetRGB(pBuf);
  }
  return FX_RGB_STRUCT<float>{};
}

RetainPtr<CPDF_IccProfile> CPDF_ICCBasedCS::GetIccProfile() const {
  return profile_;
}

void CPDF_ICCBasedCS::TranslateImageLine(pdfium::span<uint8_t> dest_span,
                                         pdfium::span<const uint8_t> src_span,
                                         int pixels,
                                         int image_width,
                                         int image_height,
                                         bool bTransMask) const {
  CHECK(!bTransMask);  // Only applies to CMYK colorspaces.

  if (profile_->IsSRGB()) {
    fxcodec::ReverseRGB(dest_span, src_span, pixels);
    return;
  }
  if (!profile_->IsSupported()) {
    if (base_cs_) {
      base_cs_->TranslateImageLine(dest_span, src_span, pixels, image_width,
                                   image_height, false);
    }
    return;
  }

  // |nMaxColors| will not overflow since |nComponents| is limited in size.
  const uint32_t nComponents = ComponentCount();
  DCHECK(fxcodec::IccTransform::IsValidIccComponents(nComponents));
  int nMaxColors = 1;
  for (uint32_t i = 0; i < nComponents; i++) {
    nMaxColors *= 52;
  }

  bool bTranslate = nComponents > 3;
  if (!bTranslate) {
    FX_SAFE_INT32 nPixelCount = image_width;
    nPixelCount *= image_height;
    if (nPixelCount.IsValid()) {
      bTranslate = nPixelCount.ValueOrDie() < nMaxColors * 3 / 2;
    }
  }
  if (bTranslate && profile_->IsSupported()) {
    profile_->TranslateScanline(dest_span, src_span, pixels);
    return;
  }
  if (cache_.empty()) {
    cache_.resize(Fx2DSizeOrDie(nMaxColors, 3));
    DataVector<uint8_t> temp_src(Fx2DSizeOrDie(nMaxColors, nComponents));
    size_t src_index = 0;
    for (int i = 0; i < nMaxColors; i++) {
      uint32_t color = i;
      uint32_t order = nMaxColors / 52;
      for (uint32_t c = 0; c < nComponents; c++) {
        temp_src[src_index++] = static_cast<uint8_t>(color / order * 5);
        color %= order;
        order /= 52;
      }
    }
    if (profile_->IsSupported()) {
      profile_->TranslateScanline(cache_, temp_src, nMaxColors);
    }
  }
  uint8_t* pDestBuf = dest_span.data();
  const uint8_t* pSrcBuf = src_span.data();
  UNSAFE_TODO({
    switch(nComponents) {
    case 1:
      for (int i = 0; i < pixels; i++) {
        int index = g_ContribTable_layer_0[*pSrcBuf++];
        index *= 3;
        *pDestBuf++ = cache_[index];
        *pDestBuf++ = cache_[index + 1];
        *pDestBuf++ = cache_[index + 2];
      }
      break;
    case 3:
      for (int i = 0; i < pixels; i++) {
        int index = g_ContribTable_layer_0[*pSrcBuf++] +
                    g_ContribTable_layer_1[*pSrcBuf++] +
                    g_ContribTable_layer_2[*pSrcBuf++];
        index *= 3;
        *pDestBuf++ = cache_[index];
        *pDestBuf++ = cache_[index + 1];
        *pDestBuf++ = cache_[index + 2];
      }
      break;
    case 4:
      for (int i = 0; i < pixels; i++) {
        int index = g_ContribTable_layer_0[*pSrcBuf++] +
                    g_ContribTable_layer_1[*pSrcBuf++] +
                    g_ContribTable_layer_2[*pSrcBuf++] +
                    g_ContribTable_layer_3[*pSrcBuf++];
        index *= 3;
        *pDestBuf++ = cache_[index];
        *pDestBuf++ = cache_[index + 1];
        *pDestBuf++ = cache_[index + 2];
      }
      break;
    case 2:
      for (int i = 0; i < pixels; i++) {
        int index = g_ContribTable_layer_0[*pSrcBuf++] +
                    g_ContribTable_layer_1[*pSrcBuf++];
        index *= 3;
        *pDestBuf++ = cache_[index];
        *pDestBuf++ = cache_[index + 1];
        *pDestBuf++ = cache_[index + 2];
      }
      break;
    default:
      for (int i = 0; i < pixels; i++) {
        int index = 0;
        for (uint32_t c = 0; c < nComponents; c++) {
          index = index * 52 + (*pSrcBuf) / 5;
          pSrcBuf++;
        }
        index *= 3;
        *pDestBuf++ = cache_[index];
        *pDestBuf++ = cache_[index + 1];
        *pDestBuf++ = cache_[index + 2];
      }
      break;
    }
  });
}

bool CPDF_ICCBasedCS::IsNormal() const {
  if (profile_->IsSRGB()) {
    return true;
  }
  if (profile_->IsSupported()) {
    return profile_->IsNormal();
  }
  if (base_cs_) {
    return base_cs_->IsNormal();
  }
  return false;
}

bool CPDF_ICCBasedCS::FindAlternateProfile(
    CPDF_Document* doc,
    const CPDF_Dictionary* dict,
    std::set<const CPDF_Object*>* pVisited,
    uint32_t nExpectedComponents) {
  RetainPtr<const CPDF_Object> pAlterCSObj =
      dict->GetDirectObjectFor("Alternate");
  if (!pAlterCSObj) {
    return false;
  }

  auto pAlterCS = CPDF_ColorSpace::Load(doc, pAlterCSObj.Get(), pVisited);
  if (!pAlterCS) {
    return false;
  }

  if (pAlterCS->GetFamily() == Family::kPattern) {
    return false;
  }

  if (pAlterCS->ComponentCount() != nExpectedComponents) {
    return false;
  }

  base_cs_ = std::move(pAlterCS);
  return true;
}

// static
RetainPtr<CPDF_ColorSpace> CPDF_ICCBasedCS::GetStockAlternateProfile(
    uint32_t nComponents) {
  if (nComponents == 1) {
    return GetStockCS(Family::kDeviceGray);
  }
  if (nComponents == 3) {
    return GetStockCS(Family::kDeviceRGB);
  }
  if (nComponents == 4) {
    return GetStockCS(Family::kDeviceCMYK);
  }
  NOTREACHED();
}

// static
std::vector<float> CPDF_ICCBasedCS::GetRanges(const CPDF_Dictionary* dict,
                                              uint32_t nComponents) {
  DCHECK(fxcodec::IccTransform::IsValidIccComponents(nComponents));
  RetainPtr<const CPDF_Array> pRanges = dict->GetArrayFor("Range");
  if (pRanges && pRanges->size() >= nComponents * 2) {
    return ReadArrayElementsToVector(pRanges.Get(), nComponents * 2);
  }

  std::vector<float> ranges;
  ranges.reserve(nComponents * 2);
  for (uint32_t i = 0; i < nComponents; i++) {
    ranges.push_back(0.0f);
    ranges.push_back(1.0f);
  }
  return ranges;
}

CPDF_SeparationCS::CPDF_SeparationCS() : CPDF_BasedCS(Family::kSeparation) {}

CPDF_SeparationCS::~CPDF_SeparationCS() = default;

void CPDF_SeparationCS::GetDefaultValue(int iComponent,
                                        float* value,
                                        float* min,
                                        float* max) const {
  *value = 1.0f;
  *min = 0;
  *max = 1.0f;
}

uint32_t CPDF_SeparationCS::v_Load(CPDF_Document* doc,
                                   const CPDF_Array* pArray,
                                   std::set<const CPDF_Object*>* pVisited) {
  is_none_type_ = pArray->GetByteStringAt(1) == "None";
  if (is_none_type_) {
    return 1;
  }

  RetainPtr<const CPDF_Object> pAltArray = pArray->GetDirectObjectAt(2);
  if (HasSameArray(pAltArray.Get())) {
    return 0;
  }

  base_cs_ = Load(doc, pAltArray.Get(), pVisited);
  if (!base_cs_) {
    return 0;
  }

  if (base_cs_->IsSpecial()) {
    return 0;
  }

  RetainPtr<const CPDF_Object> pFuncObj = pArray->GetDirectObjectAt(3);
  if (pFuncObj && !pFuncObj->IsName()) {
    auto pFunc = CPDF_Function::Load(std::move(pFuncObj));
    if (pFunc && pFunc->OutputCount() >= base_cs_->ComponentCount()) {
      func_ = std::move(pFunc);
    }
  }
  return 1;
}

std::optional<FX_RGB_STRUCT<float>> CPDF_SeparationCS::GetRGB(
    pdfium::span<const float> pBuf) const {
  if (is_none_type_) {
    return std::nullopt;
  }
  if (!func_) {
    if (!base_cs_) {
      return std::nullopt;
    }
    std::vector<float> results(base_cs_->ComponentCount(), pBuf[0]);
    return base_cs_->GetRGB(results);
  }

  // Using at least 16 elements due to the call alt_cs_->GetRGB() below.
  std::vector<float> results(std::max(func_->OutputCount(), 16u));
  uint32_t nresults = func_->Call(pBuf.first<1u>(), results).value_or(0);
  if (nresults == 0) {
    return std::nullopt;
  }
  if (base_cs_) {
    return base_cs_->GetRGB(results);
  }
  return std::nullopt;
}

CPDF_DeviceNCS::CPDF_DeviceNCS() : CPDF_BasedCS(Family::kDeviceN) {}

CPDF_DeviceNCS::~CPDF_DeviceNCS() = default;

void CPDF_DeviceNCS::GetDefaultValue(int iComponent,
                                     float* value,
                                     float* min,
                                     float* max) const {
  *value = 1.0f;
  *min = 0;
  *max = 1.0f;
}

uint32_t CPDF_DeviceNCS::v_Load(CPDF_Document* doc,
                                const CPDF_Array* pArray,
                                std::set<const CPDF_Object*>* pVisited) {
  RetainPtr<const CPDF_Array> pObj = ToArray(pArray->GetDirectObjectAt(1));
  if (!pObj) {
    return 0;
  }

  RetainPtr<const CPDF_Object> pAltCS = pArray->GetDirectObjectAt(2);
  if (!pAltCS || HasSameArray(pAltCS.Get())) {
    return 0;
  }

  base_cs_ = Load(doc, pAltCS.Get(), pVisited);
  func_ = CPDF_Function::Load(pArray->GetDirectObjectAt(3));
  if (!base_cs_ || !func_) {
    return 0;
  }

  if (base_cs_->IsSpecial()) {
    return 0;
  }

  if (func_->OutputCount() < base_cs_->ComponentCount()) {
    return 0;
  }

  return fxcrt::CollectionSize<uint32_t>(*pObj);
}

std::optional<FX_RGB_STRUCT<float>> CPDF_DeviceNCS::GetRGB(
    pdfium::span<const float> pBuf) const {
  if (!func_) {
    return std::nullopt;
  }
  // Using at least 16 elements due to the call alt_cs_->GetRGB() below.
  std::vector<float> results(std::max(func_->OutputCount(), 16u));
  uint32_t nresults =
      func_->Call(pBuf.first(ComponentCount()), results).value_or(0);

  if (nresults == 0) {
    return std::nullopt;
  }
  return base_cs_->GetRGB(results);
}
