// Instructional Glow for CubeNView: toon.frag, with the colour read from the
// mesh instead of a uniform. The N x N cube is one mesh whose vertices carry
// the colour of the sticker (or of the plastic) they belong to, so the shading
// is toon.frag's and the base colour is VAR_COLOR. Change one, change the other.
//
// Only the stickers take the rim and the glow: on the 3x3 the plastic round
// them is a separate body that no style touches, and the mesh has the plastic
// and the stickers in one colour attribute, so the plastic is told by its colour
// (uBodyColor, the colour the mesh was built with). Both are linear light, but
// not to the last digit (Qt linearises a colour property its own way), so the
// match is to within a few thousandths; no sticker is that dark.

float stickerness()
{
    vec3 off = abs(VAR_COLOR.rgb - uBodyColor.rgb);
    return max(off.r, max(off.g, off.b)) < 0.006 ? 0.0 : 1.0;
}

void MAIN()
{
    ROUGHNESS = 1.0;
    METALNESS = 0.0;
    SPECULAR_AMOUNT = 0.0;
    BASE_COLOR = vec4(VAR_COLOR.rgb * uTint, 1.0);

    vec3 viewDir = normalize(CAMERA_POSITION - VAR_WORLD_POSITION);
    float rim = 1.0 - max(dot(normalize(NORMAL), viewDir), 0.0);
    EMISSIVE_COLOR = uRimColor.rgb * pow(rim, uRimPower) * stickerness();

    // The active-layer highlight: a flat additive boost across the sticker.
    EMISSIVE_COLOR += uRimColor.rgb * uGlowStrength * 0.6 * stickerness();
}

void AMBIENT_LIGHT()
{
    DIFFUSE += VAR_COLOR.rgb * uTint * TOTAL_AMBIENT_COLOR;
}

void DIRECTIONAL_LIGHT()
{
    float ndotl = max(dot(normalize(NORMAL), TO_LIGHT_DIR), 0.0);
    float band = ndotl > 0.85 ? 1.0 : (ndotl > 0.4 ? 0.6 : 0.25);
    DIFFUSE += VAR_COLOR.rgb * uTint * LIGHT_COLOR * SHADOW_CONTRIB * band;
}
