// Copyright 2012-2014 The Ephenation Authors
//
// This file is part of Ephenation.
//
// Ephenation is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3.
//
// Ephenation is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Ephenation.  If not, see <http://www.gnu.org/licenses/>.

-- Vertex

// This vertex shader will only draw two triangles, giving a full screen.
// The vertex input is 0,0 in one corner and 1,1 in the other.
layout(location = 0) in vec2 vertex;
out vec2 screen;                          // Screen coordinate
void main(void)
{
	gl_Position = vec4(vertex*2-1, 0, 1); // Transform from interval 0 to 1, to interval -1 to 1.
	screen = vertex;                      // Copy position to the fragment shader. Only x and y is needed.
}

-- Fragment

uniform sampler2D posTex;     // World position
uniform sampler2D normalTex;  // Normals
in vec2 screen;               // The screen position
layout(location = 0) out float light;

vec4 worldPos;
vec4 normal;

// Declarations used for sun and shadow
uniform mat4 shadowmat;       // combination of projection * view matrices of the light source
uniform sampler2D shadowmapTex; // TODO: Use shadow sampler instead

// This function computes lighting from a high resolution shadow map. The purpose is to use it for
// high performance systems
float ShadowMap(vec3 pos, vec3 normal) {
	vec4 shadowmapcoord = shadowmat * vec4(pos.xyz, 1);
	shadowmapcoord.xy = DoubleResolution(shadowmapcoord.xy);
	// Scale x and y from -1..1 to 0..1 so that they can be used to lookup a bitmap.
	// z is also scaled, as OpenGL returns the interval 0..1 from the depth buffer.
	shadowmapcoord = shadowmapcoord/2 + 0.5;
	float sun = 1.0;
	if (shadowmapcoord.x > 0 && shadowmapcoord.x < 1 && shadowmapcoord.y > 0 && shadowmapcoord.y < 1 && shadowmapcoord.z < 1) {
		float depth = texture(shadowmapTex, shadowmapcoord.xy).x;
		float c = 100;
		if (UBODynamicshadows == 2)
			c = 200;
		sun = min(exp(-c*(shadowmapcoord.z - depth)), 1);
	}
	return sun;
}

void main(void)
{
	// Load data, stored in textures, from the first stage rendering.
	normal = texture(normalTex, screen);
	worldPos = texture(posTex, screen);
	// Temporary helper data
	float sun = max(dot(normal.xyz,sundir),0);
	float inSun = worldPos.a; // Is greater than 0 if this position is reached by the sun
	if (inSun > 0 && UBODynamicshadows > 0)
		inSun = ShadowMap(worldPos.xyz, normal.xyz); // Override with dynamic shadows
	// As the last step, combine all the diffuse color with the lighting and blending effects
	// Rain will take away sun lighting, but not all.
	light = inSun * sun * 1.5 * (1-UBORaining*0.9);
}
