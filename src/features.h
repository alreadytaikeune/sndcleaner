#include "list.h"
#include "distribution.h"
#include <iostream>
#include "boost/filesystem.hpp"
#include <iterator>

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
	virtual int get_size() = 0;
	virtual double get_nth_value(int n) = 0;
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
	int get_size();
	double get_nth_value(int n);
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
	int get_size();
	double get_nth_value(int n);
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
	int get_size();
	double get_nth_value(int n);
	distribution* d;
private:
	int res=10;
	int crop=-1;
	double m;
	double std_dev;
	float max_amp;
};



class FeaturesIterator : boost::iterator_facade<FeaturesIterator, 
	double, boost::single_pass_traversal_tag>{
public:
	FeaturesIterator(List<Feature*>* f){
		features=f;
		if(features)
			current_feature=features->data;
		else
			current_feature=NULL;
	}

	FeaturesIterator(){
		features=NULL;
		current_feature=NULL;
	}

	FeaturesIterator& operator++(){
		if(nth < current_feature->get_size()-1){
			nth++;
		}
		else{
			nth=0;
			features=features->next;
			if(features)
				current_feature=features->data;
			else
				current_feature=NULL;
		}
		return *this;
	}

	FeaturesIterator operator++(int){
		FeaturesIterator tmp(*this);
		operator++();
		return tmp;
	}

	bool operator== (const FeaturesIterator& other) const{
		if(!current_feature){
			if(!other.current_feature){
				return true;
			}
			return false;
		}
		return (features == other.features && current_feature == other.current_feature
			 && nth==other.nth);
	}

	bool operator!=(const FeaturesIterator& other) const{
		return !operator==(other);
	}

	double operator*() const{
		return current_feature->get_nth_value(nth);
	}

	double operator->() const{
		return current_feature->get_nth_value(nth);
	}

private:
	List<Feature*>* features;
	Feature* current_feature;
	int nth=0;
};


class Features{
public:
	Features();
	~Features();
	void add_feature(Feature* f);
	void write_to_stream(std::ostream&);
	bool is_valid();
	int get_size();
	List<Feature*> features;

	FeaturesIterator begin(){
		return FeaturesIterator(&features);
	}
	FeaturesIterator end(){
		return FeaturesIterator();
	}

};






