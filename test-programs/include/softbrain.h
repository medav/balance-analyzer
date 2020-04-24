#pragma once

#include <stdint.h>

typedef unsigned long long int bound_t;

void SB_CONFIG() { }
void SB_WAIT() { }

void SB_MEM_PORT_STREAM(
    const void * mem_addr,
    uint16_t stride,
    uint16_t access_size,
    uint16_t nstrides,
    int inport) { }

void SB_CONSTANT(
    int inport,
    uint64_t value,
    uint16_t nelems) { }

void SB_PORT_MEM_STREAM(
    int outport,
    uint16_t stride,
    uint16_t access_size,
    uint16_t nstrides,
    void * mem_addr) { }

void SB_DISCARD(
    int outport,
    uint16_t nelems) { }

void SB_MEM_SCRATCH_STREAM(
    const void * mem_addr,
    uint16_t stride,
    uint16_t access_size,
    uint16_t nstrides,
    int addr) { }

#define SB_SCRATCH_PORT_STREAM(addr, stride, access_size, num_iters, port) \
    SB_MEM_PORT_STREAM(addr, stride, access_size, num_iters, port)

#define SBBAR_SCRATCH_WRITE 0
#define SBBAR_SCRATCH_READ 1
void SB_BARRIER(int type) { }
