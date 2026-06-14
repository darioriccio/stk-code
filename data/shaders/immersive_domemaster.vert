#ifdef Explicit_Attrib_Location_Usable
layout(location = 0) in vec2 Position;
#else
in vec2 Position;
#endif

void main()
{
    gl_Position = vec4(Position, 0.0, 1.0);
}
