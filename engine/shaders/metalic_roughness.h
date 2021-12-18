#ifndef NG_FORWARD_H
#define NG_FORWARD_H


// COPYPASTA from a certain github gist

const float PI = 3.14159265358979323846;
const float ONE_OVER_PI = 1.0 / PI;
const vec3 dielectricSpecular = vec3(0.04);
const vec3 BLACK = vec3(0);

float sq(float x) { return x*x; }


// Height-Correlated Masking and Shadowing
// Smith Joint Masking-Shadowing Function
float GGX_Delta(float alphaSq, float NoX)
{
    return (-1. + sqrt(1. + alphaSq * (1. / (NoX * NoX) - 1.))) / 2.;
}

float SmithJoint_G(float alphaSq, float NoL, float NoV)
{
    return 1. / (1. + GGX_Delta(alphaSq, NoL) + GGX_Delta(alphaSq, NoV));
}

float GGX_D(float alphaSq, float NoH)
{
    float c = (sq(NoH) * (alphaSq - 1.) + 1.);
    return alphaSq / sq(c) * ONE_OVER_PI;
}

vec3 BRDF(vec3 base_color, float metallic, float roughness,
    vec3 light_direction, vec3 position, vec3 normal)
{
    vec3 L = light_direction;
    vec3 V = position;
    vec3 N = normal;
    
    vec3 H = normalize(L+V);

    float LoH = dot(L, H);
    float NoH = dot(N, H);
    float VoH = dot(V, H);
    float NoL = dot(N, L);
    float NoV = dot(N, V);

    vec3 F0 = mix(dielectricSpecular, base_color, metallic);
    vec3 cDiff = mix(base_color * (1. - dielectricSpecular.r),
                     BLACK,
                     metallic);

    float alphaSq = sq(sq(roughness));

    // Schlick's approximation
    vec3 F = F0 + (vec3(1.) - F0) * pow((1. - VoH), 5.);

    vec3 diffuse = (vec3(1.) - F) * cDiff * ONE_OVER_PI;

    float G = SmithJoint_G(alphaSq, NoL, NoV);

    float D = GGX_D(alphaSq, NoH);

    vec3 specular = (F * G * D) / (4. * NoL * NoV);
    return diffuse + specular;
}


#endif // NG_FORWARD_H
