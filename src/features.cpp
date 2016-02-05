#include "features.h"
#include "utils.h"
#include <iostream>
#include <sstream>

RmsFeature::RmsFeature(){
	res=10;
	crop=res;
	d = dist_alloc(res);
}

RmsFeature::RmsFeature(int r){
	res=r;
	crop=res;
	d = dist_alloc(res);
}

RmsFeature::RmsFeature(int r, int c){
	res=r;
	crop=c;
	d = dist_alloc(res);
}

RmsFeature::~RmsFeature(){
	std::cout << "destructor rms" << std::endl;
	dist_free(d);
}

void RmsFeature::compute_feature(void* param){
	RmsFeatureParams* p = (RmsFeatureParams*) param;

	double* data=p->data;
	int len = p->len;
	max_amp=max_abs(data, len);
	if(max_amp==0){
		std::cout << "Warning: max amp is 0" << std::endl;
		max_amp=1;
	}
	dist_set_norm(d, max_amp);
	dist_incrementd(d, data, len);
	m=mean(data, len);
	std_dev=std_deviation(data, len);
}

bool RmsFeature::is_valid(){
	if(max_amp < 1000)
		return false;
	return true;
}

void RmsFeature::write_to_stream(std::ostream& out){
	out << m << " ";
	out << std_dev << " ";
	float* dist = (float*) malloc(res*sizeof(float));
	dist_to_percentage(d, dist);
	for(int i=0; i<crop; i++){
		out << dist[i] << " ";
	}
	free(dist);
}


int RmsFeature::get_size(){
	// mean + stddev + crop samples of distrib
	return 2+crop;
}

int ZcFeature::get_size(){
	// mean + stddev + crop samples of distrib
	return 2+crop;
}

int LpcFeature::get_size(){
	// mean + stddev + crop samples of distrib
	return 2+crop;
}

double RmsFeature::get_nth_value(int n){
	if(n==0)
		return m;
	else if(n==1)
		return std_dev;
	else if(n<crop+2)
		return dist_get_ratio(d, n-2);
}

double ZcFeature::get_nth_value(int n){
	if(n==0)
		return m;
	else if(n==1)
		return std_dev;
	else if(n<crop+2)
		return dist_get_ratio(d, n-2);
}

double LpcFeature::get_nth_value(int n){
	if(n==0)
		return m;
	else if(n==1)
		return std_dev;
	else if(n<crop+2)
		return dist_get_ratio(d, n-2);
}



ZcFeature::ZcFeature(){
	res=10;
	crop=res;
	d = dist_alloc(res);
}

ZcFeature::ZcFeature(int r){
	res=r;
	crop=res;
	d = dist_alloc(res);
}

ZcFeature::ZcFeature(int r, int c){
	res=r;
	crop=c;
	d = dist_alloc(res);
}

ZcFeature::~ZcFeature(){
	dist_free(d);
}

void ZcFeature::compute_feature(void* param){
	ZcFeatureParams* p = (ZcFeatureParams*) param;

	double* data=p->data;
	int len = p->len;
	max_amp=max_abs(data, len);
	if(max_amp==0){
		std::cout << "Warning: max amp is 0" << std::endl;
		max_amp=1;
	}
	dist_set_norm(d, max_amp);
	dist_incrementd(d, data, len);
	m=mean(data, len);
	std_dev=std_deviation(data, len);
}

void ZcFeature::write_to_stream(std::ostream& out){
	out << m << " ";
	out << std_dev << " ";
	float* dist = (float*) malloc(res*sizeof(float));
	dist_to_percentage(d, dist);
	for(int i=0; i<crop; i++){
		out << dist[i] << " ";
	}
	free(dist);
}

bool ZcFeature::is_valid(){
	if(max_amp==0)
		return false;
	return true;
}


LpcFeature::LpcFeature(){
	res=10;
	crop=5;
	d = dist_alloc(res);
}

LpcFeature::LpcFeature(int r){
	res=r;
	crop=res;
	d = dist_alloc(res);
}

LpcFeature::LpcFeature(int r, int c){
	res=r;
	crop=c;
	d=dist_alloc(res);
}

LpcFeature::~LpcFeature(){
	dist_free(d);
}

void LpcFeature::compute_feature(void* param){
	LpcFeatureParams* p = (LpcFeatureParams*) param;

	float* data=p->data;
	int len = p->len;
	max_amp=max_abs(data, len);
	if(max_amp==0){
		std::cout << "Warning: max amp is 0" << std::endl;
		max_amp=1;
	}
	dist_set_norm(d, max_amp);
	dist_incrementf(d, data, len);
	m=mean(data, len);
	std_dev=std_deviation(data, len);
}

void LpcFeature::write_to_stream(std::ostream& out){
	out << m << " ";
	out << std_dev << " ";
	float* dist = (float*) malloc(res*sizeof(float));
	dist_to_percentage(d, dist);
	for(int i=0; i<crop; i++){
		out << dist[i] << " ";
	}
	free(dist);
}


bool LpcFeature::is_valid(){
	if(max_amp==0)
		return false;
	if(std_dev != std_dev)
		return false;
	if(m != m)
		return false;
	return true;
}

Features::Features(){
	features=List<Feature*>();
}

Features::~Features(){
}

void Features::add_feature(Feature* f){
	features.add(f);
}

void Features::write_to_stream(std::ostream& out){
	List<Feature*>* cur=&features;
	int i=0;
	while(cur){
		cur->data->write_to_stream(out);
		cur=cur->next;
		i++;
	}
}

bool Features::is_valid(){
	List<Feature*>* cur=&features;
	std::ostream stream(nullptr);
	stream.rdbuf(std::cout.rdbuf());
	while(cur){
		if(!cur->data->is_valid())
			return false;	
		cur=cur->next;
	}
	return true;
}


int Features::get_size(){
	List<Feature*>* cur=&features;
	int size=0;
	while(cur){
		size+=cur->data->get_size();
		cur=cur->next;
	}
	return size;
}
