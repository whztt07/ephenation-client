// Copyright 2013 The Ephenation Authors
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
    screen = vertex;                       // Copy position to the fragment shader. Only x and y is needed.
}

-- Fragment

uniform sampler2D posTex;     // World position
in vec2 screen;               // The screen position
layout(location = 0) out float light;

vec4 worldPos;

vec2 seed;

vec2 rand(vec2 a, vec2 b) {
	seed = fract(a*10.23 + b*123.1232+screen*3.123 + seed*82.12354); // A value from 0 to 1
	return seed;
}

void main(void)
{
	worldPos = texture(posTex, screen);
	float refDist = distance(UBOCamera.xyz, worldPos.xyz);
	int num = 0;
	const int SIZE = 13;
	float py = 1.0/UBOWindowHeight; // Size of one pixel
	float px = 1.0/UBOWindowWidth; // Size of one pixel
	for (int i=0; i<SIZE; i++) {
		vec2 sampleInd = screen + (rand(worldPos.xy, worldPos.zy)*2-1)*vec2(px,py)*20*UBOcalibrationFactor;
		vec3 sample = texture(posTex, sampleInd).xyz;
		float sampleDist = distance(UBOCamera.xyz, sample);
		if (sampleDist-refDist > 0.2) { num-=10; }
		if (sampleDist < refDist) num++;
	}
	if (num > SIZE*0.76)
		// As the last step, combine all the diffuse color with the lighting and blending effects
		light = 0.64;
	else {
		discard; return;
	}
}
