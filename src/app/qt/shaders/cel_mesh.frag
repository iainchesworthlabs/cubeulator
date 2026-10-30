// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
// Borderlands Cel-Shading for CubeNView: cel.frag, with the colour read from the
// mesh instead of a uniform (see toon_mesh.frag). Change one, change the other.

void MAIN()
{
    ROUGHNESS = 1.0;
    METALNESS = 0.0;
    SPECULAR_AMOUNT = 0.0;
    BASE_COLOR = vec4(VAR_COLOR.rgb * uTint, 1.0);
}

void AMBIENT_LIGHT()
{
    DIFFUSE += VAR_COLOR.rgb * uTint * TOTAL_AMBIENT_COLOR;
}

void DIRECTIONAL_LIGHT()
{
    float ndotl = max(dot(normalize(NORMAL), TO_LIGHT_DIR), 0.0);
    float band = ndotl > 0.5 ? 1.0 : 0.35;
    DIFFUSE += VAR_COLOR.rgb * uTint * LIGHT_COLOR * SHADOW_CONTRIB * band;
}
