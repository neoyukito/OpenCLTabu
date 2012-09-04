/*
 * Solver.cpp
 *
 *  Created on: 14-08-2012
 *      Author: donty
 */

#include "Solver.h"

namespace tabu {

Solver::Solver(unsigned int max_iterations, int diversification_param,
		unsigned int n_machines, unsigned int n_parts, unsigned int n_cells,
		unsigned int max_machines_cell, Matrix *incidence_matrix,
		int tabu_turns) {

	this->current_solution = NULL;
	this->global_best = NULL;
	this->global_best_cost = -1;
	this->tabu_list = NULL;
	this->n_iterations = 0;

	this->incidence_matrix = incidence_matrix;
	this->max_iterations = max_iterations;
	this->diversification_param = diversification_param;
	this->n_machines = n_machines;
	this->n_parts = n_parts;
	this->n_cells = n_cells;
	this->max_machines_cell = max_machines_cell;
	this->tabu_turns = tabu_turns;
	this->tabu_list_max_length = tabu_list_max_length;
}

Solver::~Solver() {
}

void Solver::set_incidence_matrix(Matrix *incidence_matrix) {
	this->incidence_matrix = incidence_matrix;
}

long Solver::get_cost(Solution *solution) {

	long cost = 0;

	for(unsigned int i=0;i<n_machines;i++){ // sum i=1...M
		for(unsigned int j=0;j<n_parts;j++){ // sum j=1...P
			if(incidence_matrix->getMatrix()[i][j] == 1){ // a_ij = 1
				for(unsigned int k=0;k<n_cells;k++){ // sum k=1...C
					if(solution->cell_vector[i] != (signed int)k){ //(1 - y_ik)
						//(z_jk)
						for(unsigned int i_=0;i_<n_machines;i_++){
							if( i != i_ &&
								solution->cell_vector[i_] == (signed int)k
								&& incidence_matrix->getMatrix()[i_][j] == 1
							){
								cost++;
							}
						}

					}
				}
			}
		}
	}

	// infactible penalization

	/*
	// y_ik = 1
	for(unsigned int i=0;i<n_machines;i++){ // sum i=1...M
		int in_cells = 0;
		for(unsigned int k=0;k<n_cells;k++){ // sum k=1...C
			if(solution->cell_vector[i] == (signed int)k){
				in_cells++;
			}
		}
		cost += (1 - in_cells) * n_parts;
	}
	 */

	//z_jk = 1
	for(unsigned int j=0;j<n_parts;j++){
		int in_cells = 0;
		for(unsigned int k=0;k<n_cells;k++){ // sum k=1...C
			//(z_jk)
			for(unsigned int i_=0;i_<n_machines;i_++){
				if(solution->cell_vector[i_] == (signed int)k
					&& 	incidence_matrix->getMatrix()[i_][j] == 1
				){
					in_cells++;
					break;
				}
			}
		}
		if(in_cells > 1)
			cost += (in_cells - 1) * n_parts;
		else
			cost += (1 - in_cells) * n_parts;
	}


	for(unsigned int k=0;k<n_cells;k++){
		int machines_cell = 0;
		for(unsigned int i=0;i<n_machines;i++){
			if(solution->cell_vector[i] == (signed int)k){
				machines_cell++;
			}
		}
		// y_ik <= Mmax
		if((unsigned int)machines_cell > max_machines_cell){
			cost += (machines_cell - max_machines_cell) * n_parts;
		}
	}

	return cost;
}

void Solver::init() {

	srand ( time(NULL) );

	// initial solution
	current_solution = new Solution(n_machines);

	unsigned int i=0;
	unsigned int cell = 0;
	while(i<n_machines){
		unsigned int j = 0;
		while(j<max_machines_cell && i<n_machines){
			current_solution->cell_vector[i] = cell;
			j++;
			i++;
		}
		cell++;
	}

	for(unsigned int i=0;i<n_machines;i++){
		int j = rand()%n_machines;
		int aux = current_solution->cell_vector[i];

		current_solution->cell_vector[i] = current_solution->cell_vector[j];
		current_solution->cell_vector[j] = aux;
	}

	// set global_best
	global_best = current_solution->clone();

	// set global_best_cost
	global_best_cost = get_cost(global_best);

	//tabu list
	tabu_list = new TabuList(n_machines, tabu_turns);
}

void Solver::local_search(){

	// moves : vector permutation

	Solution *initial_solution = current_solution;

	int costs[n_machines];
	for(unsigned int i=0;i<n_machines;i++)
		costs[i] = -1;
	//std::fill( costs, costs+n_machines, -1);


	int best_cost = -1;
	Solution *local_best = NULL;

	for(unsigned int i=0;i<n_machines;i++){
		for(unsigned int j=i+1;j<n_machines;j++){

			Solution *sol = initial_solution->clone();
			bool delete_sol = true;

			sol->exchange(i,j);

			int cost = get_cost(sol);

			// ------ print solution -------
			//print_solution(sol);
			// ------ print solution -------

			if(costs[i] < 0 || cost < costs[i])
				costs[i] = cost;

			if(best_cost < 0 || ((cost < best_cost
					&& !tabu_list->is_tabu(sol)
					)
				&& rand()%100 < 90)
			){
				if(local_best != NULL)
					delete local_best;

				local_best = sol;
				best_cost = costs[i];
				delete_sol = false;
			}

			if(cost < global_best_cost){
				delete global_best;
				global_best = sol->clone();
				global_best_cost = cost;
				delete_sol = false;
			}

			if(delete_sol == true)
				delete sol;
		}
	}

	delete current_solution;
	current_solution = local_best;
	tabu_list->add_tabu(current_solution);
}

void Solver::global_search() {

	int i = 0;
	while(i<diversification_param){

		int j = rand()%n_machines;
		int k = rand()%n_machines;
		int i_ = i%n_machines;

		if(j==k)
			continue;

		// replace items
		int aux = current_solution->cell_vector[i_];
		current_solution->cell_vector[i_] = current_solution->cell_vector[j];
		current_solution->cell_vector[j] = aux;
		i++;
	}

	i = 0;
	while(i<(diversification_param/2)+1){

		int j = rand()%n_machines;

		// replace items
		int k = rand()%n_cells;
		current_solution->cell_vector[j] = k;
		i++;
	}

	int cost = get_cost(current_solution);
	if(cost < global_best_cost){
		delete global_best;
		global_best = current_solution->clone();
		global_best_cost = cost;
	}

}

Solution *Solver::solve(){

	std::ofstream out_file ("out.txt");
	if (!out_file.is_open()) {
		std::cout << "Unable to open file";
	}

	Solver::init();
	// ------ print solution -------
	print_solution(current_solution);
	//print_file_solution(0, current_solution, out_file);
	// ------ print solution -------
	for(unsigned int i=0;i<max_iterations;i++){

		std::cout << "----- iteration "<< i << " ------" << std::endl;

		local_search();

		// ------ print solution -------
		std::cout << "local search  ";
		print_solution(current_solution);
		print_file_solution(i, current_solution, out_file);
		// ------ print solution -------

		global_search();

		// ------ print solution -------
		std::cout << "global search ";
		print_solution(current_solution);
		//print_file_solution(i, current_solution, out_file);
		// ------ print solution -------

		tabu_list->update_tabu();

		if(global_best_cost == 0)
			break;
	}

	return global_best;
}

void Solver::print_solution(Solution *sol){
	for(unsigned int j=0;j<n_machines;j++)
		std::cout<< sol->cell_vector[j] << " ";
	std::cout<< "  cost: "<< get_cost(sol);
	std::cout<< "  global best: "<< global_best_cost;
	std::cout << std::endl;
}

void Solver::print_file_solution(unsigned int iteration, Solution* sol, std::ofstream &file) {

	// gnuplot
	//plot "out.txt" using 1:2 title "óptimos locales" with lines,"out.txt" using 1:3 title "óptimo global" with lines

	if(file.is_open())
		file << iteration << " " << get_cost(sol) << " " << global_best_cost << "\n";
}

} /* namespace tabu */