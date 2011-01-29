// 3D World - OpenGL CS184 Computer Graphics Project - Sphere visibility code and shadow driver
// by Frank Gennari
// 4/17/02

#include "3DWorld.h"
#include "mesh.h"
#include "physics_objects.h"


// better when moving the sun/moon, with very large number of trees
// worse with sparse cobjs, and requires tree cobjs for tree shadows
bool const RAYCAST_OBJ_SHAD = 0;
bool const DISABLE_SHADOWS  = 0;
bool const SHOW_SHADOW_TIME = 0;
int const FAST_LIGHT_VIS    = 1;
float const NORM_VIS_EXTEND = 0.02;


float tan_term(1.0), atan_tan_term(1.0), sin_term(1.0);
point ocean;
pos_dir_up camera_pdu;

extern bool combined_gu;
extern int window_width, window_height, do_zoom, ocean_set, display_mode, shadow_detail, ground_effects_level;
extern float zmin, zmax, czmin, czmax, zbottom, ztop, sun_rot, moon_rot;
extern point sun_pos, moon_pos, litning_pos;
extern lightning l_strike;
extern obj_type object_types[];



bool is_visible_to_light_cobj(point const &pos, int light, float radius, int cobj, int skip_dynamic, int *cobj_ix) { // slow...

	int index;
	point lpos;
	if (!get_light_pos(lpos, light)) return 0;

	if (lpos.z < czmax || pos.z < czmax) {
		if (!coll_pt_vis_test(pos, lpos, 1.5*radius, index, cobj, skip_dynamic, 0)) { // test alpha?
			if (cobj_ix) *cobj_ix = index;
			return 0; // test collision objects
		}
	}
	return is_visible_from_light(pos, lpos, 1+FAST_LIGHT_VIS); // test mesh
}


bool coll_pt_vis_test(point pos, point pos2, float dist, int &index, int cobj, int skip_dynamic, int test_alpha) {

	float const minz(skip_dynamic ? max(zbottom, czmin) : zbottom);
	float const maxz(skip_dynamic ? czmax : (ztop + Z_SCENE_SIZE));
	// *** note that this will not work with tree and other cobjs if regenerated ***
	if (!do_line_clip_scene(pos, pos2, minz, maxz)) return 1; // assumes pos is in the simulation region
	vector3d const vcf(pos2, pos);
	float const range(vcf.mag());
	if (range < TOLERANCE) return 1; // too close
	return (!check_coll_line((pos + vcf*(dist/range)), pos2, index, cobj, skip_dynamic, test_alpha));
}


float calc_half_angle(int is_zoomed) {

	return 0.5*TO_RADIANS*PERSP_ANGLE*(is_zoomed ? 1.0/ZOOM_FACTOR : 1.0);
}


// this is not completely correct, approximating perspective view volume as cone
void calc_view_test_terms(float &tterm, float &sterm, bool is_zoomed) {

	float const angle(calc_half_angle(is_zoomed));
	float const A((float)window_width/(float)window_height); // aspect ratio
	tterm = tanf(angle)*sqrt(1.0 + A*A);
	sterm = sinf(angle);
}


void calc_viewing_cone() {

	calc_view_test_terms(tan_term, sin_term, (do_zoom != 0));
	atan_tan_term = atanf(tan_term);
}


void set_camera_pdu() {

	camera_pdu = pos_dir_up(get_camera_pos(), cview_dir, up_vector, tan_term, sin_term, NEAR_CLIP, FAR_CLIP);
}


pos_dir_up::pos_dir_up(point const &p, vector3d const &d, vector3d const &u, float t, float s, float n, float f)
		: pos(p), dir(d), upv(u), tterm(t), sterm(s), tterm_sq2_inv(2.0/(tterm*tterm)),
		near_(n), far_(f), A(double(window_width)/double(window_height))
{
	assert(near_ >= 0.0 && far_ > 0.0 && far_ > near_);
	assert(dir != zero_vector);
	orthogonalize_dir(upv, dir, upv_, 1);
	cross_product(dir, upv_, cp);
}


