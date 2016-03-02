#include "trainer.h"
#include <sstream>
#include <iostream>
#include "processor.h"
#include <fstream>
#include <iterator>
#include <assert.h>
#define Malloc(type,n) (type *)malloc((n)*sizeof(type))


Trainer::Trainer(std::string data_path, std::string out_name, 
	ProgramOptions& op, int t){
	if(out_name.empty()){
		out="results";
	}
	else{
		out=out_name;
	}
	
	files=FileSet((const char *) data_path.c_str(), true);
	options=op;
	features_vector=std::vector<Features*>(0);
	labels=std::vector<int>(0);
	label_map=std::map<std::string, int>();
	inv_label_map=std::map<int, std::string>();
	training_type=t;
}

size_t Trainer::alloc_svm(){
	size_t allocd=0;
	prob.l=nb_samples_total;
	prob.y=Malloc(double, labels.size());
	allocd+=labels.size()*sizeof(double);
	int k=0;
	int n=features_vector[0]->get_size();
	sample_width=n;
	prob.x = Malloc(svm_node*, prob.l);
	allocd+=prob.l*sizeof(svm_node*);
	for(k=0;k<nb_samples_total;k++){
		prob.x[k]=Malloc(svm_node, n);
		allocd+=n*sizeof(svm_node);
	}
	return allocd;
}


Trainer::~Trainer(){
	if(training_type==SVM){
		free_svm();
	}
}

void Trainer::free_svm(){
	if(prob.x){
		for(int k=0;k<nb_samples_total;k++){
			free(prob.x[k]);
		}
		free(prob.x);
	}
	if(prob.y)
		free(prob.y);

}

void Trainer::compute_all_features(){
	FileSetIterator it=files.begin();
	FileSetIterator end=files.end();
	SndCleaner sc(&options);
	for(;it!=end;++it){
		sc.reset(*it);
		std::string label = it.get_parent_name();
		if(label_map.find(label) == label_map.end()){ // we don't know this label
			label_map[label]=nb_labels;
			inv_label_map[nb_labels]=label;
			nb_labels++;
		}
		nr_fold=nb_labels;	
		compute_features(&sc, label);
	}
	
	if(training_type==TEXT_OUT){
		int k=0;
		std::ofstream out_file;
		out_file.open("./tests/result.txt", std::ofstream::out);
		for(std::vector<Features*>::iterator i=features_vector.begin();
			i!=features_vector.end();++i){
			if((*i)->is_valid()){
				(*i)->write_to_stream(out_file);
				out_file << inv_label_map[labels[k]] << "\n";
			}
			k++;
			delete *i;
		}
		out_file.close();
	}
	else if(training_type==COUT){
		int k=0;
		std::ostream stream(nullptr);
		stream.rdbuf(std::cout.rdbuf());
		for(std::vector<Features*>::iterator i=features_vector.begin();
			i!=features_vector.end();++i){
			if((*i)->is_valid()){
				(*i)->write_to_stream(stream);
			}
			k++;
			delete *i;
		}
	}
	else if(training_type==SVM){
		int k=0,l=0,m=0,j=0;
		// To be able to free correctly the problem
		// it is important to keep track of the 
		// initial number of samples
		nb_samples_total=features_vector.size();
		assert(nb_samples_total == labels.size());
		size_t s = alloc_svm();
		std::cout << s << " bytes have been allocated fo SVM" << std::endl;
		std::ofstream out_file;
		out_file.open("./tests/result.txt", std::ofstream::out);
		for(std::vector<Features*>::iterator i=features_vector.begin();
			i!=features_vector.end();++i){
			FeaturesIterator fend=(*i)->end();
			FeaturesIterator fi = (*i)->begin();
			if((*i)->is_valid()){
				(*i)->write_to_stream(out_file);
				out_file << inv_label_map[labels[k]] << "\n";
				l=0;
				j=0;
				for(;fi!=fend;++fi){
					if(*fi != 0){
						prob.x[m][l].index=j;
						prob.x[m][l].value=*fi;
						l++;
					}
					j++;
				}
				prob.x[m][l].index=-1;
				prob.y[m]=labels[k];

				m++;// increment m only if indeed added
			}
			k++;
			delete *i;
		}
		prob.l=m;
		std::cout << "finished computing svm features, " << nb_samples_total - prob.l << " samples have been dropped\n";
		
		std::cout << "number of samples is " << prob.l << std::endl;
		normalize_svm_data();		
	}

}



