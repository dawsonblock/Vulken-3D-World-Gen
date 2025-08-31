
/* stb_image - v2.28 - public domain image loader - http://nothings.org/stb_image.h */
#ifndef STB_IMAGE_INCLUDE_STB_IMAGE_H
#define STB_IMAGE_INCLUDE_STB_IMAGE_H
#define STBI_NO_THREAD_LOCALS
#define STBI_ONLY_PNG
#define STBI_ASSERT(x)
typedef unsigned char stbi_uc;
#ifdef __cplusplus
extern "C" {
#endif
extern stbi_uc *stbi_load(char const *filename, int *x, int *y, int *comp, int req_comp);
extern void      stbi_image_free(void *retval_from_stbi_load);
#ifdef __cplusplus
}
#endif
#endif
