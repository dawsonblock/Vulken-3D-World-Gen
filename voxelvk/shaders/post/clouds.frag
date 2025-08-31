
#version 460 core
out vec4 fragColor; in vec2 vUV;
uniform sampler2D uNoise; uniform vec3 uSunDir; uniform float uTime;
float noise(vec3 p){ return texture(uNoise, p.xz*0.0008 + uTime*0.005).r; }
void main(){
    vec3 ray = normalize(vec3(vUV*2.0-1.0, 1.2));
    float height = 1500.0;
    float thickness = 600.0;
    vec3 pos = vec3(0.0, height, 0.0);
    float t = 0.0, step = thickness/16.0;
    float alpha = 0.0; vec3 col=vec3(0.0);
    for(int i=0;i<16;i++){
        vec3 samplePos = pos + ray * (t);
        float d = noise(samplePos*0.7);
        float dens = smoothstep(0.55, 0.75, d);
        float light = clamp(dot(uSunDir, vec3(0,1,0))*0.5+0.5, 0.2, 1.0);
        vec3 c = vec3(1.0)*light;
        float a = dens * 0.06 * (1.0 - alpha);
        col += c * a; alpha += a; t += step;
        if(alpha>0.98) break;
    }
    fragColor = vec4(pow(col, vec3(1.0/2.2)), alpha);
}
