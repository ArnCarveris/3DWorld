// 3D World Dynamic Particle class declaration
// by Frank Gennari
// 7/17/06
#ifndef _DYNAMIC_PARTICLE_H_
#define _DYNAMIC_PARTICLE_H_


class dynamic_particle { // size = 60

	bool moves, shadows, lighted, collides, chdir, gravity;
	int tid, cid;
	float radius, intensity, bwidth;
	point pos;
	vector3d velocity;
	colorRGBA color;

public:
	dynamic_particle();
	dynamic_particle(point const &p, float r, vector3d const &v, colorRGBA const &c, float i,
		float bw, int t, bool m, bool s, bool l, bool coll, bool cd, bool g);
	~dynamic_particle() {remove_cobj();}
	void gen_pos();
	void draw() const; // lights, color, texture, shadowed
	void apply_physics(float stepsize, int index); // begin_motion, move, random dir change, collision (mesh and cobjs), forces applied to?
	void add_light() const; // dynamic lights
	void add_cobj_shadows(bool light_in_front) const; // cobjs, dynamic objects
	void add_mesh_shadows() const;
	void add_stencil_shadows(point const &lpos, int &inverts) const;
	void add_cobj();
	void remove_cobj();
	point const &get_pos() const {return pos;}
};


class dynamic_particle_system { // size = 16

	vector<dynamic_particle>  particles;
	vector<vector<unsigned> > bins;
	int valid_frame;

public:
	dynamic_particle_system() : valid_frame(-1) {}
	void add_particle(dynamic_particle const &p) {particles.push_back(p);}
	size_t size() const {return particles.size();}
	void clear() {particles.clear();}
	void create_particles(unsigned num, bool only_if_empty);
	void draw() const;
	void apply_physics(float stepsize=1.0);
	void add_light() const;
	void add_cobj_shadows(bool light_in_front) const;
	void add_mesh_shadows() const;
	void add_stencil_shadows(point const &lpos, int &inverts) const;
	void build_lookup_matrix();
	bool ray_intersect(point const &p1, point const &p2, float &t, colorRGBA &color);
};


extern dynamic_particle_system d_part_sys;


#endif // _DYNAMIC_PARTICLE_H_