// view frustum check: dir and upv must be normalized - checks view frustum
bool pos_dir_up::sphere_visible_test(point const &pos_, float radius) const {

	vector3d const pv(pos_, pos);
	if (dot_product(dir, pv) < 0.0) return (pv.mag_sq() < max(1.0, A*A)*radius*radius*tterm_sq2_inv); // optimization
	float const dist(pv.mag()*sterm);
	if (fabs(dot_product(upv_, pv)) > (  dist + radius)) return 0; // y-direction (up)
	if (fabs(dot_product(cp,   pv)) > (A*dist + radius)) return 0; // x-direction
	return ((dist + radius) > near_ && (dist - radius) < far_);
}


bool sphere_cobj_occluded(point const &viewer, point const &sc, float radius) {

	if (radius*radius/p2p_dist_sq(viewer, sc) < 1.0E-6) { // small and far away
		return cobj_contained(viewer, &sc, 1, -1);
	}
	point pts[8];
	
	for (unsigned i = 0; i < 8; ++i) { // really only need 4 points
		for (unsigned j = 0; j < 3; ++j) {
			pts[i][j] = sc[j] + ((i&(1<<j)) ? -1.0 : 1.0)*radius;
		}
	}
	return cobj_contained(viewer, pts, 8, -1);
}


/*
conditions under which an object is viewable:
	0. inside perspective view volume
	1. on correct side of surface mesh
	2. real visibility with mesh and cobjs
	3. cobj vis check with center
	4. cobj vis check with center and top
	5. cobj vis check with all 7 points
*/
// dir must be normalized
bool sphere_in_view(pos_dir_up const &pdu, point const &pos, float radius, int max_level) {

	point const &viewer(pdu.pos);

	if (ocean_set) {
		if (((fabs(pos.x) - radius) > ocean.x || (fabs(pos.y) - radius) > ocean.y)) return 0; // case 1
		
		if (is_over_mesh(pos)) { // added new check
			if (viewer.z < ocean.z && (pos.z - radius) > ocean.z) return 0; // case 3a
			if (viewer.z > ocean.z && (pos.z + radius) < ocean.z) return 0; // case 3b
		}
	}
	if (!pdu.sphere_visible_test(pos, radius))        return 0;
	if (max_level == 0 || world_mode != WMODE_GROUND) return 1;
	
	if (is_over_mesh(viewer)) {
		float const zmax(pos.z + radius);
		int const cxpos(get_xpos(viewer.x)), cypos(get_ypos(viewer.y));

		if (!point_outside_mesh(cxpos, cypos)) {
			float const above_mesh_dist(viewer.z - mesh_height[cypos][cxpos]);

			if (fabs(above_mesh_dist) > object_types[SMILEY].radius) { // don't use this test when the viewer is close to the mesh
				int const pxpos1(get_xpos(pos.x - radius)), pxpos2(get_xpos(pos.x + radius));
				int const pypos1(get_ypos(pos.y - radius)), pypos2(get_ypos(pos.y + radius));

				if (pxpos1 >= 0 && pypos1 >= 0 && pxpos2 < MESH_X_SIZE && pypos2 < MESH_Y_SIZE) {
					bool const c_above_m(above_mesh_dist > 0.0); // viewer above mesh
					int flag(1);

					for (int i = pypos1; flag && i <= pypos2; ++i) { // works better for small radius spheres
						for (int j = pxpos1; j <= pxpos2; ++j) {
							if ((zmax < mesh_height[i][j]) ^ c_above_m) {flag = 0; break;}
						}
					}
					if (flag) return 0; // case 6
				}
			}
		}
	}
	if (max_level == 1) return 1;
	if ((display_mode & 0x08) && sphere_cobj_occluded(viewer, pos, radius)) return 0; // intersect view with cobjs
	if (!sphere_visible_to_pt(viewer, pos, radius)) return 0; // intersect view with mesh
	if (max_level == 2) return 1;
	
	// do collision object visibility test (not guaranteed to be correct, typically used only with smileys)
	// *** might be unnecessary with real cobj tests ***
	float ext_dist(1.2*object_types[SMILEY].radius);
	if (dist_less_than(viewer, pos, ext_dist)) return 1; // too close to sphere
	int index;
	point qp[5];
	unsigned const nrays((radius == 0.0 || max_level == 3) ? 1 : ((max_level == 4) ? 2 : 5));
	get_sphere_border_pts(qp, pos, viewer, radius, nrays);

	for (unsigned i = 0; i < nrays; ++i) { // can see through transparent objects
		if (coll_pt_vis_test(qp[i], viewer, ext_dist, index, -1, 0, 1)) return 1;
	}
	return 0; // case 7
}


