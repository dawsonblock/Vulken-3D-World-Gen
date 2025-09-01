
/* stb_image_write - v1.16 - public domain - http://nothings.org/stb_image_write.h */
#ifndef STB_IMAGE_WRITE_INCLUDE_STB_IMAGE_WRITE_H
#define STB_IMAGE_WRITE_INCLUDE_STB_IMAGE_WRITE_H
typedef unsigned char stbi_uc;
#ifdef __cplusplus
extern "C" {
#endif
extern int stbi_write_png(char const *filename, int w, int h, int comp, const void *data, int stride_in_bytes);
#ifdef __cplusplus
}
#endif
#endif
