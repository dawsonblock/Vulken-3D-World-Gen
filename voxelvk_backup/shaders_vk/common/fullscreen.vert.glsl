
#version 460
layout(location=0) out vec2 vUV;
void main(){
    const vec2 pos[3] = vec2[3]( vec2(-1,-1), vec2(3,-1), vec2(-1,3) );
    vUV = (pos[gl_VertexIndex] + 1.0) * 0.5;
    gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
}
