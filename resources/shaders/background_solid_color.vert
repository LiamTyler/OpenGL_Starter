#version 450
#extension GL_ARB_separate_shader_objects : enable

layout( location = 0 ) in vec3 vertex;

layout( location = 0 ) out vec2 UV;

void main()
{
    gl_Position = vec4( vertex.xy, 1, 1 );
    UV = .5 * ( vertex.xy + vec2( 1 ) );
}