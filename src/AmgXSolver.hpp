/**
 * \file AmgXSolver.hpp
 * \brief Definition of class AmgXSolver.
 * \author Pi-Yueh Chuang (pychuang@gwu.edu)
 * \date 2015-09-01
 * \copyright Copyright (c) 2015-2019 Pi-Yueh Chuang, Lorena A. Barba.
 *            This project is released under MIT License.
 */


# pragma once

// STL
# include <string>
# include <vector>

// AmgX
# include <amgx_c.h>

// PETSc
# include <petscmat.h>
# include <petscvec.h>


/** \brief A macro to check the returned CUDA error code.
 *
 * \param call [in] Function call to CUDA API.
 */
# define CHECK(call)                                                        \
{                                                                           \
    const cudaError_t       error = call;                                   \
    if (error != cudaSuccess)                                               \
    {                                                                       \
        SETERRQ4(PETSC_COMM_WORLD, PETSC_ERR_SIG,                           \
            "Error: %s:%d, code:%d, reason: %s\n",                          \
            __FILE__, __LINE__, error, cudaGetErrorString(error));          \
    }                                                                       \
}


/** \brief A shorter macro for checking PETSc returned error code. */
# define CHK CHKERRQ(ierr)


/** \brief A wrapper class for coupling PETSc and AmgX.
 *
 * This class is a wrapper of AmgX library for PETSc. PETSc users only need to
 * pass a PETSc matrix and vectors into an AmgXSolver instance to solve their
 * linear systems. The class is designed specifically for the situation where
 * the number of MPI processes is more than the number of GPU devices.
 *
 * Eaxmple usage:
 * \code
 * int main(int argc, char **argv)
 * {
 *     // initialize matrix A, RHS, etc using PETSc
 *     ...
 *
 *     // create an instance of the solver wrapper
 *     AmgXSolver    solver;
 *     // initialize the instance with communicator, executation mode, and config file
 *     solver.initialize(comm, mode, file);
 *     // set matrix A. Currently it only accept PETSc AIJ matrix
 *     solver.setA(A);
 *     // solve. x and rhs are PETSc vectors. unkns will be the final result in the end
 *     solver.solve(unks, rhs);
 *     // get number of iterations
 *     int         iters;
 *     solver.getIters(iters);
 *     // get residual at the last iteration
 *     double      res;
 *     solver.getResidual(iters, res);
 *     // finalization
 *     solver.finalize();
 *
 *     // other codes
 *     ....
 *
 *     return 0;
 * }
 * \endcode
 */
class AmgXSolver
{
    public:

        /** \brief Default constructor. */
        AmgXSolver() = default;


        /** \brief Construct a AmgXSolver instance.
         *
         * \param comm [in] MPI communicator.
         * \param modeStr [in] A string; target mode of AmgX (e.g., dDDI).
         * \param cfgFile [in] A string; the path to AmgX configuration file.
         */
        AmgXSolver(const MPI_Comm &comm,
                const std::string &modeStr, const std::string &cfgFile);


        /** \brief Destructor. */
        ~AmgXSolver();


        /** \brief Initialize a AmgXSolver instance.
         *
         * \param comm [in] MPI communicator.
         * \param modeStr [in] A string; target mode of AmgX (e.g., dDDI).
         * \param cfgFile [in] A string; the path to AmgX configuration file.
         *
         * \return PetscErrorCode.
         */
        PetscErrorCode initialize(const MPI_Comm &comm,
                const std::string &modeStr, const std::string &cfgFile);


        /** \brief Finalize this instance.
         *
         * This function destroys AmgX data. When there are more than one
         * AmgXSolver instances, the last one destroyed is also in charge of
         * destroying the shared resource object and finalizing AmgX.
         *
         * \return PetscErrorCode.
         */
        PetscErrorCode finalize();


        /** \brief Set up the matrix used by AmgX.
         *
         * This function will automatically convert PETSc matrix to AmgX matrix.
         * If the number of MPI processes is higher than the number of available
         * GPU devices, we also redistribute the matrix in this function and
         * upload redistributed one to GPUs.
         *
         * Note: currently we can only handle AIJ/MPIAIJ format.
         *
         * \param A [in] A PETSc Mat.
         *
         * \return PetscErrorCode.
         */
        PetscErrorCode setA(const Mat &A);


        /** \brief Solve the linear system.
         *
         * \p p vector will be used as an initial guess and will be updated to the
         * solution by the end of solving.
         *
         * For cases that use more MPI processes than the number of GPUs, this
         * function will do data gathering before solving and data scattering
         * after the solving.
         *
         * \param p [in, out] A PETSc Vec object representing unknowns.
         * \param b [in] A PETSc Vec representing right-hand-side.
         *
         * \return PetscErrorCode.
         */
        PetscErrorCode solve(Vec &p, Vec &b);


