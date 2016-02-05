#ifndef _TRAINER_H_
#define _TRAINER_H_
#include "sndcleaner.h"
#include "features.h"
#include "file_set.h"
#include <vector>
#include <map>
#include <string>
#include "libsvm/svm.h"


// int comp(std::string s1, std::string s2){
// 	return strcmp(s1.c_str(), s2.c_str());
// }

enum {COUT, TEXT_OUT, SVM};

class Trainer{
public:
	Trainer(std::string data_path, std::string out_name, ProgramOptions& op, int t);
	~Trainer();
	void compute_all_features();
	void compute_features(SndCleaner* sc, std::string label);
	struct svm_model* train_svm();
	void do_cross_validation();
	void set_training_type(int train);
	std::string get_label(int k);
	void print_problem();
	void normalize_svm_data();
protected:
	size_t alloc_svm();
	void free_svm();
private:
	FileSet files;
	ProgramOptions options;
	std::string out;
	std::vector<Features*> features_vector;
	std::vector<int> labels;
	std::map<std::string, int> label_map;
	std::map<int, std::string> inv_label_map;
	int nb_labels=0;
	int rms_res=5;
	int zc_res=5;
	int lpc_res=5;
	int training_type=SVM;
	int rms_crop=5;
	int zc_crop=5;
	int lpc_crop=5;
	struct svm_parameter param;
	struct svm_problem prob;
	struct svm_model *model;
	int nr_fold=2;
	// total number of samples without removing the non-valid
	int nb_samples_total=0;
	int sample_width=0;
};

#endif