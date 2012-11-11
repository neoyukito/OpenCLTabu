/*
 * ParallelSolver.cpp
 *
 *  Created on: 31-08-2012
 *      Author: donty
 */

#include "ParallelSolver.h"

namespace tabu {

ParallelSolver::ParallelSolver(unsigned int max_iterations,
		int diversification_param, unsigned int n_machines,
		unsigned int n_parts, unsigned int n_cells,
		unsigned int max_machines_cell, Matrix* incidence_matrix,
		int tabu_turns) :
		Solver(max_iterations, diversification_param, n_machines, n_parts,
				n_cells, max_machines_cell, incidence_matrix, tabu_turns) {

}

ParallelSolver::~ParallelSolver() {

}

void print_array(int *array,int size){

	for(int i=0;i<size;i++){
		std::cout << array[i] << " ";
	}
	std::cout << std::endl;
}

void print_2DArray(int *array, int fils, int cols){

	for(int i=0;i<fils;i++){
		for(int j=0;j<cols;j++){
			std::cout << array[i*cols+j] << " ";
		}
	}
	std::cout << std::endl;
}

void print_VarArray(int *array, int fils, int *cols, int maxcols){

	for(int i=0;i<fils;i++){
		for(int j=0;j<cols[i];j++){
			std::cout << array[i*maxcols+j] << " ";
		}
	}
	std::cout << std::endl;
}

void print_vector(std::vector<std::vector<int> > vector){

	for(std::vector<int>::size_type i = 0; i != vector.size(); i++) {
		for(std::vector<int>::size_type j = 0; j != vector[i].size(); j++) {
			std::cout << vector[i][j] << " ";
		}
	}
	std::cout << std::endl;
}

inline int different(int a,int b){

	int diff = a - b;

	diff = ((diff | (~diff + 1)) >> 31) & 1;

	return diff;
}

inline int equal(int a,int b){

	int equal = a - b;

	equal = ((equal | (~equal + 1)) >> 31) & 0;

	return equal;
}

inline int zero(int a){

	int zero = ((a | (~a + 1)) >> 31) & 0;
	return zero;
}

VariableMatrix* ParallelSolver::vector_to_var_mat(
		std::vector<std::vector<int> > vector, int max_cols) {

	VariableMatrix *mat = new VariableMatrix();

	int vector_size = vector.size();

	int *storage = new cl_int[vector_size*max_cols];
	mat->rows = vector.size();
	mat->cols = new cl_int[vector_size]; // cols en cada fila

	memset(storage,0,sizeof(cl_int)*vector_size*max_cols);
	memset(mat->cols,0,sizeof(cl_int)*vector_size);

	for(int i=0;i<(signed int)vector_size;i++){
		std::copy(vector[i].begin(), vector[i].begin()+vector[i].size(), storage+(i*max_cols));
		mat->cols[i] = vector[i].size();
	}
	mat->storage = storage;

    //-------------------------------------------------------------

    //print_array(storage, vector.size()*max_cols);
    //print_array(mat->cols, vector.size());

	return mat;
}

StaticMatrix* ParallelSolver::matrix_to_StaticMatrix(Matrix *mat) {

	StaticMatrix *smat = new StaticMatrix();

	int rows = mat->rows;
	int cols = mat->cols;

	int *storage = new cl_int[rows*cols];
	smat->rows = rows;
	smat->cols = cols;

	memset(storage,0,sizeof(cl_int)*rows*cols);

	for(int i=0;i<(signed int)mat->rows;i++){
		std::copy(mat->storage[i].begin(), mat->storage[i].begin()+mat->storage[i].size(), storage+(i*cols));
	}
	smat->storage = storage;

    //print_array(storage, rows*cols);

	return smat;
}

int ParallelSolver::OpenCL_init() {

    cl_int err;

    // Platform info
    std::vector<cl::Platform> platforms;

    //Getting Platform Information
    err = cl::Platform::get(&platforms);
    if(err != CL_SUCCESS)
    {
        std::cout << "Platform::get() failed (" << err << ")" << std::endl;
        exit(SDK_FAILURE);
    }

    std::vector<cl::Platform>::iterator i;
    if(platforms.size() > 0)
    {
        for(i = platforms.begin(); i != platforms.end(); ++i)
        {
            if(!strcmp((*i).getInfo<CL_PLATFORM_VENDOR>(&err).c_str(), "Advanced Micro Devices, Inc."))
            {
                break;
            }
        }
    }
    if(err != CL_SUCCESS)
    {
        std::cout << "Platform::getInfo() failed (" << err << ")" << std::endl;
        exit(SDK_FAILURE);
    }

    /*
     * If we could find our platform, use it. Otherwise pass a NULL and get whatever the
     * implementation thinks we should be using.
     */

    cl_context_properties cps[3] = { CL_CONTEXT_PLATFORM, (cl_context_properties)(*i)(), 0 };

    // Creating a context AMD platform
    context = cl::Context(CL_DEVICE_TYPE_CPU, cps, NULL, NULL, &err);
    if (err != CL_SUCCESS) {
        std::cout << "Context::Context() failed (" << err << ")\n";
        exit(SDK_FAILURE);
    }

    //Getting device info
    std::vector<cl::Device> devices = context.getInfo<CL_CONTEXT_DEVICES>();
    if (err != CL_SUCCESS) {
        std::cout << "Context::getInfo() failed (" << err << ")\n";
        exit(SDK_FAILURE);
    }
    if (devices.size() == 0) {
        std::cout << "No device available\n";
        exit(SDK_FAILURE);
    }

    // Loading and compiling CL source
    streamsdk::SDKFile file;
    if (!file.open("TabuSolver_Kernels.cl")) {
         std::cout << "We couldn't load CL source code\n";
         exit(SDK_FAILURE);
    }

    cl::Program::Sources sources(1, std::make_pair(file.source().data(), file.source().size()));

    cl::Program program = cl::Program(context, sources, &err);
    if (err != CL_SUCCESS) {
        std::cout << "Program::Program() failed (" << err << ")\n";
        exit(SDK_FAILURE);
    }

    err = program.build(devices);
    if (err != CL_SUCCESS) {

        if(err == CL_BUILD_PROGRAM_FAILURE)
        {
            cl::string str = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(devices[0]);

            std::cout << " \n\t\t\tBUILD LOG\n";
            std::cout << " ************************************************\n";
            std::cout << str.c_str() << std::endl;
            std::cout << " ************************************************\n";
        }

        std::cout << "Program::build() failed (" << err << ")\n";
        exit(SDK_FAILURE);
    }

    kernel_cost = cl::Kernel(program, "cost", &err);
    if (err != CL_SUCCESS) {
        std::cout << "Kernel::Kernel() failed (" << err << ")\n";
        exit(SDK_FAILURE);
    }

    kernel_pen_Mmax = cl::Kernel(program, "penalization_Mmax", &err);
    if (err != CL_SUCCESS) {
        std::cout << "Kernel::Kernel() failed (" << err << ")\n";
        exit(SDK_FAILURE);
    }

    // queue profiling enabled
    queue = cl::CommandQueue(context, devices[0], CL_QUEUE_PROFILING_ENABLE, &err);
    if (err != CL_SUCCESS) {
        std::cout << "CommandQueue::CommandQueue() failed (" << err << ")\n";
        exit(SDK_FAILURE);
    }

    // fixed input buffers

    buf_cl_params = cl::Buffer(context,
    					CL_MEM_READ_ONLY,
    					sizeof(ClParams),
    					NULL,
    					&err);

    buf_incidence_matrix = cl::Buffer(context,
    					CL_MEM_READ_ONLY,
    					sizeof(int)*n_parts*n_machines,
    					NULL,
    					&err);

    // out buffer
    buf_out_cost = cl::Buffer(context,
    					CL_MEM_WRITE_ONLY,
    					sizeof(cl_uint),
    					NULL, &err);

    cl_int status;
    cl::Event writeEvt1,writeEvt2,writeEvt3,writeEvt4,writeEvt5;

    cl_uint cost = 0;
    status = queue.enqueueWriteBuffer(
    		buf_out_cost,
    		CL_FALSE,
    		0,
    		sizeof(cl_uint),
    		(void *)&cost,
    		NULL,
    		&writeEvt1);

    CHECK_OPENCL_ERROR(status, "CommandQueue::enqueueWriteBuffer() failed. (buf_out_cost)");

    ClParams *params = new ClParams();
    params->max_machines_cell = max_machines_cell;
    params->n_cells = n_cells;
    params->n_machines = n_machines;
    params->n_parts = n_parts;

    status = queue.enqueueWriteBuffer(
    		buf_cl_params,
    		CL_FALSE,
    		0,
    		sizeof(ClParams),
    		(void *)params,
    		NULL,
    		&writeEvt2);

    CHECK_OPENCL_ERROR(status, "CommandQueue::enqueueWriteBuffer() failed. (buf_max_machines_cell)");

	StaticMatrix *static_incidence = matrix_to_StaticMatrix(incidence_matrix);

    status = queue.enqueueWriteBuffer(
    		buf_incidence_matrix,
    		CL_FALSE,
    		0,
    		sizeof(int)*n_parts*n_machines,
    		(void *)(static_incidence->storage),
    		NULL,
    		&writeEvt5);

    CHECK_OPENCL_ERROR(status, "CommandQueue::enqueueWriteBuffer() failed. (buf_incidence_matrix_storage)");

    status = queue.flush();
    CHECK_OPENCL_ERROR(status, "cl::CommandQueue.flush failed.");

    std::vector<cl::Event> events_write;
    events_write.push_back(writeEvt1);
    events_write.push_back(writeEvt2);
    events_write.push_back(writeEvt5);

    cl::WaitForEvents(events_write);


    //std::cout << "delete" << std::endl;
    delete[] static_incidence->storage;
    delete static_incidence;


    delete params;

    //std::cout << "return" << std::endl;
    return SDK_SUCCESS;

}

long ParallelSolver::get_cpu_cost(Solution* solution) {

	long cost = 0;

	std::vector<std::vector<int> > machines_in_cells(n_cells,std::vector<int>());
	std::vector<std::vector<int> > machines_not_in_cells(n_cells,std::vector<int>());
	for(unsigned int k=0;k<n_cells;k++){
		for(unsigned int i=0;i<n_machines;i++){
			if(solution->cell_vector[i] == (signed int)k)
				machines_in_cells[k].push_back(i);
			else
				machines_not_in_cells[k].push_back(i);
		}
	}

	for(unsigned int k=0;k<n_cells;k++){ // sum k=1...C // celdas

		//------------penalizacion y_ik <= Mmax------------------

		int machines_cell = machines_in_cells[k].size();

		int difference = machines_cell - max_machines_cell;
		//int sign = 1 ^ ((unsigned int)difference >> 31); // if difference > 0 sign = 1 else sign = 0
		//cost += difference * n_parts * sign;
		cost += difference * n_parts * (difference > 0);

		// -------------------------------------------------------

		int machines_not_in = machines_not_in_cells[k].size();

		for(int i_n=0;i_n<machines_not_in;i_n++){ // máquinas no en celda

			int i = machines_not_in_cells[k][i_n]; // máquina no en celda
			int machines_in = machines_in_cells[k].size();
			int parts = parts_machines[i].size();

			for(int j_=0;j_<parts;j_++){ // partes de máquinas no en la celda

				int j = parts_machines[i][j_]; // parte de máquina no en celda

				for(int i_in=0;i_in<machines_in;i_in++){

					int i_ = machines_in_cells[k][i_in]; // máquina en celda

					// costo+ si la máquina en celda tiene la parte que también es de la máquina no en celda
					cost += incidence_matrix->getMatrix()[i_][j];
				}
			}
		}
	}

	for(unsigned int j=0;j<n_parts;j++){

		int in_cells = 0;

		for(unsigned int k=0;k<n_cells;k++){

			int machines_in = machines_in_cells[k].size();

			for(int i_in=0;i_in<machines_in;i_in++){

				int i_ = machines_in_cells[k][i_in]; // máquina en celda
				int parts = parts_machines[i_].size();

				for(int j_in=0;j_in<parts;j_in++){ // partes de máquinas en la celda

					int j_ = parts_machines[i_][j_in]; // parte de máquina en celda

					//in_cells += equal(j,j_);
					in_cells += (j==(unsigned)j_);
				}
			}
		}

		// cost+=n_parts*in_cells para in_cells != 1
		//cost += (in_cells - 1) * n_parts * (-1*zero(in_cells));
		cost += (in_cells - 1) * n_parts * (in_cells > 0);
//		if(in_cells > 0)
//			std::cout << "in cells " << in_cells << std::endl;
	}

	return cost;
}

void ParallelSolver::init() {

	OpenCL_init();
	Solver::init();
}

long ParallelSolver::get_cost(Solution* solution) {

	cl_uint cost=0;
    cl_int err;

    /////////////////////////runCLKernels////////////////////////////////////////

    cl_int status;

    cl::Event writeEvt1,writeEvt2;

    buf_sol = cl::Buffer(context,
    					CL_MEM_READ_ONLY,
    					sizeof(cl_int)*n_machines,
    					NULL,
    					&err);

    status = queue.enqueueWriteBuffer(
    		buf_sol,
    		CL_FALSE,
    		0,
    		sizeof(cl_uint)*n_machines,
    		(void *)(solution->cell_vector),
    		NULL,
    		&writeEvt1);

    CHECK_OPENCL_ERROR(status, "CommandQueue::enqueueWriteBuffer() failed. (buf_sol)");

    status = queue.enqueueWriteBuffer(
    		buf_out_cost,
    		CL_FALSE,
    		0,
    		sizeof(cl_uint),
    		(void *)&cost,
    		NULL,
    		&writeEvt1);

    CHECK_OPENCL_ERROR(status, "CommandQueue::enqueueWriteBuffer() failed. (buf_out_cost)");

    status = queue.flush();
    CHECK_OPENCL_ERROR(status, "cl::CommandQueue.flush failed.");

    // set kernel args

    // kernel cost
    status = kernel_cost.setArg(0, sizeof(cl_uint*),&buf_out_cost);
    CHECK_OPENCL_ERROR(status, "Kernel::setArg() failed. (buf_out_cost)");

    status = kernel_cost.setArg(1, sizeof(ClParams*), &buf_cl_params);
    CHECK_OPENCL_ERROR(status, "Kernel::setArg() failed. (buf_cl_params)");

    status = kernel_cost.setArg(2, sizeof(cl_int*),&buf_incidence_matrix);
    CHECK_OPENCL_ERROR(status, "Kernel::setArg() failed. (buf_incidence_matrix)");

    status = kernel_cost.setArg(3, sizeof(cl_int*),&buf_sol);
    CHECK_OPENCL_ERROR(status, "Kernel::setArg() failed. (buf_sol)");


    // kernel penalización Mmax
    status = kernel_pen_Mmax.setArg(0, sizeof(cl_uint*),&buf_out_cost);
    CHECK_OPENCL_ERROR(status, "Kernel::setArg() failed. (buf_out_cost)");

    status = kernel_pen_Mmax.setArg(1, sizeof(ClParams*), &buf_cl_params);
    CHECK_OPENCL_ERROR(status, "Kernel::setArg() failed. (buf_cl_params)");

    status = kernel_pen_Mmax.setArg(2, sizeof(cl_int*),&buf_sol);
    CHECK_OPENCL_ERROR(status, "Kernel::setArg() failed. (buf_sol)");

    cl::NDRange globalThreads_cost(n_machines, n_parts, n_cells);
    cl::NDRange globalThreads_pen_Mmax(n_cells);
    cl::Event ndrEvt;
    cl::Event kernel_costos_evt;
    cl::Event kernel_penMmax_evt;

    // Running CL program

    // esperar  eventos de grabado ----------------------------------------------

    std::vector<cl::Event> events_write;
    events_write.push_back(writeEvt1);
    events_write.push_back(writeEvt2);
    cl::WaitForEvents(events_write);

    // fin esperar  eventos de grabado ----------------------------------------------


    // kernel de costos ---------------------------------
    err = queue.enqueueNDRangeKernel(
    		kernel_cost,cl::NullRange, globalThreads_cost, cl::NullRange, 0, &kernel_costos_evt
    );

    if (err != CL_SUCCESS) {
        std::cout << "CommandQueue::enqueueNDRangeKernel()" \
            " failed (" << err << ")\n";
       return SDK_FAILURE;
    }

    // kernel penalización Mmax ------------------------------
    err = queue.enqueueNDRangeKernel(
    		kernel_pen_Mmax,cl::NullRange, globalThreads_pen_Mmax, cl::NullRange, 0, &kernel_penMmax_evt
    );

    if (err != CL_SUCCESS) {
        std::cout << "CommandQueue::enqueueNDRangeKernel()" \
            " failed (" << err << ")\n";
       return SDK_FAILURE;
    }

    //std::cout << "flush kernel completion" << std::endl;
    status = queue.flush();
     CHECK_OPENCL_ERROR(status, "cl::CommandQueue.flush failed.");

     // kernel time ------------------------------------------------------

     std::vector<cl::Event> events;
     events.push_back(kernel_costos_evt);
     events.push_back(kernel_penMmax_evt);

     cl::WaitForEvents(events);

     cl_ulong time_start, time_end;
     double total_time = 0;

     status = kernel_costos_evt.getProfilingInfo(CL_PROFILING_COMMAND_START, &time_start);
     CHECK_OPENCL_ERROR(status,"Error : CL_PROFILING_COMMAND_START");
     status = kernel_costos_evt.getProfilingInfo(CL_PROFILING_COMMAND_END, &time_end);
     CHECK_OPENCL_ERROR(status,"Error : CL_PROFILING_COMMAND_END");

     total_time = time_end - time_start;

     status = kernel_penMmax_evt.getProfilingInfo(CL_PROFILING_COMMAND_START, &time_start);
     CHECK_OPENCL_ERROR(status,"Error : CL_PROFILING_COMMAND_START");
     status = kernel_penMmax_evt.getProfilingInfo(CL_PROFILING_COMMAND_END, &time_end);
     CHECK_OPENCL_ERROR(status,"Error : CL_PROFILING_COMMAND_END");

     total_time += time_end - time_start;

     iter_cost_time += total_time;

     // --------------------------------------------------------------------------

     // Enqueue readBuffer
     cl::Event readEvt1;
     status = queue.enqueueReadBuffer(
    		 	 buf_out_cost,
                 CL_FALSE,
                 0,
                 sizeof(cl_uint),
                 (void *)&cost,
                 NULL,
                 &readEvt1);
     CHECK_OPENCL_ERROR(status, "CommandQueue::enqueueReadBuffer failed. (buf_out_cost)");

     status = queue.flush();
     CHECK_OPENCL_ERROR(status, "cl::CommandQueue.flush failed.");

     std::vector<cl::Event> events_read;
     events_read.push_back(readEvt1);

     cl::WaitForEvents(events_read);

    return (long)cost;
}

} /* namespace tabu */
