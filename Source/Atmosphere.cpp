// Copyright 2014 The Ephenation Authors
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
//

#include <cmath>
#include <algorithm>
#include <glm/gtx/intersect.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "Atmosphere.h"
#include "Debug.h"
#include "primitives.h"

using glm::vec3;
using glm::vec2;

#if 0
// This one isn't needed currently.
static bool ComputeCircleIntersection(vec2 C, vec2 A, vec2 D, float R, float &result) {

	// Now the line equation is x = Dx*t + Ax, y = Dy*t + Ay with 0 <= t <= 1.

	// compute the value t of the closest point to the circle center (Cx, Cy)
	float t = D.x*(C.x-A.x) + D.y*(C.y-A.y);

	// This is the projection of C on the line from A to B.

	// compute the coordinates of the point E on line and closest to C
	vec2 E;
	E.x = t*D.x+A.x;
	E.y = t*D.y+A.y;

	// compute the euclidean distance from E to C
	float LEC = glm::distance(E, C);
	// glm::sqrt( (E.x-C.x)²+(E.y-C.y)² );

	// test if the line intersects the circle
	if( LEC < R )
	{
		// compute distance from t to circle intersection point
		// dt = sqrt( R² - LEC²)
		float dt = glm::sqrt(R*R - LEC*LEC);

		// compute first intersection point
		vec2 F;
		F.x = (t-dt)*D.x + A.x;
		F.y = (t-dt)*D.y + A.y;

		// compute second intersection point
		vec2 G;
		G.x = (t+dt)*D.x + A.x;
		G.y = (t+dt)*D.y + A.y;

		return true;
	}

	// else test if the line is tangent to circle
	else if( LEC == R )
		return false; // tangent point to circle is E
	else
		return false; // line doesn't touch circle
}

#endif

static const float H_Atm = 80000.0f;
static const float R_Earth = 6371*1000;
const float atmSquared = (R_Earth+H_Atm) * (R_Earth+H_Atm);
const vec2 earthCenter(0, -R_Earth); // Height 0 is ground level
// As taken RGB from http://www.vendian.org/mncharity/dir3/blackbody/UnstableURLs/bbr_color.html at 5800K
static const vec3 sunRGB(1, 243.0f/255.0f, 234/255.0f);
// TODO: Sun direction shall not be a constant
static const vec3 sunDir(-0.577350269f, 0.577350269f, 0.577350269f); // Copied from sundir in common.glsl.

Atmosphere::Atmosphere()
{
	//ctor
}

static float HeightParameterized(float h) {
	assert(h >= 0);
	return std::sqrt(h / H_Atm);
}

static float HeightParameterizedInverse(float uh) {
	return uh*uh*H_Atm;
}

// The maximum horizontal vector, inside the atmosphere
static const float maxHorizontalDist = 2 * glm::sqrt((R_Earth+H_Atm)*(R_Earth+H_Atm) - R_Earth*R_Earth);
static const float minHorizontalDist = 10.0f; // Shorter than this has no effect
static const float coeffHor = glm::log(maxHorizontalDist / minHorizontalDist);

static float HorizontalDistParameterized(float x) {
	return glm::log(x/minHorizontalDist) / coeffHor;
}

// Convert from interval [0,1] to [minHorizontalDist, maxHorizontalDist]
static float HorizontalDistParameterizedInverse(float x) {
	return minHorizontalDist * glm::exp(coeffHor * x);
}

// cv 1 is up, -1 is down
static float ViewAngleParameterized(float cv, float h) {
	float ch = - std::sqrt(h * (2 * R_Earth + h)) / (R_Earth + h); // The Angle between the horizon and zenith for the current height
	if (cv > ch)
		return 0.5f * std::pow((cv-ch) / (1-ch), 0.2f) + 0.5f;
	else
		return 0.5f - 0.5f * std::pow((ch-cv) / (ch+1), 0.2f);
}

// Get cosinus of angle between azimuth and viewer
// cv 1 is up, -1 is down
static float ViewAngleParameterizedInverse(float uv, float h) {
	float ch = - std::sqrt(h * (2 * R_Earth + h)) / (R_Earth + h); // The Angle between the horizon and zenith for the current height
	if (uv > 0.5f)
		return ch + std::pow((uv-0.5f)*2, 5.0f) * (1.0f - ch);
	else
		return ch - std::pow(1 - uv*2, 5.0f) * (1.0f + ch);
}