void Trainer::compute_features(SndCleaner* sc, std::string label){
	sc->open_stream();
	float global_wdw_sz=5000;
	float local_wdw_sz=20;
	float overlap=0;

	// number of local windows to make a global one
	int nb_frames_global=(int) global_wdw_sz/(local_wdw_sz*(1-overlap));
	// how many local windows have we stacked so far
	int nb_frames_current=0;


	int len,lread=0;
	int p=10;
	int size=sc->get_time_in_bytes(local_wdw_sz/1000);
	int16_t* pulled_data = (int16_t *) malloc(size);

	int flen=size;
	float error=0.;
	float coefs[p+1];
	
	std::vector<double> rms_vector(0);
	std::vector<double> zc_vector(0);
	std::vector<float> lpc_vector(0);
	rms_vector.reserve((int) global_wdw_sz/local_wdw_sz + 1);
	zc_vector.reserve((int) global_wdw_sz/local_wdw_sz + 1);
	lpc_vector.reserve((int) global_wdw_sz/local_wdw_sz + 1);

	static RmsFeatureParams rms_params;
	static ZcFeatureParams zc_params;
	static LpcFeatureParams lpc_params;

	RmsFeature* rms_feature;
	ZcFeature* zc_feature;
	LpcFeature* lpc_feature;

	Features* features;

	RingBuffer* data_buffer = sc->data_buffer;
	sc->fill_buffer();
	while(1){
		if(nb_frames_current >= nb_frames_global){
			features = new Features();

			rms_params.data=&rms_vector[0];
			rms_params.len=nb_frames_current;

			zc_params.data=&zc_vector[0];
			zc_params.len=nb_frames_current;

			lpc_params.data=&lpc_vector[0];
			lpc_params.len=nb_frames_current;

			rms_feature=new RmsFeature(rms_res, rms_crop);
			rms_feature->compute_feature((void *) &rms_params);

			zc_feature=new ZcFeature(zc_res, zc_crop);
			zc_feature->compute_feature((void *) &zc_params);

			lpc_feature=new LpcFeature(lpc_res, lpc_crop);
			lpc_feature->compute_feature((void *) &lpc_params);

			features->add_feature(rms_feature);
			//std::cout << "adding feature at " << rms_feature << std::endl;
			features->add_feature(zc_feature);
			//std::cout << "adding feature at " << zc_feature << std::endl;
			features->add_feature(lpc_feature);
			//std::cout << "adding feature at " << lpc_feature << std::endl;

			//(features->features).print();

			// dist_print(rms_feature->d);
			// dist_print(zc_feature->d);
			// dist_print(lpc_feature->d);
			//std::cout << "features vect at " << &features_vector << std::endl;
			features_vector.push_back(features);
			labels.push_back(label_map[label]);
			rms_vector.clear();
			zc_vector.clear();
			lpc_vector.clear();

			nb_frames_current=0;

		}
		if(rb_get_read_space(data_buffer, 0)==0){
			if(sc->fill_buffer() <=0)
				break;
		}

		len=flen;
		lread=0;
		while(len>0){
			// the cast only affects pulled_data so we can safely add lread without pointer arithmetic error
			//lread=rb_read_overlap(data_buffer, (uint8_t *) pulled_data, 0, (size_t) len, overlap); 
			lread=rb_read(data_buffer, (uint8_t *) pulled_data, 0, (size_t) len); 	
			len-=lread;
			if(lread==0 && sc->fill_buffer()<=0){
				break;
			}
		}
		rms_vector.push_back(root_mean_square(pulled_data, (flen-len)/2));
		zc_vector.push_back(zero_crossing_rate(pulled_data, (flen-len)/2));
		lpc_filter_optimized(pulled_data, coefs, size/2, p, &error);
		lpc_vector.push_back(error);
		// if(error != error){
		// 	std::cout << "nan value" << std::endl;
		// }
		nb_frames_current++;
	}

	free(pulled_data);
}


