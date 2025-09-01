
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb/stb_image_write.h"

struct Image {
    int w=0,h=0,c=0;
    std::vector<unsigned char> rgba;
};

static bool load_rgba(const char* path, Image& out){
    int w,h,comp;
    unsigned char* data = stbi_load(path, &w,&h,&comp, 4);
    if(!data) return false;
    out.w=w; out.h=h; out.c=4;
    out.rgba.assign(data, data + (size_t)w*h*4);
    stbi_image_free(data);
    return true;
}

static bool save_png(const char* path, const Image& img){
    return stbi_write_png(path, img.w, img.h, 4, img.rgba.data(), img.w*4) != 0;
}

static void absdiff(const Image& A, const Image& B, Image& D){
    D.w=A.w; D.h=A.h; D.c=4; D.rgba.resize((size_t)A.w*A.h*4);
    for(size_t i=0;i<D.rgba.size();++i){
        int a = (int)A.rgba[i];
        int b = (int)B.rgba[i];
        int d = std::abs(a-b);
        D.rgba[i] = (unsigned char)d;
    }
}

static void metrics(const Image& A, const Image& B, double& mae, double& mse, double& psnr, int& max_abs){
    const size_t N = (size_t)A.w*A.h*3; // ignore alpha for metrics
    long long sum_abs=0;
    long double sum_sq=0.0L;
    int maxa=0;
    for(int y=0;y<A.h;++y){
        for(int x=0;x<A.w;++x){
            for(int ch=0; ch<3; ++ch){
                size_t i = ((size_t)y*A.w + x)*4 + ch;
                int d = std::abs((int)A.rgba[i] - (int)B.rgba[i]);
                sum_abs += d;
                sum_sq  += (long double)d * (long double)d;
                if(d>maxa) maxa=d;
            }
        }
    }
    mae = (double)sum_abs / (double)N;
    mse = (double)(sum_sq / (long double)N);
    psnr = (mse <= 1e-12) ? 99.0 : 10.0 * std::log10((255.0*255.0)/mse);
    max_abs = maxa;
}

static void write_json(const char* path, int w, int h, double mae, double mse, double psnr, int max_abs, bool pass){
    FILE* f = std::fopen(path, "wb");
    if(!f) return;
    std::fprintf(f, "{\n");
    std::fprintf(f, "  \"width\": %d,\n", w);
    std::fprintf(f, "  \"height\": %d,\n", h);
    std::fprintf(f, "  \"mae\": %.6f,\n", mae);
    std::fprintf(f, "  \"mse\": %.6f,\n", mse);
    std::fprintf(f, "  \"psnr\": %.6f,\n", psnr);
    std::fprintf(f, "  \"max_abs\": %d,\n", max_abs);
    std::fprintf(f, "  \"pass\": %s\n", pass ? "true":"false");
    std::fprintf(f, "}\n");
    std::fclose(f);
}

int main(int argc, char** argv){
    const char* ref_path=nullptr;
    const char* test_path=nullptr;
    const char* out_diff="diff.png";
    const char* out_json="imagediff_report.json";
    double psnr_min = 30.0;
    double mae_max = 2.0;
    bool strict_size=true;

    for(int i=1;i<argc;i++){
        if(!std::strcmp(argv[i], "-r") && i+1<argc) ref_path=argv[++i];
        else if(!std::strcmp(argv[i], "-t") && i+1<argc) test_path=argv[++i];
        else if(!std::strcmp(argv[i], "-o") && i+1<argc) out_diff=argv[++i];
        else if(!std::strcmp(argv[i], "-j") && i+1<argc) out_json=argv[++i];
        else if(!std::strcmp(argv[i], "-psnr-min") && i+1<argc) psnr_min=std::atof(argv[++i]);
        else if(!std::strcmp(argv[i], "-mae-max") && i+1<argc) mae_max=std::atof(argv[++i]);
        else if(!std::strcmp(argv[i], "-no-strict-size")) strict_size=false;
        else if(!std::strcmp(argv[i], "-h") || !std::strcmp(argv[i], "--help")){
            std::printf("Usage: imagediff -r ref.png -t test.png [-o diff.png] [-j report.json] [-psnr-min 30] [-mae-max 2]\n");
            return 0;
        }
    }
    if(!ref_path || !test_path){
        std::fprintf(stderr, "imagediff: require -r ref.png -t test.png\n");
        return 2;
    }

    Image R,T;
    if(!load_rgba(ref_path,R)){ std::fprintf(stderr,"Failed to load %s\n", ref_path); return 3; }
    if(!load_rgba(test_path,T)){ std::fprintf(stderr,"Failed to load %s\n", test_path); return 4; }

    if((R.w!=T.w || R.h!=T.h) && strict_size){
        std::fprintf(stderr,"Image sizes differ: %dx%d vs %dx%d\n", R.w,R.h,T.w,T.h);
        write_json(out_json, R.w, R.h, 0,0,0, 0, false);
        return 5;
    }

    // If sizes differ and not strict, clamp to min
    if(R.w!=T.w || R.h!=T.h){
        int w = std::min(R.w,T.w), h=std::min(R.h,T.h);
        // naive crop in-place
        auto crop = [w,h](Image& I){
            std::vector<unsigned char> out((size_t)w*h*4);
            for(int y=0;y<h;++y){
                std::memcpy(&out[(size_t)y*w*4], &I.rgba[(size_t)y*I.w*4], (size_t)w*4);
            }
            I.rgba.swap(out); I.w=w; I.h=h;
        };
        crop(R); crop(T);
    }

    double mae,mse,psnr; int max_abs;
    metrics(R,T,mae,mse,psnr,max_abs);

    Image D; absdiff(R,T,D);
    save_png(out_diff, D);

    bool pass = (psnr >= psnr_min) && (mae <= mae_max);
    write_json(out_json, R.w,R.h, mae,mse,psnr,max_abs, pass);

    std::printf("PSNR: %.3f dB, MAE: %.3f, MaxAbs: %d -> %s\n", psnr, mae, max_abs, pass?"PASS":"FAIL");
    return pass?0:1;
}
