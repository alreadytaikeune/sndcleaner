#include "utils.h"
#include <limits.h>
#include <Eigen/Dense>
#include <iostream>

/*
	TODO: to optimize with cache use
*/
unsigned binomial(unsigned n, unsigned k) {
	unsigned c = 1, i;
	if (k > n-k) k = n-k;  /* take advantage of symmetry */
	for (i = 1; i <= k; i++, n--) {
		if (c/i > UINT_MAX/n) return 0;  /* return 0 on overflow */
		c = c/i * n + c%i * n / i;  /* split c*n/i into (c/i*i + c%i)*n/i */
	}
	return c;
}

int bernoulli_cumulated_inv(unsigned n, float p, float a){
	if(p==0)
		return 0;
	int k=0;
	float tmp=0;
	while(tmp < a){
		tmp+=binomial(n,k)*pow(p,k)*pow(p,n-k);
		k++;
	}
	return k;

}


double solve_linear(double* matrix, double* coefs, double* vector, const int dim){
	typedef Eigen::Map<Eigen::MatrixXd> MatrixMapType;
	typedef Eigen::Map<Eigen::VectorXd> VectorMapType;
	MatrixMapType m(matrix, dim, dim);
	VectorMapType v(vector, dim);

	// std::cout << "m" << std::endl;
	// std::cout << m << "\n\n";
	// std::cout << "v" << std::endl;
	// std::cout << v << "\n\n";
	Eigen::VectorXd sol = m.colPivHouseholderQr().solve(v);
	//std::cout << m << "\n\n";
	for(int i=0;i<dim;i++){
		coefs[i] = sol[i];
	}

	return (m*sol-v).norm();
}

float solve_linearf(float* matrix, float* coefs, float* vector, const int dim){
	typedef Eigen::Map<Eigen::MatrixXf> MatrixMapType;
	typedef Eigen::Map<Eigen::VectorXf> VectorMapType;
	MatrixMapType m(matrix, dim, dim);
	VectorMapType v(vector, dim);

	std::cout << "m" << std::endl;
	std::cout << m << "\n\n";
	std::cout << "v" << std::endl;
	std::cout << v << "\n\n";
	Eigen::VectorXf sol = m.colPivHouseholderQr().solve(v);
	//std::cout << m << "\n\n";
	for(int i=0;i<dim;i++){
		coefs[i] = sol[i];
	}

	return (m*sol-v).norm();
}