void Trainer::do_cross_validation()
{
	if(training_type==SVM){
		int i;
		int total_correct = 0;
		double total_error = 0;
		double sumv = 0, sumy = 0, sumvv = 0, sumyy = 0, sumvy = 0;
		double *target = Malloc(double,prob.l);

		svm_cross_validation(&prob,&param,nr_fold,target);
		if(param.svm_type == EPSILON_SVR ||
		   param.svm_type == NU_SVR)
		{
			for(i=0;i<prob.l;i++)
			{
				double y = prob.y[i];
				double v = target[i];
				total_error += (v-y)*(v-y);
				sumv += v;
				sumy += y;
				sumvv += v*v;
				sumyy += y*y;
				sumvy += v*y;
			}
			printf("Cross Validation Mean squared error = %g\n",total_error/prob.l);
			printf("Cross Validation Squared correlation coefficient = %g\n",
				((prob.l*sumvy-sumv*sumy)*(prob.l*sumvy-sumv*sumy))/
				((prob.l*sumvv-sumv*sumv)*(prob.l*sumyy-sumy*sumy))
				);
		}
		else
		{
			for(i=0;i<prob.l;i++)
				if(target[i] == prob.y[i])
					++total_correct;
			printf("Cross Validation Accuracy = %g%%\n",100.0*total_correct/prob.l);
		}
		free(target);
	}
	
}


/*

	alloc_svm should imperatively be called before train_svm

*/
struct svm_model* Trainer::train_svm(){
	if(training_type != SVM){
		std::cerr << "illegal call to train_svm, trainer has not been opened in proper mode\n";
		exit(1);
	}
	param.svm_type = C_SVC;
	param.kernel_type = RBF;
	param.degree = 3;
	param.gamma = 0;	// 1/num_features
	param.coef0 = 0;
	param.nu = 0.5;
	param.cache_size = 100;
	param.C = 1;
	param.eps = 1e-3;
	param.p = 0.1;
	param.shrinking = 1;
	param.probability = 0;
	param.nr_weight = 0;
	param.weight_label = NULL;
	param.weight = NULL;

	const char *error_msg;

	error_msg = svm_check_parameter(&prob,&param);

	if(error_msg)
	{
		fprintf(stderr,"ERROR: %s\n",error_msg);
		exit(1);
	}
	do_cross_validation();
	model = svm_train(&prob,&param);

	return model;

}

void Trainer::print_problem(){
	std::cout << "printing problem at " << &prob << "\n";
	for(int k=0;k<prob.l;k++){
		std::cout << prob.y[k] << " " << k << " ";
		int idx=0;
		while(1){
			std::cout << "(" << prob.x[k][idx].index << "," << prob.x[k][idx].value << ")";
			if(prob.x[k][idx].index==-1)
				break;
			if(idx > sample_width){
				std::cerr << "Format error in svm data" << std::endl;
				exit(1);
			}
			idx++;
		}
		std::cout << "\n\n";
	}
}


void Trainer::normalize_svm_data(){
	// print_problem();
	std::cout << "normalizing problem at " << &prob << "\n";
	double* tmp = (double *) calloc(sample_width, sizeof(double));
	int* nb = (int *) calloc(sample_width, sizeof(int));
	double* min = (double *) calloc(sample_width, sizeof(double));
	for(int u=0;u<sample_width;u++)
		min[u]=-1;
	double* max = (double *) calloc(sample_width, sizeof(double));
	int idx=0;
	double val=0.;
	int i=0;
	int k;
	for(k=0;k<prob.l;k++){
		i=0;
		while(1){
			if(prob.x[k][i].index==-1)
				break;
			if(i > sample_width){
				std::cerr << "Format error in svm data" << std::endl;
				exit(1);
			}
			idx = prob.x[k][i].index;
			val = prob.x[k][i].value;
			tmp[idx]+=val;
			nb[idx]++;
			
			if(min[idx] > val | min[idx]==-1)
				min[idx] = val;
			else if(max[idx] < val)
				max[idx] = val;

			i++;
		}
	}
	// for(int k = 0;k<sample_width;k++){
	// 	std::cout << tmp[k] << std::endl;
	// }


	for(k=0;k<prob.l;k++){
		i=0;
		while(1){
			if(prob.x[k][i].index==-1)
				break;
			if(i > sample_width){
				std::cerr << "Format error in svm data" << std::endl;
				exit(1);
			}
			double val = prob.x[k][i].value;
			int idx = prob.x[k][i].index;
			double mval = tmp[idx]/nb[idx];
			prob.x[k][i].value = (val - mval)/(max[idx] - min[idx]);
			i++;
		}
	}
	print_problem();
	free(tmp);
	free(min);
	free(max);
	free(nb);
}