// cs 1 is up, -1 is down
static float SunAngleParameterization(float cs) {
	float tmp = std::tan(1.26f * 1.1f);
	return 0.5f * (std::atan(std::max(cs, -0.1975f) * tmp) / 1.1f + (1-0.26f));
}

// cs 1 is up, -1 is down
static float SunAngleParameterizationInverse(float us) {
	float tmp = std::tan(1.26f * 1.1f);
	return std::tan((2*us - 1.0f + 0.26f) * 1.1f) / tmp;
}

static float RayleighPhaseFunction(float cs) {
	return 0.8*(1.4 + 0.5*cs)/1.52; // Eq. 15, in thesis
}

// Reference system (0,0,0) is the player at sea level
static float height(vec2 p) {
	p.y += R_Earth; // Now reference is earth center
	float dist = glm::length(p);
	return dist - R_Earth;
}

static const int INTEGRATION_STEPS = 10;

static const float MieScatterCoefficient = 2.0e-6;
static const float MieExtinctionCoefficient = MieScatterCoefficient / 0.9f;
static const vec3 RayleighScatterCoefficient(6.55e-6, 1.73e-5, 2.30e-5);

static float getDensityRayleigh(float h) {
	return std::exp(-h / 8000);
}

static float getDensityMie(float h) {
	return std::exp(-h / 1200);
}

vec3 Atmosphere::Transmittance(vec2 pa, vec2 pb) const {
	float ha = height(pa), hb = height(pb);
	if (ha > hb)
		std::swap(pa, pb); // We want pa at the place with highest density
	float totalDistance = glm::distance(pa, pb);
	float stepSize = totalDistance;
	if (stepSize > 10)
		stepSize = 10;
	vec2 dir = glm::normalize(pb-pa);
	float totalDensityMie = 0.0f, totalDensityRayleigh = 0.0f;
	float previousDensityMie = 0.0f, previousDensityReyleigh = 0.0f;
	float prevDistance = 0;
	for (float distance=stepSize; distance < totalDistance; distance *= 1.05f) {
		vec2 s = pa + distance * dir;
		float h = height(s);
		float currentDensityMie = getDensityMie(h);
		float currentDensityRayleigh = getDensityRayleigh(h);
		totalDensityMie += (currentDensityMie + previousDensityMie) / 2 * (distance-prevDistance);
		totalDensityRayleigh += (currentDensityRayleigh + previousDensityReyleigh) / 2 * (distance-prevDistance);
		previousDensityMie = currentDensityMie;
		previousDensityReyleigh = currentDensityRayleigh;
		prevDistance = distance;
	}
	return glm::exp(-(totalDensityRayleigh * RayleighScatterCoefficient + totalDensityMie * MieExtinctionCoefficient));
}

void Atmosphere::SingleScattering(vec2 pa, vec2 l, vec2 v, vec3 &mie, vec3 &rayleigh) const {
	// Compute the intersection distance to the point 'pb' where the ray leaves the atmosphere.
	// See figure 4.
	float intersectionDistance = 0;
	bool found = glm::intersectRaySphere(pa, -v, earthCenter, atmSquared, intersectionDistance);
	if (!found || intersectionDistance < 10) {
		mie = vec3(0,0,0);
		rayleigh = vec3(0,0,0);
		return;
	}
	vec3 totalInscatteringMie, totalInscatteringRayleigh, previousInscatteringMie, previousInscatteringRayleigh;
	float prevDistance = 0.0f;
	int iteration = 0;
	for (float distance = 2000.0f; distance < intersectionDistance; distance *= 1.2f) {
		iteration++;
		// 'p' will iterate over the line from 'pa' to 'pb'.
		const vec2 p = pa - distance * v; // Step backwards from pa
		vec3 transmittance = FetchTransmittance(pa, p);
		const float cutOff = 1e-6;
		if (transmittance.r < cutOff && transmittance.g < cutOff && transmittance.b < cutOff) {
			// LPLOG("Giving up at %.0f (of %.0f), iteration %d", distance, intersectionDistance, iteration);
			break; // Give it up
		}
		float atmDistance;
		found = glm::intersectRaySphere(p, -l, earthCenter, atmSquared, atmDistance); assert(found);
		const vec2 pc = p - l * atmDistance; // Step backwards from p
		transmittance *= FetchTransmittance(p, pc);
		float h = height(p);
		vec3 currentInscatteringMie = getDensityMie(h) * transmittance;
		vec3 currentInscatteringRayleigh = getDensityRayleigh(h) * transmittance;
		totalInscatteringMie += (currentInscatteringMie + previousInscatteringMie)/2.0f * (distance-prevDistance);
		totalInscatteringRayleigh += (currentInscatteringRayleigh + previousInscatteringRayleigh)/2.0f * (distance-prevDistance);
		previousInscatteringMie = currentInscatteringMie;
		previousInscatteringRayleigh = currentInscatteringRayleigh;
		prevDistance = distance;
	}
	totalInscatteringMie *= MieScatterCoefficient / (4.0f * glm::pi<float>()) * sunRGB;
	totalInscatteringRayleigh *= RayleighScatterCoefficient / (4.0f * glm::pi<float>()) * sunRGB;
	mie = totalInscatteringMie;
	rayleigh = totalInscatteringRayleigh;
}

