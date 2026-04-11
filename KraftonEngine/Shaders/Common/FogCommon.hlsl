static const float FOG_DENSITY_SCALE = 0.1f;
static const float FOG_FAR_DEPTH_DISTANCE = 1000.0f;

float GetFogDensity(FogUniformParameters fog)
{
    return max(fog.ExponentialFogParameters.x, 0.0f);
}

float GetScaledFogDensity(FogUniformParameters fog)
{
    return GetFogDensity(fog) * FOG_DENSITY_SCALE;
}

float GetFogHeightFalloff(FogUniformParameters fog)
{
    return max(fog.ExponentialFogParameters.y, 0.0001f);
}

float GetFogStartDistance(FogUniformParameters fog)
{
    return max(fog.ExponentialFogParameters.w, 0.0f);
}

float GetFogHeight(FogUniformParameters fog)
{
    return fog.ExponentialFogParameters3.y;
}

float GetFogCutoffDistance(FogUniformParameters fog)
{
    return max(fog.ExponentialFogParameters3.w, 0.0f);
}

float GetFogMaxOpacity(FogUniformParameters fog)
{
    return saturate(1.0f - fog.ExponentialFogColorParameter.a);
}

float LinearizeFogViewDepth(float depth, float4x4 projection)
{
    const float proj33 = projection[2][2];
    const float proj43 = projection[3][2];
    return proj43 / min(depth - proj33, -0.0001f);
}

float ComputeFogHeightAttenuation(FogUniformParameters fog, float sampleHeight)
{
    const float relativeHeight = max(sampleHeight - GetFogHeight(fog), 0.0f);
    return exp(-relativeHeight * GetFogHeightFalloff(fog));
}

float ComputeFogWeightFromDistance(FogUniformParameters fog, float distanceToCamera, float heightAttenuation)
{
    const float startDistance = GetFogStartDistance(fog);
    const float cutoffDistance = GetFogCutoffDistance(fog);
    const float maxOpacity = GetFogMaxOpacity(fog);

    if (distanceToCamera <= startDistance)
    {
        return 0.0f;
    }

    if (cutoffDistance > 0.0f && distanceToCamera >= cutoffDistance)
    {
        return 0.0f;
    }

    const float effectiveDistance = distanceToCamera - startDistance;
    const float fogIntegral = effectiveDistance * GetScaledFogDensity(fog) * heightAttenuation;
    return min(saturate(1.0f - exp(-fogIntegral)), maxOpacity);
}