        /** \brief Get the number of iterations of the last solving.
         *
         * \param iter [out] Number of iterations.
         *
         * \return PetscErrorCode.
         */
        PetscErrorCode getIters(int &iter);


        /** \brief Get the residual at a specific iteration during the last solving.
         *
         * \param iter [in] Target iteration.
         * \param res [out] Returned residual.
         *
         * \return PetscErrorCode.
         */
        PetscErrorCode getResidual(const int &iter, double &res);


    private:

        /** \brief Current count of AmgXSolver instances.
         *
         * This static variable is used to count the number of instances. The
         * fisrt instance is responsable for initializing AmgX library and the
         * resource instance.
         */
        static int              count;

        /** \brief A flag indicating if this instance has been initialized. */
        bool                    isInitialized = false;

        /** \brief The name of the node that this MPI process belongs to. */
        std::string             nodeName;




        /** \brief Number of local GPU devices used by AmgX.*/
        PetscMPIInt             nDevs;

        /** \brief The ID of corresponding GPU device used by this MPI process. */
        PetscMPIInt             devID;

        /** \brief A flag indicating if this process will talk to GPU. */
        PetscMPIInt             gpuProc = MPI_UNDEFINED;

        /** \brief A communicator for global world. */
        MPI_Comm                globalCpuWorld;

        /** \brief A communicator for local world (i.e., in-node). */
        MPI_Comm                localCpuWorld;

        /** \brief A communicator for MPI processes that can talk to GPUs. */
        MPI_Comm                gpuWorld;

        /** \brief A communicator for processes sharing the same devices. */
        MPI_Comm                devWorld;

        /** \brief Size of \ref AmgXSolver::globalCpuWorld "globalCpuWorld". */
        PetscMPIInt             globalSize;

        /** \brief Size of \ref AmgXSolver::localCpuWorld "localCpuWorld". */
        PetscMPIInt             localSize;

        /** \brief Size of \ref AmgXSolver::gpuWorld "gpuWorld". */
        PetscMPIInt             gpuWorldSize;

        /** \brief Size of \ref AmgXSolver::devWorld "devWorld". */
        PetscMPIInt             devWorldSize;

        /** \brief Rank in \ref AmgXSolver::globalCpuWorld "globalCpuWorld". */
        PetscMPIInt             myGlobalRank;

        /** \brief Rank in \ref AmgXSolver::localCpuWorld "localCpuWorld". */
        PetscMPIInt             myLocalRank;

        /** \brief Rank in \ref AmgXSolver::gpuWorld "gpuWorld". */
        PetscMPIInt             myGpuWorldRank;

        /** \brief Rank in \ref AmgXSolver::devWorld "devWorld". */
        PetscMPIInt             myDevWorldRank;




        /** \brief A parameter used by AmgX. */
        int                     ring;

        /** \brief AmgX solver mode. */
        AMGX_Mode               mode;

        /** \brief AmgX config object. */
        AMGX_config_handle      cfg = nullptr;

        /** \brief AmgX matrix object. */
        AMGX_matrix_handle      AmgXA = nullptr;

        /** \brief AmgX vector object representing unknowns. */
        AMGX_vector_handle      AmgXP = nullptr;

        /** \brief AmgX vector object representing RHS. */
        AMGX_vector_handle      AmgXRHS = nullptr;

        /** \brief AmgX solver object. */
        AMGX_solver_handle      solver = nullptr;

        /** \brief AmgX resource object.
         *
         * Due to the design of AmgX library, using more than one resource
         * instance may cause some problems. So we make the resource instance
         * as a static member to keep only one instance.
         */
        static AMGX_resources_handle   rsrc;




        /** \brief A VecScatter for gathering/scattering between original PETSc
         *         Vec and temporary redistributed PETSc Vec.*/
        VecScatter              scatterLhs = nullptr;

        /** \brief A VecScatter for gathering/scattering between original PETSc
         *         Vec and temporary redistributed PETSc Vec.*/
        VecScatter              scatterRhs = nullptr;

        /** \brief A temporary PETSc Vec holding redistributed unknowns. */
        Vec                     redistLhs = nullptr;

        /** \brief A temporary PETSc Vec holding redistributed RHS. */
        Vec                     redistRhs = nullptr;




        /** \brief Set AmgX solver mode based on the user-provided string.
         *
         * Available modes are: dDDI, dDFI, dFFI, hDDI, hDFI, hFFI.
         *
         * \param modeStr [in] a std::string.
         * \return PetscErrorCode.
         */
        PetscErrorCode setMode(const std::string &modeStr);