void Atmosphere::MultipleScattering(glm::vec2 pa, glm::vec2 l, glm::vec2 v, glm::vec3 &mie, glm::vec3 &rayleigh) const {
	// Compute the intersection distance to the point 'pb' where the ray leaves the atmosphere.
	// See figure 4.
	float intersectionDistance;
	bool found = glm::intersectRaySphere(pa, -v, earthCenter, atmSquared, intersectionDistance);
	assert(found);
	float prevDistance = 0.0f;
	int iteration = 0;
	vec3 totalInscatteringMie, totalInscatteringRayleigh, previousInscatteringMie, previousInscatteringRayleigh;
	for (float distance = 2000.0f; distance < intersectionDistance; distance *= 1.2f) {
		iteration++;
		// 'p' will iterate over the line from 'pa' to 'pb'.
		const vec2 p = pa - distance * v; // Step backwards from pa
		vec3 transmittance = FetchTransmittance(pa, p);
		const float cutOff = 1e-6;
		if (transmittance.r < cutOff && transmittance.g < cutOff && transmittance.b < cutOff) {
			// LPLOG("Giving up at %.0f (of %.0f), iteration %d", distance, intersectionDistance, iteration);
			break; // Give it up
		}
		float h = height(p);
		vec3 gathered = GatheredLight(h, glm::acos(v.y), l);
		vec3 currentInscatteringMie = gathered * getDensityMie(h) * transmittance;
		vec3 currentInscatteringRayleigh = gathered * getDensityRayleigh(h) * transmittance;
		totalInscatteringMie += (currentInscatteringMie + previousInscatteringMie)/2.0f * (distance-prevDistance);
		totalInscatteringRayleigh += (currentInscatteringRayleigh + previousInscatteringRayleigh)/2.0f * (distance-prevDistance);
		previousInscatteringMie = currentInscatteringMie;
		previousInscatteringRayleigh = currentInscatteringRayleigh;
		prevDistance = distance;
	}
	totalInscatteringMie *= MieScatterCoefficient / (4.0f * glm::pi<float>()) * sunRGB;
	totalInscatteringRayleigh *= RayleighScatterCoefficient / (4.0f * glm::pi<float>()) * sunRGB;
	mie = totalInscatteringMie;
	rayleigh = totalInscatteringRayleigh;
}

vec3 Atmosphere::fetchScattered(float h, float cv, float cs) const {
	float uh = HeightParameterized(h);
	float uv = ViewAngleParameterized(cv, h);
	float us = SunAngleParameterization(cs);

	// TODO: Use interpolation instead of nearest
	int ih = int(uh*(NHEIGHT-1)+0.5f);
	int iv = int(uv*(NVIEW_ANGLE-1)+0.5f);
	int is = int(us*(NSUN_ANGLE-1)+0.5f);
	assert(ih >= 0 && ih < NHEIGHT && iv >= 0 && iv < NVIEW_ANGLE && is >= 0 && is < NSUN_ANGLE);
	// TODO: Interpolation could help?
	return fScattering[ih][iv][is];
}

