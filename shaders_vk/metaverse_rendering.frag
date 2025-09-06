#version 450

// Metaverse and VR rendering fragment shader
layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

// VR/AR textures
layout(binding = 0) uniform sampler2D leftEyeTexture;
layout(binding = 1) uniform sampler2D rightEyeTexture;
layout(binding = 2) uniform sampler2D depthTexture;
layout(binding = 3) uniform sampler2D stencilTexture;

// Metaverse uniforms
layout(binding = 4) uniform MetaverseUniforms {
    vec3 cameraPos;
    vec3 leftEyePos;
    vec3 rightEyePos;
    vec3 leftEyeDir;
    vec3 rightEyeDir;
    float ipd; // Interpupillary distance
    float convergenceDistance;
    float fov;
    float nearPlane;
    float farPlane;
    int renderingMode; // 0 = monoscopic, 1 = stereoscopic, 2 = VR, 3 = AR
    int eyeIndex; // 0 = left, 1 = right
    float timeWarpFactor;
    float chromaticAberration;
    float barrelDistortion;
    float vignetting;
    int useFoveatedRendering;
    int useEyeTracking;
    int useHandTracking;
    int useSpatialMapping;
    float time;
} meta;

// Hand tracking data
struct HandData {
    vec3 leftHandPos;
    vec3 rightHandPos;
    vec3 leftHandDir;
    vec3 rightHandDir;
    float leftHandConfidence;
    float rightHandConfidence;
    int leftHandGesture;
    int rightHandGesture;
};

// Spatial mapping data
struct SpatialData {
    vec3 roomCenter;
    vec3 roomSize;
    float floorHeight;
    float ceilingHeight;
    int wallCount;
    vec3 wallPositions[8];
    vec3 wallNormals[8];
};

// Hand tracking buffer
layout(binding = 5) readonly buffer HandTrackingBuffer {
    HandData handData;
} hands;

// Spatial mapping buffer
layout(binding = 6) readonly buffer SpatialMappingBuffer {
    SpatialData spatialData;
} spatial;

// VR distortion correction
vec2 vrDistortion(vec2 texCoord, int eye) {
    vec2 center = vec2(0.5);
    vec2 offset = texCoord - center;

    // Barrel distortion
    float r2 = dot(offset, offset);
    float distortion = 1.0 + meta.barrelDistortion * r2 + meta.barrelDistortion * meta.barrelDistortion * r2 * r2;

    // Chromatic aberration
    vec2 redOffset = offset * (1.0 + meta.chromaticAberration);
    vec2 greenOffset = offset;
    vec2 blueOffset = offset * (1.0 - meta.chromaticAberration);

    // Apply distortion
    vec2 distorted = center + offset * distortion;

    return distorted;
}

// Foveated rendering
float foveatedRendering(vec2 texCoord, int eye) {
    vec2 center = vec2(0.5);
    float distance = length(texCoord - center);

    // Foveated rendering based on distance from center
    float foveation = 1.0 - smoothstep(0.0, 0.5, distance);

    return foveation;
}

// Eye tracking
vec2 eyeTrackingOffset(vec2 texCoord, int eye) {
    vec2 offset = vec2(0.0);

    if(meta.useEyeTracking == 1) {
        // Simulate eye tracking offset
        vec2 eyePos = eye == 0 ? vec2(0.3, 0.5) : vec2(0.7, 0.5);
        vec2 gazeDirection = normalize(texCoord - eyePos);

        // Apply eye tracking offset
        offset = gazeDirection * 0.1;
    }

    return offset;
}

// Hand tracking interaction
vec3 handTrackingInteraction(vec3 worldPos) {
    vec3 color = vec3(0.0);

    if(meta.useHandTracking == 1) {
        // Left hand interaction
        float leftHandDistance = length(worldPos - hands.handData.leftHandPos);
        if(leftHandDistance < 0.5) {
            color += vec3(1.0, 0.0, 0.0) * (1.0 - leftHandDistance / 0.5);
        }

        // Right hand interaction
        float rightHandDistance = length(worldPos - hands.handData.rightHandPos);
        if(rightHandDistance < 0.5) {
            color += vec3(0.0, 1.0, 0.0) * (1.0 - rightHandDistance / 0.5);
        }
    }

    return color;
}

