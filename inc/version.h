#pragma once

#define VER_MAJOR    1
#define VER_MINOR    0
#define VER_RELEASE  0
#define VER_BUILD    1
//#define VER_BRANCH   0

#ifdef linux
#ifdef __cplusplus
extern "C"
{
#endif 

#ifdef VER_BRANCH

#define __GENERIC_VERION(Major, Minor, Release, Build, Branch)\
VMF_API VMF_VOID VERSION_quality_metric_##Major##_##Minor##_##Release##_##Build##_##Branch(){ }
#define GENERIC_VERION(Major, Minor, Release, Build, Branch) __GENERIC_VERION(Major, Minor, Release, Build, Branch)
GENERIC_VERION(VER_MAJOR, VER_MINOR, VER_RELEASE, VER_BUILD, VER_BRANCH)

#else

#define __GENERIC_VERION(Major, Minor, Release, Build)\
void VERSION_quality_metric_##Major##_##Minor##_##Release##_##Build(){ }
#define GENERIC_VERION(Major, Minor, Release, Build) __GENERIC_VERION(Major, Minor, Release, Build)
GENERIC_VERION(VER_MAJOR, VER_MINOR, VER_RELEASE, VER_BUILD)

#endif
#ifdef __cplusplus
}
#endif 
#endif