vec3 Atmosphere::GatheredLight(float h, float v, vec2 l) const {
	vec3 gathered;
	float cs = -l.y;

	// Algorithm 3, but iterating over the precomputed scattering table
	float prevCos = -1, prevSine = 0;
	for (int viewAngleIndex = 0; viewAngleIndex < NVIEW_ANGLE; viewAngleIndex++) {
		float uViewAngle = float(viewAngleIndex) / (NVIEW_ANGLE-1);
		float cosViewAngle = ViewAngleParameterizedInverse(uViewAngle, h);
		float viewAngle = glm::acos(cosViewAngle);
		float sinViewAngle = glm::sqrt(1-cosViewAngle*cosViewAngle);
		float circumference = 2 * glm::pi<float>() * (sinViewAngle + prevSine)/2; // 2 * pi * r
		// If fetchScattered always return 1, the total sum in "gathered" would be the area of a sphere with radius 1m
		gathered += fetchScattered(h, cosViewAngle, cs) * (cosViewAngle - prevCos) * circumference * RayleighPhaseFunction(glm::cos(viewAngle - v));
		// gathered += (cosViewAngle - prevCos) * circumference;
		prevCos = cosViewAngle;
		prevSine = sinViewAngle;
	}
	return gathered;
}

void Atmosphere::PreComputeTransmittance() {
	for (int angleIndex = 0; angleIndex < NVIEW_ANGLE; angleIndex++) {
		float uv = float(angleIndex) / (NVIEW_ANGLE-1);
		for (int heightIndex1 = 0; heightIndex1 < NHEIGHT; heightIndex1++) {
			float uh1 = float(heightIndex1) / (NHEIGHT-1);
			float h1 = HeightParameterizedInverse(uh1);
			vec2 pa(0, h1);
			float cosViewAngle = ViewAngleParameterizedInverse(uv, h1); // This is the angle between the view and azimuth.
			float sinViewAngle = glm::sqrt(1 - cosViewAngle*cosViewAngle);
			vec2 v(sinViewAngle, -cosViewAngle); // Pointing from 'pa'. Sign of sinViewAngle doesn't really matter as it is symmetrically.
			vec3 transm(1,1,1);
			if (heightIndex1 < NHEIGHT-1) {
				// This is the normal case
				float intersectionDistance;
				bool intersectionFound = glm::intersectRaySphere(pa, v, earthCenter, atmSquared, intersectionDistance);
				assert(intersectionFound);
				vec2 pb = pa + intersectionDistance * v;
				transm = this->Transmittance(pa, pb);
			}
			fTransmittance[heightIndex1][angleIndex] = transm;
		}
	}
}

// Find point on line a to b, which is nearest p.
static vec2 GetNearestPoint(vec2 p, vec2 a, vec2 b) {
	vec2 a_to_p = p-a;
	vec2 a_to_b = b-a;

	float l2 = a_to_b.x*a_to_b.x + a_to_b.y*a_to_b.y;
	float atp_dot_atb = glm::dot(a_to_p, a_to_b);

	float dist = atp_dot_atb / l2;

	vec2 ret(a.x + a_to_b.x*dist, a.y + a_to_b.y*dist);

	return ret;
}

// Take point 'pa', outgoing direction 'v' (normalized), and fetch transmittance to the horizon
vec3 Atmosphere::FetchTransmittanceToHorizon(vec2 pa, vec2 v) const {
	// Rotate 'pa' and the 'cv' angle, to get the new pa.x = 0.
	vec2 dir = glm::normalize(pa - earthCenter); // Normalized vector from earth center to pa.
	float correction = glm::acos(dir.y) * 360 / 2 / glm::pi<float>() * glm::sign(dir.x); // Measured in degrees
	vec2 pa2 = glm::rotate(pa-earthCenter, correction) + earthCenter; // x should be almost 0, and only 'y' is used from now on.
	if (pa2.y < 0)
		return vec3(0,0,0);
	vec2 v2 = glm::rotate(v, correction);
	float uv = ViewAngleParameterized(-v2.y, pa2.y);
	float uh = HeightParameterized(pa2.y);
	int ih = int(uh*(NHEIGHT-1));
	int iv = int(uv*(NVIEW_ANGLE-1));
	assert(ih >= 0 && ih < NHEIGHT && iv >= 0 && iv < NVIEW_ANGLE);
	if (ih == NHEIGHT-1)
		ih--;
	if (iv == NVIEW_ANGLE-1)
		iv--;
	float m = uh*(NHEIGHT-1) - ih;
	vec3 mixedView1 = glm::mix(fTransmittance[ih][iv], fTransmittance[ih+1][iv], m);
	vec3 mixedView2 = glm::mix(fTransmittance[ih][iv+1], fTransmittance[ih+1][iv+1], m);
	m = uv*(NVIEW_ANGLE-1) - iv;
	return glm::mix(mixedView1, mixedView2, m);
};

