uniform float x0, y0, dx, dy, rx, ry, zscale;

in vec3 vpos;

#include <simplex_noise.part>
//#include <noise_deriv.part>

void main() {

	float x = x0 + dx*vpos.x;
	float y = y0 + dy*vpos.y;
	float zval = gen_simplex_noise_height(x, y);
	//float zval = fbmd_9(8.0*vec2(x, y)).x;
	fg_FragColor = vec4(zscale*zval, 0.0, 0.0, 1.0); // only red channel is used
}

