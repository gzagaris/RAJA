/*
 * Copyright (c) 2016, Lawrence Livermore National Security, LLC.
 * Produced at the Lawrence Livermore National Laboratory.
 *
 * All rights reserved.
 *
 * This source code cannot be distributed without permission and
 * further review from Lawrence Livermore National Laboratory.
 */

#ifndef RAJA_forallN_cuda_HXX__
#define RAJA_forallN_cuda_HXX__

#include<RAJA/config.hxx>
#include<RAJA/int_datatypes.hxx>

namespace RAJA {


struct CudaDim {
  dim3 num_threads;
  dim3 num_blocks;
  
  __host__ __device__ void print(void) const {
    printf("<<< (%d,%d,%d), (%d,%d,%d) >>>\n",
      num_blocks.x, num_blocks.y, num_blocks.z,
      num_threads.x, num_threads.y, num_threads.z);
  }
};


template<typename POL>
struct CudaPolicy {};

template<typename VIEWDIM, int threads_per_block>
struct CudaThreadBlock {
  int begin;
  int end;
  
  VIEWDIM view;
  
  CudaThreadBlock(CudaDim &dims, RangeSegment const &is) : 
    begin(is.getBegin()), end(is.getEnd())
  {  
    setDims(dims);
  }

  __device__ inline int operator()(void){
    
    int idx = begin + view(blockIdx) * threads_per_block + view(threadIdx);
    if(idx >= end){
      idx = -1;
    }
    return idx;
  }
  
  void inline setDims(CudaDim &dims){
    int n = end-begin;
    if(n < threads_per_block){
      view(dims.num_threads) = n;
      view(dims.num_blocks) = 1;
    }
    else{
      view(dims.num_threads) = threads_per_block;
      
      int blocks = n / threads_per_block;
      if(n % threads_per_block){
        ++ blocks;
      }
      view(dims.num_blocks) = blocks;
    }
  }  
};

template<typename VIEW, int THREADS>
using cuda_threadblock_exec = CudaPolicy<CudaThreadBlock<VIEW, THREADS>>;


// Simple launcher, that maps thread (x,y) to indices
template <typename BODY, typename ... CARGS>
__global__ void cudaLauncherN(BODY body, CARGS ... cargs){
  //int i = ci();
  //int j = cj();
  //if(i >= 0 && j >= 0){
    body((cargs())...);
  //}
}


template<int ...> struct integer_sequence {};

template<int N, int ...S> struct gen_sequence : gen_sequence<N-1, N-1, S...> {};

template<int ...S> struct gen_sequence<0, S...>{ typedef integer_sequence<S...> type; };

template<typename CuARG0, typename ISET0, typename ... CuARGS, typename ... ISETS>
struct ForallN_Executor< 
  ForallN_PolicyPair<CudaPolicy<CuARG0>, ISET0>,
  ForallN_PolicyPair<CudaPolicy<CuARGS>, ISETS>... > 
{

  ForallN_PolicyPair<CudaPolicy<CuARG0>, ISET0> iset0;  
  std::tuple<ForallN_PolicyPair<CudaPolicy<CuARGS>, ISETS>...> isets;
  
  ForallN_Executor(
    ForallN_PolicyPair<CudaPolicy<CuARG0>, ISET0> const & iset0_, 
    ForallN_PolicyPair<CudaPolicy<CuARGS>, ISETS> const & ...isets_) 
    :  iset0(iset0_), isets(isets_...) 
  { }

  template<typename BODY>
  RAJA_INLINE
  void operator()(BODY body) const {
    unpackIndexSets(body, typename gen_sequence<sizeof...(ISETS)>::type()); 
  }
  
  template<typename BODY, int ... N>
  RAJA_INLINE
  void unpackIndexSets(BODY body, integer_sequence<N...>) const {
  
    CudaDim dims;
    
    callLauncher(dims, body, CuARG0(dims, iset0), CuARGS(dims, std::get<N>(isets))...);
  }
  
  
  template<typename BODY, typename ... CARGS>
  RAJA_INLINE
  void callLauncher(CudaDim const &dims, BODY body, CARGS const &... cargs) const {
  dims.print();
    cudaLauncherN<<<dims.num_blocks, dims.num_threads>>>(body, cargs...);
    cudaErrchk(cudaPeekAtLastError());
    cudaErrchk(cudaDeviceSynchronize());
  }
};

template<typename CuARG0, typename ISET0>
struct ForallN_Executor< 
  ForallN_PolicyPair<CudaPolicy<CuARG0>, ISET0> > 
{
  ISET0 iset0;
  
  ForallN_Executor(
    ForallN_PolicyPair<CudaPolicy<CuARG0>, ISET0> const & iset0_) 
    :  iset0(iset0_) 
  { }

  template<typename BODY>
  RAJA_INLINE
  void operator()(BODY body) const {
    CudaDim dims;
    CuARG0 c0(dims, iset0);
    
    cudaLauncherN<<<dims.num_blocks, dims.num_threads>>>(body, c0);
    cudaErrchk(cudaPeekAtLastError());
    cudaErrchk(cudaDeviceSynchronize());
  }
};




} // namespace RAJA
  
#endif

