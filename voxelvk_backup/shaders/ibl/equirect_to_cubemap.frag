
#version 460 core
out vec3 fragColor; in vec3 vDir;
uniform sampler2D uEquirect;
vec2 dirToEquirect(vec3 d){
    float phi = atan(d.z, d.x); float theta = acos(clamp(d.y, -1.0, 1.0));
    return vec2((phi+3.14159265)/(2.0*3.14159265), theta/3.14159265);
}
void main(){ fragColor = texture(uEquirect, dirToEquirect(normalize(vDir))).rgb; }
