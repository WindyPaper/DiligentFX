"#ifndef _BASIC_STRUCTURES_FXH_\n"
"#define _BASIC_STRUCTURES_FXH_\n"
"\n"
"\n"
"#ifdef __cplusplus\n"
"\n"
"#   ifndef BOOL\n"
"#      define BOOL int32_t // Do not use bool, because sizeof(bool)==1 !\n"
"#   endif\n"
"\n"
"#   ifndef CHECK_STRUCT_ALIGNMENT\n"
"#       define CHECK_STRUCT_ALIGNMENT(s) static_assert( sizeof(s) % 16 == 0, \"sizeof(\" #s \") is not multiple of 16\" );\n"
"#   endif\n"
"\n"
"#else\n"
"\n"
"#   ifndef BOOL\n"
"#       define BOOL bool\n"
"#   endif\n"
"\n"
"#   ifndef CHECK_STRUCT_ALIGNMENT\n"
"#       define CHECK_STRUCT_ALIGNMENT(s)\n"
"#   endif\n"
"\n"
"#endif\n"
"\n"
"\n"
"struct CascadeAttribs\n"
"{\n"
"	float4 f4LightSpaceScale;\n"
"	float4 f4LightSpaceScaledBias;\n"
"    float4 f4StartEndZ;\n"
"};\n"
"CHECK_STRUCT_ALIGNMENT(CascadeAttribs)\n"
"\n"
"\n"
"#define MAX_CASCADES 8\n"
"struct ShadowMapAttribs\n"
"{\n"
"    // 0\n"
"#ifdef __cplusplus\n"
"    float4x4 mWorldToLightViewT; // Matrices in HLSL are COLUMN-major while float4x4 is ROW major\n"
"#else\n"
"    matrix mWorldToLightView;  // Transform from view space to light projection space\n"
"#endif\n"
"    // 16\n"
"    CascadeAttribs Cascades[MAX_CASCADES];\n"
"\n"
"#ifdef __cplusplus\n"
"    float fCascadeCamSpaceZEnd[MAX_CASCADES];\n"
"    float4x4 mWorldToShadowMapUVDepthT[MAX_CASCADES];\n"
"#else\n"
"	float4 f4CascadeCamSpaceZEnd[MAX_CASCADES/4];\n"
"    matrix mWorldToShadowMapUVDepth[MAX_CASCADES];\n"
"#endif\n"
"\n"
"    // Do not use bool, because sizeof(bool)==1 !\n"
"	BOOL bVisualizeCascades;\n"
"\n"
"    // float3 f3Padding;\n"
"    // OpenGL compiler does not handle 3-component vectors properly\n"
"    // and screws up the structure layout.\n"
"    // Opengl.org suggests not using vec3 at all\n"
"    int Padding0;\n"
"    int Padding1;\n"
"    int Padding2;\n"
"};\n"
"CHECK_STRUCT_ALIGNMENT(ShadowMapAttribs)\n"
"\n"
"\n"
"struct LightAttribs\n"
"{\n"
"    float4 f4DirOnLight;\n"
"    float4 f4AmbientLight;\n"
"    float4 f4ExtraterrestrialSunColor;\n"
"\n"
"    ShadowMapAttribs ShadowAttribs;\n"
"};\n"
"CHECK_STRUCT_ALIGNMENT(LightAttribs)\n"
"\n"
"\n"
"struct CameraAttribs\n"
"{\n"
"    float4 f4CameraPos;            ///< Camera world position\n"
"    float fNearPlaneZ; \n"
"    float fFarPlaneZ; // fNearPlaneZ < fFarPlaneZ\n"
"    float2 f2Dummy;\n"
"\n"
"#ifdef __cplusplus\n"
"    float4x4 mViewProjT;\n"
"    //float4x4 mViewT;\n"
"    float4x4 mProjT;\n"
"    float4x4 mViewProjInvT;\n"
"#else\n"
"    matrix mViewProj;\n"
"    //matrix mView;\n"
"    matrix mProj;\n"
"    matrix mViewProjInv;\n"
"#endif\n"
"};\n"
"CHECK_STRUCT_ALIGNMENT(CameraAttribs)\n"
"\n"
"\n"
"#endif //_BASIC_STRUCTURES_FXH_\n"
