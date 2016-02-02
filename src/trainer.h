#ifndef _TRAINER_H_
#define _TRAINER_H_
#include "sndcleaner.h"
#include "features.h"
#include "file_set.h"
#include <vector>


class Trainer{
public:
	Trainer(std::string data_path, std::string label, ProgramOptions& op);
	void compute_all_features();
	void compute_features(SndCleaner* sc, std::vector<Features *>& features_vector);
private:
	FileSet files;
	std::string label;
	ProgramOptions options;

	int rms_res=5;
	int zc_res=5;
	int lpc_res=10;

	int rms_crop=5;
	int zc_crop=5;
	int lpc_crop=5;
};

#endif