int get_light_pos(point &lpos, int light) {

	if (light == LIGHT_SUN) {
		lpos = sun_pos;
		return 1;
	}
	else if (light == LIGHT_MOON) {
		lpos = moon_pos;
		return 1;
	}
	return 0;
}


inline bool back_face_test(int xpos, int ypos, point const &lpos) {

	point pos;
	get_matrix_point(xpos, ypos, pos);
	return (dot_product_ptv(vertex_normals[ypos][xpos], lpos, pos) < 0.0); // back-face culling
}


bool light_visible_from_vertex(int xpos, int ypos, point const &lpos, int fast) {

	point pos;
	get_matrix_point(xpos, ypos, pos);
	pos += vertex_normals[ypos][xpos]*NORM_VIS_EXTEND; // small offset so that line does not intersect with the starting surface
	return (!line_intersect_mesh(lpos, pos, fast));
}


class mesh_shadow_gen {

	float const *mh;
	unsigned char *smask;
	float const *sh_in_x, *sh_in_y;
	float *sh_out_x, *sh_out_y;
	int xsize, ysize;
	vector3d dir;

	void trace_shadow_path(point v1) {
		assert(smask != NULL);
		float const dist(2.0*XY_SUM_SIZE/sqrt(dir.x*dir.x + dir.y*dir.y));
		point v2(v1 + vector3d(dir.x*dist, dir.y*dist, 0.0));
		float const d[3][2] = {{-X_SCENE_SIZE, get_xval(xsize)}, {-Y_SCENE_SIZE, get_yval(ysize)}, {zmin, zmax}};
		if (!do_line_clip(v1, v2, d)) return; // edge case ([zmin, zmax] should contain 0.0)
		int const xa(get_xpos(v1.x)), ya(get_ypos(v1.y)), xb(get_xpos(v2.x)), yb(get_ypos(v2.y));
		int const dx(xb - xa), dy(yb - ya), steps(max(abs(dx), abs(dy)));
		double const xinc(dx/(double)steps), yinc(dy/(double)steps);
		bool const dim(fabs(dir.x) < fabs(dir.y));
		double x(xa), y(ya);
		bool inited(0);
		point cur(all_zeros);
		int xstart(0), ystart(0), xend(xsize-1), yend(ysize-1);
		if (dir.x <= 0.0) swap(xstart, xend);
		if (dir.y <= 0.0) swap(ystart, yend);

		for (int k = 0; ; ++k) { // DDA algorithm
			int const xp((int)x), yp((int)y);
			
			if (xp >= 0 && yp >= 0 && xp < xsize && yp < ysize) {
				int const xp1(min(xsize-1, xp+1)), yp1(min(ysize-1, yp+1));
				float const xpi(x - (float)xp), ypi(y - (float)yp);
				float const mh00(mh[yp*xsize+xp]), mh01(mh[yp*xsize+xp1]), mh10(mh[yp1*xsize+xp]), mh11(mh[yp1*xsize+xp1]);
				float const mh((1.0 - xpi)*((1.0 - ypi)*mh00 + ypi*mh10) + xpi*((1.0 - ypi)*mh01 + ypi*mh11));
				point const pt((-X_SCENE_SIZE + DX_VAL*x), (-Y_SCENE_SIZE + DY_VAL*y), mh);

				// use starting shadow height value
				if (sh_in_y != NULL && xp == xstart && sh_in_y[yp] > MESH_MIN_Z) {
					cur    = point(pt.x, pt.y, sh_in_y[yp]);
					inited = 1;
				}
				else if (sh_in_x != NULL && yp == ystart && sh_in_x[xp] > MESH_MIN_Z) {
					cur    = point(pt.x, pt.y, sh_in_x[xp]);
					inited = 1;
				}
				float const shadow_z((pt[dim] - cur[dim])*dir.z/dir[dim] + cur.z);

				if (inited && shadow_z > pt.z) { // shadowed
					smask[yp*xsize+xp] |= MESH_SHADOW;
					// set ending shadow height value
					if (sh_out_y != NULL && xp == xend) sh_out_y[yp] = shadow_z;
					if (sh_out_x != NULL && yp == yend) sh_out_x[xp] = shadow_z;
				}
				else {
					cur = pt; // update point
				}
				inited = 1;
			}
			else if (k > steps) {
				break;
			}
			x += xinc;
			y += yinc;
		}
	}

public:
	mesh_shadow_gen(float const *const h, unsigned char *sm, int xsz, int ysz, float const *shix, float const *shiy, float *shox, float *shoy)
		: mh(h), smask(sm), sh_in_x(shix), sh_in_y(shiy), sh_out_x(shox), sh_out_y(shoy), xsize(xsz), ysize(ysz) {
		assert(mh != NULL && smask != NULL);
	}

