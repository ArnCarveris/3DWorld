uniform float height = 1.0;
uniform float dist_const = 10.0;
uniform float dist_slope = 0.5;
uniform float x1, y1, x2, y2, wx2, wy2, zmin, zmax, translate_y;
uniform sampler2D height_tex, shadow_normal_tex, weight_tex, noise_tex;

void main()
{
	gl_TexCoord[0] = gl_MultiTexCoord0;
	vec4 vertex = gl_Vertex;
	vertex.y   += translate_y;
	vertex.z   += zmin + (zmax - zmin)*texture2D(height_tex, vec2((vertex.x - x1)/(x2 - x1), (vertex.y - y1)/(y2 - y1))).r;
	vec2 tc     = vec2(vertex.x/wx2, vertex.y/wy2);

	if (enable_grass_wind) {
		vertex.xyz += get_grass_wind_delta(vertex.xyz, height);
	}
	vec4 epos   = gl_ModelViewMatrix  * vertex;
	gl_Position = gl_ProjectionMatrix * epos;
	gl_FogFragCoord = length(epos.xyz);
	float grass_weight = texture2D(weight_tex, tc).b;
	float noise_weight = texture2D(noise_tex,  vec2(10.0*gl_Color.r, 10.0*gl_Color.g)).r; // "hash" the color
	grass_weight   *= 1.0 - clamp(dist_slope*(gl_FogFragCoord - dist_const), 0.0, 1.0); // decrease weight far away from camera
	
	// calculate lighting
	vec4 shadow_normal = texture2D(shadow_normal_tex, tc);
	vec3 normal = normalize(gl_NormalMatrix * (2.0*shadow_normal.xyz - 1.0)) * shadow_normal.w; // eye space
	vec4 color  = gl_Color * gl_LightModel.ambient;
	if (enable_light0) color += add_light_comp_pos(normal, epos, 0);
	if (enable_light1) color += add_light_comp_pos(normal, epos, 1);
	color.a = ((grass_weight < noise_weight) ? 0.0 : color.a); // skip some grass blades by making them transparent
	gl_FrontColor  = color;
} 