// Spatial mapping
vec3 spatialMapping(vec3 worldPos) {
    vec3 color = vec3(0.0);

    if(meta.useSpatialMapping == 1) {
        // Check if position is inside room
        vec3 roomCenter = spatial.spatialData.roomCenter;
        vec3 roomSize = spatial.spatialData.roomSize;

        if(abs(worldPos.x - roomCenter.x) < roomSize.x / 2.0 &&
           abs(worldPos.y - roomCenter.y) < roomSize.y / 2.0 &&
           abs(worldPos.z - roomCenter.z) < roomSize.z / 2.0) {

            // Inside room - normal rendering
            color = vec3(1.0);
        } else {
            // Outside room - occluded
            color = vec3(0.0);
        }

        // Wall detection
        for(int i = 0; i < spatial.spatialData.wallCount; ++i) {
            vec3 wallPos = spatial.spatialData.wallPositions[i];
            vec3 wallNormal = spatial.spatialData.wallNormals[i];

            float distance = dot(worldPos - wallPos, wallNormal);
            if(distance < 0.1) {
                color = mix(color, vec3(0.5, 0.5, 0.5), 0.5);
            }
        }
    }

    return color;
}

// Time warp
vec2 timeWarp(vec2 texCoord, int eye) {
    vec2 warped = texCoord;

    if(meta.timeWarpFactor > 0.0) {
        // Simulate time warp for VR
        vec2 center = vec2(0.5);
        vec2 offset = texCoord - center;

        // Apply time warp
        float warp = meta.timeWarpFactor * sin(meta.time * 10.0);
        warped = center + offset * (1.0 + warp);
    }

    return warped;
}

// Vignetting
float vignetting(vec2 texCoord) {
    vec2 center = vec2(0.5);
    float distance = length(texCoord - center);

    float vignette = 1.0 - smoothstep(0.3, 0.8, distance) * meta.vignetting;

    return vignette;
}

// Stereoscopic rendering
vec4 stereoscopicRendering(vec2 texCoord) {
    vec4 color = vec4(0.0);

    if(meta.renderingMode == 1) { // Stereoscopic
        if(meta.eyeIndex == 0) { // Left eye
            color = texture(leftEyeTexture, texCoord);
        } else { // Right eye
            color = texture(rightEyeTexture, texCoord);
        }
    } else if(meta.renderingMode == 2) { // VR
        // VR rendering with distortion correction
        vec2 distortedCoord = vrDistortion(texCoord, meta.eyeIndex);

        if(meta.eyeIndex == 0) { // Left eye
            color = texture(leftEyeTexture, distortedCoord);
        } else { // Right eye
            color = texture(rightEyeTexture, distortedCoord);
        }

        // Apply foveated rendering
        if(meta.useFoveatedRendering == 1) {
            float foveation = foveatedRendering(texCoord, meta.eyeIndex);
            color.rgb *= foveation;
        }

        // Apply eye tracking
        vec2 eyeOffset = eyeTrackingOffset(texCoord, meta.eyeIndex);
        color = texture(leftEyeTexture, texCoord + eyeOffset);

    } else if(meta.renderingMode == 3) { // AR
        // AR rendering with real-world overlay
        vec4 realWorld = texture(leftEyeTexture, texCoord);
        vec4 virtualWorld = texture(rightEyeTexture, texCoord);

        // Blend real and virtual worlds
        float alpha = virtualWorld.a;
        color = mix(realWorld, virtualWorld, alpha);
    } else { // Monoscopic
        color = texture(leftEyeTexture, texCoord);
    }

    return color;
}

// Metaverse features
vec3 metaverseFeatures(vec3 worldPos, vec2 texCoord) {
    vec3 features = vec3(0.0);

    // Hand tracking interaction
    features += handTrackingInteraction(worldPos);

    // Spatial mapping
    features += spatialMapping(worldPos);

    // Social presence (simplified)
    vec3 socialPresence = vec3(0.0);
    // Simulate other users in the metaverse
    for(int i = 0; i < 4; ++i) {
        vec3 userPos = vec3(
            sin(meta.time + float(i)) * 2.0,
            0.0,
            cos(meta.time + float(i)) * 2.0
        );

        float distance = length(worldPos - userPos);
        if(distance < 1.0) {
            socialPresence += vec3(0.1, 0.1, 0.1) * (1.0 - distance);
        }
    }
    features += socialPresence;

    return features;
}

void main() {
    // Calculate world position
    vec3 worldPos = fragPos;

    // Apply time warp
    vec2 warpedTexCoord = timeWarp(fragTexCoord, meta.eyeIndex);

    // Stereoscopic rendering
    vec4 color = stereoscopicRendering(warpedTexCoord);

    // Metaverse features
    vec3 metaverseFeatures = metaverseFeatures(worldPos, fragTexCoord);
    color.rgb += metaverseFeatures;

    // Apply vignetting
    float vignette = vignetting(fragTexCoord);
    color.rgb *= vignette;

    // Final output
    outColor = color;
}