vec3 Atmosphere::FetchTransmittance(vec2 pa, vec2 pb) const {
	float ha = height(pa), hb = height(pb);
	if (ha < 0 || hb < 0)
		return vec3(0,0,0); // Don't transmit any light below ground level
	float epsilon = H_Atm/100;
	assert(ha < H_Atm+epsilon && hb < H_Atm+epsilon); // It is possible to move the point to the atmosphere, but costs effort.
	if (hb < ha)
		std::swap(pa, pb); // Now we now 'pa' is below 'pb'
	// The precomputed table gives the transmittance all the way to the atmosphere, but we need the transmittance
	// only to 'pb'. So we take the whole way transmittance from pa to the atmosphere, and subtract
	// the transmittance from 'pb' to the atmosphere.
	// To do this, we need to find the point that intersects the atmosphere.
	vec2 v = glm::normalize(pb-pa); // Normalized vector pointing from pa to pb.
	float intersectionDistance;
	bool intersectionFound = glm::intersectRaySphere(pa, v, earthCenter, atmSquared, intersectionDistance);
	assert(intersectionFound);
	vec2 pAtm = pa + v*intersectionDistance;

	vec3 transm1 = FetchTransmittanceToHorizon(pa, v);
	if (glm::length(pb - pAtm) < epsilon)
		return transm1; // pb was already near end of atmosphere
	vec3 transm2 = FetchTransmittanceToHorizon(pb, v);
	// When near earth, sometimes vector from pb goes below ground, but not from pa, because of rounding
	if(transm2.r == 0 || transm2.g == 0 || transm2.b == 0)
		return vec3(0,0,0);
	return transm1 / transm2;
}

void Atmosphere::PreComputeSingleScattering() {
	for (int heightIndex = 0; heightIndex < NHEIGHT; heightIndex++) {
		float uHeight = float(heightIndex) / (NHEIGHT-1);
		float h = HeightParameterizedInverse(uHeight);
		vec2 pa(0,h);
		for (int viewAngleIndex = 0; viewAngleIndex < NVIEW_ANGLE; viewAngleIndex++) {
			float uViewAngle = float(viewAngleIndex) / (NVIEW_ANGLE-1);
			float cosViewAngle = ViewAngleParameterizedInverse(uViewAngle, h);
			float sinViewAngle = glm::sqrt(1 - cosViewAngle*cosViewAngle); // Pythagoras
			// The view angle is the angle from the azimuth
			vec2 v(sinViewAngle, -cosViewAngle); // Pointing towards 'pa'
			for (int sunAngleIndex = 0; sunAngleIndex < NSUN_ANGLE; sunAngleIndex++) {
				float uSunAngle = float(sunAngleIndex) / (NSUN_ANGLE-1);
				float cosSunAngle = SunAngleParameterizationInverse(uSunAngle);
				float sinSunAgle = glm::sqrt(1 - cosSunAngle*cosSunAngle);
				// The sun angle is the angle between the azimuth and the sun
				vec2 l(sinSunAgle, -cosSunAngle); // Pointing towards 'pa'
				vec3 mie, rayleigh;
				SingleScattering(pa, l, v, mie, rayleigh);
				fScattering[heightIndex][viewAngleIndex][sunAngleIndex] = mie + rayleigh;
			}
		}
	}
}