        /** \brief Get the number of GPU devices on this computing node.
         *
         * \return PetscErrorCode.
         */
        PetscErrorCode setDeviceCount();


        /** \brief Set the ID of the corresponding GPU used by this process.
         *
         * \return PetscErrorCode.
         */
        PetscErrorCode setDeviceIDs();


        /** \brief Initialize all MPI communicators.
         *
         * The \p comm provided will be duplicated and saved to the
         * \ref AmgXSolver::globalCpuWorld "globalCpuWorld".
         *
         * \param comm [in] Global communicator.
         * \return PetscErrorCode.
         */
        PetscErrorCode initMPIcomms(const MPI_Comm &comm);


        /** \brief Perform necessary initialization of AmgX.
         *
         * This function initializes AmgX for current instance. Based on
         * \ref AmgXSolver::count "count", only the instance initialized first
         * is in charge of initializing AmgX and the resource instance.
         *
         * \param cfgFile [in] Path to AmgX solver configuration file.
         * \return PetscErrorCode.
         */
        PetscErrorCode initAmgX(const std::string &cfgFile);


        /** \brief Get IS for the row indices that processes in
         *      \ref AmgXSolver::gpuWorld "gpuWorld" will held.
         *
         * \param A [in] PETSc matrix.
         * \param devIS [out] PETSc IS.
         * \return PetscErrorCode.
         */
        PetscErrorCode getDevIS(const Mat &A, IS &devIS);


        /** \brief Get local sequential PETSc Mat of the redistributed matrix.
         *
         * \param A [in] Original PETSc Mat.
         * \param devIS [in] PETSc IS representing redistributed row indices.
         * \param localA [out] Local sequential redistributed matrix.
         * \return PetscErrorCode.
         */
        PetscErrorCode getLocalA(const Mat &A, const IS &devIS, Mat &localA);


        /** \brief Redistribute matrix.
         *
         * \param A [in] Original PETSc Mat object.
         * \param devIS [in] PETSc IS representing redistributed rows.
         * \param newA [out] Redistributed matrix.
         * \return PetscErrorCode.
         */
        PetscErrorCode redistMat(const Mat &A, const IS &devIS, Mat &newA);


        /** \brief Get \ref AmgXSolver::scatterLhs "scatterLhs" and
         *      \ref AmgXSolver::scatterRhs "scatterRhs".
         *
         * \param A1 [in] Original PETSc Mat object.
         * \param A2 [in] Redistributed PETSc Mat object.
         * \param devIS [in] PETSc IS representing redistributed row indices.
         * \return PetscErrorCode.
         */
        PetscErrorCode getVecScatter(const Mat &A1, const Mat &A2, const IS &devIS);


        /** \brief Get data of compressed row layout of local sparse matrix.
         *
         * \param localA [in] Sequential local redistributed PETSc Mat.
         * \param localN [out] Number of local rows.
         * \param row [out] Row vector in compressed row layout.
         * \param col [out] Column vector in compressed row layout.
         * \param data [out] Data vector in compressed row layout.
         * \return PetscErrorCode.
         */
        PetscErrorCode getLocalMatRawData(const Mat &localA,
                PetscInt &localN, std::vector<PetscInt> &row,
                std::vector<PetscInt64> &col, std::vector<PetscScalar> &data);


        /** \brief Destroy the sequential local redistributed matrix.
         *
         * \param A [in] Original PETSc Mat.
         * \param localA [in, out] Local matrix, returning null pointer.
         * \return PetscErrorCode.
         */
        PetscErrorCode destroyLocalA(const Mat &A, Mat &localA);


        /** \brief Get a partition vector required by AmgX.
         *
         * \param devIS [in] PETSc IS representing redistributed row indices.
         * \param N [in] Total number of rows in global matrix.
         * \param partVec [out] Partition vector.
         * \return PetscErrorCode.
         */
        PetscErrorCode getPartVec(const IS &devIS,
                const PetscInt &N, std::vector<PetscInt> &partVec);


        /** \brief Function that actually solves the system.
         *
         * This function won't check if the process is involved in AmgX solver.
         * So if calling this function from processes not in the
         * \ref AmgXSolver::gpuWorld "gpuWorld", something bad will happen.
         * This function hence won't do data gathering/scattering, either.
         *
         * \param p [in, out] PETSc Vec for unknowns.
         * \param b [in] PETSc Vec for RHS.
         * \return PetscErrorCode.
         */
        PetscErrorCode solve_real(Vec &p, Vec &b);
};
