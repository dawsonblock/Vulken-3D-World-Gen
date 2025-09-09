#version 460
layout(location=0) out vec2 vUv;
layout(std430, binding=0) buffer Particles { vec4 data[]; }; // pos.xyz, size
layout(set=0,binding=0) uniform Camera { mat4 VP; } uCam;
void main(){
  uint idx = gl_VertexIndex;
  vec3 pos = data[idx*2+0].xyz;
  float size = max(0.003, data[idx*2+1].x); // prefilled at upload
  // point sprite
  gl_Position = uCam.VP * vec4(pos,1.0);
  gl_PointSize = size * 1200.0 / max(1.0, gl_Position.w);
  vUv = vec2(0.0); // unused
}