void Atmosphere::Debug() {
	this->Init();

	// Verify parameterized functions and their inverse values
	const float epsilon = 0.00001, delta = 0.1f;
	for (float uh=0; uh < 1+epsilon; uh += delta) {
		float h = HeightParameterizedInverse(uh);
		assert(glm::length(uh - HeightParameterized(h)) < epsilon);
		for (float uv = 0; uv < 1+epsilon; uv += delta) {
			float v = ViewAngleParameterizedInverse(uv, h);
			assert(glm::length(uv - ViewAngleParameterized(v, h)) < epsilon);
		}
	}

	for (float us = delta; us < 1+epsilon; us += delta) {
		float s = SunAngleParameterizationInverse(us);
		float us2 = SunAngleParameterization(s);
		assert(glm::length(us - us2) < epsilon);
	}

	vec2 sources[] = {
		{ 0, 0 },
		{ 0, 1000 },
		{ 0, 10000 },
		{ 1000, 0 },
		{ 10000, 0 },
		{ 10000, 10000 },
		{ 40000, 40000 },
	};
	// All destinations in this list shall be at the horizon
	vec2 destinations[] = {
		{ 0, H_Atm },
		{ 225136.6f, 76070.5f }, // Rotated -2 degrees
		{ -225136.6f, 76070.5f }, // Rotated 2 degrees
	};
	// Verify FetchTransmittanceToHorizon()
	for (auto pb : destinations) {
		for (auto pa : sources) {
			vec2 v = glm::normalize(pb-pa);
			vec3 transm = Transmittance(pa, pb);
			vec3 transm2 = FetchTransmittanceToHorizon(pa, v);
			float len = glm::length(transm - transm2);
			assert(len < 0.02f);
			// LPLOG("Transmittance from (%.0f %.0f) to (%.0f %.0f): %f, %f, %f (%f %f %f) diff %f", pa.x, pa.y, pb.x, pb.y, transm.r, transm.g, transm.b, transm2.r, transm2.g, transm2.b, len);
		}
	}
	vec2 pa(0,0);
	for (float ux=1; ux>=0; ux -= 0.15f) {
		vec2 pb(HorizontalDistParameterizedInverse(ux)/2, 0);
		vec3 transm = Transmittance(pa, pb);
		vec3 transm2 = FetchTransmittance(pa, pb);
		LPLOG("Transmittance dist %.0fm: %.2f, %.2f, %.2f (%.2f %.2f %.2f)", pb.x, transm.r, transm.g, transm.b, transm2.r, transm2.g, transm2.b);
	}

	for (float i=1; i>=0; i -= 0.15f) {
		vec2 pb(100000, HeightParameterizedInverse(i));
		vec3 transm = Transmittance(pa, pb);
		vec3 transm2 = FetchTransmittance(pa, pb);
		LPLOG("Transmittance height %.0fm: %.2f, %.2f, %.2f (%.2f %.2f %.2f)", pb.y, transm.r, transm.g, transm.b, transm2.r, transm2.g, transm2.b);
	}

	{
		vec2 pb(1, HeightParameterizedInverse(1));
		vec3 transm = Transmittance(pa, pb);
		vec3 transm2 = FetchTransmittance(pa, pb);
		LPLOG("Transmittance height %.0f, upwards: %.2f, %.2f, %.2f (%.2f %.2f %.2f)", pb.y, transm.r, transm.g, transm.b, transm2.r, transm2.g, transm2.b);
	}

	LPLOG("==========================================");
	vec2 v(0,-1); // Pointing towards pa
	LPLOG("Single scattering rotating vector, height 0, view angle to azimuth");
	for (int i=0; i<16; i++) {
		vec2 v = glm::rotate(vec2(0,-1), 90.0f/15.0f*i);
		vec3 mie, rayleigh;
		SingleScattering(vec2(0,0), vec2(-sunDir), v, mie, rayleigh);
		LPLOG("Single scattering view angle %.1f", glm::acos(-v.y)/2/glm::pi<float>()*360);
		LPLOG("Mie      (%f, %f, %f)", mie.r, mie.g, mie.b);
		LPLOG("Rayleigh (%f, %f, %f)", rayleigh.r, rayleigh.g, rayleigh.b);
	}

	LPLOG("==========================================");
	LPLOG("Single scattering, table walk");
	for (int heightIndex = 0; heightIndex < NHEIGHT; heightIndex += NHEIGHT/4) {
		float uHeight = float(heightIndex) / (NHEIGHT-1);
		float h = HeightParameterizedInverse(uHeight);
		LPLOG("Height %.1f ******", h);
		for (int viewAngleIndex = NVIEW_ANGLE-1; viewAngleIndex > 0; viewAngleIndex -= 4) {
			float uViewAngle = float(viewAngleIndex) / (NVIEW_ANGLE-1);
			float cosViewAngle = ViewAngleParameterizedInverse(uViewAngle, h);
			float viewAngle = glm::acos(cosViewAngle);
			LPLOG("View angle %.1f", viewAngle/2/glm::pi<float>()*360);
			for (int sunAngleIndex = NSUN_ANGLE-1; sunAngleIndex > 0; sunAngleIndex -= NSUN_ANGLE/4) {
				float uSunAngle = float(sunAngleIndex) / (NSUN_ANGLE-1);
				float cosSunAngle = SunAngleParameterizationInverse(uSunAngle);
				float sunAngle = glm::acos(cosSunAngle);
				LPLOG("[%d][%d][%d] Sun %.1f: %.3g %.3g %.3g", heightIndex, viewAngleIndex, sunAngleIndex, sunAngle/2/glm::pi<float>()*360, fScattering[heightIndex][viewAngleIndex][sunAngleIndex].r, fScattering[heightIndex][viewAngleIndex][sunAngleIndex].g, fScattering[heightIndex][viewAngleIndex][sunAngleIndex].b);
			}
		}
	}

	LPLOG("==========================================");
	LPLOG("Gathering");
	for (int heightIndex = 0; heightIndex < NHEIGHT; heightIndex += 16) {
		float uHeight = float(heightIndex) / (NHEIGHT-1);
		float h = HeightParameterizedInverse(uHeight);
		LPLOG("Height %.1f ******", h);
		for (int viewAngleIndex = NVIEW_ANGLE-1; viewAngleIndex > 0; viewAngleIndex -= 4) {
			float uViewAngle = float(viewAngleIndex) / (NVIEW_ANGLE-1);
			float cosViewAngle = ViewAngleParameterizedInverse(uViewAngle, h);
			float viewAngle = glm::acos(cosViewAngle);
			LPLOG("View angle %.1f", viewAngle/2/glm::pi<float>()*360);
			for (int sunAngleIndex = NSUN_ANGLE-1; sunAngleIndex > 0; sunAngleIndex -= 8) {
				float uSunAngle = float(sunAngleIndex) / (NSUN_ANGLE-1);
				float cosSunAngle = SunAngleParameterizationInverse(uSunAngle);
				vec2 l(glm::sqrt(1-cosSunAngle*cosSunAngle), -cosSunAngle); // Incoming sun, towards 'p'
				float sunAngle = glm::acos(cosSunAngle);
				vec3 gathered = GatheredLight(h, viewAngle, l);
				LPLOG("Sun %.1f: %.3f %.3f %.3f", sunAngle/2/glm::pi<float>()*360, gathered.r, gathered.g, gathered.b);
			}
		}
	}

	LPLOG("==========================================");
	v = vec2(0,-1); // Pointing towards pa
	LPLOG("1:st order multiple scattering rotating vector, height 0");
	for (int i=0; i<16; i++) {
		vec3 mie, rayleigh;
		MultipleScattering(vec2(0,0), vec2(-sunDir), v, mie, rayleigh);
		LPLOG("View angle %.1f", glm::acos(-v.y)/2/glm::pi<float>()*360);
		LPLOG("Mie      (%f, %f, %f)", mie.r, mie.g, mie.b);
		LPLOG("Rayleigh (%f, %f, %f)", rayleigh.r, rayleigh.g, rayleigh.b);
		v = glm::rotate(v, 90.0f/15.0f);
	}
}

GLuint Atmosphere::LoadTexture() {
	this->Init();

	GLuint textureId;
	glGenTextures(1, &textureId);
	LPLOG("Loading texture %d", textureId);
	glBindTexture(GL_TEXTURE_3D, textureId);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB16F, NHEIGHT, NVIEW_ANGLE, NSUN_ANGLE, 0, GL_RGB, GL_FLOAT, 0); // TODO: No data sent yet
	LPLOG("Load texture %d", textureId);
	checkError("Atmosphere::LoadTexture", false);
	return textureId;
}

void Atmosphere::Init() {
	if (fInitialized)
		return;

	this->PreComputeTransmittance();
	this->PreComputeSingleScattering();
	fInitialized = true;
}
