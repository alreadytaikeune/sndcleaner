#include "list.h"
#include "distribution.h"
#include <iostream>


typedef struct RmsFeatureParams{
	double * data;
	int len;
} RmsFeatureParams;


typedef struct ZcFeatureParams{
	double * data;
	int len;
} ZcFeatureParams;


typedef struct LpcFeatureParams{
	float * data;
	int len;
} LpcFeatureParams;


class Feature{
public:
	Feature(){};
	virtual void write_to_stream(std::ostream&) = 0;
	virtual void compute_feature(void* p) = 0;
	virtual bool is_valid() = 0;
};


class RmsFeature : public Feature{
public:
	RmsFeature();
	RmsFeature(int res);
	RmsFeature(int res, int crop);
	~RmsFeature();
	void write_to_stream(std::ostream&);
	void compute_feature(void* p);
	bool is_valid();
	distribution* d;
private:
	int res=10;
	int crop=-1;
	double m;
	double std_dev;
	double max_amp;
};

class ZcFeature : public Feature{
public:
	ZcFeature();
	ZcFeature(int res);
	ZcFeature(int res, int crop);
	~ZcFeature();
	void write_to_stream(std::ostream&);
	void compute_feature(void* p);
	bool is_valid();
	distribution* d;
private:
	int res=10;
	int crop=-1;
	double m;
	double std_dev;
	double max_amp;
};

class LpcFeature : public Feature{
public:
	LpcFeature();
	LpcFeature(int res);
	LpcFeature(int res, int crop);
	~LpcFeature();
	void write_to_stream(std::ostream&);
	void compute_feature(void* p);
	bool is_valid();
	distribution* d;
private:
	int res=10;
	int crop=-1;
	double m;
	double std_dev;
	float max_amp;
};

class Features{
public:
	Features();
	~Features();
	void add_feature(Feature* f);
	void write_to_stream(std::ostream&);
	bool is_valid();
	List<Feature*> features;
};






