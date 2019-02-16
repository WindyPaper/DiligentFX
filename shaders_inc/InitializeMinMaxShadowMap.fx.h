"#include \"AtmosphereShadersCommon.fxh\"\n"
"\n"
"Texture2D<float4> g_tex2DSliceUVDirAndOrigin;\n"
"\n"
"Texture2DArray<float> g_tex2DLightSpaceDepthMap;\n"
"SamplerState          g_tex2DLightSpaceDepthMap_sampler;\n"
"\n"
"cbuffer cbPostProcessingAttribs\n"
"{\n"
"    EpipolarLightScatteringAttribs g_PPAttribs;\n"
"};\n"
"\n"
"#if !USE_COMBINED_MIN_MAX_TEXTURE\n"
"cbuffer cbMiscDynamicParams\n"
"{\n"
"    MiscDynamicParams g_MiscParams;\n"
"}\n"
"#endif\n"
"\n"
"// Note that min/max shadow map does not contain finest resolution level\n"
"// The first level it contains corresponds to step == 2\n"
"void InitializeMinMaxShadowMapPS(in FullScreenTriangleVSOutput VSOut,\n"
"                                 out float2 f2MinMaxDepth : SV_Target)\n"
"{\n"
"    uint uiSliceInd;\n"
"    float fCascadeInd;\n"
"#if USE_COMBINED_MIN_MAX_TEXTURE\n"
"    fCascadeInd = floor(VSOut.f4PixelPos.y / float(NUM_EPIPOLAR_SLICES));\n"
"    uiSliceInd = uint(VSOut.f4PixelPos.y - fCascadeInd * float(NUM_EPIPOLAR_SLICES));\n"
"    fCascadeInd += g_PPAttribs.fFirstCascadeToRayMarch;\n"
"#else\n"
"    uiSliceInd = uint(VSOut.f4PixelPos.y);\n"
"    fCascadeInd = g_MiscParams.fCascadeInd;\n"
"#endif\n"
"    // Load slice direction in shadow map\n"
"    float4 f4SliceUVDirAndOrigin = g_tex2DSliceUVDirAndOrigin.Load( uint3(uiSliceInd, fCascadeInd, 0) );\n"
"    // Calculate current sample position on the ray\n"
"    float2 f2CurrUV = f4SliceUVDirAndOrigin.zw + f4SliceUVDirAndOrigin.xy * floor(VSOut.f4PixelPos.x) * 2.f;\n"
"    \n"
"    float4 f4MinDepth = F4ONE;\n"
"    float4 f4MaxDepth = F4ZERO;\n"
"    // Gather 8 depths which will be used for PCF filtering for this sample and its immediate neighbor \n"
"    // along the epipolar slice\n"
"    // Note that if the sample is located outside the shadow map, Gather() will return 0 as \n"
"    // specified by the samLinearBorder0. As a result volumes outside the shadow map will always be lit\n"
"    for( float i=0.0; i<=1.0; ++i )\n"
"    {\n"
"        float4 f4Depths = g_tex2DLightSpaceDepthMap.Gather(g_tex2DLightSpaceDepthMap_sampler, float3(f2CurrUV + i * f4SliceUVDirAndOrigin.xy, fCascadeInd) );\n"
"        f4MinDepth = min(f4MinDepth, f4Depths);\n"
"        f4MaxDepth = max(f4MaxDepth, f4Depths);\n"
"    }\n"
"\n"
"    f4MinDepth.xy = min(f4MinDepth.xy, f4MinDepth.zw);\n"
"    f4MinDepth.x = min(f4MinDepth.x, f4MinDepth.y);\n"
"\n"
"    f4MaxDepth.xy = max(f4MaxDepth.xy, f4MaxDepth.zw);\n"
"    f4MaxDepth.x = max(f4MaxDepth.x, f4MaxDepth.y);\n"
"#if !IS_32BIT_MIN_MAX_MAP\n"
"    const float R16_UNORM_PRECISION = 1.0 / float(1<<16);\n"
"    f4MinDepth.x = floor(f4MinDepth.x/R16_UNORM_PRECISION)*R16_UNORM_PRECISION;\n"
"    f4MaxDepth.x =  ceil(f4MaxDepth.x/R16_UNORM_PRECISION)*R16_UNORM_PRECISION;\n"
"#endif\n"
"    f2MinMaxDepth = float2(f4MinDepth.x, f4MaxDepth.x);\n"
"}\n"
