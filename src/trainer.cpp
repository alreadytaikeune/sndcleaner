#include "trainer.h"
#include <sstream>
#include <iostream>
#include "processor.h"
#include <fstream>
#include "libsvm/svm.h"

Trainer::Trainer(std::string data_path, std::string lab, ProgramOptions& op){
	label=lab;
	files=FileSet((const char *) data_path.c_str(), true);
	options=op;
}


void Trainer::compute_all_features(){
	FileSetIterator it=files.begin();
	FileSetIterator end=files.end();
	SndCleaner* sc;
	std::ostream stream(nullptr);
	std::ofstream out_file;
	out_file.open("./tests/"+ label + ".txt", std::ofstream::out);
	stream.rdbuf(std::cout.rdbuf());
	std::vector<Features*> features_vector(0);
	for(;it!=end;++it){
		options.filename=*it;
		std::cout << "training with " << options.filename << std::endl;
		sc=new SndCleaner(&options);
		compute_features(sc, features_vector);
		delete sc;
	}

	for(std::vector<Features*>::iterator i=features_vector.begin();
		i!=features_vector.end();++i){
		if((*i)->is_valid()){
			(*i)->write_to_stream(out_file);
			out_file << label << "\n";
		}
		delete *i;
	}
	out_file.close();

}



void Trainer::compute_features(SndCleaner* sc, std::vector<Features*>& features_vector){
	std::cout << "computing features of " << sc->get_filename() << std::endl;
	sc->open_stream();
	float global_wdw_sz=1000;
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

	RmsFeatureParams rms_params;
	ZcFeatureParams zc_params;
	LpcFeatureParams lpc_params;

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