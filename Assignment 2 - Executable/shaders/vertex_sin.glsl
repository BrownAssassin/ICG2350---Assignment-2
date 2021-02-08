#version 410

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec3 outPos;
layout(location = 1) out vec3 outColor;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out vec2 outUV;

uniform mat4 u_ModelViewProjection;
uniform mat4 u_View;
uniform mat4 u_Model;
uniform mat3 u_NormalMatrix;
uniform vec3 u_LightPos;

uniform float effectState;
uniform float sinTime;

void main() {
    
    vec3 vert = inPosition;

    if (effectState == 1.0) {
        vert.z = sin(vert.x * 3.0 + sinTime * 0.1) * 0.25;
        gl_Position = u_ModelViewProjection * vec4(vert, 1.0);
    }
    else {
        gl_Position = u_ModelViewProjection * vec4(inPosition, 1.0);
    }

    outPos = (u_Model * vec4(inPosition, 1.0)).xyz;

    outNormal = u_NormalMatrix * inNormal;

    outUV = inUV;

    outColor = inColor;

}
