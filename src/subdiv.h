// 3D World
// by Frank Gennari
// 1/8/06
#ifndef _SUBDIV_H_
#define _SUBDIV_H_

#include "3DWorld.h"


class sd_sphere_d; // forward reference
class upsurface;


class sphere_point_norm { // size = 12

	unsigned ndiv;
	point **points;
	vector3d **norms;

public:
	friend class sd_sphere_d;
	sphere_point_norm() : ndiv(0), points(NULL), norms(NULL) {}
	void alloc(unsigned ndiv_);
	void set_pointer_stride(unsigned ndiv_);
	void free();
};


class sd_sphere_d { // size = 40

	bool local;
	point pos;
	float radius, def_pert;
	float const *perturb_map;
	upsurface const *surf;
	sphere_point_norm spn;

public:
	sd_sphere_d() : local(0), pos(all_zeros), radius(0.0), perturb_map(NULL), surf(NULL) {}
	sd_sphere_d(point const &p, float r, int n, float const *pm=NULL, float dp=0.0, upsurface const *const s=NULL)
		: local(0) {set_data(p, r, n, pm, dp, s);}
	void gen_points_norms(float s_beg=0.0, float s_end=1.0, float t_beg=0.0, float t_end=1.0);
	float get_rmax() const;
	void draw_subdiv_sphere(point const &vfrom, int texture, bool disable_bfc, bool const *const render_map=NULL,
		float const *const exp_map=NULL, point const *const pt_shift=NULL, float expand=0.0,
		float s_beg=0.0, float s_end=1.0, float t_beg=0.0, float t_end=1.0, unsigned sv1=1) const;
	void draw_ndiv_pow2(unsigned ndiv) const;
	bool is_local() const {return local;}
	void set_data(point const &p, float r, int n, float const *pm, float dp=0.0, upsurface const *const s=NULL);
	void make_local_copy();
	void free_local_data();
	point    **get_points() const {return spn.points;}
	vector3d **get_norms()  const {return spn.norms; }

	bool equal(point const &p, float r, int n) const {
		return (p == pos && r == radius && n == spn.ndiv && spn.points != NULL); // test is_local() ?
	}
};


#endif