	void run(point const &lpos) { // assumes light source directional/at infinity
		dir = (all_zeros - lpos).get_norm();
		float const xval(get_xval((dir.x > 0) ? 0 : xsize)), yval(get_yval((dir.y > 0) ? 0 : ysize));
		float xv(-X_SCENE_SIZE), yv(-Y_SCENE_SIZE);

		for (int y = 0; y < 2*ysize; ++y) { // half increments
			trace_shadow_path(point(xval, yv, 0.0));
			yv += 0.5*DY_VAL;
		}
		for (int x = 0; x < 2*xsize; ++x) { // half increments
			trace_shadow_path(point(xv, yval, 0.0));
			xv += 0.5*DX_VAL;
		}
	}
};


void calc_mesh_shadows(unsigned l, point const &lpos, float const *const mh, unsigned char *smask, int xsize, int ysize,
					   float const *sh_in_x, float const *sh_in_y, float *sh_out_x, float *sh_out_y)
{
	bool const no_shadow(l == LIGHT_MOON && combined_gu);
	bool const all_shadowed(!no_shadow && lpos.z < zmin);

	for (int i = 0; i < xsize*ysize; ++i) {
		smask[i] = (all_shadowed ? MESH_SHADOW : 0);
	}
	if (shadow_detail == 0 || no_shadow || FAST_VISIBILITY_CALC == 3) return;
	if (lpos.x == 0.0 && lpos.y == 0.0) return; // straight down = no mesh shadows
	mesh_shadow_gen(mh, smask, xsize, ysize, sh_in_x, sh_in_y, sh_out_x, sh_out_y).run(lpos);
}


void calc_visibility(char light_sources) {

	if (world_mode == WMODE_UNIVERSE) return;
	if (!(light_sources & TREE_ONLY)) clear_all_lightmaps(1);
	int const sst(SHOW_SHADOW_TIME && l_strike.enabled != 1);
	RESET_TIME;

	if (light_sources & TREE_ONLY) {
		add_cobj_shadows(light_sources);
		return;
	}
	update_sun_and_moon();
	if (world_mode == WMODE_INF_TERRAIN || DISABLE_SHADOWS || ground_effects_level == 0) return;
	point lpos[NUM_LIGHT_SRC];
	lpos[LIGHT_SUN]  = sun_pos;
	lpos[LIGHT_MOON] = moon_pos;

	for (unsigned l = 0; l < NUM_LIGHT_SRC; ++l) {
		if (!(light_sources & (1 << l))) continue;
		// we use the first element of mesh_height and shadow_mask assuming they are allocated as one large array
		calc_mesh_shadows(l, lpos[l], mesh_height[0], shadow_mask[l][0], MESH_X_SIZE, MESH_Y_SIZE);
	} // for l
	if (sst) PRINT_TIME(" Landscape shadow");
	if (!RAYCAST_OBJ_SHAD) add_cobj_shadows(light_sources);
	if (sst) PRINT_TIME(" Tree shadow");
	PRINT_TIME(" Shadow");
}



