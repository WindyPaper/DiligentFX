"// UnwarpEpipolarScattering.fx\n"
"// Transforms scattering and extinction from epipolar space to camera space and combines with the\n"
"// back buffer\n"
"\n"
"#include \"BasicStructures.fxh\"\n"
"#include \"AtmosphereShadersCommon.fxh\"\n"
"\n"
"cbuffer cbParticipatingMediaScatteringParams\n"
"{\n"
"    AirScatteringAttribs g_MediaParams;\n"
"}\n"
"\n"
"cbuffer cbLightParams\n"
"{\n"
"    LightAttribs g_LightAttribs;\n"
"}\n"
"\n"
"cbuffer cbPostProcessingAttribs\n"
"{\n"
"    EpipolarLightScatteringAttribs g_PPAttribs;\n"
"};\n"
"\n"
"cbuffer cbCameraAttribs\n"
"{\n"
"    CameraAttribs g_CameraAttribs;\n"
"}\n"
"\n"
"Texture2D<float4> g_tex2DSliceEndPoints;\n"
"SamplerState      g_tex2DSliceEndPoints_sampler; // Linear clamp\n"
"\n"
"Texture2D<float>  g_tex2DEpipolarCamSpaceZ;\n"
"SamplerState      g_tex2DEpipolarCamSpaceZ_sampler; // Linear clamp\n"
"\n"
"Texture2D<float3> g_tex2DScatteredColor;\n"
"SamplerState      g_tex2DScatteredColor_sampler; // Linear clamp\n"
"\n"
"Texture2D<float>  g_tex2DCamSpaceZ;\n"
"SamplerState      g_tex2DCamSpaceZ_sampler; // Linear clamp\n"
"\n"
"Texture2D<float4> g_tex2DColorBuffer;\n"
"SamplerState      g_tex2DColorBuffer_sampler; // Point clamp\n"
"\n"
"Texture2D<float4> g_tex2DSliceUVDirAndOrigin;\n"
"\n"
"Texture2D<float2> g_tex2DMinMaxLightSpaceDepth;\n"
"\n"
"Texture2DArray<float>  g_tex2DLightSpaceDepthMap;\n"
"SamplerComparisonState g_tex2DLightSpaceDepthMap_sampler;\n"
"\n"
"Texture2D<float>  g_tex2DAverageLuminance;\n"
"\n"
"#if EXTINCTION_EVAL_MODE == EXTINCTION_EVAL_MODE_EPIPOLAR\n"
"    Texture2D<float3> g_tex2DEpipolarExtinction;\n"
"    SamplerState      g_tex2DEpipolarExtinction_sampler; // Linear clamp\n"
"#endif\n"
"\n"
"#include \"Extinction.fxh\"\n"
"#include \"ToneMapping.fxh\"\n"
"\n"
"void UnwarpEpipolarInsctrImage( in float2 f2PosPS, \n"
"                                in float fCamSpaceZ,\n"
"                                out float3 f3Inscattering,\n"
"                                out float3 f3Extinction )\n"
"{\n"
"    // Compute direction of the ray going from the light through the pixel\n"
"    float2 f2RayDir = normalize( f2PosPS - g_PPAttribs.f4LightScreenPos.xy );\n"
"\n"
"    // Find, which boundary the ray intersects. For this, we will \n"
"    // find which two of four half spaces the f2RayDir belongs to\n"
"    // Each of four half spaces is produced by the line connecting one of four\n"
"    // screen corners and the current pixel:\n"
"    //    ________________        _______\'________           ________________           \n"
"    //   |\'            . \'|      |      \'         |         |                |          \n"
"    //   | \'       . \'    |      |     \'          |      .  |                |          \n"
"    //   |  \'  . \'        |      |    \'           |        \'|.        hs1    |          \n"
"    //   |   *.           |      |   *     hs0    |         |  \'*.           |          \n"
"    //   |  \'   \' .       |      |  \'             |         |      \' .       |          \n"
"    //   | \'        \' .   |      | \'              |         |          \' .   |          \n"
"    //   |\'____________ \'_|      |\'_______________|         | ____________ \'_.          \n"
"    //                           \'                                             \'\n"
"    //                           ________________  .        \'________________  \n"
"    //                           |             . \'|         |\'               | \n"
"    //                           |   hs2   . \'    |         | \'              | \n"
"    //                           |     . \'        |         |  \'             | \n"
"    //                           | . *            |         |   *            | \n"
"    //                         . \'                |         |    \'           | \n"
"    //                           |                |         | hs3 \'          | \n"
"    //                           |________________|         |______\'_________| \n"
"    //                                                              \'\n"
"    // The equations for the half spaces are the following:\n"
"    //bool hs0 = (f2PosPS.x - (-1)) * f2RayDir.y < f2RayDir.x * (f2PosPS.y - (-1));\n"
"    //bool hs1 = (f2PosPS.x -  (1)) * f2RayDir.y < f2RayDir.x * (f2PosPS.y - (-1));\n"
"    //bool hs2 = (f2PosPS.x -  (1)) * f2RayDir.y < f2RayDir.x * (f2PosPS.y -  (1));\n"
"    //bool hs3 = (f2PosPS.x - (-1)) * f2RayDir.y < f2RayDir.x * (f2PosPS.y -  (1));\n"
"    // Note that in fact the outermost visible screen pixels do not lie exactly on the boundary (+1 or -1), but are biased by\n"
"    // 0.5 screen pixel size inwards. Using these adjusted boundaries improves precision and results in\n"
"    // smaller number of pixels which require inscattering correction\n"
"    float4 f4Boundaries = GetOutermostScreenPixelCoords(g_PPAttribs.f4ScreenResolution);//left, bottom, right, top\n"
"    float4 f4HalfSpaceEquationTerms = (f2PosPS.xxyy - f4Boundaries.xzyw/*float4(-1,1,-1,1)*/) * f2RayDir.yyxx;\n"
"    bool4 b4HalfSpaceFlags = Less( f4HalfSpaceEquationTerms.xyyx, f4HalfSpaceEquationTerms.zzww );\n"
"\n"
"    // Now compute mask indicating which of four sectors the f2RayDir belongs to and consiquently\n"
"    // which border the ray intersects:\n"
"    //    ________________ \n"
"    //   |\'            . \'|         0 : hs3 && !hs0\n"
"    //   | \'   3   . \'    |         1 : hs0 && !hs1\n"
"    //   |  \'  . \'        |         2 : hs1 && !hs2\n"
"    //   |0  *.       2   |         3 : hs2 && !hs3\n"
"    //   |  \'   \' .       |\n"
"    //   | \'   1    \' .   |\n"
"    //   |\'____________ \'_|\n"
"    //\n"
"    bool4 b4SectorFlags = And( b4HalfSpaceFlags.wxyz, Not(b4HalfSpaceFlags.xyzw) );\n"
"    // Note that b4SectorFlags now contains true (1) for the exit boundary and false (0) for 3 other\n"
"\n"
"    // Compute distances to boundaries according to following lines:\n"
"    //float fDistToLeftBoundary   = abs(f2RayDir.x) > 1e-5 ? ( -1 - g_PPAttribs.f4LightScreenPos.x) / f2RayDir.x : -FLT_MAX;\n"
"    //float fDistToBottomBoundary = abs(f2RayDir.y) > 1e-5 ? ( -1 - g_PPAttribs.f4LightScreenPos.y) / f2RayDir.y : -FLT_MAX;\n"
"    //float fDistToRightBoundary  = abs(f2RayDir.x) > 1e-5 ? (  1 - g_PPAttribs.f4LightScreenPos.x) / f2RayDir.x : -FLT_MAX;\n"
"    //float fDistToTopBoundary    = abs(f2RayDir.y) > 1e-5 ? (  1 - g_PPAttribs.f4LightScreenPos.y) / f2RayDir.y : -FLT_MAX;\n"
"    float4 f4DistToBoundaries = ( f4Boundaries - g_PPAttribs.f4LightScreenPos.xyxy ) / (f2RayDir.xyxy + BoolToFloat( Less( abs(f2RayDir.xyxy), 1e-6*F4ONE) ) );\n"
"    // Select distance to the exit boundary:\n"
"    float fDistToExitBoundary = dot( BoolToFloat( b4SectorFlags ), f4DistToBoundaries );\n"
"    // Compute exit point on the boundary:\n"
"    float2 f2ExitPoint = g_PPAttribs.f4LightScreenPos.xy + f2RayDir * fDistToExitBoundary;\n"
"\n"
"    // Compute epipolar slice for each boundary:\n"
"    //if( LeftBoundary )\n"
"    //    fEpipolarSlice = 0.0  - (LeftBoudaryIntersecPoint.y   -   1 )/2 /4;\n"
"    //else if( BottomBoundary )\n"
"    //    fEpipolarSlice = 0.25 + (BottomBoudaryIntersecPoint.x - (-1))/2 /4;\n"
"    //else if( RightBoundary )\n"
"    //    fEpipolarSlice = 0.5  + (RightBoudaryIntersecPoint.y  - (-1))/2 /4;\n"
"    //else if( TopBoundary )\n"
"    //    fEpipolarSlice = 0.75 - (TopBoudaryIntersecPoint.x      - 1 )/2 /4;\n"
"    float4 f4EpipolarSlice = float4(0, 0.25, 0.5, 0.75) + \n"
"        saturate( (f2ExitPoint.yxyx - f4Boundaries.wxyz)*float4(-1.0, +1.0, +1.0, -1.0) / (f4Boundaries.wzwz - f4Boundaries.yxyx) ) / 4.0;\n"
"    // Select the right value:\n"
"    float fEpipolarSlice = dot( BoolToFloat(b4SectorFlags), f4EpipolarSlice);\n"
"\n"
"    // Now find two closest epipolar slices, from which we will interpolate\n"
"    // First, find index of the slice which precedes our slice\n"
"    // Note that 0 <= fEpipolarSlice <= 1, and both 0 and 1 refer to the first slice\n"
"    float fPrecedingSliceInd = min( floor(fEpipolarSlice*float(NUM_EPIPOLAR_SLICES)), float(NUM_EPIPOLAR_SLICES-1) );\n"
"\n"
"    // Compute EXACT texture coordinates of preceding and succeeding slices and their weights\n"
"    // Note that slice 0 is stored in the first texel which has exact texture coordinate 0.5/NUM_EPIPOLAR_SLICES\n"
"    // (search for \"fEpipolarSlice = saturate(f2UV.x - 0.5f / (float)NUM_EPIPOLAR_SLICES)\"):\n"
"    float fSrcSliceV[2];\n"
"    // Compute V coordinate to refer exactly the center of the slice row\n"
"    fSrcSliceV[0] = fPrecedingSliceInd/float(NUM_EPIPOLAR_SLICES) + 0.5/float(NUM_EPIPOLAR_SLICES);\n"
"    // Use frac() to wrap around to the first slice from the next-to-last slice:\n"
"    fSrcSliceV[1] = frac( fSrcSliceV[0] + 1.0/float(NUM_EPIPOLAR_SLICES) );\n"
"        \n"
"    // Compute slice weights\n"
"    float fSliceWeights[2];\n"
"    fSliceWeights[1] = (fEpipolarSlice*float(NUM_EPIPOLAR_SLICES)) - fPrecedingSliceInd;\n"
"    fSliceWeights[0] = 1.0 - fSliceWeights[1];\n"
"\n"
"    f3Inscattering = F3ZERO;\n"
"    f3Extinction = F3ZERO;\n"
"    float fTotalWeight = 0.0;\n"
"    [unroll]\n"
"    for(int i=0; i<2; ++i)\n"
"    {\n"
"        // Load epipolar line endpoints\n"
"        float4 f4SliceEndpoints = g_tex2DSliceEndPoints.SampleLevel( g_tex2DSliceEndPoints_sampler, float2(fSrcSliceV[i], 0.5), 0 );\n"
"\n"
"        // Compute line direction on the screen\n"
"        float2 f2SliceDir = f4SliceEndpoints.zw - f4SliceEndpoints.xy;\n"
"        float fSliceLenSqr = dot(f2SliceDir, f2SliceDir);\n"
"        \n"
"        // Project current pixel onto the epipolar line\n"
"        float fSamplePosOnLine = dot((f2PosPS - f4SliceEndpoints.xy), f2SliceDir) / max(fSliceLenSqr, 1e-8);\n"
"        // Compute index of the slice on the line\n"
"        // Note that the first sample on the line (fSamplePosOnLine==0) is exactly the Entry Point, while \n"
"        // the last sample (fSamplePosOnLine==1) is exactly the Exit Point\n"
"        // (search for \"fSamplePosOnEpipolarLine *= (float)MAX_SAMPLES_IN_SLICE / ((float)MAX_SAMPLES_IN_SLICE-1.f)\")\n"
"        float fSampleInd = fSamplePosOnLine * float(MAX_SAMPLES_IN_SLICE-1);\n"
"       \n"
"        // We have to manually perform bilateral filtering of the scattered radiance texture to\n"
"        // eliminate artifacts at depth discontinuities\n"
"\n"
"        float fPrecedingSampleInd = floor(fSampleInd);\n"
"        // Get bilinear filtering weight\n"
"        float fUWeight = fSampleInd - fPrecedingSampleInd;\n"
"        // Get texture coordinate of the left source texel. Again, offset by 0.5 is essential\n"
"        // to align with the texel center\n"
"        float fPrecedingSampleU = (fPrecedingSampleInd + 0.5) / float(MAX_SAMPLES_IN_SLICE);\n"
"    \n"
"        float2 f2SctrColorUV = float2(fPrecedingSampleU, fSrcSliceV[i]);\n"
"\n"
"        // Gather 4 camera space z values\n"
"        // Note that we need to bias f2SctrColorUV by 0.5 texel size to refer the location between all four texels and\n"
"        // get the required values for sure\n"
"        // The values in float4, which Gather() returns are arranged as follows:\n"
"        //   _______ _______\n"
"        //  |       |       |\n"
"        //  |   x   |   y   |\n"
"        //  |_______o_______|  o gather location\n"
"        //  |       |       |\n"
"        //  |   *w  |   z   |  * f2SctrColorUV\n"
"        //  |_______|_______|\n"
"        //  |<----->|\n"
"        //     1/f2ScatteredColorTexDim.x\n"
"        \n"
"        // x == g_tex2DEpipolarCamSpaceZ.SampleLevel(samPointClamp, f2SctrColorUV, 0, int2(0,1))\n"
"        // y == g_tex2DEpipolarCamSpaceZ.SampleLevel(samPointClamp, f2SctrColorUV, 0, int2(1,1))\n"
"        // z == g_tex2DEpipolarCamSpaceZ.SampleLevel(samPointClamp, f2SctrColorUV, 0, int2(1,0))\n"
"        // w == g_tex2DEpipolarCamSpaceZ.SampleLevel(samPointClamp, f2SctrColorUV, 0, int2(0,0))\n"
"\n"
"        const float2 f2ScatteredColorTexDim = float2(MAX_SAMPLES_IN_SLICE, NUM_EPIPOLAR_SLICES);\n"
"        float2 f2SrcLocationsCamSpaceZ = g_tex2DEpipolarCamSpaceZ.Gather(g_tex2DEpipolarCamSpaceZ_sampler, f2SctrColorUV + float2(0.5, 0.5) / f2ScatteredColorTexDim.xy).wz;\n"
"        \n"
"        // Compute depth weights in a way that if the difference is less than the threshold, the weight is 1 and\n"
"        // the weights fade out to 0 as the difference becomes larger than the threshold:\n"
"        float2 f2MaxZ = max( f2SrcLocationsCamSpaceZ, max(fCamSpaceZ,1.0) );\n"
"        float2 f2DepthWeights = saturate( g_PPAttribs.fRefinementThreshold / max( abs(fCamSpaceZ-f2SrcLocationsCamSpaceZ)/f2MaxZ, g_PPAttribs.fRefinementThreshold ) );\n"
"        // Note that if the sample is located outside the [-1,1]x[-1,1] area, the sample is invalid and fCurrCamSpaceZ == fInvalidCoordinate\n"
"        // Depth weight computed for such sample will be zero\n"
"        f2DepthWeights = pow(f2DepthWeights, 4.0*F2ONE);\n"
"\n"
"        // Multiply bilinear weights with the depth weights:\n"
"        float2 f2BilateralUWeights = float2(1.0-fUWeight, fUWeight) * f2DepthWeights * fSliceWeights[i];\n"
"        // If the sample projection is behind [0,1], we have to discard this slice\n"
"        // We however must take into account the fact that if at least one sample from the two \n"
"        // bilinear sources is correct, the sample can still be properly computed\n"
"        //        \n"
"        //            -1       0       1                  N-2     N-1      N              Sample index\n"
"        // |   X   |   X   |   X   |   X   |  ......   |   X   |   X   |   X   |   X   |\n"
"        //         1-1/(N-1)   0    1/(N-1)                        1   1+1/(N-1)          fSamplePosOnLine   \n"
"        //             |                                                   |\n"
"        //             |<-------------------Clamp range------------------->|                   \n"
"        //\n"
"        f2BilateralUWeights *= (abs(fSamplePosOnLine - 0.5) < 0.5 + 1.0 / float(MAX_SAMPLES_IN_SLICE-1)) ? 1.0 : 0.0;\n"
"        // We now need to compute the following weighted summ:\n"
"        //f3FilteredSliceCol = \n"
"        //    f2BilateralUWeights.x * g_tex2DScatteredColor.SampleLevel(samPoint, f2SctrColorUV, 0, int2(0,0)) +\n"
"        //    f2BilateralUWeights.y * g_tex2DScatteredColor.SampleLevel(samPoint, f2SctrColorUV, 0, int2(1,0));\n"
"\n"
"        // We will use hardware to perform bilinear filtering and get this value using single bilinear fetch:\n"
"\n"
"        // Offset:                  (x=1,y=0)                (x=1,y=0)               (x=0,y=0)\n"
"        float fSubpixelUOffset = f2BilateralUWeights.y / max(f2BilateralUWeights.x + f2BilateralUWeights.y, 0.001);\n"
"        fSubpixelUOffset /= f2ScatteredColorTexDim.x;\n"
"        \n"
"        float3 f3FilteredSliceInsctr = \n"
"            (f2BilateralUWeights.x + f2BilateralUWeights.y) * \n"
"                g_tex2DScatteredColor.SampleLevel(g_tex2DScatteredColor_sampler, f2SctrColorUV + float2(fSubpixelUOffset, 0), 0);\n"
"        f3Inscattering += f3FilteredSliceInsctr;\n"
"\n"
"#if EXTINCTION_EVAL_MODE == EXTINCTION_EVAL_MODE_EPIPOLAR\n"
"        float3 f3FilteredSliceExtinction = \n"
"            (f2BilateralUWeights.x + f2BilateralUWeights.y) * \n"
"                g_tex2DEpipolarExtinction.SampleLevel(g_tex2DEpipolarExtinction_sampler, f2SctrColorUV + float2(fSubpixelUOffset, 0), 0);\n"
"        f3Extinction += f3FilteredSliceExtinction;\n"
"#endif\n"
"\n"
"        // Update total weight\n"
"        fTotalWeight += dot(f2BilateralUWeights, F2ONE);\n"
"    }\n"
"\n"
"#if CORRECT_INSCATTERING_AT_DEPTH_BREAKS\n"
"    if( fTotalWeight < 1e-2 )\n"
"    {\n"
"        // Discarded pixels will keep 0 value in stencil and will be later\n"
"        // processed to correct scattering\n"
"        discard;\n"
"    }\n"
"#endif\n"
"    \n"
"    f3Inscattering /= fTotalWeight;\n"
"    f3Extinction /= fTotalWeight;\n"
"}\n"
"\n"
"\n"
"void ApplyInscatteredRadiancePS(FullScreenTriangleVSOutput VSOut,\n"
"                                // IMPORTANT: non-system generated pixel shader input\n"
"                                // arguments must have the exact same name as vertex shader \n"
"                                // outputs and must go in the same order.\n"
"                                // Moreover, even if the shader is not using the argument,\n"
"                                // it still must be declared.\n"
"\n"
"                                out float4 f4Color : SV_Target)\n"
"{\n"
"    float2 f2UV = NormalizedDeviceXYToTexUV(VSOut.f2NormalizedXY);\n"
"    float fCamSpaceZ = g_tex2DCamSpaceZ.SampleLevel(g_tex2DCamSpaceZ_sampler, f2UV, 0);\n"
"    \n"
"    float3 f3Inscttering, f3Extinction;\n"
"    UnwarpEpipolarInsctrImage(VSOut.f2NormalizedXY, fCamSpaceZ, f3Inscttering, f3Extinction);\n"
"\n"
"    float3 f3BackgroundColor = F3ZERO;\n"
"    [branch]\n"
"    if( !g_PPAttribs.bShowLightingOnly )\n"
"    {\n"
"        f3BackgroundColor = g_tex2DColorBuffer.SampleLevel( g_tex2DColorBuffer_sampler, f2UV, 0).rgb;\n"
"        // fFarPlaneZ is pre-multiplied with 0.999999f\n"
"        f3BackgroundColor *= (fCamSpaceZ > g_CameraAttribs.fFarPlaneZ) ? g_LightAttribs.f4Intensity.rgb : F3ONE;\n"
"\n"
"#if EXTINCTION_EVAL_MODE == EXTINCTION_EVAL_MODE_PER_PIXEL\n"
"        float3 f3ReconstructedPosWS = ProjSpaceXYZToWorldSpace(float3(VSOut.f2NormalizedXY.xy, fCamSpaceZ), g_CameraAttribs.mProj, g_CameraAttribs.mViewProjInv);\n"
"        f3Extinction = GetExtinction(g_CameraAttribs.f4Position.xyz, f3ReconstructedPosWS);\n"
"#endif\n"
"        f3BackgroundColor *= f3Extinction;\n"
"    }\n"
"\n"
"#if PERFORM_TONE_MAPPING\n"
"    float fAveLogLum = GetAverageSceneLuminance(g_tex2DAverageLuminance);\n"
"    f4Color.rgb = ToneMap(f3BackgroundColor + f3Inscttering, g_PPAttribs.ToneMapping, fAveLogLum);\n"
"#else\n"
"    const float DELTA = 0.00001;\n"
"    f4Color.rgb = log( max(DELTA, dot(f3BackgroundColor + f3Inscttering, RGB_TO_LUMINANCE)) ) * F3ONE;\n"
"#endif\n"
"    f4Color.a = 1.0;\n"
"}\n"
